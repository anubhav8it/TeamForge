#pragma once

#include "AppLog.h"

#include "core/CoverageOnlyStrategy.h"
#include "core/Interest.h"
#include "core/JsonStorage.h"
#include "core/MatchResult.h"
#include "core/MatchingEngine.h"
#include "core/ProjectRequirement.h"
#include "core/Repository.h"
#include "core/Team.h"
#include "core/WeightedMatchingStrategy.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

// QML-facing adapter over the C++ core, available in QML as the singleton `Backend`.
//
// It owns no matching rules of its own: it converts core objects into QVariant maps/lists,
// forwards user actions to MatchingEngine, Team and the JSON storage, and turns every
// TeamForgeError into `lastError` (actions return false) so exceptions never reach the QML
// engine. The "current team" is the team being assembled for the selected requirement; saved
// teams are finalised ones, persisted to teams.json.
//
// Workspaces. The app has three local workspaces: "participant", "host" and "developer"
// ("" = the entry screen). Permissions are enforced here, not only hidden in QML: every action
// checks the active workspace and throws PermissionError otherwise. Participants and hosts get
// short, friendly error messages; the developer workspace gets the backend message, and the
// full details (type, operation, record, storage, time) are kept in `lastErrorDetail` and the
// local log. A developer can preview the participant or host workspace with exactly those
// permissions. The workspace choice, the participant's own profile id and recent projects are
// kept in <data dir>/session.json; there is no account system.
//
// Interest workflow. A participant expresses interest in a project; the host reviews the
// request and accepts or declines it (InterestBook, saved to interest_requests.json). Accepted
// participants are offered to the host's team building; nobody joins a team automatically.
// Expressed interest is also the matching's engagement factor (10%).
class TeamForgeController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Backend)
    QML_SINGLETON

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(QVariantList students READ students NOTIFY dataChanged)
    Q_PROPERTY(QVariantList requirements READ requirements NOTIFY dataChanged)
    Q_PROPERTY(QVariantList savedTeams READ savedTeams NOTIFY dataChanged)
    Q_PROPERTY(QStringList skills READ skills NOTIFY dataChanged)
    // Distinct skills across profiles, projects and the skill catalogue (projects may ask for
    // skills nobody offers yet; a developer may catalogue a skill before anyone offers it).
    Q_PROPERTY(int skillCount READ skillCount NOTIFY dataChanged)
    // Category names used in the skill catalogue (skills.json), sorted.
    Q_PROPERTY(QStringList skillCategories READ skillCategories NOTIFY dataChanged)
    Q_PROPERTY(QVariantList factorDefinitions READ factorDefinitions CONSTANT)
    Q_PROPERTY(QVariantList strategies READ strategies CONSTANT)

    Q_PROPERTY(QString workspace READ workspace NOTIFY workspaceChanged)
    Q_PROPERTY(bool previewing READ isPreviewing NOTIFY workspaceChanged)
    Q_PROPERTY(QString lastWorkspace READ lastWorkspace NOTIFY workspaceChanged)

    Q_PROPERTY(QString currentRequirementId READ currentRequirementId WRITE setCurrentRequirementId NOTIFY selectionChanged)
    // The selected requirement in the same shape as `requirements` entries; empty if none.
    Q_PROPERTY(QVariantMap currentRequirement READ currentRequirement NOTIFY selectionChanged)
    // All requirements, most recently selected or edited first (kept in session.json).
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY selectionChanged)
    Q_PROPERTY(QString strategy READ strategy WRITE setStrategy NOTIFY selectionChanged)
    Q_PROPERTY(int topK READ topK WRITE setTopK NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap currentTeam READ currentTeam NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList rankedCandidates READ rankedCandidates NOTIFY selectionChanged)

    // Participant workspace: the local participant's own profile and their project fits.
    Q_PROPERTY(QString myProfileId READ myProfileId NOTIFY profileChanged)
    Q_PROPERTY(QVariantMap myProfile READ myProfile NOTIFY profileChanged)
    Q_PROPERTY(QVariantList opportunities READ opportunities NOTIFY profileChanged)
    // The participant's interest requests, newest first: {id, requirementId, name, type, status, score}.
    Q_PROPERTY(QVariantList myInterests READ myInterests NOTIFY profileChanged)

    // Host: accepted participants of the selected project, scored against the current team (same
    // shape as rankedCandidates entries).
    Q_PROPERTY(QVariantList acceptedCandidates READ acceptedCandidates NOTIFY selectionChanged)

    // Local demonstration identities: [{kind, id, name, label}] for the demo participant
    // (TF-P001, Anubhav Bisht) and the demo host (TF-H001). No passwords; the host workspace
    // always acts as the one local host.
    Q_PROPERTY(QVariantList demoIdentities READ demoIdentities CONSTANT)
    Q_PROPERTY(QVariantMap hostIdentity READ hostIdentity CONSTANT)

    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantMap lastErrorDetail READ lastErrorDetail NOTIFY lastErrorChanged)

    // Developer workspace.
    Q_PROPERTY(QVariantList logEntries READ logEntries NOTIFY logChanged)
    Q_PROPERTY(QVariantMap systemStatus READ systemStatus NOTIFY statusChanged)

