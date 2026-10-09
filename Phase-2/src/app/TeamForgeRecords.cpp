// Developer workspace: the complete record database (participants, projects, teams, skills), the
// skill catalogue and the raw-record inspector. Part of TeamForgeController. Queries return
// nothing outside the developer workspace; actions require it.
#include "TeamForgeController.h"

#include "BridgeSupport.h"

#include "core/Errors.h"
#include "core/JsonStorage.h"
#include "core/Skill.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <map>

using namespace teamforge;
using namespace bridge;

namespace {

QString normalisedQuery(const QString& query)
{
    return query.trimmed().toLower().replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
}

QString tick(bool ok)
{
    return ok ? QStringLiteral("✓") : QStringLiteral("✗");
}

QVariantMap tableRow(const QString& label, const QStringList& values, bool ok, bool isSkill = false)
{
    return {{QStringLiteral("label"), label},
            {QStringLiteral("values"), values},
            {QStringLiteral("ok"), ok},
            {QStringLiteral("skill"), isSkill}};
}

} // namespace

bool TeamForgeController::allowed(Permission permission) const
{
    try {
        require(permission);
        return true;
    } catch (const PermissionError&) {
        return false;
    }
}

// --- participants ----------------------------------------------------------------------------

QVariantMap TeamForgeController::queryParticipants(const QVariantMap& filter) const
{
    if (!allowed(Permission::DeveloperTools))
        return {};
    const QVariantList all = students(); // also builds searchCache_
    const QString q = normalisedQuery(filter.value(QStringLiteral("query")).toString());
    const std::string skillQuery = ss(q);
    const QString skillText = filter.value(QStringLiteral("skill")).toString().trimmed();
    std::string skill;
    try {
        skill = skillText.isEmpty() ? std::string() : normalizeSkill(ss(skillText));
    } catch (const TeamForgeError&) {
    }
    const QString experience = filter.value(QStringLiteral("experience")).toString();
    const QString sort = filter.value(QStringLiteral("sort")).toString();

    std::vector<int> hits;
    hits.reserve(searchCache_.size());
    for (std::size_t i = 0; i < searchCache_.size(); ++i) {
        const SearchEntry& entry = searchCache_[i];
        if (!skill.empty() && std::find(entry.skills.begin(), entry.skills.end(), skill) == entry.skills.end())
            continue;
        if (!experience.isEmpty() && experienceBand(entry.averageLevel) != experience)
            continue;
        bool match = q.isEmpty() || entry.name.contains(q) || entry.id.contains(q);
        for (std::size_t s = 0; !match && s < entry.skills.size(); ++s)
            match = skillMatchesQuery(entry.skills[s], skillQuery);
        if (match)
            hits.push_back(static_cast<int>(i));
    }
    // The cache is in name order; the other sorts are stable on top of it.
    const auto by = [this](auto key) {
        return [this, key](int a, int b) {
            return key(searchCache_[static_cast<std::size_t>(a)]) > key(searchCache_[static_cast<std::size_t>(b)]);
        };
    };
    if (sort == QLatin1String("id"))
        std::stable_sort(hits.begin(), hits.end(), [this](int a, int b) {
            return searchCache_[static_cast<std::size_t>(a)].id < searchCache_[static_cast<std::size_t>(b)].id;
        });
    else if (sort == QLatin1String("experience"))
        std::stable_sort(hits.begin(), hits.end(), by([](const SearchEntry& e) { return e.averageLevel; }));
    else if (sort == QLatin1String("skills"))
        std::stable_sort(hits.begin(), hits.end(), by([](const SearchEntry& e) { return e.skills.size(); }));

    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(hits.size()));
    for (int i : hits)
        rows.append(all.at(i));
    return {{QStringLiteral("total"), static_cast<int>(searchCache_.size())},
            {QStringLiteral("count"), static_cast<int>(hits.size())},
            {QStringLiteral("rows"), rows}};
}

// --- skills ----------------------------------------------------------------------------------

