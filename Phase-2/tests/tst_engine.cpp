#include "TestSupport.h"

#include "core/CoverageOnlyStrategy.h"
#include "core/Errors.h"
#include "core/MatchingEngine.h"
#include "core/WeightedMatchingStrategy.h"

#include <algorithm>

using namespace teamforge;
using namespace testsupport;

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void retrievalUsesIndexAndSkipsMembers();
    void rankReturnsBestFirst();
    void greedyTeamSuggestion();
    void suggestionStopsWhenCandidatesRunOut();
    void updateAndRemoveKeepStructuresInSync();
    void skillLookupsThroughEngine();
    void openEndedSkillsFlowThroughEveryStructure();
    void rebuildingStructuresMatchesIncrementalState();

private:
    MatchingEngine engine_;
};

void TestEngine::init()
{
    engine_ = MatchingEngine();
    for (const Student& s : {studentA(), studentB(), studentC(), studentD(), studentE()})
        engine_.addStudent(s);
}

void TestEngine::retrievalUsesIndexAndSkipsMembers()
{
    Team team("t", scenarioRequirement());
    team += studentB();
    std::vector<std::string> ids;
    for (const Student *s : engine_.retrieveCandidates(team))
        ids.push_back(s->id());
    // Epsilon offers no required skill; Beta is already a member.
    QCOMPARE(ids, strings({"a", "c", "d"}));
}

void TestEngine::rankReturnsBestFirst()
{
    const Team empty("t", scenarioRequirement());
    // Empty-team scores (tst_scoring): Gamma 0.74, Beta 0.72, Alpha 0.59.
    // Delta: coverage 2/4, comp 1, exp mean(3/5, 3/5) = 0.6, engagement 0.5, div 1
    //   0.2 + 0.25 + 0.09 + 0.05 + 0.1 = 0.69
    const auto ranked = engine_.rank(empty, scenarioStrategy(), 3);
    QCOMPARE(ranked.size(), std::size_t{3});
    QCOMPARE(ranked[0].studentId, std::string("c"));
    QCOMPARE(ranked[1].studentId, std::string("b"));
    QCOMPARE(ranked[2].studentId, std::string("d"));
    TF_COMPARE_NEAR(ranked[1].score, 0.72);
    TF_COMPARE_NEAR(ranked[2].score, 0.69);
}

void TestEngine::greedyTeamSuggestion()
{
    // Round 1 (empty team): Gamma 0.74 is best.
    // Round 2 (team {Gamma}, gaps c++ and qt):
    //   Beta  fills both gaps:       0.2 + 0.25 + 0.12 + 0.05 + 0.1 = 0.72
    //   Alpha fills c++:             0.1 + 0.25 + 0.09 + 0.05 + 0.1 = 0.59
    //   Delta fills qt, dups testing: 0.2 + 0.125 + 0.09 + 0.05 + 0.1 = 0.565
    // Team {Gamma, Beta} covers everything and meets the minimum of 2, so it stops.
    const Team team = engine_.suggestTeam("suggested", scenarioRequirement(), scenarioStrategy());
    std::vector<std::string> ids;
    for (const Student& s : team.members())
        ids.push_back(s.id());
    QCOMPARE(ids, strings({"c", "b"}));
    QVERIFY(team.isComplete());
    team.validateForFinalization();
}

void TestEngine::suggestionStopsWhenCandidatesRunOut()
{
    MatchingEngine small;
    small.addStudent(studentA());
    small.addStudent(studentE());
    // Only Alpha offers any required skill: the team cannot reach full coverage or min size 2.
    const Team team = small.suggestTeam("t", scenarioRequirement(), CoverageOnlyStrategy());
    QCOMPARE(team.size(), std::size_t{1});
    QCOMPARE(team.missingSkills(), strings({"dsa", "qt", "testing"}));
    QVERIFY_THROWS_EXCEPTION(TeamConstraintError, team.validateForFinalization());
}

