#include "TeamForgeController.h"

#include "BridgeSupport.h"
#include "SampleData.h"

#include "core/Errors.h"
#include "core/JsonStorage.h"
#include "core/Skill.h"
#include "core/Validation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <algorithm>
#include <exception>
#include <set>
#include <typeinfo>
#include <utility>

using namespace teamforge;
using namespace bridge;

namespace {

// Id of the team being assembled; it appears in core validation messages.
const std::string kCurrentTeamId = "Current team";

// Display metadata for the five factors. The definitions mirror the comment at the top of
// core/MatchingStrategy.h, which is authoritative; the values themselves come from the core.
// `shortLabel` is the plain-language name users see ("Skill fit"); `label` the report's term.
struct FactorInfo
{
    const char *key;
    const char *label;
    const char *shortLabel;
    const char *description;
    double FactorScores::*value;
    double MatchWeights::*weight;
};

const FactorInfo kFactors[] = {
    {"coverage", "Coverage", "Skill fit",
     "Share of the required skills the candidate has at or above the minimum level.",
     &FactorScores::coverage, &MatchWeights::coverage},
    {"complementarity", "Complementarity", "Team fit",
     "Of the required skills the candidate covers, the share the current team does not already "
     "cover (fills a gap = 1, duplicates = 0).",
     &FactorScores::complementarity, &MatchWeights::complementarity},
    {"experience", "Experience", "Experience",
     "Average level (out of 5) across the required skills the candidate lists.",
     &FactorScores::experience, &MatchWeights::experience},
    {"engagement", "Engagement / interest", "Engagement",
     "1 once the participant has expressed interest in this project; 0.5 (neutral) before that.",
     &FactorScores::engagement, &MatchWeights::engagement},
    {"diversity", "Diversity / balance", "Balance",
     "Lower when teammates already share the candidate's specialisation: "
     "1 / (1 + number of such teammates).",
     &FactorScores::diversity, &MatchWeights::diversity},
};

const QString kParticipant = QStringLiteral("participant");
const QString kHost = QStringLiteral("host");
const QString kDeveloper = QStringLiteral("developer");

bool isWorkspaceName(const QString& name)
{
    return name == kParticipant || name == kHost || name == kDeveloper;
}

// $TEAMFORGE_ALLOWED_WORKSPACES, e.g. "participant,host". Unset or empty: every workspace.
QStringList allowedWorkspacesFromEnvironment()
{
    QStringList allowed;
    const QStringList parts = qEnvironmentVariable("TEAMFORGE_ALLOWED_WORKSPACES").split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString name = part.trimmed().toLower();
        if (isWorkspaceName(name) && !allowed.contains(name))
            allowed.append(name);
    }
    return allowed;
}

QString errorTypeOf(const TeamForgeError& error)
{
    if (dynamic_cast<const ValidationError *>(&error)) return QStringLiteral("ValidationError");
    if (dynamic_cast<const NotFoundError *>(&error)) return QStringLiteral("NotFoundError");
    if (dynamic_cast<const DuplicateError *>(&error)) return QStringLiteral("DuplicateError");
    if (dynamic_cast<const TeamConstraintError *>(&error)) return QStringLiteral("TeamConstraintError");
    if (dynamic_cast<const PersistenceError *>(&error)) return QStringLiteral("PersistenceError");
    if (dynamic_cast<const PermissionError *>(&error)) return QStringLiteral("PermissionError");
    return QStringLiteral("TeamForgeError");
}

} // namespace

TeamForgeController::TeamForgeController(QObject *parent)
    : QObject(parent)
    , dataDirectory_(QDir::cleanPath(defaultDataDirectory()))
    , sampleDirectory_(sampledata::defaultSampleDirectory())
    , allowedWorkspaces_(allowedWorkspacesFromEnvironment())
{
    weighted_.setEngagementSource(&interests_);
    baseline_.setEngagementSource(&interests_);
    log_.setFile(dataDirectory_ + QStringLiteral("/teamforge.log"));
    const sampledata::Result seeded = sampledata::ensureSeeded(dataDirectory_, sampleDirectory_);
    seedMessage_ = seeded.message;
    logEvent(AppLog::Level::Info, QStringLiteral("startup"),
             QStringLiteral("TeamForge %1 starting; data in %2").arg(version(), dataDirectory_));
    logEvent(seeded.ok ? AppLog::Level::Info : AppLog::Level::Error, QStringLiteral("data"), seeded.message);
    loadData();
    loadSession();
}

TeamForgeController::TeamForgeController(const QString& dataDirectory, QObject *parent)
    : QObject(parent)
    , dataDirectory_(QDir::cleanPath(dataDirectory))
    , sampleDirectory_(sampledata::defaultSampleDirectory())
    , allowedWorkspaces_(allowedWorkspacesFromEnvironment())
{
    weighted_.setEngagementSource(&interests_);
    baseline_.setEngagementSource(&interests_);
    log_.setFile(dataDirectory_ + QStringLiteral("/teamforge.log"));
    loadData();
    loadSession();
}