public:
    // Data directory: $TEAMFORGE_DATA_DIR if set, otherwise "data" next to the executable, or
    // <local app data>/TeamForge/data when that folder is not writable.
    // This constructor (the one QML uses) first syncs that folder with the bundled samples.
    explicit TeamForgeController(QObject *parent = nullptr);
    explicit TeamForgeController(const QString& dataDirectory, QObject *parent = nullptr);

    QString version() const;
    QString dataDirectory() const { return dataDirectory_; }
    QVariantList students() const;
    QVariantList requirements() const;
    QVariantList savedTeams() const;
    QStringList skills() const;
    int skillCount() const;
    QStringList skillCategories() const;
    QVariantList factorDefinitions() const;
    QVariantList strategies() const;

    QString workspace() const { return workspace_; }
    bool isPreviewing() const { return previewing_; }
    QString lastWorkspace() const { return lastWorkspace_; }

    QString currentRequirementId() const { return QString::fromStdString(currentRequirementId_); }
    void setCurrentRequirementId(const QString& requirementId);
    QVariantMap currentRequirement() const;
    QVariantList recentProjects() const;
    QString strategy() const { return useBaseline_ ? QStringLiteral("baseline") : QStringLiteral("weighted"); }
    void setStrategy(const QString& strategyId);
    int topK() const { return topK_; }
    void setTopK(int k);
    QVariantMap currentTeam() const;
    QVariantList rankedCandidates() const;

    QString myProfileId() const { return QString::fromStdString(ownProfileId()); }
    QVariantMap myProfile() const;
    QVariantList opportunities() const;
    QVariantList myInterests() const;
    QVariantList acceptedCandidates() const;
    QVariantList demoIdentities() const;
    QVariantMap hostIdentity() const;

    bool isDirty() const { return dirty_; }
    QString lastError() const { return lastError_; }
    QVariantMap lastErrorDetail() const { return lastErrorDetail_; }
    QVariantList logEntries() const { return log_.entries(); }
    QVariantMap systemStatus() const;

    // Where reset / seeding read the samples from (tests point this at the repository's data/).
    void setSampleDirectory(const QString& directory) { sampleDirectory_ = directory; }

    // --- workspaces ----------------------------------------------------------------------
    // "participant", "host" or "developer". Leaving returns to the entry screen.
    Q_INVOKABLE bool enterWorkspace(const QString& workspace);
    Q_INVOKABLE void leaveWorkspace();
    // Which workspaces this run may open. By default (and always in the desktop build) all
    // three. The browser build signs people in and sets $TEAMFORGE_ALLOWED_WORKSPACES
    // (comma-separated) from the account's role before the app starts; enterWorkspace() then
    // refuses the others. Read once, when the controller is created.
    Q_INVOKABLE bool workspaceAllowed(const QString& workspace) const;
    // Developer only: show the participant or host workspace with exactly its permissions.
    // `identity` picks who to be: a participant id (participant preview; default: the session's
    // own profile, else the demo participant) or a project id to open (host preview). No
    // onboarding is needed and the session is not changed. While previewing, the developer can
    // switch straight to the other workspace's preview.
    Q_INVOKABLE bool previewWorkspace(const QString& workspace, const QString& identity = QString());
    Q_INVOKABLE void endPreview();

    // Persistence. reload() replaces the in-memory state only if every file loads.
    Q_INVOKABLE bool reload();
    Q_INVOKABLE bool save();

    // --- host: current team ----------------------------------------------------------------
    Q_INVOKABLE bool addToTeam(const QString& studentId);
    Q_INVOKABLE bool removeFromTeam(const QString& studentId);
    Q_INVOKABLE void clearTeam();
    // Replaces the current members with MatchingEngine::suggestTeam's greedy pick.
    Q_INVOKABLE bool suggestTeam();
    // Validates the current team, stores it under `teamId` and starts a new empty team.
    Q_INVOKABLE bool finalizeTeam(const QString& teamId);
    // finalizeTeam() followed by save(): the team is written to teams.json immediately.
    Q_INVOKABLE bool saveTeam(const QString& teamId);
    Q_INVOKABLE bool deleteSavedTeam(const QString& teamId);

    // Scores one student against the current team with the active strategy, in the same shape
    // as a `rankedCandidates` entry. A current member is scored as if they were added last.
    // Returns an empty map (and sets lastError) if there is no selected project.
    Q_INVOKABLE QVariantMap evaluateCandidate(const QString& studentId);
    // Scores a student against `requirementId` as the first member of a new team (weighted
    // strategy): a participant's fit for an opportunity. Same shape as evaluateCandidate.
    Q_INVOKABLE QVariantMap evaluateFit(const QString& studentId, const QString& requirementId);

    // --- profiles and projects -------------------------------------------------------------
    // Add-or-update by id (developer). Maps use the same keys as the JSON files; skills may be
    // given as {"skill": level} or [{"skill": ..., "level"/"minLevel": ...}].
    Q_INVOKABLE bool saveStudent(const QVariantMap& student);
    Q_INVOKABLE bool removeStudent(const QString& studentId);
    Q_INVOKABLE bool saveRequirement(const QVariantMap& requirement);
    Q_INVOKABLE bool removeRequirement(const QString& requirementId);
    // An unused id ("req-NNN") for a new requirement.
    Q_INVOKABLE QString newRequirementId() const;

    // Participant: create or update the local participant's own profile and save it to disk.
    // A new profile gets a fresh id; its diversity `role` is its strongest skill.
    Q_INVOKABLE bool saveMyProfile(const QVariantMap& profile);
    // Participant: use an existing directory profile as "me".
    Q_INVOKABLE bool claimProfile(const QString& studentId);
    // Participant: continue as the demo participant (TF-P001).
    Q_INVOKABLE bool useDemoParticipant();
    Q_INVOKABLE void forgetMyProfile();
    // A one-line skills-and-experience sentence for a draft profile ([{skill, level}]).
    Q_INVOKABLE QString composeSummary(const QVariantList& skills) const;
    // {completion: 0..1, checklist: [{key, label, done}]} for a profile or an unsaved draft.
    Q_INVOKABLE QVariantMap profileCompletion(const QVariantMap& profile) const;

    // --- interest workflow ---------------------------------------------------------------------
    // Participant: express interest in a project (saved immediately).
    Q_INVOKABLE bool expressInterest(const QString& requirementId);
    // Host: the project's requests, best fit first within each status:
    // [{id, status, studentId, number, name, program, topSkills, experience, averageLevel, score}].
    Q_INVOKABLE QVariantList projectInterests(const QString& requirementId) const;
    // Host: open a request (Interested -> Under review), accept or decline it (saved immediately).
    Q_INVOKABLE bool reviewInterest(const QString& requestId);
    Q_INVOKABLE bool acceptInterest(const QString& requestId);
    Q_INVOKABLE bool declineInterest(const QString& requestId);

    // Directory search (any workspace): case-insensitive name substring, or a skill whose
    // name, any word of it, or its initials ("ml" -> "machine learning") starts with the query.
    // `skill` filters to one exact skill; `sort` is "name" or "level". Same entries as `students`.
    Q_INVOKABLE QVariantList searchStudents(const QString& query, const QString& skill, const QString& sort) const;

    // Skill structures: BFS over SkillGraph, prefix search in SkillBST, most offered skills.
    Q_INVOKABLE QVariantList relatedSkills(const QString& skill, int maxDepth);
    Q_INVOKABLE QStringList skillsWithPrefix(const QString& prefix);
    Q_INVOKABLE QVariantList popularSkills(int count) const;

    // --- developer -------------------------------------------------------------------------
    // Each returns {name, ok, message, details: [strings], durationMs} and logs the outcome.
    Q_INVOKABLE QVariantMap resetSampleData();
    Q_INVOKABLE QVariantMap rebuildSkillIndex();
    Q_INVOKABLE QVariantMap rebuildSkillTree();
    Q_INVOKABLE QVariantMap rebuildSkillGraph();
    Q_INVOKABLE QVariantMap validateData();
    Q_INVOKABLE QVariantMap runMatchingSmokeTest();
    Q_INVOKABLE QVariantMap runPersistenceCheck();
    Q_INVOKABLE QVariantMap runIndexCheck();
    Q_INVOKABLE QVariantList runAllChecks();

    // --- developer: records -------------------------------------------------------------------
    // The full participant database, filtered and sorted in C++. `filter` keys (all optional):
    // query (name, id or skill as in searchStudents), skill (exact), experience ("Beginner" ..
    // "Advanced") and sort ("name", "id", "experience", "skills"). Returns {total, count, rows};
    // rows are `students` entries, whose `number` is the record's place in the database (A-Z).
    Q_INVOKABLE QVariantMap queryParticipants(const QVariantMap& filter) const;
    // Every known skill (offered, required or catalogued). `filter`: query, category ("" = all,
    // "Uncategorised"), sort ("participants", "projects", "name"). Returns {total, count, rows:
    // [{name, category, participants, projects, learners}]}.
    Q_INVOKABLE QVariantMap querySkills(const QVariantMap& filter) const;
    // One skill: counts, the projects that need it, the strongest holders, related skills (BFS
    // over SkillGraph, by co-occurrence weight) and its state in SkillIndex, SkillBST, SkillGraph.
    Q_INVOKABLE QVariantMap skillDetail(const QString& skill) const;
    // Create a skill in the catalogue, or change its category.
    Q_INVOKABLE bool saveSkill(const QString& skill, const QString& category);
    // Rename (or merge into an existing skill) across every profile, learning interest, project
    // and the catalogue. A merged skill keeps the higher level / minimum.
    Q_INVOKABLE bool renameSkill(const QString& from, const QString& to);
    // Remove a skill everywhere. Refused if it is the only skill of a profile or a project.
    Q_INVOKABLE bool deleteSkill(const QString& skill);
    // A record seen four ways: as loaded (in memory, stored format), as stored on disk, in the
    // skill structures, and as matching sees it. `kind`: "participant", "project", "team", "skill".
    Q_INVOKABLE QVariantMap inspectRecord(const QString& kind, const QString& id) const;
    // Replace (or add) a participant or project from its raw JSON, parsed and validated exactly
    // like the data files. Applied in memory; save() writes it.
    Q_INVOKABLE bool applyRecordJson(const QString& kind, const QString& json);

    Q_INVOKABLE void clearError();

