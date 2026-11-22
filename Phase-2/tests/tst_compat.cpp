#include "core/Compatibility.h"
#include "core/ProjectRequirement.h"
#include "core/Team.h"
#include "core/WeightedMatchingStrategy.h"

#include <QTest>

#include <algorithm>

using namespace teamforge;

// Team recruitment compatibility (core/Compatibility.h): the score is the project's weighted
// matching, the report explains it, and missing information is never treated as inability.
class TestCompat : public QObject
{
    Q_OBJECT

private slots:
    void bands();
    void teamWithoutRequiredSkills();
    void reportMatchesTheMatchingEngine();
    void skillsSortedIntoMatchingBelowAndNotShown();
    void evidenceIsKeptApartFromClaims();
    void unratedSkillsGetTheBenefitOfTheDoubt();
    void emptyProfileIsNotALowMatch();
    void interestCountsAsEngagement();
    void fillingAGapBeatsDuplicating();
    void teamCoverageStates();
    void suggestedRole();
    void explanationListsEveryFactor();

private:
    static TeamNeeds razor()
    {
        TeamNeeds team;
        team.name = "Razor";
        team.required = {{"c++", 3}, {"qt", 2}, {"ui design", 2}, {"testing", 2}};
        team.maxMembers = 4;
        team.members = {Student("tf-001", "Mukul", "Integration", {{"c++", 4}, {"git", 4}})};
        team.openRoles = {"Frontend", "Tester"};
        return team;
    }
};

void TestCompat::bands()
{
    QCOMPARE(bandFor(0.70), FitBand::Strong);
    QCOMPARE(bandFor(0.69), FitBand::Good);
    QCOMPARE(bandFor(0.50), FitBand::Good);
    QCOMPARE(bandFor(0.30), FitBand::Partial);
    QCOMPARE(bandFor(0.29), FitBand::Low);
    QCOMPARE(bandLabel(FitBand::NotEnoughInformation), std::string("Not enough information"));
}

void TestCompat::teamWithoutRequiredSkills()
{
    TeamNeeds team = razor();
    team.required.clear();
    CandidateProfile person{"tf-002", "Asha", "Frontend", {{"qt", 3}}, {}, {}, {}, false};
    const CompatibilityReport report = evaluateCompatibility(person, team);
    QVERIFY(!report.enoughInformation);
    QCOMPARE(report.band, FitBand::NotEnoughInformation);
    QVERIFY(!report.explanation.empty());
}

void TestCompat::reportMatchesTheMatchingEngine()
{
    const TeamNeeds team = razor();
    CandidateProfile person{"tf-002", "Asha", "Frontend", {{"qt", 3}, {"ui design", 1}, {"python", 4}}, {}, {}, {}, false};
    const CompatibilityReport report = evaluateCompatibility(person, team);

    // The same calculation done directly with the engine.
    const ProjectRequirement requirement("x", "Razor", team.required, 1, 4);
    Team current("x", requirement);
    current.addMember(team.members.front());
    const WeightedMatchingStrategy strategy;
    const MatchResult direct = strategy.evaluate(Student("tf-002", "Asha", "Frontend", person.levels), current);
    QCOMPARE(report.score, direct.score);
    QCOMPARE(report.percent, int(std::lround(direct.score * 100)));
    // Deterministic: the same input gives the same report.
    QCOMPARE(evaluateCompatibility(person, team).score, report.score);
}

void TestCompat::skillsSortedIntoMatchingBelowAndNotShown()
{
    CandidateProfile person{"tf-002", "Asha", "Frontend", {{"QT", 3}, {"ui design", 1}, {"python", 4}}, {}, {}, {}, false};
    const CompatibilityReport report = evaluateCompatibility(person, razor());
    QCOMPARE(report.matching.size(), std::size_t(1));
    QCOMPARE(report.matching[0].skill, std::string("qt"));
    QVERIFY(report.matching[0].fillsGap);
    QCOMPARE(report.belowLevel.size(), std::size_t(1));
    QCOMPARE(report.belowLevel[0].skill, std::string("ui design"));
    QCOMPARE(report.notShown, (std::vector<std::string>{"c++", "testing"}));
    // "not shown", never "cannot": the wording asks the leader to check.
    const bool asks = std::any_of(report.improvements.begin(), report.improvements.end(),
                                  [](const std::string& s) { return s.find("may simply be missing") != std::string::npos; });
    QVERIFY(asks);
    const bool grow = std::any_of(report.improvements.begin(), report.improvements.end(),
                                  [](const std::string& s) { return s == "Grow ui design from level 1 to 2."; });
    QVERIFY(grow);
}