QString TeamForgeController::defaultDataDirectory()
{
    const QString fromEnvironment = qEnvironmentVariable("TEAMFORGE_DATA_DIR");
    if (!fromEnvironment.isEmpty())
        return fromEnvironment;
    // Portable: "data" beside the executable. If that folder can't be written (the app was
    // unpacked somewhere protected, such as Program Files), use the user's local app data.
    const QString portable = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
    if (QDir().mkpath(portable)) {
        QTemporaryFile probe(portable + QStringLiteral("/.write-test-XXXXXX"));
        if (probe.open())
            return portable;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/data");
}

QString TeamForgeController::version() const
{
    const QString fromApp = QCoreApplication::applicationVersion();
    return fromApp.isEmpty() ? QStringLiteral("dev") : fromApp;
}

// --- read-only views -----------------------------------------------------------------------

// The student and requirement lists are read by many QML bindings; with hundreds of profiles
// they are built once per data change (see notifyDataChanged) instead of on every read.
QVariantList TeamForgeController::students() const
{
    if (!studentsCache_) {
        std::vector<Student> sorted = engine_.students().values();
        std::stable_sort(sorted.begin(), sorted.end(), [](const Student& a, const Student& b) {
            return QString::localeAwareCompare(qs(a.name()), qs(b.name())) < 0;
        });
        QVariantList list;
        list.reserve(static_cast<qsizetype>(sorted.size()));
        searchCache_.clear();
        searchCache_.reserve(sorted.size());
        for (const Student& student : sorted) {
            QVariantMap map = studentToMap(student);
            map.insert(QStringLiteral("number"), static_cast<int>(list.size()) + 1); // database No. (A-Z)
            list.append(map);
            SearchEntry entry{qs(student.name()).toLower(), qs(student.id()).toLower(), {},
                              map.value(QStringLiteral("averageLevel")).toDouble()};
            for (const auto& skill : student.skillsOffered())
                entry.skills.push_back(skill.first);
            searchCache_.push_back(std::move(entry));
        }
        studentsCache_ = std::move(list);
    }
    return *studentsCache_;
}

QVariantList TeamForgeController::requirements() const
{
    if (!requirementsCache_) {
        QVariantList list;
        for (const ProjectRequirement& requirement : requirements_.values()) {
            QVariantMap map = requirementToMap(requirement);
            map.insert(QStringLiteral("number"), static_cast<int>(list.size()) + 1);
            list.append(map);
        }
        requirementsCache_ = std::move(list);
    }
    return *requirementsCache_;
}

QVariantMap TeamForgeController::cachedRequirement(const std::string& id) const
{
    const QString key = qs(id);
    for (const QVariant& entry : requirements()) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == key)
            return map;
    }
    return {};
}

QVariantMap TeamForgeController::currentRequirement() const
{
    return cachedRequirement(currentRequirementId_);
}

QVariantList TeamForgeController::recentProjects() const
{
    QVariantList list;
    for (const std::string& id : recentRequirementIds_) {
        const QVariantMap map = cachedRequirement(id);
        if (!map.isEmpty())
            list.append(map);
    }
    for (const ProjectRequirement& requirement : requirements_.values()) {
        if (std::find(recentRequirementIds_.begin(), recentRequirementIds_.end(), requirement.id())
            == recentRequirementIds_.end())
            list.append(cachedRequirement(requirement.id()));
    }
    return list;
}

QVariantMap TeamForgeController::requirementToMap(const ProjectRequirement& requirement) const
{
    // Per-skill supply in the participant pool: offering it at all, and at the minimum level.
    QVariantList skills;
    for (const auto& [skill, minLevel] : requirement.requiredSkills()) {
        int qualified = 0;
        const std::set<std::string> offering = engine_.skillIndex().studentsWith(skill);
        for (const std::string& id : offering) {
            if (engine_.students().get(id).offers(skill, minLevel))
                ++qualified;
        }
        skills.append(QVariantMap{{QStringLiteral("skill"), qs(skill)},
                                  {QStringLiteral("minLevel"), minLevel},
                                  {QStringLiteral("offeredCount"), static_cast<int>(offering.size())},
                                  {QStringLiteral("qualifiedCount"), qualified}});
    }
    const auto savedTeamCount = std::count_if(savedTeams_.begin(), savedTeams_.end(), [&](const TeamRecord& r) {
        return r.requirementId == requirement.id();
    });
    // Interest requests by review stage.
    int byStatus[4] = {0, 0, 0, 0};
    for (const InterestRequest& request : interests_.forRequirement(requirement.id()))
        ++byStatus[static_cast<int>(request.status())];
    const QVariantMap interest{{QStringLiteral("interested"), byStatus[0]},
                               {QStringLiteral("under_review"), byStatus[1]},
                               {QStringLiteral("accepted"), byStatus[2]},
                               {QStringLiteral("declined"), byStatus[3]},
                               {QStringLiteral("total"), byStatus[0] + byStatus[1] + byStatus[2] + byStatus[3]}};
    const Team empty("probe", requirement);
    return {
        {QStringLiteral("id"), qs(requirement.id())},
        {QStringLiteral("interest"), interest},
        {QStringLiteral("name"), qs(requirement.name())},
        {QStringLiteral("type"), qs(requirement.type())},
        {QStringLiteral("summary"), qs(requirement.summary())},
        {QStringLiteral("requiredSkills"), skills},
        {QStringLiteral("minTeamSize"), static_cast<int>(requirement.minTeamSize())},
        {QStringLiteral("maxTeamSize"), static_cast<int>(requirement.maxTeamSize())},
        {QStringLiteral("candidateCount"), static_cast<int>(engine_.retrieveCandidates(empty).size())},
        {QStringLiteral("savedTeamCount"), static_cast<int>(savedTeamCount)},
    };
}

QVariantList TeamForgeController::savedTeams() const
{
    QVariantList list;
    for (const TeamRecord& record : savedTeams_) {
        try {
            QVariantMap map = teamToMap(buildTeam(record));
            map.insert(QStringLiteral("number"), static_cast<int>(list.size()) + 1);
            list.append(map);
        } catch (const TeamForgeError& error) {
            // Integrity checks in the mutating actions should make this unreachable.
            list.append(QVariantMap{{QStringLiteral("id"), qs(record.id)},
                                    {QStringLiteral("error"), QString::fromUtf8(error.what())}});
        }
    }
    return list;
}

QStringList TeamForgeController::skills() const
{
    return toQStringList(engine_.skillsWithPrefix(""));
}

int TeamForgeController::skillCount() const
{
    return static_cast<int>(skillRows().size()); // offered, required or catalogued
}

QStringList TeamForgeController::skillCategories() const
{
    std::set<QString> categories;
    for (const auto& entry : skillCatalog_) {
        if (!entry.second.empty())
            categories.insert(qs(entry.second));
    }
    return {categories.begin(), categories.end()};
}

QString TeamForgeController::categoryOf(const std::string& skill) const
{
    const auto it = skillCatalog_.find(skill);
    return it == skillCatalog_.end() || it->second.empty() ? QStringLiteral("Uncategorised") : qs(it->second);
}