signals:
    void dataChanged();
    void selectionChanged();
    void dirtyChanged();
    void lastErrorChanged();
    void workspaceChanged();
    void profileChanged();
    void interestsChanged();
    void logChanged();
    void statusChanged();

private:
    // A saved team as stored on disk; rebuilt into a Team against current profiles when needed.
    struct TeamRecord
    {
        std::string id;
        std::string requirementId;
        std::vector<std::string> memberIds;
    };

    enum class Permission { EditOwnProfile, EditAnyProfile, ManageProjects, BuildTeams, Evaluate, DeveloperTools };

    // Directory search keys, built with the students cache.
    struct SearchEntry
    {
        QString name;                    // lower case
        QString id;                      // lower case
        std::vector<std::string> skills; // normalised
        double averageLevel;
    };
    // One row of the skill database, built once per data change.
    struct SkillRow
    {
        std::string name;
        int number; // place in the skill database (A-Z), 1-based
        QString category;
        int participants;
        int projects;
        int learners;
    };

    static QString defaultDataDirectory();

    // Runs `action` as user operation `operation` (on `subject`, e.g. a student id). On a
    // TeamForgeError it records the details, logs them and sets the role-appropriate lastError,
    // then returns false. Actions clear lastError on success; read-only queries pass
    // clearErrorOnSuccess = false so they never hide an error.
    bool run(const char *operation, const QString& subject, const std::function<void()>& action,
             bool clearErrorOnSuccess = true);
    void reportError(const char *operation, const QString& subject, const QString& type, const QString& message);
    QString friendlyMessage(const QString& operation, const QString& type) const;
    void require(Permission permission) const;
    bool allowed(Permission permission) const; // require() without the throw, for read-only queries
    bool isDeveloperView() const { return workspace_ == QLatin1String("developer") || workspace_.isEmpty(); }

    bool loadData(); // reload() without the permission check (startup)
    // The participant profile in use: the session's own profile, or the preview profile while a
    // developer previews the participant workspace.
    const std::string& ownProfileId() const { return previewing_ ? previewProfileId_ : myProfileId_; }
    std::string& ownProfileSlot() { return previewing_ ? previewProfileId_ : myProfileId_; }
    std::string pickPreviewProfile() const;
    // Add-or-update with the integrity checks shared by the form and raw-JSON paths.
    void upsertStudent(const teamforge::Student& student);
    void upsertRequirement(const teamforge::ProjectRequirement& requirement);
    // Applies `edit` to every profile and project, all or nothing (validated before any change).
    void rewriteSkills(const std::function<teamforge::SkillLevels(const teamforge::SkillLevels&)>& edit,
                       const std::function<std::vector<std::string>(const std::set<std::string>&)>& editWanted);
    const std::vector<SkillRow>& skillRows() const;
    QString categoryOf(const std::string& skill) const;
    // Shared by accept / decline / review: moves a request, saves, refreshes the rankings.
    bool moveInterest(const char *operation, const QString& requestId, teamforge::InterestStatus status);
    QVariantMap interestToMap(const teamforge::InterestRequest& request) const;
    // Rebuilds the current Team from member ids and recomputes the ranking.
    void refresh();
    const teamforge::MatchingStrategy& activeStrategy() const;
    teamforge::Team requireCurrentTeam() const;
    teamforge::Team buildTeam(const TeamRecord& record) const;
    static TeamRecord recordOf(const teamforge::Team& team);
    QVariantMap teamToMap(const teamforge::Team& team) const;
    QVariantMap requirementToMap(const teamforge::ProjectRequirement& requirement) const;
    // `team` is the team the result was scored against (for the per-skill breakdown).
    // `weightedScore`: the result came from the weighted strategy, so per-factor contributions apply.
    QVariantMap matchResultToMap(const teamforge::MatchResult& result, const teamforge::Team& team,
                                 bool weightedScore) const;
    QVariantMap opportunityFor(const teamforge::Student& student, const teamforge::ProjectRequirement& requirement) const;
    void touchRecent(const std::string& requirementId);
    // Drops the cached lists, then emits dataChanged, profileChanged and statusChanged.
    void notifyDataChanged();
    QVariantMap cachedRequirement(const std::string& id) const;
    void setDirty(bool dirty);
    void setLastError(const QString& message);
    void setWorkspace(const QString& workspace, bool previewing);

    void loadSession();
    void saveSession();
    QString sessionFile() const;

    void logEvent(AppLog::Level level, const QString& category, const QString& message);
    // Times `check`, logs the outcome and packs it into the {name, ok, message, ...} map.
    QVariantMap runCheck(const QString& name, const std::function<QString(QStringList&)>& check);

    QString dataDirectory_;
    QString sampleDirectory_;
    teamforge::MatchingEngine engine_;
    teamforge::Repository<teamforge::ProjectRequirement> requirements_{"Requirement"};
    std::vector<TeamRecord> savedTeams_;
    teamforge::storage::SkillCatalog skillCatalog_;
    teamforge::InterestBook interests_; // also the strategies' engagement source

    QString workspace_;      // "" = entry screen
    QStringList allowedWorkspaces_; // empty = all (see workspaceAllowed)
    bool previewing_ = false;
    QString lastWorkspace_;  // from the session, for the entry screen
    std::string myProfileId_;
    std::string previewProfileId_;

    std::string currentRequirementId_;
    std::vector<std::string> recentRequirementIds_; // most recent first; kept in session.json
    std::vector<std::string> currentMemberIds_;
    std::optional<teamforge::Team> currentTeam_;
    std::vector<teamforge::MatchResult> ranked_;

    teamforge::WeightedMatchingStrategy weighted_;
    teamforge::CoverageOnlyStrategy baseline_;
    bool useBaseline_ = false;
    int topK_ = 10;

    bool dirty_ = false;
    QString lastError_;
    QVariantMap lastErrorDetail_;
    QDateTime lastLoaded_;
    QDateTime lastSaved_;
    QString seedMessage_;

    AppLog log_;

    mutable std::optional<QVariantList> studentsCache_;     // sorted by name
    mutable std::vector<SearchEntry> searchCache_;          // parallel to studentsCache_
    mutable std::optional<QVariantList> requirementsCache_;
    mutable std::optional<QVariantList> opportunitiesCache_;
    mutable std::optional<std::vector<SkillRow>> skillRowsCache_;
};
