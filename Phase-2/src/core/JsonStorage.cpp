#include "core/JsonStorage.h"

#include "core/Errors.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1String>
#include <QSaveFile>
#include <QString>

#include <set>
#include <utility>

namespace teamforge::storage {

namespace {

QString toQString(const std::string& text)
{
    return QString::fromStdString(text);
}

std::string toStdString(const QString& text)
{
    return text.toStdString();
}

QJsonArray readCollection(const std::string& path, const char *key)
{
    QFile file(toQString(path));
    if (!file.open(QIODevice::ReadOnly))
        throw PersistenceError("Cannot open '" + path + "': " + toStdString(file.errorString()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        throw PersistenceError("'" + path + "' is not valid JSON (offset "
                               + std::to_string(parseError.offset)
                               + "): " + toStdString(parseError.errorString()));
    }
    if (!document.isObject())
        throw PersistenceError("'" + path + "': top level must be a JSON object");

    const QJsonObject root = document.object();
    const int version = root.value(QLatin1String("version")).toInt(-1);
    if (version != kFormatVersion) {
        throw PersistenceError("'" + path + "': unsupported format version " + std::to_string(version)
                               + " (expected " + std::to_string(kFormatVersion) + ")");
    }
    const QJsonValue items = root.value(QLatin1String(key));
    if (!items.isArray())
        throw PersistenceError("'" + path + "': '" + key + "' must be an array");
    return items.toArray();
}

void writeCollection(const std::string& path, const char *key, const QJsonArray& items)
{
    QJsonObject root;
    root.insert(QLatin1String("version"), kFormatVersion);
    root.insert(QLatin1String(key), items);

    QSaveFile file(toQString(path));
    if (!file.open(QIODevice::WriteOnly))
        throw PersistenceError("Cannot write '" + path + "': " + toStdString(file.errorString()));
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
        throw PersistenceError("Failed to save '" + path + "': " + toStdString(file.errorString()));
}

// Field readers throw ValidationError; loadAll() adds the file/record context.

QJsonValue requireField(const QJsonObject& object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined())
        throw ValidationError(std::string("missing field '") + key + "'");
    return value;
}

std::string stringField(const QJsonObject& object, const char *key)
{
    const QJsonValue value = requireField(object, key);
    if (!value.isString())
        throw ValidationError(std::string("field '") + key + "' must be a string");
    return toStdString(value.toString());
}

int intValue(const QJsonValue& value, const std::string& what)
{
    const int asInt = value.toInt();
    if (!value.isDouble() || static_cast<double>(asInt) != value.toDouble())
        throw ValidationError(what + " must be an integer");
    return asInt;
}

int intField(const QJsonObject& object, const char *key)
{
    return intValue(requireField(object, key), std::string("field '") + key + "'");
}

// Optional: a missing field is an empty string.
std::string optionalStringField(const QJsonObject& object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined())
        return {};
    if (!value.isString())
        throw ValidationError(std::string("field '") + key + "' must be a string");
    return toStdString(value.toString());
}

// Optional: a missing field is an empty list.
std::vector<std::string> stringListField(const QJsonObject& object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isUndefined())
        return {};
    if (!value.isArray())
        throw ValidationError(std::string("field '") + key + "' must be an array of strings");
    std::vector<std::string> result;
    for (const QJsonValue& item : value.toArray()) {
        if (!item.isString())
            throw ValidationError(std::string("field '") + key + "' must be an array of strings");
        result.push_back(toStdString(item.toString()));
    }
    return result;
}

SkillLevels skillLevelsField(const QJsonObject& object, const char *key)
{
    const QJsonValue value = requireField(object, key);
    if (!value.isObject())
        throw ValidationError(std::string("field '") + key + "' must be an object of skill: level");
    SkillLevels result;
    const QJsonObject levels = value.toObject();
    for (auto it = levels.begin(); it != levels.end(); ++it) {
        const std::string skill = toStdString(it.key());
        result[skill] = intValue(it.value(), "level of '" + skill + "'");
    }
    return result;
}

QJsonArray toJsonArray(const std::vector<std::string>& items)
{
    QJsonArray array;
    for (const std::string& item : items)
        array.append(toQString(item));
    return array;
}

