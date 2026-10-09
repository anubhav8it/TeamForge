#include "SampleData.h"
#include "TeamForgeController.h"

#include "TestSupport.h"

#include "core/CoverageOnlyStrategy.h"
#include "core/JsonStorage.h"
#include "core/MatchingEngine.h"
#include "core/WeightedMatchingStrategy.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <memory>

using namespace teamforge;

// The bridge must add no matching behaviour: rankings and suggestions it exposes are checked
// against MatchingEngine run directly on the same data files.
class TestController : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void loadsSampleData();
    void rankingIsPassedThroughFromCore();
    void teamActionsUseCoreRules();
    void suggestAndFinalize();
    void validationErrorsAreReported();
    void saveAndReloadRoundTrip();
    void referentialIntegrity();
    void missingDataDirectoryIsReported();
    void skillQueries();
    void projectReadModels();
    void evaluateCandidateMatchesCore();
    void saveTeamPersistsImmediately();
    void workspacePermissionsAreEnforced();
    void normalUsersGetFriendlyErrors();
    void participantProfileAndOpportunities();
    void openEndedSkillsWorkEndToEnd();
    void directorySearchSemantics();
    void developerChecksAndReset();
    void sessionPersistsAcrossRestarts();
    void sampleSeedingKeepsSavedWork();
    void developerParticipantDatabase();
    void skillCatalogueAndEditing();
    void rawRecordInspection();
    void developerPreviewNeedsNoOnboarding();
    void summarySentences();
    void interestWorkflow();
    void previewWithChosenIdentity();
    void databaseNumbering();
    void rankingsDifferByProject();
    void workspacesLimitedByAccountRole();

private:
    QString dataDir() const { return dir_->path(); }
    MatchingEngine coreEngine() const;
    // The report's weights with the sample data's engagement, as the controller uses them.
    WeightedMatchingStrategy coreStrategy() const
    {
        WeightedMatchingStrategy strategy;
        strategy.setEngagementSource(&coreInterests_);
        return strategy;
    }
    InterestBook coreInterests_;
    ProjectRequirement coreRequirement(const std::string& id) const;
    // Counts from the data files, so the tests follow the sample data if it is regenerated.
    int fileStudentCount() const;
    int fileRequirementCount() const;

    std::unique_ptr<QTemporaryDir> dir_;
};

void TestController::init()
{
    dir_ = std::make_unique<QTemporaryDir>();
    QVERIFY(dir_->isValid());
    for (const QString& file : sampledata::files())
        QVERIFY(QFile::copy(QStringLiteral(TEAMFORGE_SOURCE_DIR "/data/") + file, dir_->filePath(file)));
    // The engagement factor comes from the interest requests, so core comparisons load them too.
    coreInterests_ = InterestBook();
    for (const InterestRequest& request : storage::loadInterests(dataDir().toStdString() + "/interest_requests.json"))
        coreInterests_.add(request);
}

MatchingEngine TestController::coreEngine() const
{
    MatchingEngine engine;
    for (const Student& s : storage::loadStudents(dataDir().toStdString() + "/students.json"))
        engine.addStudent(s);
    return engine;
}

int TestController::fileStudentCount() const
{
    return static_cast<int>(storage::loadStudents(dataDir().toStdString() + "/students.json").size());
}

int TestController::fileRequirementCount() const
{
    return static_cast<int>(storage::loadRequirements(dataDir().toStdString() + "/requirements.json").size());
}

ProjectRequirement TestController::coreRequirement(const std::string& id) const
{
    for (const ProjectRequirement& r : storage::loadRequirements(dataDir().toStdString() + "/requirements.json")) {
        if (r.id() == id)
            return r;
    }
    throw NotFoundError(id);
}

void TestController::loadsSampleData()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY2(c.lastError().isEmpty(), qPrintable(c.lastError()));
    QCOMPARE(c.students().size(), fileStudentCount());
    QCOMPARE(c.requirements().size(), fileRequirementCount());
    QCOMPARE(c.currentRequirementId(), QStringLiteral("req-001")); // first requirement selected
    QVERIFY(!c.rankedCandidates().isEmpty());
    QCOMPARE(c.currentTeam().value("size").toInt(), 0);
    QVERIFY(!c.isDirty());
    QCOMPARE(c.factorDefinitions().size(), 5);
    QCOMPARE(c.factorDefinitions().first().toMap().value("weight").toDouble(), 0.40);
}

void TestController::rankingIsPassedThroughFromCore()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    const MatchingEngine engine = coreEngine();
    const Team empty("t", coreRequirement("req-001"));

    const auto expected = engine.rank(empty, coreStrategy(), 10);
    const QVariantList ranked = c.rankedCandidates();
    QCOMPARE(ranked.size(), static_cast<qsizetype>(expected.size()));
    for (qsizetype i = 0; i < ranked.size(); ++i) {
        const QVariantMap r = ranked[i].toMap();
        QCOMPARE(r.value("studentId").toString().toStdString(), expected[i].studentId);
        TF_COMPARE_NEAR(r.value("score").toDouble(), expected[i].score);
        // The displayed per-factor contributions add up to the core's score.
        double sum = 0.0;
        for (const QVariant& factor : r.value("factors").toList())
            sum += factor.toMap().value("contribution").toDouble();
        TF_COMPARE_NEAR(sum, expected[i].score);
    }

    c.setStrategy("baseline");
    const auto baseline = engine.rank(empty, CoverageOnlyStrategy(), 10);
    const QVariantList rankedBaseline = c.rankedCandidates();
    QCOMPARE(rankedBaseline.size(), static_cast<qsizetype>(baseline.size()));
    for (qsizetype i = 0; i < rankedBaseline.size(); ++i)
        QCOMPARE(rankedBaseline[i].toMap().value("studentId").toString().toStdString(), baseline[i].studentId);
    QVERIFY(!rankedBaseline.first().toMap().value("factors").toList().first().toMap().contains("contribution"));

    c.setTopK(3);
    QCOMPARE(c.rankedCandidates().size(), 3);
}

