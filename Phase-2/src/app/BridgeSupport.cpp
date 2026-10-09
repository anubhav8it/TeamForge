#include "BridgeSupport.h"

#include "core/Errors.h"

#include <QMetaType>

#include <algorithm>

using namespace teamforge;

namespace bridge {

namespace {

// Initials of a multi-word skill: "machine learning" -> "ml", "ui design" -> "ud".
std::string initialsOf(const std::string& skill)
{
    std::string initials;
    bool atWordStart = true;
    for (char c : skill) {
        const bool separator = c == ' ' || c == '/' || c == '-';
        if (!separator && atWordStart)
            initials.push_back(c);
        atWordStart = separator;
    }
    return initials;
}

} // namespace

bool skillMatchesQuery(const std::string& skill, const std::string& query)
{
    if (skill.rfind(query, 0) == 0)
        return true;
    for (std::size_t i = 1; i < skill.size(); ++i) {
        const char before = skill[i - 1];
        if ((before == ' ' || before == '/' || before == '-') && skill.compare(i, query.size(), query) == 0)
            return true;
    }
    const std::string initials = initialsOf(skill);
    return initials.size() >= 2 && initials == query;
}
QVariantList skillLevelList(const SkillLevels& levels, const char *levelKey)
{
    QVariantList list;
    for (const auto& [skill, level] : levels)
        list.append(QVariantMap{{QStringLiteral("skill"), qs(skill)}, {QString::fromLatin1(levelKey), level}});
    return list;
}

QString experienceBand(double averageLevel)
{
    return averageLevel >= 4.0   ? QStringLiteral("Advanced")
           : averageLevel >= 3.0 ? QStringLiteral("Intermediate")
           : averageLevel >= 2.0 ? QStringLiteral("Developing")
                                 : QStringLiteral("Beginner");
}

QVariantMap studentToMap(const Student& student)
{
    double levelSum = 0.0;
    for (const auto& entry : student.skillsOffered())
        levelSum += entry.second;
    const double average = levelSum / static_cast<double>(student.skillsOffered().size());
    // Strongest skills first (ties keep name order), for compact rows.
    std::vector<std::pair<std::string, int>> ranked(student.skillsOffered().begin(), student.skillsOffered().end());
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    QStringList topSkills;
    for (std::size_t i = 0; i < ranked.size() && i < 3; ++i)
        topSkills.append(qs(ranked[i].first));
    return {
        {QStringLiteral("topSkills"), topSkills},
        {QStringLiteral("experience"), experienceBand(average)},
        {QStringLiteral("id"), qs(student.id())},
        {QStringLiteral("name"), qs(student.name())},
        {QStringLiteral("role"), qs(student.role())},
        {QStringLiteral("summary"), qs(student.summary())},
        {QStringLiteral("program"), qs(student.program())},
        {QStringLiteral("skillsOffered"), skillLevelList(student.skillsOffered(), "level")},
        {QStringLiteral("averageLevel"), average},
        {QStringLiteral("skillsWanted"), toQStringList(student.skillsWanted())},
    };
}

int wholeNumber(const QVariant& value, const std::string& what)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    const auto whole = static_cast<int>(number);
    if (!ok || static_cast<double>(whole) != number)
        throw ValidationError(what + " must be a whole number");
    return whole;
}

SkillLevels skillLevelsFrom(const QVariant& value, const char *levelKey)
{
    SkillLevels result;
    auto insert = [&](const std::string& skill, const QVariant& level) {
        if (!result.emplace(skill, wholeNumber(level, "Level for '" + skill + "'")).second)
            throw ValidationError("Skill '" + skill + "' is listed more than once");
    };
    if (value.typeId() == QMetaType::QVariantMap) {
        const QVariantMap map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            insert(ss(it.key()), it.value());
    } else if (value.typeId() == QMetaType::QVariantList) {
        for (const QVariant& item : value.toList()) {
            const QVariantMap entry = item.toMap();
            insert(ss(entry.value(QStringLiteral("skill")).toString()), entry.value(QString::fromLatin1(levelKey)));
        }
    } else if (value.isValid()) {
        throw ValidationError("Skills must be an object of skill: level or a list of entries");
    }
    return result;
}

std::vector<std::string> stringsFrom(const QVariant& value)
{
    std::vector<std::string> result;
    for (const QString& item : value.toStringList())
        result.push_back(ss(item));
    return result;
}

Student studentFrom(const QVariantMap& map)
{
    return Student(ss(map.value(QStringLiteral("id")).toString()),
                   ss(map.value(QStringLiteral("name")).toString()),
                   ss(map.value(QStringLiteral("role")).toString()),
                   skillLevelsFrom(map.value(QStringLiteral("skillsOffered")), "level"),
                   stringsFrom(map.value(QStringLiteral("skillsWanted"))),
                   ss(map.value(QStringLiteral("summary")).toString()),
                   ss(map.value(QStringLiteral("program")).toString()));
}

ProjectRequirement requirementFrom(const QVariantMap& map)
{
    return ProjectRequirement(ss(map.value(QStringLiteral("id")).toString()),
                              ss(map.value(QStringLiteral("name")).toString()),
                              skillLevelsFrom(map.value(QStringLiteral("requiredSkills")), "minLevel"),
                              wholeNumber(map.value(QStringLiteral("minTeamSize")), "Minimum team size"),
                              wholeNumber(map.value(QStringLiteral("maxTeamSize")), "Maximum team size"),
                              ss(map.value(QStringLiteral("type")).toString()),
                              ss(map.value(QStringLiteral("summary")).toString()));
}

} // namespace bridge