QJsonObject toJsonObject(const SkillLevels& levels)
{
    QJsonObject object;
    for (const auto& [skill, level] : levels)
        object.insert(toQString(skill), level);
    return object;
}

template <typename T, typename Parse>
std::vector<T> loadAll(const std::string& path, const char *key, const char *entity, Parse parse)
{
    const QJsonArray items = readCollection(path, key);
    std::vector<T> result;
    result.reserve(static_cast<std::size_t>(items.size()));
    std::set<std::string> seenIds;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const std::string context = "'" + path + "': " + entity + " #" + std::to_string(i + 1);
        if (!items.at(i).isObject())
            throw PersistenceError(context + " must be a JSON object");
        try {
            T item = parse(items.at(i).toObject());
            if (!seenIds.insert(item.id()).second)
                throw DuplicateError("duplicate id '" + item.id() + "'");
            result.push_back(std::move(item));
        } catch (const TeamForgeError& error) {
            throw PersistenceError(context + ": " + error.what());
        }
    }
    return result;
}

// One record <-> one JSON object. The file loaders/savers and the single-record functions
// (developer inspector) share these, so both use exactly the stored format.

Student parseStudent(const QJsonObject& o)
{
    // Files from before availability was removed may still carry "availability"; like any
    // unknown field it is ignored, and the next save drops it.
    return Student(stringField(o, "id"), stringField(o, "name"), stringField(o, "role"),
                   skillLevelsField(o, "skillsOffered"), stringListField(o, "skillsWanted"),
                   optionalStringField(o, "summary"), optionalStringField(o, "program"));
}

QJsonObject studentObject(const Student& s)
{
    QJsonObject o;
    o.insert(QLatin1String("id"), toQString(s.id()));
    o.insert(QLatin1String("name"), toQString(s.name()));
    o.insert(QLatin1String("role"), toQString(s.role()));
    if (!s.summary().empty())
        o.insert(QLatin1String("summary"), toQString(s.summary()));
    if (!s.program().empty())
        o.insert(QLatin1String("program"), toQString(s.program()));
    o.insert(QLatin1String("skillsOffered"), toJsonObject(s.skillsOffered()));
    o.insert(QLatin1String("skillsWanted"), toJsonArray({s.skillsWanted().begin(), s.skillsWanted().end()}));
    return o;
}

ProjectRequirement parseRequirement(const QJsonObject& o)
{
    // An old "timeSlots" field is ignored, like "availability" above.
    return ProjectRequirement(stringField(o, "id"), stringField(o, "name"),
                              skillLevelsField(o, "requiredSkills"), intField(o, "minTeamSize"),
                              intField(o, "maxTeamSize"), optionalStringField(o, "type"),
                              optionalStringField(o, "summary"));
}

QJsonObject requirementObject(const ProjectRequirement& r)
{
    QJsonObject o;
    o.insert(QLatin1String("id"), toQString(r.id()));
    o.insert(QLatin1String("name"), toQString(r.name()));
    o.insert(QLatin1String("type"), toQString(r.type()));
    if (!r.summary().empty())
        o.insert(QLatin1String("summary"), toQString(r.summary()));
    o.insert(QLatin1String("requiredSkills"), toJsonObject(r.requiredSkills()));
    o.insert(QLatin1String("minTeamSize"), static_cast<int>(r.minTeamSize()));
    o.insert(QLatin1String("maxTeamSize"), static_cast<int>(r.maxTeamSize()));
    return o;
}

QJsonObject teamObject(const Team& team)
{
    std::vector<std::string> memberIds;
    for (const Student& member : team.members())
        memberIds.push_back(member.id());
    QJsonObject o;
    o.insert(QLatin1String("id"), toQString(team.id()));
    o.insert(QLatin1String("requirementId"), toQString(team.requirement().id()));
    o.insert(QLatin1String("memberIds"), toJsonArray(memberIds));
    return o;
}

std::string toText(const QJsonObject& object)
{
    return toStdString(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)));
}

QJsonObject objectFromText(const std::string& json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        throw ValidationError("Not valid JSON (offset " + std::to_string(parseError.offset)
                              + "): " + toStdString(parseError.errorString()));
    }
    if (!document.isObject())
        throw ValidationError("A record must be a JSON object");
    return document.object();
}

} // namespace