const std::vector<TeamForgeController::SkillRow>& TeamForgeController::skillRows() const
{
    if (!skillRowsCache_) {
        std::map<std::string, SkillRow> rows;
        const auto row = [&](const std::string& name) -> SkillRow& {
            auto it = rows.find(name);
            if (it == rows.end())
                it = rows.emplace(name, SkillRow{name, 0, categoryOf(name), 0, 0, 0}).first;
            return it->second;
        };
        for (const auto& [skill, count] : engine_.skillTree().inOrder()) // AVL in-order: sorted
            row(skill).participants = count;
        for (const ProjectRequirement& requirement : requirements_.values()) {
            for (const auto& entry : requirement.requiredSkills())
                ++row(entry.first).projects;
        }
        for (const auto& entry : skillCatalog_)
            row(entry.first);
        // Learning interests are counted for known skills only; they don't define new ones.
        for (const Student& student : engine_.students().values()) {
            for (const std::string& wanted : student.skillsWanted()) {
                const auto it = rows.find(wanted);
                if (it != rows.end())
                    ++it->second.learners;
            }
        }
        std::vector<SkillRow> list;
        list.reserve(rows.size());
        for (auto& entry : rows) {
            entry.second.number = static_cast<int>(list.size()) + 1; // A-Z
            list.push_back(std::move(entry.second));
        }
        skillRowsCache_ = std::move(list);
    }
    return *skillRowsCache_;
}

QVariantMap TeamForgeController::querySkills(const QVariantMap& filter) const
{
    if (!allowed(Permission::DeveloperTools))
        return {};
    const QString q = normalisedQuery(filter.value(QStringLiteral("query")).toString());
    const std::string query = ss(q);
    const QString category = filter.value(QStringLiteral("category")).toString();
    const QString sort = filter.value(QStringLiteral("sort")).toString();

    std::vector<const SkillRow *> hits;
    for (const SkillRow& row : skillRows()) {
        if (!category.isEmpty() && row.category != category)
            continue;
        if (!query.empty() && row.name.find(query) == std::string::npos && !skillMatchesQuery(row.name, query))
            continue;
        hits.push_back(&row);
    }
    if (sort == QLatin1String("participants"))
        std::stable_sort(hits.begin(), hits.end(), [](const SkillRow *a, const SkillRow *b) { return a->participants > b->participants; });
    else if (sort == QLatin1String("projects"))
        std::stable_sort(hits.begin(), hits.end(), [](const SkillRow *a, const SkillRow *b) { return a->projects > b->projects; });

    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(hits.size()));
    for (const SkillRow *row : hits)
        rows.append(QVariantMap{{QStringLiteral("name"), qs(row->name)},
                                {QStringLiteral("number"), row->number},
                                {QStringLiteral("category"), row->category},
                                {QStringLiteral("participants"), row->participants},
                                {QStringLiteral("projects"), row->projects},
                                {QStringLiteral("learners"), row->learners}});
    return {{QStringLiteral("total"), static_cast<int>(skillRows().size())},
            {QStringLiteral("count"), static_cast<int>(hits.size())},
            {QStringLiteral("rows"), rows}};
}

QVariantMap TeamForgeController::skillDetail(const QString& skill) const
{
    if (!allowed(Permission::DeveloperTools))
        return {};
    std::string name;
    try {
        name = normalizeSkill(ss(skill));
    } catch (const TeamForgeError&) {
        return {};
    }
    const auto& rows = skillRows();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const SkillRow& r) { return r.name == name; });
    if (row == rows.end())
        return {};

    // Strongest holders first (SkillIndex lookup, then each profile's level).
    const std::set<std::string> holderIds = engine_.skillIndex().studentsWith(name);
    std::vector<const Student *> holders;
    for (const std::string& id : holderIds)
        holders.push_back(&engine_.students().get(id));
    std::stable_sort(holders.begin(), holders.end(), [&](const Student *a, const Student *b) {
        return a->levelIn(name) > b->levelIn(name);
    });
    QVariantList people;
    for (std::size_t i = 0; i < holders.size() && i < 8; ++i)
        people.append(QVariantMap{{QStringLiteral("id"), qs(holders[i]->id())},
                                  {QStringLiteral("name"), qs(holders[i]->name())},
                                  {QStringLiteral("level"), holders[i]->levelIn(name)}});

    QVariantList projects;
    for (const ProjectRequirement& requirement : requirements_.values()) {
        if (requirement.needsSkill(name))
            projects.append(QVariantMap{{QStringLiteral("id"), qs(requirement.id())},
                                        {QStringLiteral("name"), qs(requirement.name())},
                                        {QStringLiteral("minLevel"), requirement.minLevelFor(name)}});
    }

    // Neighbours in the co-occurrence graph (BFS, one hop), strongest links first.
    const SkillGraph& graph = engine_.skillGraph();
    const std::vector<SkillDistance> neighbours = graph.hasSkill(name) ? graph.relatedSkills(name, 1)
                                                                       : std::vector<SkillDistance>{};
    std::vector<std::pair<std::string, int>> weighted;
    for (const SkillDistance& n : neighbours)
        weighted.emplace_back(n.skill, graph.edgeWeight(name, n.skill));
    std::stable_sort(weighted.begin(), weighted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    QVariantList related;
    for (std::size_t i = 0; i < weighted.size() && i < 12; ++i)
        related.append(QVariantMap{{QStringLiteral("skill"), qs(weighted[i].first)},
                                   {QStringLiteral("weight"), weighted[i].second}});

    const int treeCount = engine_.skillTree().count(name);
    const bool offered = treeCount > 0;
    const bool consistent = static_cast<int>(holderIds.size()) == treeCount
                            && engine_.skillIndex().hasSkill(name) == offered && graph.hasSkill(name) == offered;
    return {
        {QStringLiteral("name"), qs(name)},
        {QStringLiteral("category"), row->category},
        {QStringLiteral("inCatalog"), skillCatalog_.count(name) > 0},
        {QStringLiteral("participants"), row->participants},
        {QStringLiteral("learners"), row->learners},
        {QStringLiteral("projectCount"), row->projects},
        {QStringLiteral("projects"), projects},
        {QStringLiteral("holders"), people},
        {QStringLiteral("related"), related},
        {QStringLiteral("degree"), static_cast<int>(neighbours.size())},
        {QStringLiteral("inIndex"), engine_.skillIndex().hasSkill(name)},
        {QStringLiteral("indexEntries"), static_cast<int>(holderIds.size())},
        {QStringLiteral("inTree"), offered},
        {QStringLiteral("treeCount"), treeCount},
        {QStringLiteral("inGraph"), graph.hasSkill(name)},
        {QStringLiteral("consistent"), consistent},
    };
}

