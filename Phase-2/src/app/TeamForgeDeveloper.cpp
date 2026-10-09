// Developer workspace: system status, maintenance actions and self-checks. Part of
// TeamForgeController. Every action here requires the developer workspace.
#include "TeamForgeController.h"

#include "BridgeSupport.h"
#include "SampleData.h"

#include "core/Errors.h"
#include "core/JsonStorage.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

using namespace teamforge;
using namespace bridge;

QVariantMap TeamForgeController::systemStatus() const
{
    const SkillBST& tree = engine_.skillTree();
    const QVariantMap index{
        {QStringLiteral("indexSkills"), static_cast<int>(engine_.skillIndex().skillCount())},
        {QStringLiteral("treeSkills"), static_cast<int>(tree.size())},
        {QStringLiteral("treeHeight"), tree.height()},
        {QStringLiteral("treeBalanced"), tree.isValidAvl()},
        {QStringLiteral("graphSkills"), static_cast<int>(engine_.skillGraph().skillCount())},
        {QStringLiteral("graphEdges"), static_cast<int>(engine_.skillGraph().edgeCount())},
    };
    // Quick health: the three structures agree on the skill set and the tree is a valid AVL.
    const bool structuresAgree = engine_.skillIndex().skillCount() == tree.size()
                                 && engine_.skillGraph().skillCount() == tree.size();
    const bool healthy = structuresAgree && tree.isValidAvl() && QFileInfo(dataDirectory_).isDir();
    return {
        {QStringLiteral("version"), version()},
        {QStringLiteral("workspace"), workspace_.isEmpty() ? QStringLiteral("entry") : workspace_},
        {QStringLiteral("previewing"), previewing_},
        {QStringLiteral("participants"), static_cast<int>(engine_.students().size())},
        {QStringLiteral("projects"), static_cast<int>(requirements_.size())},
        {QStringLiteral("savedTeams"), static_cast<int>(savedTeams_.size())},
        {QStringLiteral("interests"), static_cast<int>(interests_.size())},
        {QStringLiteral("skills"), skillCount()},
        {QStringLiteral("offeredSkills"), static_cast<int>(tree.size())},
        {QStringLiteral("dataDirectory"), dataDirectory_},
        {QStringLiteral("sessionFile"), sessionFile()},
        {QStringLiteral("logFile"), log_.file()},
        {QStringLiteral("dirty"), dirty_},
        {QStringLiteral("lastLoaded"), lastLoaded_.toString(QStringLiteral("HH:mm:ss"))},
        {QStringLiteral("lastSaved"), lastSaved_.isValid() ? lastSaved_.toString(QStringLiteral("HH:mm:ss")) : QString()},
        {QStringLiteral("seed"), seedMessage_},
        {QStringLiteral("index"), index},
        {QStringLiteral("healthy"), healthy},
        {QStringLiteral("errorCount"), log_.errorCount()},
        {QStringLiteral("myProfileId"), qs(myProfileId_)},
    };
}

QVariantMap TeamForgeController::runCheck(const QString& name, const std::function<QString(QStringList&)>& check)
{
    QStringList details;
    QElapsedTimer timer;
    timer.start();
    QString message;
    bool ok = run("developerCheck", name, [&] {
        require(Permission::DeveloperTools);
        message = check(details);
    }, false);
    if (!ok)
        message = lastErrorDetail_.value(QStringLiteral("message")).toString();
    const double ms = static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
    logEvent(ok ? AppLog::Level::Info : AppLog::Level::Error, QStringLiteral("developer"),
             QStringLiteral("%1: %2 (%3 ms)").arg(name, ok ? message : QStringLiteral("FAILED - ") + message)
                 .arg(ms, 0, 'f', 1));
    emit statusChanged();
    return {{QStringLiteral("name"), name},
            {QStringLiteral("ok"), ok},
            {QStringLiteral("message"), message},
            {QStringLiteral("details"), details},
            {QStringLiteral("durationMs"), ms}};
}

QVariantMap TeamForgeController::resetSampleData()
{
    QVariantMap result = runCheck(QStringLiteral("Reset seed data"), [this](QStringList& details) {
        const sampledata::Result restored = sampledata::restore(dataDirectory_, sampleDirectory_);
        if (!restored.ok)
            throw PersistenceError(ss(restored.message));
        if (!restored.backupDir.isEmpty())
            details.append(QStringLiteral("Backup: %1").arg(restored.backupDir));
        seedMessage_ = restored.message;
        return restored.message;
    });
    if (result.value(QStringLiteral("ok")).toBool()) {
        currentTeam_.reset();
        loadData();
        if (!engine_.students().contains(myProfileId_))
            myProfileId_.clear();
        saveSession();
        emit profileChanged();
    }
    return result;
}