void TestController::teamActionsUseCoreRules()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QSignalSpy selection(&c, &TeamForgeController::selectionChanged);

    QVERIFY(c.addToTeam("TF-P001"));
    QVERIFY(selection.count() >= 1);
    QCOMPARE(c.currentTeam().value("memberIds").toStringList(), QStringList{"TF-P001"});
    for (const QVariant& r : c.rankedCandidates())
        QVERIFY(r.toMap().value("studentId").toString() != "TF-P001"); // members are not ranked

    QVERIFY(!c.addToTeam("TF-P001"));
    QVERIFY2(c.lastError().contains("already"), qPrintable(c.lastError()));
    QVERIFY(!c.addToTeam("nobody"));
    QVERIFY(c.lastError().contains("not found"));

    // Coverage and gaps come from core Team.
    Team expected("t", coreRequirement("req-001"));
    expected += coreEngine().students().get("TF-P001");
    QStringList missing;
    for (const std::string& s : expected.missingSkills())
        missing.append(QString::fromStdString(s));
    QCOMPARE(c.currentTeam().value("missingSkills").toStringList(), missing);
    TF_COMPARE_NEAR(c.currentTeam().value("coverageRatio").toDouble(), expected.coverageRatio());

    for (const char *id : {"tf-001", "tf-003", "tf-004"})
        QVERIFY(c.addToTeam(id));
    QVERIFY(c.currentTeam().value("isFull").toBool()); // req-001 allows 4
    QVERIFY(!c.addToTeam("tf-107"));
    QVERIFY(c.lastError().contains("full"));

    QVERIFY(c.removeFromTeam("TF-P001"));
    QCOMPARE(c.currentTeam().value("size").toInt(), 3);
    QVERIFY(!c.removeFromTeam("TF-P001"));
    c.clearTeam();
    QCOMPARE(c.currentTeam().value("size").toInt(), 0);
}

void TestController::suggestAndFinalize()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(!c.currentTeam().value("canFinalize").toBool()); // min size 3
    QVERIFY(!c.finalizeTeam("Too small"));

    QVERIFY(c.suggestTeam());
    const Team expected = coreEngine().suggestTeam("x", coreRequirement("req-001"), coreStrategy());
    QStringList expectedIds;
    for (const Student& m : expected.members())
        expectedIds.append(QString::fromStdString(m.id()));
    QCOMPARE(c.currentTeam().value("memberIds").toStringList(), expectedIds);
    QVERIFY(c.currentTeam().value("canFinalize").toBool());

    QVERIFY(!c.finalizeTeam("   ")); // name required
    QVERIFY(c.finalizeTeam("Hackathon A"));
    QVERIFY(c.isDirty());
    QCOMPARE(c.savedTeams().size(), 1);
    QCOMPARE(c.savedTeams().first().toMap().value("memberIds").toStringList(), expectedIds);
    QCOMPARE(c.currentTeam().value("size").toInt(), 0); // a new team is started

    QVERIFY(c.suggestTeam());
    QVERIFY(!c.finalizeTeam("Hackathon A"));
    QVERIFY(c.lastError().contains("already exists"));

    QVERIFY(c.deleteSavedTeam("Hackathon A"));
    QCOMPARE(c.savedTeams().size(), 0);
}

void TestController::validationErrorsAreReported()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));

    QVariantMap student{{"id", "new-1"}, {"name", "New Student"}, {"role", "DSA"},
                        {"skillsOffered", QVariantMap{{"c++", 9}}}};
    QVERIFY(!c.saveStudent(student));
    QVERIFY2(c.lastError().contains("between 1 and 5"), qPrintable(c.lastError()));
    student["skillsOffered"] = QVariantMap{{"c++", 2.5}};
    QVERIFY(!c.saveStudent(student));
    QVERIFY(c.lastError().contains("whole number"));
    student["name"] = QStringLiteral("  ");
    student["skillsOffered"] = QVariantMap{{"c++", 3}};
    QVERIFY(!c.saveStudent(student));
    QCOMPARE(c.students().size(), fileStudentCount());
    QVERIFY(!c.isDirty());

    student["name"] = QStringLiteral("New Student");
    student["skillsOffered"] = QVariantList{QVariantMap{{"skill", "C++"}, {"level", 3}}}; // list form
    QVERIFY2(c.saveStudent(student), qPrintable(c.lastError()));
    QVERIFY(c.lastError().isEmpty());
    QCOMPARE(c.students().size(), fileStudentCount() + 1);
    QVERIFY(c.isDirty());

    QVariantMap requirement{{"id", "req-x"}, {"name", "X"}, {"requiredSkills", QVariantMap{{"c++", 3}}},
                            {"minTeamSize", 0}, {"maxTeamSize", 3}};
    QVERIFY(!c.saveRequirement(requirement));
    QVERIFY(c.lastError().contains("at least 1"));

    c.setCurrentRequirementId("missing");
    QVERIFY(c.lastError().contains("not found"));
    QCOMPARE(c.currentRequirementId(), QStringLiteral("req-001"));
    c.setStrategy("bogus");
    QVERIFY(!c.lastError().isEmpty());
    c.setTopK(0);
    QVERIFY(!c.lastError().isEmpty());
    c.clearError();
    QVERIFY(c.lastError().isEmpty());
}

void TestController::saveAndReloadRoundTrip()
{
    {
        TeamForgeController c(dataDir());
        QVERIFY(c.enterWorkspace("developer"));
        QVERIFY(c.saveStudent({{"id", "new-1"}, {"name", "New Student"}, {"role", "QA"},
                               {"skillsOffered", QVariantMap{{"testing", 4}}}}));
        QVERIFY(c.suggestTeam());
        QVERIFY(c.finalizeTeam("Saved Team"));
        QVERIFY2(c.save(), qPrintable(c.lastError()));
        QVERIFY(!c.isDirty());
    }
    TeamForgeController reloaded(dataDir());
    QVERIFY2(reloaded.lastError().isEmpty(), qPrintable(reloaded.lastError()));
    QCOMPARE(reloaded.students().size(), fileStudentCount());
    bool foundNew = false;
    for (const QVariant& s : reloaded.students())
        foundNew = foundNew || s.toMap().value("id").toString() == "new-1";
    QVERIFY(foundNew); // the student added before save() survived the round trip
    QCOMPARE(reloaded.savedTeams().size(), 1);
    QCOMPARE(reloaded.savedTeams().first().toMap().value("id").toString(), QStringLiteral("Saved Team"));
}

void TestController::referentialIntegrity()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(c.suggestTeam());
    const QStringList members = c.currentTeam().value("memberIds").toStringList();
    QVERIFY(c.finalizeTeam("Locked"));

    QVERIFY(!c.removeStudent(members.first()));
    QVERIFY(c.lastError().contains("saved team"));
    QVERIFY(!c.removeRequirement("req-001"));

    // Shrinking the maximum below the saved team's size is rejected and rolled back.
    QVariantMap smaller{{"id", "req-001"}, {"name", "CampusConnect"},
                        {"requiredSkills", QVariantMap{{"c++", 3}}}, {"minTeamSize", 1}, {"maxTeamSize", 1}};
    QVERIFY(!c.saveRequirement(smaller));
    QCOMPARE(c.requirements().first().toMap().value("maxTeamSize").toInt(), 4);

    // Any participant outside the saved team can still be removed.
    QString outsider;
    for (const QVariant& s : c.students()) {
        const QString id = s.toMap().value("id").toString();
        if (!members.contains(id)) { outsider = id; break; }
    }
    const int before = static_cast<int>(c.students().size());
    QVERIFY(c.removeStudent(outsider));
    QCOMPARE(c.students().size(), before - 1);
}