std::vector<Student> loadStudents(const std::string& path)
{
    return loadAll<Student>(path, "students", "student", parseStudent);
}

void saveStudents(const std::string& path, const std::vector<Student>& students)
{
    QJsonArray items;
    for (const Student& s : students)
        items.append(studentObject(s));
    writeCollection(path, "students", items);
}

std::vector<ProjectRequirement> loadRequirements(const std::string& path)
{
    return loadAll<ProjectRequirement>(path, "requirements", "requirement", parseRequirement);
}

void saveRequirements(const std::string& path, const std::vector<ProjectRequirement>& requirements)
{
    QJsonArray items;
    for (const ProjectRequirement& r : requirements)
        items.append(requirementObject(r));
    writeCollection(path, "requirements", items);
}

std::vector<InterestRequest> loadInterests(const std::string& path)
{
    return loadAll<InterestRequest>(path, "requests", "interest request", [](const QJsonObject& o) {
        return InterestRequest(stringField(o, "id"), stringField(o, "studentId"), stringField(o, "requirementId"),
                               interestStatusFrom(stringField(o, "status")));
    });
}

void saveInterests(const std::string& path, const std::vector<InterestRequest>& requests)
{
    QJsonArray items;
    for (const InterestRequest& r : requests) {
        QJsonObject o;
        o.insert(QLatin1String("id"), toQString(r.id()));
        o.insert(QLatin1String("studentId"), toQString(r.studentId()));
        o.insert(QLatin1String("requirementId"), toQString(r.requirementId()));
        o.insert(QLatin1String("status"), toQString(interestStatusName(r.status())));
        items.append(o);
    }
    writeCollection(path, "requests", items);
}

SkillCatalog loadSkillCatalog(const std::string& path)
{
    const QJsonArray items = readCollection(path, "skills");
    SkillCatalog catalog;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const std::string context = "'" + path + "': skill #" + std::to_string(i + 1);
        try {
            if (!items.at(i).isObject())
                throw ValidationError("must be a JSON object");
            const QJsonObject o = items.at(i).toObject();
            const std::string name = normalizeSkill(stringField(o, "name"));
            if (!catalog.emplace(name, optionalStringField(o, "category")).second)
                throw DuplicateError("duplicate skill '" + name + "'");
        } catch (const TeamForgeError& error) {
            throw PersistenceError(context + ": " + error.what());
        }
    }
    return catalog;
}

void saveSkillCatalog(const std::string& path, const SkillCatalog& catalog)
{
    QJsonArray items;
    for (const auto& [name, category] : catalog) {
        QJsonObject o;
        o.insert(QLatin1String("name"), toQString(name));
        o.insert(QLatin1String("category"), toQString(category));
        items.append(o);
    }
    writeCollection(path, "skills", items);
}

std::string toJson(const Student& student)
{
    return toText(studentObject(student));
}

std::string toJson(const ProjectRequirement& requirement)
{
    return toText(requirementObject(requirement));
}

std::string toJson(const Team& team)
{
    return toText(teamObject(team));
}

Student studentFromJson(const std::string& json)
{
    return parseStudent(objectFromText(json));
}

ProjectRequirement requirementFromJson(const std::string& json)
{
    return parseRequirement(objectFromText(json));
}

std::string storedRecord(const std::string& path, const char *collection, const std::string& id,
                         const char *idKey)
{
    for (const QJsonValue& item : readCollection(path, collection)) {
        const QJsonObject object = item.toObject();
        if (object.value(QLatin1String(idKey)).toString() == toQString(id))
            return toText(object);
    }
    return {};
}

std::vector<Team> loadTeams(const std::string& path, const Repository<Student>& students,
                            const Repository<ProjectRequirement>& requirements)
{
    return loadAll<Team>(path, "teams", "team", [&](const QJsonObject& o) {
        Team team(stringField(o, "id"), requirements.get(stringField(o, "requirementId")));
        for (const std::string& memberId : stringListField(o, "memberIds"))
            team.addMember(students.get(memberId));
        return team;
    });
}

void saveTeams(const std::string& path, const std::vector<Team>& teams)
{
    QJsonArray items;
    for (const Team& team : teams)
        items.append(teamObject(team));
    writeCollection(path, "teams", items);
}

} // namespace teamforge::storage