QVariantList TeamForgeController::factorDefinitions() const
{
    QVariantList list;
    const MatchWeights& weights = weighted_.weights();
    for (const FactorInfo& factor : kFactors) {
        list.append(QVariantMap{{QStringLiteral("key"), QString::fromLatin1(factor.key)},
                                {QStringLiteral("label"), QString::fromLatin1(factor.label)},
                                {QStringLiteral("shortLabel"), QString::fromLatin1(factor.shortLabel)},
                                {QStringLiteral("description"), QString::fromLatin1(factor.description)},
                                {QStringLiteral("weight"), weights.*factor.weight}});
    }
    return list;
}

QVariantList TeamForgeController::strategies() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("weighted")}, {QStringLiteral("name"), qs(weighted_.name())}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("baseline")}, {QStringLiteral("name"), qs(baseline_.name())}},
    };
}

QVariantMap TeamForgeController::currentTeam() const
{
    return currentTeam_ ? teamToMap(*currentTeam_) : QVariantMap{};
}

QVariantList TeamForgeController::rankedCandidates() const
{
    QVariantList list;
    for (const MatchResult& result : ranked_)
        list.append(matchResultToMap(result, *currentTeam_, !useBaseline_)); // ranked_ is only filled with a team
    return list;
}

QVariantMap TeamForgeController::teamToMap(const Team& team) const
{
    QVariantList members;
    QStringList memberIds;
    for (const Student& member : team.members()) {
        members.append(studentToMap(member));
        memberIds.append(qs(member.id()));
    }
    QString finalizeIssue;
    try {
        team.validateForFinalization();
    } catch (const TeamForgeError& error) {
        finalizeIssue = QString::fromUtf8(error.what());
    }
    return {
        {QStringLiteral("id"), qs(team.id())},
        {QStringLiteral("requirementId"), qs(team.requirement().id())},
        {QStringLiteral("requirementName"), qs(team.requirement().name())},
        {QStringLiteral("members"), members},
        {QStringLiteral("memberIds"), memberIds},
        {QStringLiteral("size"), static_cast<int>(team.size())},
        {QStringLiteral("minTeamSize"), static_cast<int>(team.requirement().minTeamSize())},
        {QStringLiteral("maxTeamSize"), static_cast<int>(team.requirement().maxTeamSize())},
        {QStringLiteral("isFull"), team.isFull()},
        {QStringLiteral("coveredSkills"), toQStringList(team.coveredSkills())},
        {QStringLiteral("missingSkills"), toQStringList(team.missingSkills())},
        {QStringLiteral("coverageRatio"), team.coverageRatio()},
        {QStringLiteral("isComplete"), team.isComplete()},
        {QStringLiteral("canFinalize"), finalizeIssue.isEmpty()},
        {QStringLiteral("finalizeIssue"), finalizeIssue},
        // Display helper; whether the team may be finalised is decided by canFinalize above.
        {QStringLiteral("membersNeeded"),
         static_cast<int>(team.size() < team.requirement().minTeamSize()
                              ? team.requirement().minTeamSize() - team.size() : 0)},
    };
}

QVariantMap TeamForgeController::matchResultToMap(const MatchResult& result, const Team& team,
                                                  bool weightedScore) const
{
    // Required skill by skill: the candidate's level against the minimum, and whether covering
    // it fills a team gap. Statuses: "covered" (level >= minimum), "below", "missing".
    QVariantList skillBreakdown;
    const Student *student = engine_.students().find(result.studentId);
    for (const auto& [skill, minLevel] : team.requirement().requiredSkills()) {
        const int level = student != nullptr ? student->levelIn(skill) : 0;
        const QString status = level >= minLevel ? QStringLiteral("covered")
                               : level > 0       ? QStringLiteral("below")
                                                 : QStringLiteral("missing");
        skillBreakdown.append(QVariantMap{
            {QStringLiteral("skill"), qs(skill)},
            {QStringLiteral("minLevel"), minLevel},
            {QStringLiteral("level"), level},
            {QStringLiteral("status"), status},
            {QStringLiteral("fillsGap"), std::find(result.gapsFilled.begin(), result.gapsFilled.end(), skill)
                                             != result.gapsFilled.end()},
            {QStringLiteral("teamCovers"), team.covers(skill)},
        });
    }

    // The weighted strategy's score is sum(weight * factor), so per-factor contributions are
    // shown for it; the baseline only reports the raw factor values.
    QVariantList factors;
    for (const FactorInfo& factor : kFactors) {
        const double value = result.factors.*factor.value;
        QVariantMap entry{{QStringLiteral("key"), QString::fromLatin1(factor.key)},
                          {QStringLiteral("label"), QString::fromLatin1(factor.label)},
                          {QStringLiteral("shortLabel"), QString::fromLatin1(factor.shortLabel)},
                          {QStringLiteral("value"), value}};
        if (weightedScore) {
            const double weight = weighted_.weights().*factor.weight;
            entry.insert(QStringLiteral("weight"), weight);
            entry.insert(QStringLiteral("contribution"), weight * value);
        }
        factors.append(entry);
    }
    return {
        {QStringLiteral("studentId"), qs(result.studentId)},
        {QStringLiteral("studentName"), qs(result.studentName)},
        {QStringLiteral("score"), result.score},
        {QStringLiteral("factors"), factors},
        {QStringLiteral("coveredSkills"), toQStringList(result.coveredSkills)},
        {QStringLiteral("gapsFilled"), toQStringList(result.gapsFilled)},
        {QStringLiteral("skillBreakdown"), skillBreakdown},
        {QStringLiteral("isMember"), team.contains(result.studentId)},
    };
}

// --- workspaces ------------------------------------------------------------------------------