void TestController::missingDataDirectoryIsReported()
{
    TeamForgeController c(dataDir() + "/does-not-exist");
    QVERIFY(c.lastError().contains("does not exist"));
    QCOMPARE(c.lastErrorDetail().value("type").toString(), QStringLiteral("PersistenceError"));
    QVERIFY(c.students().isEmpty());
    QVERIFY(c.currentTeam().isEmpty());
    c.enterWorkspace("developer");
    QVERIFY(!c.addToTeam("tf-001"));
    QVERIFY(c.lastError().contains("Select a project"));
}

void TestController::skillQueries()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(c.skills().contains("c++"));
    // Prefix search is the core's AVL tree, any spelling of the prefix.
    QStringList expected;
    for (const std::string& skill : coreEngine().skillsWithPrefix("da"))
        expected.append(QString::fromStdString(skill));
    QVERIFY(expected.contains("data analysis"));
    QCOMPARE(c.skillsWithPrefix("Da"), expected);
    bool foundQt = false;
    for (const QVariant& related : c.relatedSkills("qml", 1))
        foundQt = foundQt || related.toMap().value("skill").toString() == "qt";
    QVERIFY(foundQt);
}

void TestController::projectReadModels()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    const QVariantMap current = c.currentRequirement();
    QCOMPARE(current.value("id").toString(), QStringLiteral("req-001"));
    QCOMPARE(current.value("type").toString(), QStringLiteral("Hackathon"));

    // Supply counts agree with the core index: c++ is offered by 6 sample students, 5 at >= 3.
    const MatchingEngine engine = coreEngine();
    for (const QVariant& entry : current.value("requiredSkills").toList()) {
        const QVariantMap skill = entry.toMap();
        const std::string name = skill.value("skill").toString().toStdString();
        const int minLevel = skill.value("minLevel").toInt();
        int qualified = 0;
        for (const std::string& id : engine.skillIndex().studentsWith(name))
            qualified += engine.students().get(id).offers(name, minLevel) ? 1 : 0;
        QCOMPARE(skill.value("offeredCount").toInt(), static_cast<int>(engine.skillIndex().studentsWith(name).size()));
        QCOMPARE(skill.value("qualifiedCount").toInt(), qualified);
    }
    const Team empty("t", coreRequirement("req-001"));
    QCOMPARE(current.value("candidateCount").toInt(), static_cast<int>(engine.retrieveCandidates(empty).size()));

    // New project: free id, type kept, becomes most recent once selected.
    const QString id = c.newRequirementId();
    QVERIFY(id != "req-001" && id != "req-002");
    QVERIFY(c.saveRequirement({{"id", id}, {"name", "Robotics Sprint"}, {"type", "Competition"},
                               {"requiredSkills", QVariantMap{{"c++", 2}, {"electronics", 3}}},
                               {"minTeamSize", 2}, {"maxTeamSize", 3}}));
    c.setCurrentRequirementId(id);
    QCOMPARE(c.currentRequirement().value("type").toString(), QStringLiteral("Competition"));
    QCOMPARE(c.recentProjects().first().toMap().value("id").toString(), id);
    QCOMPARE(c.recentProjects().size(), fileRequirementCount() + 1);
}

void TestController::evaluateCandidateMatchesCore()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(c.addToTeam("tf-003"));
    const MatchingEngine engine = coreEngine();
    Team team("t", coreRequirement("req-001"));
    team += engine.students().get("tf-003");

    const QVariantMap outsider = c.evaluateCandidate("tf-004");
    const MatchResult expected = coreStrategy().evaluate(engine.students().get("tf-004"), team);
    TF_COMPARE_NEAR(outsider.value("score").toDouble(), expected.score);
    QVERIFY(!outsider.value("isMember").toBool());
    QCOMPARE(outsider.value("skillBreakdown").toList().size(), 6); // one row per required skill

    // A member is scored as if added last, i.e. against the team without them.
    const QVariantMap member = c.evaluateCandidate("tf-003");
    const MatchResult alone = coreStrategy().evaluate(engine.students().get("tf-003"),
                                                                   Team("t", coreRequirement("req-001")));
    TF_COMPARE_NEAR(member.value("score").toDouble(), alone.score);
    QVERIFY(member.value("isMember").toBool());

    QVERIFY(c.evaluateCandidate("nobody").isEmpty());
    QVERIFY(c.lastError().contains("not found"));

    // Queries never clear an error left by a failed action.
    QVERIFY(!c.addToTeam("tf-003"));
    const QString error = c.lastError();
    QVERIFY(!c.evaluateCandidate("tf-004").isEmpty());
    c.skillsWithPrefix("q");
    c.relatedSkills("qt", 1);
    QCOMPARE(c.lastError(), error);
}

void TestController::saveTeamPersistsImmediately()
{
    {
        TeamForgeController c(dataDir());
        QVERIFY(c.enterWorkspace("developer"));
        QVERIFY(!c.saveTeam("Too small")); // validation still applies
        QVERIFY(c.suggestTeam());
        QVERIFY2(c.saveTeam("Shipped"), qPrintable(c.lastError()));
        QVERIFY(!c.isDirty());
        QCOMPARE(c.currentRequirement().value("savedTeamCount").toInt(), 1);
    }
    TeamForgeController reloaded(dataDir());
    QCOMPARE(reloaded.savedTeams().size(), 1);
}