void TestEngine::updateAndRemoveKeepStructuresInSync()
{
    // Alpha learns Qt; drops git.
    engine_.updateStudent(Student("a", "Alpha", "C++/OOP", {{"c++", 4}, {"qt", 2}}));
    QVERIFY(engine_.skillIndex().studentsWith("qt").count("a") == 1);
    QVERIFY(engine_.skillIndex().studentsWith("git").empty());
    QVERIFY(!engine_.skillTree().contains("git"));
    QCOMPARE(engine_.skillTree().count("qt"), 3); // Alpha, Beta, Delta
    QCOMPARE(engine_.skillGraph().edgeWeight("c++", "qt"), 2);

    engine_.removeStudent("b");
    QVERIFY(!engine_.students().contains("b"));
    QCOMPARE(engine_.skillTree().count("qt"), 2);
    QCOMPARE(engine_.skillGraph().edgeWeight("c++", "qt"), 1);
    QVERIFY(engine_.skillTree().isValidAvl());

    QVERIFY_THROWS_EXCEPTION(NotFoundError, engine_.removeStudent("b"));
    QVERIFY_THROWS_EXCEPTION(NotFoundError, engine_.updateStudent(Student("zz", "Z", "R", {{"c++", 1}})));
    QVERIFY_THROWS_EXCEPTION(DuplicateError, engine_.addStudent(studentC()));
}

void TestEngine::skillLookupsThroughEngine()
{
    QCOMPARE(engine_.skillsWithPrefix(" T"), strings({"testing"}));
    QCOMPARE(engine_.skillsWithPrefix(""), strings({"c++", "dsa", "git", "java", "python", "qt", "testing"}));
    const auto related = engine_.relatedSkills("java", 3);
    QVERIFY(related.empty()); // Epsilon's only skill: isolated node

    const auto nearQt = engine_.relatedSkills("qt", 1);
    QVERIFY(std::find(nearQt.begin(), nearQt.end(), SkillDistance{"c++", 1}) != nearQt.end());
}

void TestEngine::openEndedSkillsFlowThroughEveryStructure()
{
    // Skills are arbitrary strings: a brand-new one is normalised, indexed, autocompleted,
    // linked in the graph and matched against a requirement that asks for it.
    engine_.addStudent(Student("r", "Rho", "Robotics", {{"  ROS2 ", 4}, {"Edge   AI", 3}}));
    QVERIFY(engine_.skillIndex().studentsWith("ros2").count("r") == 1);
    QCOMPARE(engine_.skillsWithPrefix("ro"), strings({"ros2"}));
    QCOMPARE(engine_.skillGraph().edgeWeight("ros2", "edge ai"), 1);

    const ProjectRequirement robots("rq", "Robots", {{"ROS2", 3}, {"edge ai", 2}}, 1, 2);
    const auto ranked = engine_.rank(Team("t", robots), WeightedMatchingStrategy(), 5);
    QCOMPARE(ranked.size(), std::size_t{1});
    QCOMPARE(ranked.front().studentId, std::string("r"));
    QCOMPARE(ranked.front().coveredSkills, strings({"edge ai", "ros2"}));
}

void TestEngine::rebuildingStructuresMatchesIncrementalState()
{
    engine_.updateStudent(Student("a", "Alpha", "C++/OOP", {{"c++", 4}, {"qt", 2}}));
    engine_.removeStudent("e");
    const auto prefixBefore = engine_.skillsWithPrefix("");
    const auto qtBefore = engine_.skillIndex().studentsWith("qt");
    const int heightBefore = engine_.skillTree().height();
    const std::size_t edgesBefore = engine_.skillGraph().edgeCount();

    engine_.rebuildSkillIndex();
    engine_.rebuildSkillTree();
    engine_.rebuildSkillGraph();

    QCOMPARE(engine_.skillsWithPrefix(""), prefixBefore);
    QCOMPARE(engine_.skillTree().count("qt"), 3);
    QVERIFY(engine_.skillTree().isValidAvl());
    QVERIFY(engine_.skillTree().height() <= heightBefore + 1);
    QVERIFY(engine_.skillIndex().studentsWith("qt") == qtBefore);
    QCOMPARE(engine_.skillGraph().edgeCount(), edgesBefore);
    QCOMPARE(engine_.skillGraph().edgeWeight("c++", "qt"), 2);
}

QTEST_APPLESS_MAIN(TestEngine)
#include "tst_engine.moc"