void TeamForgeController::setWorkspace(const QString& workspace, bool previewing)
{
    const bool changed = workspace != workspace_ || previewing != previewing_;
    workspace_ = workspace;
    previewing_ = previewing;
    // Strategy comparison is a developer tool; every other view ranks with the report's weights.
    if (workspace_ != kDeveloper && useBaseline_) {
        useBaseline_ = false;
        refresh();
    }
    if (!workspace.isEmpty() && !previewing)
        lastWorkspace_ = workspace;
    // The shown message belongs to the previous workspace; the full details stay available to
    // the developer workspace for the rest of the session.
    setLastError({});
    if (changed) {
        opportunitiesCache_.reset();
        emit workspaceChanged();
        emit profileChanged();
        emit statusChanged();
    }
}

bool TeamForgeController::enterWorkspace(const QString& workspace)
{
    if (!isWorkspaceName(workspace)) {
        setLastError(QStringLiteral("Unknown workspace"));
        return false;
    }
    if (!workspaceAllowed(workspace)) {
        setLastError(QStringLiteral("This account cannot open the %1 workspace").arg(workspace));
        return false;
    }
    setWorkspace(workspace, false);
    logEvent(AppLog::Level::Info, QStringLiteral("workspace"), QStringLiteral("Entered the %1 workspace").arg(workspace));
    saveSession();
    return true;
}

bool TeamForgeController::workspaceAllowed(const QString& workspace) const
{
    return isWorkspaceName(workspace) && (allowedWorkspaces_.isEmpty() || allowedWorkspaces_.contains(workspace));
}

void TeamForgeController::leaveWorkspace()
{
    if (workspace_.isEmpty())
        return;
    logEvent(AppLog::Level::Info, QStringLiteral("workspace"), QStringLiteral("Left the %1 workspace").arg(workspace_));
    setWorkspace({}, false);
}

bool TeamForgeController::previewWorkspace(const QString& workspace, const QString& identity)
{
    return run("previewWorkspace", workspace, [&] {
        if (!previewing_) // switching between previews is allowed; the real workspace is developer
            require(Permission::DeveloperTools);
        if (workspace != kParticipant && workspace != kHost)
            throw ValidationError("Only the participant and host workspaces can be previewed");
        const std::string chosen = ss(identity.trimmed());
        if (workspace == kParticipant) {
            if (!chosen.empty())
                previewProfileId_ = engine_.students().get(chosen).id(); // NotFoundError
            else if (previewProfileId_.empty())
                previewProfileId_ = myProfileId_.empty() ? pickPreviewProfile() : myProfileId_;
        } else if (!chosen.empty()) {
            requirements_.get(chosen); // NotFoundError
            if (chosen != currentRequirementId_) {
                currentRequirementId_ = chosen;
                currentMemberIds_.clear();
            }
        }
        setWorkspace(workspace, true);
        refresh();
        logEvent(AppLog::Level::Info, QStringLiteral("workspace"),
                 QStringLiteral("Developer previewing the %1 workspace").arg(workspace));
    });
}

void TeamForgeController::endPreview()
{
    if (!previewing_)
        return;
    setWorkspace(kDeveloper, false);
    previewProfileId_.clear();
    logEvent(AppLog::Level::Info, QStringLiteral("workspace"), QStringLiteral("Preview ended"));
}

std::string TeamForgeController::pickPreviewProfile() const
{
    // The demo participant if present, otherwise a complete, skill-rich seeded profile, chosen
    // deterministically (most skills, then id).
    if (engine_.students().contains(kDemoParticipant))
        return kDemoParticipant;
    const Student *best = nullptr;
    for (const Student& student : engine_.students().values()) {
        const bool complete = !student.summary().empty() && !student.program().empty()
                              && student.skillsOffered().size() >= 3 && !student.skillsWanted().empty();
        if (complete && (best == nullptr || student.skillsOffered().size() > best->skillsOffered().size()))
            best = &student;
    }
    return best != nullptr ? best->id() : std::string();
}

void TeamForgeController::require(Permission permission) const
{
    const QString& w = workspace_;
    const bool developer = w == kDeveloper;
    bool allowed = false;
    switch (permission) {
    case Permission::EditOwnProfile: allowed = w == kParticipant || developer; break;
    case Permission::EditAnyProfile: allowed = developer; break;
    case Permission::ManageProjects:
    case Permission::BuildTeams: allowed = w == kHost || developer; break;
    case Permission::Evaluate: allowed = !w.isEmpty(); break;
    case Permission::DeveloperTools: allowed = developer; break;
    }
    if (!allowed) {
        throw PermissionError("This action is not available in the "
                              + (w.isEmpty() ? std::string("entry") : ss(w)) + " workspace");
    }
}

// --- selection -----------------------------------------------------------------------------

void TeamForgeController::setCurrentRequirementId(const QString& requirementId)
{
    run("selectProject", requirementId, [&] {
        const std::string id = ss(requirementId);
        requirements_.get(id); // NotFoundError if unknown
        if (id != currentRequirementId_) {
            currentRequirementId_ = id;
            currentMemberIds_.clear();
            logEvent(AppLog::Level::Info, QStringLiteral("matching"),
                     QStringLiteral("Active project: %1").arg(qs(requirements_.get(id).name())));
        }
        touchRecent(id);
    });
    refresh();
}

void TeamForgeController::setStrategy(const QString& strategyId)
{
    run("setStrategy", strategyId, [&] {
        require(Permission::DeveloperTools);
        if (strategyId == QStringLiteral("weighted"))
            useBaseline_ = false;
        else if (strategyId == QStringLiteral("baseline"))
            useBaseline_ = true;
        else
            throw ValidationError("Unknown strategy '" + ss(strategyId) + "'");
    });
    refresh();
}

void TeamForgeController::setTopK(int k)
{
    run("setTopK", QString::number(k), [&] {
        if (k < 1)
            throw ValidationError("The number of candidates to show must be at least 1");
        topK_ = k;
    });
    refresh();
}

// --- persistence ---------------------------------------------------------------------------

bool TeamForgeController::reload()
{
    if (!run("reload", dataDirectory_, [this] { require(Permission::DeveloperTools); }))
        return false;
    return loadData();
}