void TestController::workspacePermissionsAreEnforced()
{
    TeamForgeController c(dataDir());
    QCOMPARE(c.workspace(), QString()); // the entry screen allows no actions
    QVERIFY(!c.addToTeam("TF-P001"));
    QVERIFY(c.lastError().contains("not available"));

    // Participant: own profile and fits only.
    QVERIFY(c.enterWorkspace("participant"));
    QVERIFY(!c.addToTeam("TF-P001"));
    QVERIFY(!c.suggestTeam());
    QVERIFY(!c.saveRequirement({{"id", "req-x"}, {"name", "X"}, {"requiredSkills", QVariantMap{{"c++", 3}}},
                                {"minTeamSize", 1}, {"maxTeamSize", 3}}));
    QVERIFY(!c.removeRequirement("req-001"));
    QVERIFY(!c.saveStudent({{"id", "tf-001"}, {"name", "Changed"}, {"role", "R"}, {"skillsOffered", QVariantMap{{"c++", 1}}}}));
    QVERIFY(!c.runIndexCheck().value("ok").toBool());
    QVERIFY(!c.resetSampleData().value("ok").toBool());
    QVERIFY(!c.reload());
    c.setStrategy("baseline");
    QCOMPARE(c.strategy(), QStringLiteral("weighted"));
    QVERIFY(c.evaluateCandidate("tf-004").isEmpty());
    QCOMPARE(c.requirements().size(), fileRequirementCount()); // nothing changed

    // Host: projects and teams, but no developer tools and no profile edits.
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(c.addToTeam("TF-P001"));
    QVERIFY(!c.saveStudent({{"id", "tf-001"}, {"name", "Changed"}, {"role", "R"}, {"skillsOffered", QVariantMap{{"c++", 1}}}}));
    QVERIFY(!c.rebuildSkillTree().value("ok").toBool());
    QVERIFY(!c.previewWorkspace("participant"));
    c.setStrategy("baseline");
    QCOMPARE(c.strategy(), QStringLiteral("weighted"));
    QVERIFY(!c.saveMyProfile({{"name", "Host"}, {"skillsOffered", QVariantMap{{"c++", 2}}}}));

    // Developer: everything, and previews with exactly the previewed permissions.
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(c.runIndexCheck().value("ok").toBool());
    QVERIFY(c.previewWorkspace("host"));
    QCOMPARE(c.workspace(), QStringLiteral("host"));
    QVERIFY(c.isPreviewing());
    QVERIFY(!c.rebuildSkillIndex().value("ok").toBool());
    c.endPreview();
    QCOMPARE(c.workspace(), QStringLiteral("developer"));
    QVERIFY(!c.isPreviewing());
    QVERIFY(!c.enterWorkspace("admin"));
}

void TestController::normalUsersGetFriendlyErrors()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(!c.saveRequirement({{"id", "req-x"}, {"name", "X"}, {"requiredSkills", QVariantMap{{"c++", 3}}},
                                {"minTeamSize", 4}, {"maxTeamSize", 2}}));
    QCOMPARE(c.lastError(), QStringLiteral("Couldn't save this project. Check the required details."));
    // No ids, paths or exception names in what the host sees ...
    QVERIFY(!c.lastError().contains("req-x"));
    QVERIFY(!c.lastError().contains(dataDir()));
    // ... but the developer details keep all of it.
    const QVariantMap detail = c.lastErrorDetail();
    QCOMPARE(detail.value("type").toString(), QStringLiteral("ValidationError"));
    QCOMPARE(detail.value("operation").toString(), QStringLiteral("saveRequirement"));
    QCOMPARE(detail.value("record").toString(), QStringLiteral("req-x"));
    QCOMPARE(detail.value("storage").toString(), dataDir());
    QVERIFY(detail.value("message").toString().contains("maximum team size"));
    QVERIFY(!detail.value("time").toString().isEmpty());
    bool logged = false;
    for (const QVariant& entry : c.logEntries())
        logged = logged || entry.toMap().value("message").toString().contains("ValidationError in saveRequirement");
    QVERIFY(logged);

    QVERIFY(c.suggestTeam());
    QVERIFY(c.saveTeam("Alpha"));
    QVERIFY(c.suggestTeam());
    QVERIFY(!c.saveTeam("Alpha"));
    QCOMPARE(c.lastError(), QStringLiteral("A team with this name already exists."));

    QVERIFY(c.enterWorkspace("participant"));
    QVERIFY(!c.saveMyProfile({{"name", "No Skills"}}));
    QCOMPARE(c.lastError(), QStringLiteral("Couldn't update your profile. Check the required fields."));
    QVERIFY(c.lastErrorDetail().value("message").toString().contains("at least one skill"));

    // The developer workspace shows the backend message itself.
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(!c.addToTeam("nobody"));
    QVERIFY(c.lastError().contains("not found"));
}

void TestController::participantProfileAndOpportunities()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("participant"));
    QVERIFY(c.myProfile().isEmpty());
    QVERIFY(c.opportunities().isEmpty());

    const QVariantMap draft{{"name", "Ira Menon"}, {"program", "B.Tech CSE · 2nd year"},
                            {"skillsOffered", QVariantList{QVariantMap{{"skill", "Python"}, {"level", 4}},
                                                           QVariantMap{{"skill", "NLP"}, {"level", 3}},
                                                           QVariantMap{{"skill", "statistics"}, {"level", 3}}}},
                            {"skillsWanted", QStringList{"research writing"}}};
    QVERIFY(c.composeSummary(draft.value("skillsOffered").toList()).startsWith("Experienced with Python"));
    QVariantMap withSummary = draft;
    withSummary["summary"] = c.composeSummary(draft.value("skillsOffered").toList());
    QVERIFY2(c.saveMyProfile(withSummary), qPrintable(c.lastErrorDetail().value("message").toString()));
    const QString me = c.myProfileId();
    QVERIFY(me.startsWith("tf-p"));
    QVERIFY(!c.isDirty()); // a participant's edits are saved immediately
    QCOMPARE(c.myProfile().value("completion").toDouble(), 1.0);
    QCOMPARE(c.myProfile().value("role").toString(), QStringLiteral("python")); // strongest skill

    // One opportunity per project, best first, scored by the same strategy as the host's ranking.
    const QVariantList opportunities = c.opportunities();
    QCOMPARE(opportunities.size(), fileRequirementCount());
    for (qsizetype i = 1; i < opportunities.size(); ++i)
        QVERIFY(opportunities[i - 1].toMap().value("score").toDouble() >= opportunities[i].toMap().value("score").toDouble());
    const QVariantMap best = opportunities.first().toMap();
    QCOMPARE(best.value("id").toString(), QStringLiteral("req-005")); // the NLP research project
    const MatchingEngine engine = coreEngine(); // the new profile is already saved to disk
    const MatchResult expected = coreStrategy().evaluate(engine.students().get(me.toStdString()),
                                                                     Team("t", coreRequirement("req-005")));
    TF_COMPARE_NEAR(best.value("score").toDouble(), expected.score);
    TF_COMPARE_NEAR(c.evaluateFit(me, "req-005").value("score").toDouble(), expected.score);
    QVERIFY(best.value("interestStatus").toString().isEmpty()); // no interest expressed yet

    // Editing keeps the id and the role; nobody else's fit is visible to a participant.
    QVariantMap edited = withSummary;
    edited["name"] = "Ira M.";
    QVERIFY(c.saveMyProfile(edited));
    QCOMPARE(c.myProfileId(), me);
    QVERIFY(c.evaluateFit("tf-001", "req-001").isEmpty());
    QCOMPARE(c.lastError(), QStringLiteral("That isn't available in your participant workspace."));

    TeamForgeController restarted(dataDir());
    QCOMPARE(restarted.myProfileId(), me); // kept in session.json
    QCOMPARE(restarted.myProfile().value("name").toString(), QStringLiteral("Ira M."));

    QVERIFY(restarted.enterWorkspace("participant"));
    QVERIFY(restarted.claimProfile("TF-P001"));
    QCOMPARE(restarted.myProfile().value("name").toString(), QStringLiteral("Anubhav Bisht"));
    QVERIFY(!restarted.claimProfile("nobody"));
}