void TestCompat::evidenceIsKeptApartFromClaims()
{
    CandidateProfile person{"tf-002", "Asha", "Frontend", {{"qt", 3}, {"testing", 2}, {"go", 2}}, {}, {}, {}, false};
    person.details.projects = {"Campus map in Qt Quick with GPS", "Worked at Google on docs"};
    TeamNeeds team = razor();
    team.required["go"] = 1;
    const CompatibilityReport report = evaluateCompatibility(person, team);
    auto find = [&](const std::string& skill) {
        return *std::find_if(report.matching.begin(), report.matching.end(), [&](const SkillFit& f) { return f.skill == skill; });
    };
    QCOMPARE(find("qt").evidence, Evidence::Projects);       // mentioned in a project
    QCOMPARE(find("testing").evidence, Evidence::Declared);  // only self-assessed
    QCOMPARE(find("go").evidence, Evidence::Declared);       // "Google" does not count as "go"
    person.details.skillYears = {{"Testing", 1.5}};
    QCOMPARE(evaluateCompatibility(person, team).matching[2].evidence, Evidence::Projects); // resume years
}

void TestCompat::unratedSkillsGetTheBenefitOfTheDoubt()
{
    CandidateProfile person{"acct-7", "Ravi", "", {}, {"Testing", "Figma"}, {}, {}, false};
    const CompatibilityReport report = evaluateCompatibility(person, razor());
    QVERIFY(report.enoughInformation);
    QCOMPARE(report.matching.size(), std::size_t(1));
    QCOMPARE(report.matching[0].skill, std::string("testing"));
    QCOMPARE(report.matching[0].level, 2); // the team's minimum
    QCOMPARE(report.matching[0].evidence, Evidence::LevelNotStated);
    const bool asks = std::any_of(report.improvements.begin(), report.improvements.end(),
                                  [](const std::string& s) { return s.find("No level given") != std::string::npos; });
    QVERIFY(asks);
}

void TestCompat::emptyProfileIsNotALowMatch()
{
    CandidateProfile person{"acct-8", "New Person", "", {}, {}, {}, {}, false};
    const CompatibilityReport report = evaluateCompatibility(person, razor());
    QVERIFY(!report.enoughInformation);
    QCOMPARE(report.band, FitBand::NotEnoughInformation);
    QVERIFY(report.explanation.front().find("says nothing") != std::string::npos);
    QCOMPARE(report.notShown.size(), std::size_t(4)); // reported as "not shown", not as gaps in ability
}

void TestCompat::interestCountsAsEngagement()
{
    CandidateProfile person{"tf-002", "Asha", "Frontend", {{"qt", 3}}, {}, {}, {}, false};
    const CompatibilityReport neutral = evaluateCompatibility(person, razor());
    person.expressedInterest = true;
    const CompatibilityReport keen = evaluateCompatibility(person, razor());
    QCOMPARE(neutral.factors.engagement, 0.5);
    QCOMPARE(keen.factors.engagement, 1.0);
    QVERIFY(std::abs((keen.score - neutral.score) - 0.05) < 1e-9); // 10% weight x 0.5
}

void TestCompat::fillingAGapBeatsDuplicating()
{
    // Same level, but c++ is already covered by a member and qt is not.
    CandidateProfile duplicate{"a", "Dup", "Frontend", {{"c++", 3}}, {}, {}, {}, false};
    CandidateProfile filler{"b", "Fill", "Frontend", {{"qt", 3}}, {}, {}, {}, false};
    QVERIFY(evaluateCompatibility(filler, razor()).score > evaluateCompatibility(duplicate, razor()).score);
}

void TestCompat::teamCoverageStates()
{
    TeamNeeds team = razor();
    team.members.push_back(Student("tf-003", "Ira", "Design", {{"ui design", 1}}));
    const std::vector<SkillCoverage> coverage = teamCoverage(team);
    auto state = [&](const std::string& skill) {
        return std::find_if(coverage.begin(), coverage.end(), [&](const SkillCoverage& c) { return c.skill == skill; })->state;
    };
    QCOMPARE(state("c++"), CoverageState::Covered);
    QCOMPARE(state("ui design"), CoverageState::Partial); // someone has it, below the level
    QCOMPARE(state("qt"), CoverageState::Missing);
    QCOMPARE(coverage.front().coveredBy, (std::vector<std::string>{"Mukul"}));
}

void TestCompat::suggestedRole()
{
    CandidateProfile tester{"t", "Tess", "Tester", {{"testing", 4}}, {}, {}, {}, false};
    QCOMPARE(evaluateCompatibility(tester, razor()).suggestedRole, std::string("Tester")); // an open role
    CandidateProfile builder{"q", "Quinn", "Data", {{"qt", 4}, {"testing", 3}}, {}, {}, {}, false};
    QCOMPARE(evaluateCompatibility(builder, razor()).suggestedRole, std::string("Lead for qt and testing"));
}

void TestCompat::explanationListsEveryFactor()
{
    CandidateProfile person{"tf-002", "Asha", "Frontend", {{"qt", 3}}, {}, {}, {}, false};
    const CompatibilityReport report = evaluateCompatibility(person, razor());
    QCOMPARE(report.explanation.size(), std::size_t(6));
    QVERIFY(report.explanation[0].find("weight 40%") != std::string::npos);
    QVERIFY(report.explanation[5].find("does not verify") != std::string::npos);
}

QTEST_GUILESS_MAIN(TestCompat)
#include "tst_compat.moc"