bool TeamForgeController::loadData()
{
    const bool ok = run("load", dataDirectory_, [this] {
        if (!QFileInfo(dataDirectory_).isDir())
            throw PersistenceError("Data directory '" + ss(dataDirectory_) + "' does not exist");
        const std::string dir = ss(dataDirectory_) + "/";

        MatchingEngine engine;
        for (const Student& student : storage::loadStudents(dir + "students.json"))
            engine.addStudent(student);
        Repository<ProjectRequirement> requirements("Requirement");
        for (const ProjectRequirement& requirement : storage::loadRequirements(dir + "requirements.json"))
            requirements.add(requirement);
        std::vector<TeamRecord> teams;
        if (QFileInfo::exists(qs(dir + "teams.json"))) { // optional on first run
            for (const Team& team : storage::loadTeams(dir + "teams.json", engine.students(), requirements))
                teams.push_back(recordOf(team));
        }
        storage::SkillCatalog catalog;
        if (QFileInfo::exists(qs(dir + "skills.json"))) // optional: categories are display metadata
            catalog = storage::loadSkillCatalog(dir + "skills.json");
        InterestBook interests;
        if (QFileInfo::exists(qs(dir + "interest_requests.json"))) { // optional on first run
            for (const InterestRequest& request : storage::loadInterests(dir + "interest_requests.json")) {
                if (!engine.students().contains(request.studentId()) || !requirements.contains(request.requirementId()))
                    throw PersistenceError("'" + dir + "interest_requests.json': request '" + request.id()
                                           + "' refers to an unknown participant or project");
                interests.add(request);
            }
        }

        engine_ = std::move(engine);
        requirements_ = std::move(requirements);
        savedTeams_ = std::move(teams);
        skillCatalog_ = std::move(catalog);
        interests_ = std::move(interests); // same object: the strategies keep pointing at it
        currentMemberIds_.clear();
        if (!requirements_.contains(currentRequirementId_))
            currentRequirementId_ = requirements_.empty() ? std::string() : requirements_.values().front().id();
        if (!myProfileId_.empty() && !engine_.students().contains(myProfileId_))
            myProfileId_.clear();
        if (!previewProfileId_.empty() && !engine_.students().contains(previewProfileId_))
            previewProfileId_ = pickPreviewProfile();
        setDirty(false);
        lastLoaded_ = QDateTime::currentDateTime();
        logEvent(AppLog::Level::Info, QStringLiteral("data"),
                 QStringLiteral("Loaded %1 participants, %2 projects, %3 saved teams")
                     .arg(engine_.students().size()).arg(requirements_.size()).arg(savedTeams_.size()));
    });
    notifyDataChanged();
    refresh();
    return ok;
}

bool TeamForgeController::save()
{
    return run("save", dataDirectory_, [this] {
        if (workspace_.isEmpty())
            require(Permission::DeveloperTools);
        if (!QDir().mkpath(dataDirectory_))
            throw PersistenceError("Cannot create data directory '" + ss(dataDirectory_) + "'");
        const std::string dir = ss(dataDirectory_) + "/";
        std::vector<Team> teams;
        for (const TeamRecord& record : savedTeams_)
            teams.push_back(buildTeam(record));
        storage::saveStudents(dir + "students.json", engine_.students().values());
        storage::saveRequirements(dir + "requirements.json", requirements_.values());
        storage::saveTeams(dir + "teams.json", teams);
        storage::saveSkillCatalog(dir + "skills.json", skillCatalog_);
        storage::saveInterests(dir + "interest_requests.json", interests_.values());
        setDirty(false);
        lastSaved_ = QDateTime::currentDateTime();
        logEvent(AppLog::Level::Info, QStringLiteral("data"), QStringLiteral("Saved all data to %1").arg(dataDirectory_));
        emit statusChanged();
    });
}

// --- current team --------------------------------------------------------------------------

bool TeamForgeController::addToTeam(const QString& studentId)
{
    const bool ok = run("addToTeam", studentId, [&] {
        require(Permission::BuildTeams);
        Team team = requireCurrentTeam();
        team.addMember(engine_.students().get(ss(studentId))); // duplicate / full / unknown checks
        currentMemberIds_.push_back(ss(studentId));
        logEvent(AppLog::Level::Info, QStringLiteral("team"),
                 QStringLiteral("Added %1 to the team for %2").arg(qs(engine_.students().get(ss(studentId)).name()),
                                                                   qs(team.requirement().name())));
    });
    refresh();
    return ok;
}

bool TeamForgeController::removeFromTeam(const QString& studentId)
{
    const bool ok = run("removeFromTeam", studentId, [&] {
        require(Permission::BuildTeams);
        Team team = requireCurrentTeam();
        team.removeMember(ss(studentId));
        currentMemberIds_.erase(std::find(currentMemberIds_.begin(), currentMemberIds_.end(), ss(studentId)));
        logEvent(AppLog::Level::Info, QStringLiteral("team"), QStringLiteral("Removed %1 from the team").arg(studentId));
    });
    refresh();
    return ok;
}

void TeamForgeController::clearTeam()
{
    if (!run("clearTeam", {}, [this] { require(Permission::BuildTeams); }))
        return;
    currentMemberIds_.clear();
    logEvent(AppLog::Level::Info, QStringLiteral("team"), QStringLiteral("Cleared the current team"));
    refresh();
}

bool TeamForgeController::suggestTeam()
{
    const bool ok = run("suggestTeam", qs(currentRequirementId_), [this] {
        require(Permission::BuildTeams);
        const Team team = engine_.suggestTeam(kCurrentTeamId, requireCurrentTeam().requirement(), activeStrategy());
        currentMemberIds_.clear();
        for (const Student& member : team.members())
            currentMemberIds_.push_back(member.id());
        logEvent(AppLog::Level::Info, QStringLiteral("team"),
                 QStringLiteral("Suggested a team of %1 (coverage %2%)")
                     .arg(team.size()).arg(qRound(team.coverageRatio() * 100)));
    });
    refresh();
    return ok;
}