void TestController::openEndedSkillsWorkEndToEnd()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("participant"));
    QVERIFY(c.skillsWithPrefix("kubernetes o").isEmpty());
    QVERIFY(c.saveMyProfile({{"name", "New Skills"},
                             {"skillsOffered", QVariantMap{{"Kubernetes   Operators", 4}, {"CUDA", 3}}}}));
    // Normalised, then searchable through the AVL tree and the directory search.
    QCOMPARE(c.skillsWithPrefix("KUBERNETES O"), QStringList{"kubernetes operators"});
    QVERIFY(c.skills().contains("kubernetes operators"));
    bool found = false;
    for (const QVariant& s : c.searchStudents("operators", {}, "name"))
        found = found || s.toMap().value("id").toString() == c.myProfileId();
    QVERIFY(found);

    // A host can require it, and matching finds the one person who offers it.
    QVERIFY(c.enterWorkspace("host"));
    const QString id = c.newRequirementId();
    QVERIFY(c.saveRequirement({{"id", id}, {"name", "Cluster Lab"}, {"type", "Research Project"},
                               {"requiredSkills", QVariantMap{{"kubernetes operators", 3}}},
                               {"minTeamSize", 1}, {"maxTeamSize", 2}}));
    c.setCurrentRequirementId(id);
    QCOMPARE(c.rankedCandidates().size(), 1);
    QCOMPARE(c.rankedCandidates().first().toMap().value("studentName").toString(), QStringLiteral("New Skills"));
    QCOMPARE(c.currentRequirement().value("candidateCount").toInt(), 1);
}

void TestController::directorySearchSemantics()
{
    TeamForgeController c(dataDir());
    const QVariantList all = c.searchStudents({}, {}, "name");
    QCOMPARE(all.size(), fileStudentCount());

    // "ml" finds machine learning (initials) but never matches only through "html/css".
    const QVariantList ml = c.searchStudents("ML", {}, "name");
    QVERIFY(!ml.isEmpty());
    bool anyMachineLearning = false;
    for (const QVariant& entry : ml) {
        const QVariantMap s = entry.toMap();
        QStringList skills;
        for (const QVariant& o : s.value("skillsOffered").toList())
            skills.append(o.toMap().value("skill").toString());
        anyMachineLearning = anyMachineLearning || skills.contains("machine learning");
        const bool viaName = s.value("name").toString().toLower().contains("ml");
        const bool viaSkill = std::any_of(skills.begin(), skills.end(), [](const QString& skill) {
            QString initials;
            for (const QString& word : skill.split(QRegularExpression("[ /-]"), Qt::SkipEmptyParts))
                initials += word.at(0);
            return skill.startsWith("ml") || skill.contains(" ml") || skill.contains("/ml") || initials == "ml";
        });
        QVERIFY2(viaName || viaSkill, qPrintable(s.value("name").toString()));
    }
    QVERIFY(anyMachineLearning);

    QCOMPARE(c.searchStudents("bisht", {}, "name").first().toMap().value("name").toString(),
             QStringLiteral("Anubhav Bisht"));
    for (const QVariant& entry : c.searchStudents({}, "QML", "name")) {
        bool offers = false;
        for (const QVariant& o : entry.toMap().value("skillsOffered").toList())
            offers = offers || o.toMap().value("skill").toString() == "qml";
        QVERIFY(offers);
    }
    const QVariantList byLevel = c.searchStudents({}, {}, "level");
    for (qsizetype i = 1; i < byLevel.size(); ++i)
        QVERIFY(byLevel[i - 1].toMap().value("averageLevel").toDouble() >= byLevel[i].toMap().value("averageLevel").toDouble());
}

void TestController::developerChecksAndReset()
{
    TeamForgeController c(dataDir());
    c.setSampleDirectory(QStringLiteral(TEAMFORGE_SOURCE_DIR "/data"));
    QVERIFY(c.enterWorkspace("developer"));
    const QVariantMap status = c.systemStatus();
    QCOMPARE(status.value("participants").toInt(), fileStudentCount());
    QVERIFY(status.value("skills").toInt() >= 500);
    QVERIFY(status.value("healthy").toBool());

    for (const QVariant& check : c.runAllChecks())
        QVERIFY2(check.toMap().value("ok").toBool(), qPrintable(check.toMap().value("message").toString()));
    for (const QVariantMap& rebuilt : {c.rebuildSkillIndex(), c.rebuildSkillTree(), c.rebuildSkillGraph()})
        QVERIFY(rebuilt.value("ok").toBool());
    QVERIFY(c.runIndexCheck().value("ok").toBool());

    // Reset restores the samples (and backs up what was there).
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(c.suggestTeam());
    QVERIFY(c.saveTeam("To be reset"));
    QVERIFY(c.enterWorkspace("developer"));
    const QVariantMap reset = c.resetSampleData();
    QVERIFY2(reset.value("ok").toBool(), qPrintable(reset.value("message").toString()));
    QVERIFY(c.savedTeams().isEmpty());
    QVERIFY(reset.value("details").toStringList().join(' ').contains("backup-"));
}

void TestController::sessionPersistsAcrossRestarts()
{
    {
        TeamForgeController c(dataDir());
        QVERIFY(c.enterWorkspace("host"));
        c.setCurrentRequirementId("req-003");
        c.setCurrentRequirementId("req-006");
    }
    TeamForgeController restarted(dataDir());
    QCOMPARE(restarted.currentRequirementId(), QStringLiteral("req-006"));
    QCOMPARE(restarted.recentProjects().at(0).toMap().value("id").toString(), QStringLiteral("req-006"));
    QCOMPARE(restarted.recentProjects().at(1).toMap().value("id").toString(), QStringLiteral("req-003"));
    QCOMPARE(restarted.lastWorkspace(), QStringLiteral("host"));
    QCOMPARE(restarted.workspace(), QString()); // the app always starts on the entry screen
}