QVariantMap TeamForgeController::rebuildSkillIndex()
{
    return runCheck(QStringLiteral("Rebuild SkillIndex"), [this](QStringList& details) {
        const std::size_t before = engine_.skillIndex().skillCount();
        engine_.rebuildSkillIndex();
        details.append(QStringLiteral("Skills before: %1, after: %2").arg(before).arg(engine_.skillIndex().skillCount()));
        refresh();
        return QStringLiteral("Hash index rebuilt: %1 skills").arg(engine_.skillIndex().skillCount());
    });
}

QVariantMap TeamForgeController::rebuildSkillTree()
{
    return runCheck(QStringLiteral("Rebuild SkillBST"), [this](QStringList& details) {
        const int heightBefore = engine_.skillTree().height();
        engine_.rebuildSkillTree();
        details.append(QStringLiteral("Height before: %1, after: %2").arg(heightBefore).arg(engine_.skillTree().height()));
        details.append(engine_.skillTree().isValidAvl() ? QStringLiteral("AVL invariants hold")
                                                        : QStringLiteral("AVL invariants BROKEN"));
        notifyDataChanged();
        return QStringLiteral("AVL tree rebuilt: %1 skills, height %2")
            .arg(engine_.skillTree().size()).arg(engine_.skillTree().height());
    });
}

QVariantMap TeamForgeController::rebuildSkillGraph()
{
    return runCheck(QStringLiteral("Refresh SkillGraph"), [this](QStringList& details) {
        const std::size_t edgesBefore = engine_.skillGraph().edgeCount();
        engine_.rebuildSkillGraph();
        details.append(QStringLiteral("Edges before: %1, after: %2").arg(edgesBefore).arg(engine_.skillGraph().edgeCount()));
        return QStringLiteral("Co-occurrence graph rebuilt: %1 skills, %2 edges")
            .arg(engine_.skillGraph().skillCount()).arg(engine_.skillGraph().edgeCount());
    });
}

QVariantMap TeamForgeController::validateData()
{
    return runCheck(QStringLiteral("Validate stored data"), [this](QStringList& details) {
        // Re-read the files from disk through the same loaders: any invalid record or broken
        // reference throws PersistenceError with the file and record number.
        const std::string dir = ss(dataDirectory_) + "/";
        Repository<Student> students("Student");
        for (const Student& s : storage::loadStudents(dir + "students.json"))
            students.add(s);
        Repository<ProjectRequirement> requirements("Requirement");
        for (const ProjectRequirement& r : storage::loadRequirements(dir + "requirements.json"))
            requirements.add(r);
        const auto teams = storage::loadTeams(dir + "teams.json", students, requirements);
        std::size_t interestCount = 0;
        if (QFileInfo::exists(qs(dir + "interest_requests.json"))) {
            for (const InterestRequest& request : storage::loadInterests(dir + "interest_requests.json")) {
                if (!students.contains(request.studentId()) || !requirements.contains(request.requirementId()))
                    throw PersistenceError("Interest request '" + request.id() + "' refers to an unknown record");
                ++interestCount;
            }
        }
        std::set<std::string> names;
        int duplicateNames = 0;
        for (const Student& s : students.values())
            duplicateNames += names.insert(s.name()).second ? 0 : 1;
        details.append(QStringLiteral("%1 participants, %2 projects, %3 saved teams, %4 interest requests on disk")
                           .arg(students.size()).arg(requirements.size()).arg(teams.size()).arg(interestCount));
        details.append(QStringLiteral("Duplicate participant names: %1").arg(duplicateNames));
        if (students.size() != engine_.students().size() || requirements.size() != requirements_.size())
            details.append(QStringLiteral("Memory differs from disk: unsaved changes"));
        return QStringLiteral("All records valid; saved teams reference existing people and projects");
    });
}

QVariantMap TeamForgeController::runMatchingSmokeTest()
{
    return runCheck(QStringLiteral("Matching smoke test"), [this](QStringList& details) {
        int problems = 0;
        for (const ProjectRequirement& requirement : requirements_.values()) {
            const Team empty("smoke", requirement);
            const auto first = engine_.rank(empty, weighted_, 10);
            const auto second = engine_.rank(empty, weighted_, 10);
            bool deterministic = first.size() == second.size();
            bool ordered = true;
            bool inRange = true;
            for (std::size_t i = 0; i < first.size() && deterministic; ++i)
                deterministic = first[i].studentId == second[i].studentId;
            for (std::size_t i = 0; i < first.size(); ++i) {
                inRange = inRange && first[i].score >= 0.0 && first[i].score <= 1.0 + 1e-9;
                ordered = ordered && (i == 0 || first[i - 1].score >= first[i].score - 1e-12);
            }
            const Team suggested = engine_.suggestTeam("smoke", requirement, weighted_);
            const bool sizeOk = suggested.size() <= requirement.maxTeamSize();
            const bool ok = deterministic && ordered && inRange && sizeOk;
            problems += ok ? 0 : 1;
            details.append(QStringLiteral("%1 %2: top %3%, suggested team of %4, coverage %5%")
                               .arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), qs(requirement.name()))
                               .arg(first.empty() ? 0 : qRound(first.front().score * 100))
                               .arg(suggested.size())
                               .arg(qRound(suggested.coverageRatio() * 100)));
        }
        if (problems > 0)
            throw ValidationError(std::to_string(problems) + " project(s) failed the smoke test");
        return QStringLiteral("Rankings deterministic, ordered and in range for %1 projects").arg(requirements_.size());
    });
}