bool TeamForgeController::saveSkill(const QString& skill, const QString& category)
{
    const bool ok = run("saveSkill", skill, [&] {
        require(Permission::DeveloperTools);
        const std::string name = normalizeSkill(ss(skill)); // ValidationError if blank
        const bool known = skillCatalog_.count(name) > 0;
        skillCatalog_[name] = ss(category.trimmed());
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("skill"),
                 QStringLiteral("%1 skill '%2' (%3)").arg(known ? QStringLiteral("Updated") : QStringLiteral("Catalogued"),
                                                         qs(name), categoryOf(name)));
    });
    if (ok)
        notifyDataChanged();
    return ok;
}

void TeamForgeController::rewriteSkills(const std::function<SkillLevels(const SkillLevels&)>& edit,
                                        const std::function<std::vector<std::string>(const std::set<std::string>&)>& editWanted)
{
    // Build every changed record first (constructors validate), then apply: all or nothing.
    std::vector<Student> students;
    for (const Student& s : engine_.students().values()) {
        const SkillLevels offered = edit(s.skillsOffered());
        const std::vector<std::string> wanted = editWanted(s.skillsWanted());
        if (offered == s.skillsOffered() && wanted == std::vector<std::string>(s.skillsWanted().begin(), s.skillsWanted().end()))
            continue;
        students.emplace_back(s.id(), s.name(), s.role(), offered, wanted, s.summary(), s.program());
    }
    std::vector<ProjectRequirement> requirements;
    for (const ProjectRequirement& r : requirements_.values()) {
        const SkillLevels required = edit(r.requiredSkills());
        if (required == r.requiredSkills())
            continue;
        requirements.emplace_back(r.id(), r.name(), required, static_cast<int>(r.minTeamSize()),
                                  static_cast<int>(r.maxTeamSize()), r.type(), r.summary());
    }
    for (const Student& s : students)
        engine_.updateStudent(s); // keeps SkillIndex, SkillBST and SkillGraph in sync
    for (const ProjectRequirement& r : requirements)
        requirements_.update(r);
    logEvent(AppLog::Level::Info, QStringLiteral("skill"),
             QStringLiteral("Updated %1 profiles and %2 projects").arg(students.size()).arg(requirements.size()));
}