void TestController::sampleSeedingKeepsSavedWork()
{
    QTemporaryDir runtime;
    const QString samples = QStringLiteral(TEAMFORGE_SOURCE_DIR "/data");
    const sampledata::Result first = sampledata::ensureSeeded(runtime.path(), samples);
    QVERIFY(first.ok && first.replaced);
    QVERIFY(QFile::exists(runtime.filePath(".sample-data")));

    // Saved work survives while the samples are unchanged.
    QFile teams(runtime.filePath("teams.json"));
    QVERIFY(teams.open(QIODevice::WriteOnly | QIODevice::Truncate));
    teams.write(R"({"version": 1, "teams": [{"id": "Mine", "requirementId": "req-001", "memberIds": ["tf-001"]}]})");
    teams.close();
    const sampledata::Result second = sampledata::ensureSeeded(runtime.path(), samples);
    QVERIFY(second.ok && !second.replaced);
    QVERIFY(teams.open(QIODevice::ReadOnly));
    QVERIFY(teams.readAll().contains("Mine"));
    teams.close();

    // Missing files are restored; a different sample set replaces everything with a backup.
    QVERIFY(QFile::remove(runtime.filePath("requirements.json")));
    QVERIFY(sampledata::ensureSeeded(runtime.path(), samples).ok);
    QVERIFY(QFile::exists(runtime.filePath("requirements.json")));
    QFile stamp(runtime.filePath(".sample-data"));
    QVERIFY(stamp.open(QIODevice::WriteOnly | QIODevice::Truncate));
    stamp.write("older-samples\n");
    stamp.close();
    const sampledata::Result upgraded = sampledata::ensureSeeded(runtime.path(), samples);
    QVERIFY(upgraded.ok && upgraded.replaced);
    QVERIFY(!upgraded.backupDir.isEmpty());
    QVERIFY(QFile::exists(upgraded.backupDir + "/teams.json"));
}

void TestController::developerParticipantDatabase()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(c.queryParticipants({}).isEmpty()); // the full database is a developer view
    QVERIFY(c.enterWorkspace("developer"));

    const QVariantMap all = c.queryParticipants({});
    QCOMPARE(all.value("total").toInt(), fileStudentCount());
    QCOMPARE(all.value("count").toInt(), fileStudentCount()); // every record, not a page
    QCOMPARE(all.value("rows").toList().size(), fileStudentCount());

    const QVariantMap advanced = c.queryParticipants({{"experience", "Advanced"}});
    QVERIFY(advanced.value("count").toInt() > 0 && advanced.value("count").toInt() < fileStudentCount());
    for (const QVariant& row : advanced.value("rows").toList())
        QVERIFY(row.toMap().value("averageLevel").toDouble() >= 4.0);

    const QVariantMap pythonAdvanced = c.queryParticipants({{"experience", "Advanced"}, {"skill", "Python"}});
    QVERIFY(pythonAdvanced.value("count").toInt() > 0);
    QVERIFY(pythonAdvanced.value("count").toInt() < advanced.value("count").toInt());
    for (const QVariant& row : pythonAdvanced.value("rows").toList()) {
        const QVariantList skills = row.toMap().value("skillsOffered").toList();
        QVERIFY(std::any_of(skills.begin(), skills.end(), [](const QVariant& s) { return s.toMap().value("skill") == "python"; }));
        QVERIFY(!row.toMap().contains("availability")); // availability is gone from the product
    }

    QCOMPARE(c.queryParticipants({{"query", "TF-P001"}}).value("count").toInt(), 1); // by id
    const QVariantList bySkills = c.queryParticipants({{"sort", "skills"}}).value("rows").toList();
    for (qsizetype i = 1; i < bySkills.size(); ++i)
        QVERIFY(bySkills[i - 1].toMap().value("skillsOffered").toList().size() >= bySkills[i].toMap().value("skillsOffered").toList().size());
}

void TestController::skillCatalogueAndEditing()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(!c.saveSkill("quantum annealing", "Scientific computing")); // developer only
    QVERIFY(c.enterWorkspace("developer"));

    const QVariantMap all = c.querySkills({});
    QVERIFY(all.value("total").toInt() >= 500);
    QCOMPARE(all.value("total").toInt(), c.skillCount());
    QVERIFY(c.skillCategories().size() >= 20);
    const QVariantMap ai = c.querySkills({{"category", "AI / ML"}});
    QVERIFY(ai.value("count").toInt() > 10);
    const QVariantList popular = c.querySkills({{"sort", "participants"}}).value("rows").toList();
    QVERIFY(popular.first().toMap().value("participants").toInt() >= popular.last().toMap().value("participants").toInt());

    const QVariantMap python = c.skillDetail("Python");
    QVERIFY(python.value("inIndex").toBool() && python.value("inTree").toBool() && python.value("inGraph").toBool());
    QVERIFY(python.value("consistent").toBool());
    QVERIFY(!python.value("related").toList().isEmpty());
    QVERIFY(python.value("projectCount").toInt() >= 3);

    // Create: catalogued before anyone offers it.
    QVERIFY(c.saveSkill("Quantum Annealing", "Scientific computing"));
    const QVariantMap created = c.skillDetail("quantum annealing");
    QCOMPARE(created.value("category").toString(), QStringLiteral("Scientific computing"));
    QCOMPARE(created.value("participants").toInt(), 0);
    QVERIFY(c.skillsWithPrefix("Quantum A").contains("quantum annealing")); // autocompletes before anyone offers it
    QCOMPARE(c.skillCount(), all.value("total").toInt() + 1);

    // Rename moves every holder and project; the structures stay consistent.
    const int kafkaHolders = c.skillDetail("kafka").value("participants").toInt();
    QVERIFY(kafkaHolders > 0);
    QVERIFY2(c.renameSkill("kafka", "Apache Kafka"), qPrintable(c.lastError()));
    QVERIFY(c.skillDetail("kafka").isEmpty());
    QCOMPARE(c.skillDetail("apache kafka").value("participants").toInt(), kafkaHolders);
    QVERIFY(c.runIndexCheck().value("ok").toBool());

    // Delete is refused while it is someone's only skill, then works.
    QVERIFY(c.saveStudent({{"id", "tf-solo"}, {"name", "Solo Skill"}, {"role", "Test"},
                           {"skillsOffered", QVariantMap{{"apache kafka", 3}}}}));
    QVERIFY(!c.deleteSkill("apache kafka"));
    QVERIFY(c.lastError().contains("only skill"));
    QVERIFY(c.removeStudent("tf-solo"));
    QVERIFY2(c.deleteSkill("apache kafka"), qPrintable(c.lastError()));
    QVERIFY(c.skillDetail("apache kafka").isEmpty());
    QVERIFY(c.runIndexCheck().value("ok").toBool());

    // The catalogue is saved with the data.
    QVERIFY(c.save());
    TeamForgeController reloaded(dataDir());
    QVERIFY(reloaded.enterWorkspace("developer"));
    QCOMPARE(reloaded.skillDetail("quantum annealing").value("category").toString(), QStringLiteral("Scientific computing"));
}