QVariantMap TeamForgeController::runPersistenceCheck()
{
    return runCheck(QStringLiteral("Persistence check"), [this](QStringList& details) {
        // Round-trip the in-memory state through the JSON storage in a temporary folder.
        QTemporaryDir temp;
        if (!temp.isValid())
            throw PersistenceError("Cannot create a temporary folder");
        const std::string dir = ss(temp.path()) + "/";
        std::vector<Team> teams;
        for (const TeamRecord& record : savedTeams_)
            teams.push_back(buildTeam(record));
        storage::saveStudents(dir + "students.json", engine_.students().values());
        storage::saveRequirements(dir + "requirements.json", requirements_.values());
        storage::saveTeams(dir + "teams.json", teams);
        storage::saveInterests(dir + "interest_requests.json", interests_.values());

        Repository<Student> students("Student");
        for (const Student& s : storage::loadStudents(dir + "students.json"))
            students.add(s);
        Repository<ProjectRequirement> requirements("Requirement");
        for (const ProjectRequirement& r : storage::loadRequirements(dir + "requirements.json"))
            requirements.add(r);
        const auto loadedTeams = storage::loadTeams(dir + "teams.json", students, requirements);
        const auto loadedInterests = storage::loadInterests(dir + "interest_requests.json");

        int mismatches = 0;
        for (const InterestRequest& request : loadedInterests) {
            const InterestRequest *original = interests_.find(request.studentId(), request.requirementId());
            mismatches += original != nullptr && original->status() == request.status() ? 0 : 1;
        }
        for (const Student& original : engine_.students().values()) {
            const Student *copy = students.find(original.id());
            const bool same = copy != nullptr && copy->name() == original.name()
                              && copy->skillsOffered() == original.skillsOffered()
                              && copy->summary() == original.summary() && copy->program() == original.program();
            mismatches += same ? 0 : 1;
        }
        details.append(QStringLiteral("Round-tripped %1 participants, %2 projects, %3 teams, %4 interest requests")
                           .arg(students.size()).arg(requirements.size()).arg(loadedTeams.size()).arg(loadedInterests.size()));
        if (mismatches > 0 || students.size() != engine_.students().size() || loadedTeams.size() != teams.size()
            || loadedInterests.size() != interests_.size())
            throw PersistenceError(std::to_string(mismatches) + " record(s) changed in the round trip");
        return QStringLiteral("Save and reload preserve every record");
    });
}

QVariantMap TeamForgeController::runIndexCheck()
{
    return runCheck(QStringLiteral("Index consistency"), [this](QStringList& details) {
        // Every offered skill must be in all three structures with matching counts.
        int problems = 0;
        std::map<std::string, int> offeredBy;
        for (const Student& student : engine_.students().values()) {
            for (const auto& entry : student.skillsOffered()) {
                ++offeredBy[entry.first];
                if (engine_.skillIndex().studentsWith(entry.first).count(student.id()) == 0)
                    ++problems;
            }
        }
        for (const auto& [skill, count] : offeredBy) {
            if (engine_.skillTree().count(skill) != count || !engine_.skillGraph().hasSkill(skill))
                ++problems;
        }
        const bool sizes = engine_.skillIndex().skillCount() == offeredBy.size()
                           && engine_.skillTree().size() == offeredBy.size()
                           && engine_.skillGraph().skillCount() == offeredBy.size();
        details.append(QStringLiteral("%1 offered skills; index %2, tree %3, graph %4")
                           .arg(offeredBy.size()).arg(engine_.skillIndex().skillCount())
                           .arg(engine_.skillTree().size()).arg(engine_.skillGraph().skillCount()));
        details.append(QStringLiteral("AVL height %1 (minimum possible %2)")
                           .arg(engine_.skillTree().height())
                           .arg(static_cast<int>(std::ceil(std::log2(static_cast<double>(offeredBy.size()) + 1)))));
        if (problems > 0 || !sizes || !engine_.skillTree().isValidAvl())
            throw ValidationError(std::to_string(problems) + " inconsistencies between the skill structures");
        return QStringLiteral("SkillIndex, SkillBST and SkillGraph agree");
    });
}

QVariantList TeamForgeController::runAllChecks()
{
    return {validateData(), runIndexCheck(), runMatchingSmokeTest(), runPersistenceCheck()};
}