bool TeamForgeController::renameSkill(const QString& from, const QString& to)
{
    const bool ok = run("renameSkill", from, [&] {
        require(Permission::DeveloperTools);
        const std::string a = normalizeSkill(ss(from));
        const std::string b = normalizeSkill(ss(to));
        if (a == b)
            throw ValidationError("The new name is the same as the current one");
        const auto& rows = skillRows();
        if (std::none_of(rows.begin(), rows.end(), [&](const SkillRow& r) { return r.name == a; }))
            throw NotFoundError("Skill '" + a + "' not found");
        rewriteSkills(
            [&](const SkillLevels& levels) {
                SkillLevels result = levels;
                const auto it = result.find(a);
                if (it != result.end()) {
                    const int level = it->second;
                    result.erase(it);
                    result[b] = std::max(result[b], level); // merging keeps the higher level
                }
                return result;
            },
            [&](const std::set<std::string>& wanted) {
                std::set<std::string> result = wanted;
                if (result.erase(a) > 0)
                    result.insert(b);
                return std::vector<std::string>(result.begin(), result.end());
            });
        const auto entry = skillCatalog_.find(a);
        if (entry != skillCatalog_.end()) {
            skillCatalog_.emplace(b, entry->second); // an existing target keeps its category
            skillCatalog_.erase(a);
        }
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("skill"), QStringLiteral("Renamed skill '%1' to '%2'").arg(qs(a), qs(b)));
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

bool TeamForgeController::deleteSkill(const QString& skill)
{
    const bool ok = run("deleteSkill", skill, [&] {
        require(Permission::DeveloperTools);
        const std::string name = normalizeSkill(ss(skill));
        const auto& rows = skillRows();
        if (std::none_of(rows.begin(), rows.end(), [&](const SkillRow& r) { return r.name == name; }))
            throw NotFoundError("Skill '" + name + "' not found");
        // A profile or project must keep at least one skill; say which records block the delete.
        for (const Student& s : engine_.students().values()) {
            if (s.skillsOffered().size() == 1 && s.skillsOffered().count(name) > 0)
                throw ValidationError("'" + name + "' is the only skill of " + s.name() + " (" + s.id()
                                      + "); give that profile another skill first");
        }
        for (const ProjectRequirement& r : requirements_.values()) {
            if (r.requiredSkills().size() == 1 && r.requiredSkills().count(name) > 0)
                throw ValidationError("'" + name + "' is the only skill project " + r.name()
                                      + " requires; add another skill to it first");
        }
        rewriteSkills(
            [&](const SkillLevels& levels) {
                SkillLevels result = levels;
                result.erase(name);
                return result;
            },
            [&](const std::set<std::string>& wanted) {
                std::set<std::string> result = wanted;
                result.erase(name);
                return std::vector<std::string>(result.begin(), result.end());
            });
        skillCatalog_.erase(name);
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("skill"), QStringLiteral("Deleted skill '%1'").arg(qs(name)));
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

// --- raw records -----------------------------------------------------------------------------

QVariantMap TeamForgeController::inspectRecord(const QString& kind, const QString& id) const
{
    if (!allowed(Permission::DeveloperTools))
        return {};
    const std::string dir = ss(dataDirectory_) + "/";
    const std::string key = ss(id);
    QString title;
    QString subtitle;
    QString loaded;
    QString stored;
    QString storedNote;
    QStringList indexColumns;
    QVariantList indexed;
    QStringList matchingColumns;
    QVariantList matching;
    const auto readStored = [&](const char *file, const char *collection, const std::string& recordId,
                                const char *idKey = "id") {
        try {
            stored = qs(storage::storedRecord(dir + file, collection, recordId, idKey));
        } catch (const TeamForgeError& error) {
            storedNote = QString::fromUtf8(error.what());
        }
    };

    try {
        if (kind == QLatin1String("participant")) {
            const Student& s = engine_.students().get(key);
            title = qs(s.name());
            subtitle = qs(s.id());
            loaded = qs(storage::toJson(s));
            readStored("students.json", "students", key);
            indexColumns = {QStringLiteral("Level"), QStringLiteral("SkillIndex"), QStringLiteral("SkillBST"),
                            QStringLiteral("SkillGraph")};
            for (const auto& [skill, level] : s.skillsOffered()) {
                const bool inIndex = engine_.skillIndex().studentsWith(skill).count(s.id()) > 0;
                const bool inTree = engine_.skillTree().count(skill) > 0;
                const bool inGraph = engine_.skillGraph().hasSkill(skill);
                indexed.append(tableRow(qs(skill), {QString::number(level), tick(inIndex), tick(inTree), tick(inGraph)},
                                        inIndex && inTree && inGraph, true));
            }
            // Matching-visible: retrieved through the index for a project when the person offers
            // at least one required skill; the fit is the weighted score as the first member.
            matchingColumns = {QStringLiteral("Retrieved"), QStringLiteral("Skills met"), QStringLiteral("Fit")};
            for (const ProjectRequirement& r : requirements_.values()) {
                const Team empty("probe", r);
                const MatchResult result = weighted_.evaluate(s, empty);
                int met = 0;
                bool retrieved = false;
                for (const auto& [skill, minLevel] : r.requiredSkills()) {
                    met += s.offers(skill, minLevel) ? 1 : 0;
                    retrieved = retrieved || s.offers(skill);
                }
                matching.append(tableRow(qs(r.name()),
                                         {retrieved ? QStringLiteral("Yes") : QStringLiteral("No"),
                                          QStringLiteral("%1 / %2").arg(met).arg(r.requiredSkills().size()),
                                          QStringLiteral("%1%").arg(qRound(result.score * 100))},
                                         retrieved));
            }
        } else if (kind == QLatin1String("project")) {
            const ProjectRequirement& r = requirements_.get(key);
            title = qs(r.name());
            subtitle = qs(r.id());
            loaded = qs(storage::toJson(r));
            readStored("requirements.json", "requirements", key);
            indexColumns = {QStringLiteral("Minimum"), QStringLiteral("Offered by"), QStringLiteral("Qualified"),
                            QStringLiteral("In BST / graph")};
            for (const auto& [skill, minLevel] : r.requiredSkills()) {
                const std::set<std::string> offering = engine_.skillIndex().studentsWith(skill);
                int qualified = 0;
                for (const std::string& holder : offering)
                    qualified += engine_.students().get(holder).offers(skill, minLevel) ? 1 : 0;
                const bool inTree = engine_.skillTree().count(skill) > 0;
                const bool inGraph = engine_.skillGraph().hasSkill(skill);
                indexed.append(tableRow(qs(skill),
                                        {QString::number(minLevel), QString::number(offering.size()),
                                         QString::number(qualified), tick(inTree) + QStringLiteral(" / ") + tick(inGraph)},
                                        qualified > 0, true));
            }
            const Team empty("probe", r);
            // The top of the ranking as the engine scores it: every factor (0-1) and the score.
            for (const QVariant& factor : factorDefinitions())
                matchingColumns.append(factor.toMap().value(QStringLiteral("shortLabel")).toString());
            matchingColumns.append(QStringLiteral("Score"));
            const auto value = [](double v) { return QString::number(v, 'f', 2); };
            for (const MatchResult& result : engine_.rank(empty, weighted_, 10)) {
                const FactorScores& f = result.factors;
                matching.append(tableRow(qs(result.studentName),
                                         {value(f.coverage), value(f.complementarity), value(f.experience),
                                          value(f.engagement), value(f.diversity),
                                          QStringLiteral("%1%").arg(qRound(result.score * 100))},
                                         true));
            }
        } else if (kind == QLatin1String("team")) {
            const auto record = std::find_if(savedTeams_.begin(), savedTeams_.end(),
                                             [&](const TeamRecord& t) { return t.id == key; });
            if (record == savedTeams_.end())
                throw NotFoundError("Saved team '" + key + "' not found");
            const Team team = buildTeam(*record);
            title = qs(team.id());
            subtitle = qs(team.requirement().name());
            loaded = qs(storage::toJson(team));
            readStored("teams.json", "teams", key);
            indexColumns = {QStringLiteral("Status")};
            for (const std::string& skill : team.requirement().skillNames())
                indexed.append(tableRow(qs(skill), {team.covers(skill) ? QStringLiteral("Covered") : QStringLiteral("Missing")},
                                        team.covers(skill), true));
            matchingColumns = {QStringLiteral("Value")};
            matching.append(tableRow(QStringLiteral("Members"), {QStringLiteral("%1 (%2–%3)").arg(team.size())
                                         .arg(team.requirement().minTeamSize()).arg(team.requirement().maxTeamSize())}, true));
            matching.append(tableRow(QStringLiteral("Coverage"), {QStringLiteral("%1%").arg(qRound(team.coverageRatio() * 100))},
                                     team.isComplete()));
        } else if (kind == QLatin1String("skill")) {
            const std::string name = normalizeSkill(key);
            const QVariantMap detail = skillDetail(qs(name));
            if (detail.isEmpty())
                throw NotFoundError("Skill '" + name + "' not found");
            title = qs(name);
            subtitle = detail.value(QStringLiteral("category")).toString();
            const auto entry = skillCatalog_.find(name);
            if (entry != skillCatalog_.end())
                loaded = QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("name"), qs(name)},
                                                                     {QStringLiteral("category"), qs(entry->second)}})
                                               .toJson(QJsonDocument::Indented));
            readStored("skills.json", "skills", name, "name");
            indexColumns = {QStringLiteral("State")};
            indexed.append(tableRow(QStringLiteral("SkillIndex"), {QStringLiteral("%1 entries").arg(detail.value(QStringLiteral("indexEntries")).toInt())},
                                    detail.value(QStringLiteral("inIndex")).toBool()));
            indexed.append(tableRow(QStringLiteral("SkillBST"), {QStringLiteral("count %1").arg(detail.value(QStringLiteral("treeCount")).toInt())},
                                    detail.value(QStringLiteral("inTree")).toBool()));
            indexed.append(tableRow(QStringLiteral("SkillGraph"), {QStringLiteral("%1 neighbours").arg(detail.value(QStringLiteral("degree")).toInt())},
                                    detail.value(QStringLiteral("inGraph")).toBool()));
            matchingColumns = {QStringLiteral("Minimum")};
            for (const QVariant& project : detail.value(QStringLiteral("projects")).toList()) {
                const QVariantMap p = project.toMap();
                matching.append(tableRow(p.value(QStringLiteral("name")).toString(),
                                         {QString::number(p.value(QStringLiteral("minLevel")).toInt())}, true));
            }
        } else {
            return {{QStringLiteral("error"), QStringLiteral("Unknown record kind '%1'").arg(kind)}};
        }
    } catch (const TeamForgeError& error) {
        return {{QStringLiteral("error"), QString::fromUtf8(error.what())}};
    }

    // Compare what the stored record means, not its spelling: it is parsed with the loaders'
    // rules and written back in canonical form (so list order, key order and skill spelling
    // don't count as differences).
    QString canonicalStored = stored;
    try {
        if (!stored.isEmpty() && kind == QLatin1String("participant"))
            canonicalStored = qs(storage::toJson(storage::studentFromJson(ss(stored))));
        else if (!stored.isEmpty() && kind == QLatin1String("project"))
            canonicalStored = qs(storage::toJson(storage::requirementFromJson(ss(stored))));
    } catch (const TeamForgeError& error) {
        storedNote = QStringLiteral("The stored record is invalid: ") + QString::fromUtf8(error.what());
    }
    QString storedState = QStringLiteral("missing");
    if (!storedNote.isEmpty())
        storedState = QStringLiteral("error");
    else if (!stored.isEmpty())
        storedState = QJsonDocument::fromJson(canonicalStored.toUtf8()).object() == QJsonDocument::fromJson(loaded.toUtf8()).object()
                          ? QStringLiteral("same") : QStringLiteral("different");
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("subtitle"), subtitle},
        {QStringLiteral("loaded"), loaded},
        {QStringLiteral("stored"), stored},
        {QStringLiteral("storedState"), storedState},
        {QStringLiteral("storedNote"), storedNote},
        {QStringLiteral("storagePath"), dataDirectory_},
        {QStringLiteral("editable"), kind == QLatin1String("participant") || kind == QLatin1String("project")
                                         || kind == QLatin1String("skill")},
        {QStringLiteral("indexColumns"), indexColumns},
        {QStringLiteral("indexed"), indexed},
        {QStringLiteral("matchingColumns"), matchingColumns},
        {QStringLiteral("matching"), matching},
    };
}

bool TeamForgeController::applyRecordJson(const QString& kind, const QString& json)
{
    const bool ok = run("applyRecordJson", kind, [&] {
        require(Permission::DeveloperTools);
        if (kind == QLatin1String("participant")) {
            upsertStudent(storage::studentFromJson(ss(json)));
        } else if (kind == QLatin1String("project")) {
            upsertRequirement(storage::requirementFromJson(ss(json)));
        } else if (kind == QLatin1String("skill")) {
            const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
            if (!document.isObject() || !document.object().value(QStringLiteral("name")).isString())
                throw ValidationError("A skill record is {\"name\": ..., \"category\": ...}");
            const QJsonObject o = document.object();
            const std::string name = normalizeSkill(ss(o.value(QStringLiteral("name")).toString()));
            skillCatalog_[name] = ss(o.value(QStringLiteral("category")).toString().trimmed());
            setDirty(true);
            logEvent(AppLog::Level::Info, QStringLiteral("skill"), QStringLiteral("Edited skill record '%1'").arg(qs(name)));
        } else {
            throw ValidationError("Only participants, projects and skills can be edited as raw records");
        }
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}