void TestController::rawRecordInspection()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(c.inspectRecord("participant", "TF-P001").isEmpty());
    QVERIFY(c.enterWorkspace("developer"));

    QVariantMap record = c.inspectRecord("participant", "TF-P001");
    QCOMPARE(record.value("storedState").toString(), QStringLiteral("same"));
    QVERIFY(record.value("loaded").toString().contains("\"skillsOffered\""));
    QVERIFY(!record.value("indexed").toList().isEmpty());
    for (const QVariant& row : record.value("indexed").toList())
        QVERIFY(row.toMap().value("ok").toBool()); // every skill is in all three structures
    QCOMPARE(record.value("matching").toList().size(), fileRequirementCount());

    // Edit the raw record: memory changes, disk doesn't until saved.
    QJsonObject json = QJsonDocument::fromJson(record.value("loaded").toString().toUtf8()).object();
    json["program"] = "B.Tech CSE · 3rd year";
    QVERIFY2(c.applyRecordJson("participant", QString::fromUtf8(QJsonDocument(json).toJson())), qPrintable(c.lastError()));
    QCOMPARE(c.inspectRecord("participant", "TF-P001").value("storedState").toString(), QStringLiteral("different"));
    QVERIFY(c.save());
    QCOMPARE(c.inspectRecord("participant", "TF-P001").value("storedState").toString(), QStringLiteral("same"));

    // Raw edits go through the same validation as the data files.
    QVERIFY(!c.applyRecordJson("participant", "{not json"));
    json["skillsOffered"] = QJsonObject{{"python", 9}};
    QVERIFY(!c.applyRecordJson("participant", QString::fromUtf8(QJsonDocument(json).toJson())));

    const QVariantMap project = c.inspectRecord("project", "req-001");
    QCOMPARE(project.value("storedState").toString(), QStringLiteral("same"));
    QVERIFY(!project.value("matching").toList().isEmpty());
    QVERIFY(c.inspectRecord("skill", "python").value("storedState").toString() == QStringLiteral("same"));
    QVERIFY(c.inspectRecord("participant", "nobody").contains("error"));
}

void TestController::developerPreviewNeedsNoOnboarding()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(c.myProfileId().isEmpty());
    QVERIFY(c.previewWorkspace("participant"));
    QCOMPARE(c.myProfileId(), QStringLiteral("TF-P001")); // the demo participant
    QCOMPARE(c.myProfile().value("completion").toDouble(), 1.0);
    QVERIFY(!c.opportunities().isEmpty());
    QVERIFY(c.previewWorkspace("host")); // switch preview without going back
    QCOMPARE(c.workspace(), QStringLiteral("host"));
    QVERIFY(c.isPreviewing());
    c.endPreview();
    QCOMPARE(c.workspace(), QStringLiteral("developer"));
    QVERIFY(c.myProfileId().isEmpty()); // the session's own profile was never changed

    TeamForgeController restarted(dataDir());
    QVERIFY(restarted.myProfileId().isEmpty());
}

void TestController::summarySentences()
{
    TeamForgeController c(dataDir());
    // Summary sentences spell acronyms and versioned names in capitals.
    QCOMPARE(c.composeSummary({QVariantMap{{"skill", "ros2 navigation"}, {"level", 5}},
                               QVariantMap{{"skill", "UI design"}, {"level", 4}},
                               QVariantMap{{"skill", "machine learning"}, {"level", 3}}}),
             QStringLiteral("Experienced with ROS2 Navigation, UI Design and Machine Learning."));
    // Availability and scheduling are gone from the product and from the score.
    const QVariantMap first = c.students().first().toMap();
    QVERIFY(!first.contains("availability"));
    QVERIFY(!c.requirements().first().toMap().contains("timeSlots"));
    QStringList keys;
    for (const QVariant& factor : c.factorDefinitions())
        keys.append(factor.toMap().value("key").toString());
    QCOMPARE(keys, (QStringList{"coverage", "complementarity", "experience", "engagement", "diversity"}));
    TF_COMPARE_NEAR(c.factorDefinitions().at(3).toMap().value("weight").toDouble(), 0.10);
    QCOMPARE(c.profileCompletion({}).value("checklist").toList().size(), 4);
}

void TestController::interestWorkflow()
{
    TeamForgeController c(dataDir());
    const auto opportunity = [&](const QString& id) {
        for (const QVariant& entry : c.opportunities()) {
            if (entry.toMap().value("id").toString() == id)
                return entry.toMap();
        }
        return QVariantMap{};
    };

    // The participant (the demo participant) expresses interest; doing it twice is refused.
    QVERIFY(c.enterWorkspace("participant"));
    QVERIFY(c.useDemoParticipant());
    QCOMPARE(c.myProfileId(), QStringLiteral("TF-P001"));
    const int seeded = static_cast<int>(c.myInterests().size());
    QVERIFY(seeded >= 1);
    const QVariantMap before = opportunity("req-003");
    QVERIFY(before.value("interestStatus").toString().isEmpty());
    QVERIFY2(c.expressInterest("req-003"), qPrintable(c.lastErrorDetail().value("message").toString()));
    QVERIFY(!c.isDirty()); // saved straight away
    QCOMPARE(c.myInterests().size(), seeded + 1);
    const QVariantMap after = opportunity("req-003");
    QCOMPARE(after.value("interestStatus").toString(), QStringLiteral("interested"));
    // Engagement: neutral 0.5 -> 1.0, worth 10%.
    TF_COMPARE_NEAR(after.value("score").toDouble() - before.value("score").toDouble(), 0.05);
    QVERIFY(!c.expressInterest("req-003"));
    QCOMPARE(c.lastError(), QStringLiteral("You've already expressed interest in this project."));
    const QString requestId = after.value("interestId").toString();
    QVERIFY(!requestId.isEmpty());
    QVERIFY(!c.acceptInterest(requestId));            // only hosts review
    QVERIFY(c.projectInterests("req-003").isEmpty()); // nor see other people's requests

    // The host reviews and accepts; the person is offered for the team but not added to it.
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(!c.expressInterest("req-001")); // hosts don't express interest
    const auto statusOf = [&](const QString& id) {
        for (const QVariant& entry : c.projectInterests("req-003")) {
            if (entry.toMap().value("id").toString() == id)
                return entry.toMap().value("status").toString();
        }
        return QString();
    };
    QCOMPARE(statusOf(requestId), QStringLiteral("interested"));
    QVERIFY(c.reviewInterest(requestId));
    QCOMPARE(statusOf(requestId), QStringLiteral("under_review"));
    QVERIFY(c.reviewInterest(requestId)); // opening it again changes nothing
    QCOMPARE(statusOf(requestId), QStringLiteral("under_review"));
    c.setCurrentRequirementId("req-003");
    const qsizetype acceptedBefore = c.acceptedCandidates().size();
    QVERIFY(c.acceptInterest(requestId));
    QCOMPARE(c.acceptedCandidates().size(), acceptedBefore + 1);
    bool offered = false;
    for (const QVariant& entry : c.acceptedCandidates())
        offered = offered || entry.toMap().value("studentId").toString() == "TF-P001";
    QVERIFY(offered);
    QCOMPARE(c.currentTeam().value("size").toInt(), 0);
    QVERIFY(c.addToTeam("TF-P001"));
    QVERIFY(c.declineInterest(requestId)); // the host may change their mind
    QVERIFY(!c.reviewInterest("int-9999"));
    QVERIFY(c.requirements().at(2).toMap().value("interest").toMap().value("declined").toInt() >= 1);

    // Persisted: after a restart the participant sees the decision.
    TeamForgeController restarted(dataDir());
    QVERIFY(restarted.enterWorkspace("participant"));
    QString status;
    for (const QVariant& entry : restarted.myInterests()) {
        if (entry.toMap().value("id").toString() == "req-003")
            status = entry.toMap().value("interestStatus").toString();
    }
    QCOMPARE(status, QStringLiteral("declined"));
}