bool TeamForgeController::finalizeTeam(const QString& teamId)
{
    const bool ok = run("finalizeTeam", teamId, [&] {
        require(Permission::BuildTeams);
        const Team current = requireCurrentTeam();
        current.validateForFinalization();
        const std::string id = requireNonEmpty(ss(teamId), "Team name");
        const bool taken = std::any_of(savedTeams_.begin(), savedTeams_.end(),
                                       [&](const TeamRecord& record) { return record.id == id; });
        if (taken)
            throw DuplicateError("A saved team named '" + id + "' already exists");
        TeamRecord record = recordOf(current);
        record.id = id;
        savedTeams_.push_back(std::move(record));
        currentMemberIds_.clear();
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("team"),
                 QStringLiteral("Saved team '%1' for %2").arg(teamId, qs(current.requirement().name())));
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

bool TeamForgeController::deleteSavedTeam(const QString& teamId)
{
    const bool ok = run("deleteSavedTeam", teamId, [&] {
        require(Permission::BuildTeams);
        const auto it = std::find_if(savedTeams_.begin(), savedTeams_.end(),
                                     [&](const TeamRecord& record) { return record.id == ss(teamId); });
        if (it == savedTeams_.end())
            throw NotFoundError("Saved team '" + ss(teamId) + "' not found");
        savedTeams_.erase(it);
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("team"), QStringLiteral("Deleted saved team '%1'").arg(teamId));
    });
    if (ok)
        notifyDataChanged();
    refresh(); // saved-team counts in currentRequirement
    return ok;
}

bool TeamForgeController::saveTeam(const QString& teamId)
{
    return finalizeTeam(teamId) && save();
}

QVariantMap TeamForgeController::evaluateCandidate(const QString& studentId)
{
    QVariantMap map;
    run("evaluateCandidate", studentId, [&] {
        require(Permission::BuildTeams);
        Team team = requireCurrentTeam();
        const std::string id = ss(studentId);
        const Student& student = engine_.students().get(id);
        if (team.contains(id))
            team.removeMember(id); // score a member's contribution as if added last
        map = matchResultToMap(activeStrategy().evaluate(student, team), team, !useBaseline_);
        map.insert(QStringLiteral("isMember"), currentTeam_->contains(id));
    }, false);
    return map;
}

// --- profiles and requirements ---------------------------------------------------------------

bool TeamForgeController::saveStudent(const QVariantMap& student)
{
    const bool ok = run("saveStudent", student.value(QStringLiteral("id")).toString(), [&] {
        require(Permission::EditAnyProfile);
        upsertStudent(studentFrom(student)); // core validation
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

bool TeamForgeController::removeStudent(const QString& studentId)
{
    const bool ok = run("removeStudent", studentId, [&] {
        require(Permission::EditAnyProfile);
        const std::string id = ss(studentId);
        for (const TeamRecord& record : savedTeams_) {
            if (std::find(record.memberIds.begin(), record.memberIds.end(), id) != record.memberIds.end())
                throw ValidationError(engine_.students().get(id).name() + " is in saved team '" + record.id + "'; delete that team first");
        }
        engine_.removeStudent(id);
        interests_.removeStudent(id);
        currentMemberIds_.erase(std::remove(currentMemberIds_.begin(), currentMemberIds_.end(), id),
                                currentMemberIds_.end());
        if (myProfileId_ == id)
            myProfileId_.clear();
        if (previewProfileId_ == id)
            previewProfileId_ = pickPreviewProfile();
        setDirty(true);
        logEvent(AppLog::Level::Info, QStringLiteral("profile"), QStringLiteral("Removed profile %1").arg(studentId));
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

bool TeamForgeController::saveRequirement(const QVariantMap& requirement)
{
    const bool ok = run("saveRequirement", requirement.value(QStringLiteral("id")).toString(), [&] {
        require(Permission::ManageProjects);
        upsertRequirement(requirementFrom(requirement)); // core validation
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

void TeamForgeController::upsertStudent(const Student& student)
{
    const bool exists = engine_.students().contains(student.id());
    if (exists)
        engine_.updateStudent(student);
    else
        engine_.addStudent(student);
    setDirty(true);
    logEvent(AppLog::Level::Info, QStringLiteral("profile"),
             QStringLiteral("%1 profile %2").arg(exists ? QStringLiteral("Updated") : QStringLiteral("Created"), qs(student.id())));
}

void TeamForgeController::upsertRequirement(const ProjectRequirement& parsed)
{
    const ProjectRequirement *existing = requirements_.find(parsed.id());
    if (existing == nullptr) {
        requirements_.add(parsed);
    } else {
        const ProjectRequirement previous = *existing;
        requirements_.update(parsed);
        try {
            for (const TeamRecord& record : savedTeams_) {
                if (record.requirementId == parsed.id())
                    buildTeam(record); // a smaller maximum size may no longer fit
            }
        } catch (const TeamForgeError&) {
            requirements_.update(previous);
            throw;
        }
    }
    if (currentRequirementId_.empty())
        currentRequirementId_ = parsed.id();
    touchRecent(parsed.id());
    setDirty(true);
    logEvent(AppLog::Level::Info, QStringLiteral("project"),
             QStringLiteral("%1 project '%2' (%3 required skills)")
                 .arg(existing == nullptr ? QStringLiteral("Created") : QStringLiteral("Updated"),
                      qs(parsed.name()))
                 .arg(parsed.requiredSkills().size()));
}

QString TeamForgeController::newRequirementId() const
{
    for (int n = static_cast<int>(requirements_.size()) + 1;; ++n) {
        const QString id = QStringLiteral("req-%1").arg(n, 3, 10, QLatin1Char('0'));
        if (!requirements_.contains(ss(id)))
            return id;
    }
}

bool TeamForgeController::removeRequirement(const QString& requirementId)
{
    const bool ok = run("removeRequirement", requirementId, [&] {
        require(Permission::ManageProjects);
        const std::string id = ss(requirementId);
        for (const TeamRecord& record : savedTeams_) {
            if (record.requirementId == id)
                throw ValidationError(requirements_.get(id).name() + " is used by saved team '" + record.id + "'; delete that team first");
        }
        const std::string name = requirements_.get(id).name();
        requirements_.remove(id);
        interests_.removeRequirement(id);
        recentRequirementIds_.erase(std::remove(recentRequirementIds_.begin(), recentRequirementIds_.end(), id),
                                    recentRequirementIds_.end());
        if (currentRequirementId_ == id) {
            currentMemberIds_.clear();
            currentRequirementId_ = requirements_.empty() ? std::string() : requirements_.values().front().id();
        }
        setDirty(true);
        saveSession();
        logEvent(AppLog::Level::Info, QStringLiteral("project"), QStringLiteral("Deleted project '%1'").arg(qs(name)));
    });
    if (ok)
        notifyDataChanged();
    refresh();
    return ok;
}

// --- skill structures ------------------------------------------------------------------------

QVariantList TeamForgeController::relatedSkills(const QString& skill, int maxDepth)
{
    QVariantList list;
    run("relatedSkills", skill, [&] {
        for (const SkillDistance& related : engine_.relatedSkills(ss(skill), maxDepth))
            list.append(QVariantMap{{QStringLiteral("skill"), qs(related.skill)},
                                    {QStringLiteral("distance"), related.distance}});
    }, false);
    return list;
}

QStringList TeamForgeController::skillsWithPrefix(const QString& prefix)
{
    QStringList list;
    run("skillsWithPrefix", prefix, [&] {
        // Offered skills come from the AVL tree (SkillBST prefix search). Skills only a project
        // requires or only the catalogue knows are merged in, so they autocomplete too.
        std::set<std::string> merged;
        for (const std::string& skill : engine_.skillsWithPrefix(ss(prefix)))
            merged.insert(skill);
        const std::string key = prefix.trimmed().isEmpty() ? std::string() : normalizeSkill(ss(prefix));
        for (auto it = skillCatalog_.lower_bound(key); it != skillCatalog_.end() && it->first.compare(0, key.size(), key) == 0; ++it)
            merged.insert(it->first);
        for (const ProjectRequirement& requirement : requirements_.values()) {
            for (const auto& entry : requirement.requiredSkills()) {
                if (entry.first.compare(0, key.size(), key) == 0)
                    merged.insert(entry.first);
            }
        }
        list = toQStringList(merged);
    }, false);
    return list;
}

QVariantList TeamForgeController::popularSkills(int count) const
{
    std::vector<std::pair<std::string, int>> all = engine_.skillTree().inOrder();
    std::stable_sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    QVariantList list;
    for (std::size_t i = 0; i < all.size() && static_cast<int>(i) < count; ++i)
        list.append(QVariantMap{{QStringLiteral("skill"), qs(all[i].first)}, {QStringLiteral("count"), all[i].second}});
    return list;
}

void TeamForgeController::clearError()
{
    setLastError({});
}

// --- helpers ---------------------------------------------------------------------------------

bool TeamForgeController::run(const char *operation, const QString& subject, const std::function<void()>& action,
                              bool clearErrorOnSuccess)
{
    try {
        action();
        if (clearErrorOnSuccess)
            setLastError({});
        return true;
    } catch (const TeamForgeError& error) {
        reportError(operation, subject, errorTypeOf(error), QString::fromUtf8(error.what()));
    } catch (const std::exception& error) {
        reportError(operation, subject, QStringLiteral("std::exception"),
                    QStringLiteral("Unexpected error: ") + QString::fromUtf8(error.what()));
    }
    return false;
}

void TeamForgeController::reportError(const char *operation, const QString& subject, const QString& type,
                                      const QString& message)
{
    const QString op = QString::fromLatin1(operation);
    lastErrorDetail_ = {
        {QStringLiteral("type"), type},
        {QStringLiteral("operation"), op},
        {QStringLiteral("message"), message},
        {QStringLiteral("record"), subject},
        {QStringLiteral("storage"), dataDirectory_},
        {QStringLiteral("workspace"), workspace_.isEmpty() ? QStringLiteral("entry") : workspace_},
        {QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate)},
    };
    const QString logLine = QStringLiteral("%1 in %2%3: %4")
                                .arg(type, op, subject.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(subject), message);
    logEvent(type == QLatin1String("PersistenceError") ? AppLog::Level::Error : AppLog::Level::Warning,
             QStringLiteral("error"), logLine);
    lastErrorDetail_.insert(QStringLiteral("logEntry"), logLine);
    // Developers (and headless use) get the backend message; everyone else a friendly line
    // without paths, ids or exception names.
    const QString shown = isDeveloperView() ? message : friendlyMessage(op, type);
    if (lastError_ == shown)
        emit lastErrorChanged(); // detail changed
    setLastError(shown);
}

QString TeamForgeController::friendlyMessage(const QString& op, const QString& type) const
{
    const bool participant = workspace_ == kParticipant;
    if (type == QLatin1String("PermissionError"))
        return participant ? QStringLiteral("That isn't available in your participant workspace.")
                           : QStringLiteral("That isn't available in the host workspace.");
    if (type == QLatin1String("PersistenceError"))
        return QStringLiteral("Couldn't save your changes. Please try again.");
    if (op == QLatin1String("saveMyProfile") || op == QLatin1String("claimProfile"))
        return QStringLiteral("Couldn't update your profile. Check the required fields.");
    if (op == QLatin1String("evaluateFit"))
        return QStringLiteral("Couldn't load this opportunity.");
    if (op == QLatin1String("expressInterest"))
        return type == QLatin1String("DuplicateError") ? QStringLiteral("You've already expressed interest in this project.")
                                                      : QStringLiteral("Couldn't send your interest. Please try again.");
    if (op == QLatin1String("acceptInterest") || op == QLatin1String("declineInterest")
        || op == QLatin1String("reviewInterest"))
        return QStringLiteral("Couldn't update this request. Please try again.");
    if (op == QLatin1String("saveRequirement"))
        return type == QLatin1String("TeamConstraintError")
                   ? QStringLiteral("A saved team is larger than this team size. Delete it or keep a larger maximum.")
                   : QStringLiteral("Couldn't save this project. Check the required details.");
    if (op == QLatin1String("removeRequirement"))
        return QStringLiteral("This project has saved teams. Delete them before deleting the project.");
    if (op == QLatin1String("addToTeam")) {
        if (type == QLatin1String("TeamConstraintError")) return QStringLiteral("Your team is full.");
        if (type == QLatin1String("DuplicateError")) return QStringLiteral("This person is already in your team.");
        return QStringLiteral("Couldn't add this person to the team.");
    }
    if (op == QLatin1String("finalizeTeam")) {
        if (type == QLatin1String("DuplicateError")) return QStringLiteral("A team with this name already exists.");
        if (type == QLatin1String("TeamConstraintError")) return QStringLiteral("Add more members before saving the team.");
        return QStringLiteral("Enter a team name to save the team.");
    }
    if (op == QLatin1String("suggestTeam"))
        return QStringLiteral("Couldn't suggest a team for this project.");
    if (op == QLatin1String("load"))
        return QStringLiteral("Couldn't load TeamForge data.");
    return participant ? QStringLiteral("Something went wrong. Please try again.")
                       : QStringLiteral("Couldn't complete that action. Please try again.");
}

void TeamForgeController::refresh()
{
    currentTeam_.reset();
    ranked_.clear();
    if (const ProjectRequirement *requirement = requirements_.find(currentRequirementId_)) {
        Team team(kCurrentTeamId, *requirement);
        std::vector<std::string> kept;
        QStringList dropped;
        for (const std::string& id : currentMemberIds_) {
            const Student *student = engine_.students().find(id);
            if (student == nullptr || team.isFull()) { // removed, or the maximum size shrank
                dropped.append(qs(id));
                continue;
            }
            team.addMember(*student);
            kept.push_back(id);
        }
        currentMemberIds_ = std::move(kept);
        ranked_ = engine_.rank(team, activeStrategy(), static_cast<std::size_t>(topK_));
        currentTeam_ = std::move(team);
        if (!dropped.isEmpty())
            setLastError(isDeveloperView() ? QStringLiteral("Removed from the current team: ") + dropped.join(QStringLiteral(", "))
                                           : QStringLiteral("Some members no longer fit this project and were removed."));
    }
    emit selectionChanged();
}

const MatchingStrategy& TeamForgeController::activeStrategy() const
{
    if (useBaseline_)
        return baseline_;
    return weighted_;
}

Team TeamForgeController::requireCurrentTeam() const
{
    if (!currentTeam_)
        throw ValidationError("Select a project first");
    return *currentTeam_;
}

Team TeamForgeController::buildTeam(const TeamRecord& record) const
{
    Team team(record.id, requirements_.get(record.requirementId));
    for (const std::string& memberId : record.memberIds)
        team.addMember(engine_.students().get(memberId));
    return team;
}

TeamForgeController::TeamRecord TeamForgeController::recordOf(const Team& team)
{
    TeamRecord record{team.id(), team.requirement().id(), {}};
    for (const Student& member : team.members())
        record.memberIds.push_back(member.id());
    return record;
}

void TeamForgeController::notifyDataChanged()
{
    studentsCache_.reset();
    searchCache_.clear();
    requirementsCache_.reset();
    opportunitiesCache_.reset();
    skillRowsCache_.reset();
    emit dataChanged();
    emit profileChanged();
    emit statusChanged();
}

void TeamForgeController::touchRecent(const std::string& requirementId)
{
    recentRequirementIds_.erase(std::remove(recentRequirementIds_.begin(), recentRequirementIds_.end(), requirementId),
                                recentRequirementIds_.end());
    recentRequirementIds_.insert(recentRequirementIds_.begin(), requirementId);
    saveSession();
}

void TeamForgeController::setDirty(bool dirty)
{
    if (dirty_ == dirty)
        return;
    dirty_ = dirty;
    emit dirtyChanged();
    emit statusChanged();
}

void TeamForgeController::setLastError(const QString& message)
{
    if (lastError_ == message)
        return;
    lastError_ = message;
    emit lastErrorChanged();
}

void TeamForgeController::logEvent(AppLog::Level level, const QString& category, const QString& message)
{
    log_.add(level, category, message);
    emit logChanged();
}

// --- session -------------------------------------------------------------------------------

QString TeamForgeController::sessionFile() const
{
    return dataDirectory_ + QStringLiteral("/session.json");
}

// The local session: last workspace, the participant's own profile, recent projects. Small and
// best-effort: a missing or unreadable file just means a fresh session.
void TeamForgeController::loadSession()
{
    QFile file(sessionFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QString last = root.value(QStringLiteral("lastWorkspace")).toString();
    lastWorkspace_ = isWorkspaceName(last) ? last : QString();
    const std::string me = ss(root.value(QStringLiteral("myProfileId")).toString());
    myProfileId_ = engine_.students().contains(me) ? me : std::string();
    recentRequirementIds_.clear();
    for (const QJsonValue& id : root.value(QStringLiteral("recentProjects")).toArray()) {
        if (requirements_.contains(ss(id.toString())))
            recentRequirementIds_.push_back(ss(id.toString()));
    }
    if (!recentRequirementIds_.empty() && recentRequirementIds_.front() != currentRequirementId_) {
        currentRequirementId_ = recentRequirementIds_.front();
        currentMemberIds_.clear();
        refresh();
    }
    opportunitiesCache_.reset();
    emit workspaceChanged();
    emit profileChanged();
}

void TeamForgeController::saveSession()
{
    if (!QFileInfo(dataDirectory_).isDir())
        return;
    QJsonArray recent;
    for (const std::string& id : recentRequirementIds_)
        recent.append(qs(id));
    const QJsonObject root{{QStringLiteral("version"), 1},
                           {QStringLiteral("lastWorkspace"), lastWorkspace_},
                           {QStringLiteral("myProfileId"), qs(myProfileId_)},
                           {QStringLiteral("recentProjects"), recent}};
    QSaveFile file(sessionFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) < 0 || !file.commit())
        log_.warning(QStringLiteral("session"), QStringLiteral("Could not write %1").arg(sessionFile()));
}