void TestController::previewWithChosenIdentity()
{
    TeamForgeController c(dataDir());
    const QVariantList demos = c.demoIdentities();
    QCOMPARE(demos.size(), 2);
    QCOMPARE(demos.at(0).toMap().value("id").toString(), QStringLiteral("TF-P001"));
    QCOMPARE(demos.at(0).toMap().value("name").toString(), QStringLiteral("Anubhav Bisht"));
    QCOMPARE(demos.at(1).toMap().value("id").toString(), QStringLiteral("TF-H001"));
    QCOMPARE(c.hostIdentity().value("name").toString(), QStringLiteral("TeamForge Demo Host"));

    QVERIFY(c.enterWorkspace("participant"));
    QVERIFY(!c.previewWorkspace("host")); // only the developer can preview

    QVERIFY(c.enterWorkspace("developer"));
    QVERIFY(c.previewWorkspace("participant", "tf-101"));
    QCOMPARE(c.myProfileId(), QStringLiteral("tf-101"));
    QVERIFY(!c.opportunities().isEmpty());
    QVERIFY(c.previewWorkspace("participant", "tf-104")); // another identity, still previewing
    QCOMPARE(c.myProfileId(), QStringLiteral("tf-104"));
    QVERIFY(!c.previewWorkspace("participant", "nobody"));
    QVERIFY(c.previewWorkspace("host", "req-006"));
    QCOMPARE(c.workspace(), QStringLiteral("host"));
    QCOMPARE(c.currentRequirementId(), QStringLiteral("req-006"));
    QVERIFY(!c.rankedCandidates().isEmpty());
    c.endPreview();
    QCOMPARE(c.workspace(), QStringLiteral("developer"));
    QVERIFY(c.myProfileId().isEmpty());
}

void TestController::databaseNumbering()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("developer"));
    // Participants are numbered 1..N in database (A-Z) order; ids stay as they are.
    const QVariantList all = c.students();
    QHash<QString, int> numberOf;
    for (qsizetype i = 0; i < all.size(); ++i) {
        QCOMPARE(all[i].toMap().value("number").toInt(), static_cast<int>(i) + 1);
        numberOf.insert(all[i].toMap().value("id").toString(), static_cast<int>(i) + 1);
    }
    // A record keeps its number whatever the filter or sort.
    for (const QVariant& row : c.queryParticipants({{"sort", "id"}, {"skill", "python"}}).value("rows").toList())
        QCOMPARE(row.toMap().value("number").toInt(), numberOf.value(row.toMap().value("id").toString()));
    QVERIFY(numberOf.contains("TF-P001") && numberOf.contains("tf-001"));

    for (qsizetype i = 0; i < c.requirements().size(); ++i)
        QCOMPARE(c.requirements()[i].toMap().value("number").toInt(), static_cast<int>(i) + 1);
    const QVariantList skills = c.querySkills({{"sort", "name"}}).value("rows").toList();
    QCOMPARE(skills.first().toMap().value("number").toInt(), 1);
    QCOMPARE(skills.last().toMap().value("number").toInt(), static_cast<int>(skills.size()));
    QVERIFY(c.enterWorkspace("host"));
    QVERIFY(c.suggestTeam());
    QVERIFY(c.saveTeam("Numbered"));
    QCOMPARE(c.savedTeams().first().toMap().value("number").toInt(), 1);
}
void TestController::rankingsDifferByProject()
{
    TeamForgeController c(dataDir());
    QVERIFY(c.enterWorkspace("host"));
    // Each project's top five are different people: requirements are varied enough to matter.
    QSet<QString> leaders;
    QSet<QString> topFives;
    for (const QVariant& project : c.requirements()) {
        c.setCurrentRequirementId(project.toMap().value("id").toString());
        QStringList top;
        for (const QVariant& r : c.rankedCandidates().mid(0, 5))
            top.append(r.toMap().value("studentId").toString());
        QCOMPARE(top.size(), 5);
        leaders.insert(top.first());
        topFives.insert(top.join(","));
    }
    QCOMPARE(topFives.size(), static_cast<int>(c.requirements().size()));
    QVERIFY(leaders.size() >= 6);
}

// The browser build limits the workspaces to the signed-in account's role through
// $TEAMFORGE_ALLOWED_WORKSPACES; unset (the desktop) means all three.
void TestController::workspacesLimitedByAccountRole()
{
    {
        TeamForgeController all(dataDir());
        QVERIFY(all.workspaceAllowed("participant"));
        QVERIFY(all.workspaceAllowed("host"));
        QVERIFY(all.workspaceAllowed("developer"));
        QVERIFY(!all.workspaceAllowed("admin"));
    }
    struct EnvGuard {
        EnvGuard() { qputenv("TEAMFORGE_ALLOWED_WORKSPACES", "participant, HOST,unknown"); }
        ~EnvGuard() { qunsetenv("TEAMFORGE_ALLOWED_WORKSPACES"); }
    };
    std::unique_ptr<TeamForgeController> limited;
    {
        EnvGuard guard;
        limited = std::make_unique<TeamForgeController>(dataDir());
    }
    QVERIFY(limited->workspaceAllowed("participant"));
    QVERIFY(limited->workspaceAllowed("host"));
    QVERIFY(!limited->workspaceAllowed("developer"));
    QVERIFY(!limited->workspaceAllowed("unknown"));
    QVERIFY(!limited->enterWorkspace("developer"));
    QCOMPARE(limited->workspace(), QString());
    QVERIFY(!limited->lastError().isEmpty());
    QVERIFY(limited->enterWorkspace("host"));
    QCOMPARE(limited->workspace(), QStringLiteral("host"));
}

QTEST_GUILESS_MAIN(TestController)
#include "tst_controller.moc"
