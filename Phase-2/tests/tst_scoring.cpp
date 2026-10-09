#include "TestSupport.h"

#include "core/CoverageOnlyStrategy.h"
#include "core/Errors.h"
#include "core/Team.h"
#include "core/WeightedMatchingStrategy.h"

using namespace teamforge;
using namespace testsupport;

// Expected values are computed by hand from the factor definitions in MatchingStrategy.h and
// the weights 0.40 / 0.25 / 0.15 / 0.10 / 0.10 (coverage, complementarity, experience,
// engagement, diversity). Engagement comes from scenarioInterest(): Gamma has expressed interest
// (1.0), everyone else is neutral (0.5).
class TestScoring : public QObject
{
    Q_OBJECT

private slots:
    void defaultWeightsMatchReport();
    void factorsForEmptyTeam();
    void weightedScoreForEmptyTeam();
    void teamContextChangesComplementarityAndDiversity();
    void gapFillerOutscoresDuplicate();
    void customWeights();
    void engagementFollowsExpressedInterest();
    void invalidWeightsAreRejected();
    void coverageOnlyBaseline();
    void scoresStayInUnitRange();
    void orderingBreaksTiesById();
};

void TestScoring::defaultWeightsMatchReport()
{
    const MatchWeights w;
    TF_COMPARE_NEAR(w.coverage, 0.40);
    TF_COMPARE_NEAR(w.complementarity, 0.25);
    TF_COMPARE_NEAR(w.experience, 0.15);
    TF_COMPARE_NEAR(w.engagement, 0.10);
    TF_COMPARE_NEAR(w.diversity, 0.10);
    w.validate();
}

void TestScoring::factorsForEmptyTeam()
{
    // Alpha: c++ 4 (>= 3, covered), dsa 2 (< 3, offered but not covered), git not required.
    //   coverage        = 1 covered / 4 required          = 0.25
    //   complementarity = 1 gap filled / 1 covered          = 1.0  (empty team has every gap)
    //   experience      = mean(4/5, 2/5)                    = 0.6
    //   engagement      = no interest expressed (neutral)   = 0.5
    //   diversity       = 1 / (1 + 0 same-role members)     = 1.0
    const Team team("t", scenarioRequirement());
    const MatchResult r = scenarioStrategy().evaluate(studentA(), team);

    QCOMPARE(r.studentId, std::string("a"));
    TF_COMPARE_NEAR(r.factors.coverage, 0.25);
    TF_COMPARE_NEAR(r.factors.complementarity, 1.0);
    TF_COMPARE_NEAR(r.factors.experience, 0.6);
    TF_COMPARE_NEAR(r.factors.engagement, 0.5);
    TF_COMPARE_NEAR(r.factors.diversity, 1.0);
    QCOMPARE(r.coveredSkills, strings({"c++"}));
    QCOMPARE(r.gapsFilled, strings({"c++"}));
}

void TestScoring::weightedScoreForEmptyTeam()
{
    const Team team("t", scenarioRequirement());
    const WeightedMatchingStrategy strategy = scenarioStrategy();
    // Alpha: 0.4*0.25 + 0.25*1 + 0.15*0.6 + 0.1*0.5 + 0.1*1 = 0.59
    TF_COMPARE_NEAR(strategy.evaluate(studentA(), team).score, 0.59);
    // Beta: coverage 2/4, comp 1, exp mean(5/5, 3/5) = 0.8, engagement 0.5 (neutral), div 1
    //   0.4*0.5 + 0.25 + 0.15*0.8 + 0.05 + 0.1 = 0.72
    TF_COMPARE_NEAR(strategy.evaluate(studentB(), team).score, 0.72);
    // Gamma: coverage 2/4, comp 1, exp mean(4/5, 2/5) = 0.6, engagement 1 (interested), div 1
    //   0.2 + 0.25 + 0.09 + 0.1 + 0.1 = 0.74
    TF_COMPARE_NEAR(strategy.evaluate(studentC(), team).score, 0.74);
}

void TestScoring::teamContextChangesComplementarityAndDiversity()
{
    // Beta already covers c++ and qt and has Alpha's role.
    Team team("t", scenarioRequirement());
    team += studentB();
    const MatchResult r = scenarioStrategy().evaluate(studentA(), team);

    TF_COMPARE_NEAR(r.factors.complementarity, 0.0); // Alpha's only covered skill is a duplicate
    TF_COMPARE_NEAR(r.factors.diversity, 0.5);       // 1 / (1 + 1)
    QVERIFY(r.gapsFilled.empty());
    // 0.4*0.25 + 0 + 0.15*0.6 + 0.1*0.5 + 0.1*0.5 = 0.29
    TF_COMPARE_NEAR(r.score, 0.29);
}

void TestScoring::gapFillerOutscoresDuplicate()
{
    Team team("t", scenarioRequirement());
    team += studentB(); // gaps left: dsa, testing
    const WeightedMatchingStrategy strategy = scenarioStrategy();

    const MatchResult gamma = strategy.evaluate(studentC(), team);
    const MatchResult alpha = strategy.evaluate(studentA(), team);
    // Gamma fills both remaining gaps: 0.2 + 0.25 + 0.09 + 0.1 + 0.1 = 0.74
    TF_COMPARE_NEAR(gamma.score, 0.74);
    QCOMPARE(gamma.gapsFilled, strings({"dsa", "testing"}));
    QVERIFY(alpha < gamma);
}

void TestScoring::customWeights()
{
    const Team team("t", scenarioRequirement());
    WeightedMatchingStrategy onlyEngagement(MatchWeights{0.0, 0.0, 0.0, 1.0, 0.0});
    onlyEngagement.setEngagementSource(&scenarioInterest());
    TF_COMPARE_NEAR(onlyEngagement.evaluate(studentA(), team).score, 0.5);
    TF_COMPARE_NEAR(onlyEngagement.evaluate(studentC(), team).score, 1.0);
}

void TestScoring::engagementFollowsExpressedInterest()
{
    const Team team("t", scenarioRequirement());
    // Without a source everyone is neutral.
    const WeightedMatchingStrategy neutral;
    TF_COMPARE_NEAR(neutral.evaluate(studentC(), team).factors.engagement, kNeutralEngagement);
    TF_COMPARE_NEAR(neutral.evaluate(studentC(), team).score, 0.69); // 0.74 - 0.1 * 0.5

    // Expressing interest raises the factor to 1 for that project only; the host's decision
    // does not change it.
    InterestBook book;
    WeightedMatchingStrategy strategy;
    strategy.setEngagementSource(&book);
    const double before = strategy.evaluate(studentA(), team).score;
    const InterestRequest& request = book.express("a", "req-t");
    TF_COMPARE_NEAR(strategy.evaluate(studentA(), team).factors.engagement, kInterestedEngagement);
    TF_COMPARE_NEAR(strategy.evaluate(studentA(), team).score - before, 0.05);
    book.setStatus(request.id(), InterestStatus::Declined);
    TF_COMPARE_NEAR(strategy.evaluate(studentA(), team).factors.engagement, kInterestedEngagement);
    const Team other("t2", ProjectRequirement("req-other", "Other", {{"c++", 3}}, 1, 2));
    TF_COMPARE_NEAR(strategy.evaluate(studentA(), other).factors.engagement, kNeutralEngagement);
}

void TestScoring::invalidWeightsAreRejected()
{
    const MatchWeights sumTooHigh{0.5, 0.5, 0.5, 0.0, 0.0};
    const MatchWeights negative{1.2, -0.2, 0.0, 0.0, 0.0};
    QVERIFY_THROWS_EXCEPTION(ValidationError, sumTooHigh.validate());
    QVERIFY_THROWS_EXCEPTION(ValidationError, negative.validate());
    QVERIFY_THROWS_EXCEPTION(ValidationError, WeightedMatchingStrategy{sumTooHigh});
}

void TestScoring::coverageOnlyBaseline()
{
    Team team("t", scenarioRequirement());
    const CoverageOnlyStrategy baseline;
    TF_COMPARE_NEAR(baseline.evaluate(studentA(), team).score, 0.25);

    // With Beta in the team the baseline still scores Alpha 0.25: it cannot see that Alpha
    // only duplicates Beta's c++. This is the gap the weighted strategy is meant to close.
    team += studentB();
    TF_COMPARE_NEAR(baseline.evaluate(studentA(), team).score, 0.25);
    TF_COMPARE_NEAR(baseline.evaluate(studentC(), team).score, 0.5);
}

void TestScoring::scoresStayInUnitRange()
{
    const Student perfect("p", "Perfect", "Solo", {{"c++", 5}, {"qt", 5}, {"dsa", 5}, {"testing", 5}});
    InterestBook book;
    book.express("p", "req-t");
    WeightedMatchingStrategy strategy;
    strategy.setEngagementSource(&book);
    const Team empty("t", scenarioRequirement());
    TF_COMPARE_NEAR(strategy.evaluate(perfect, empty).score, 1.0);
    // Epsilon offers no required skill: only diversity (0.1) and neutral engagement (0.05).
    TF_COMPARE_NEAR(strategy.evaluate(studentE(), empty).score, 0.15);
}

void TestScoring::orderingBreaksTiesById()
{
    MatchResult x1;
    x1.studentId = "x1";
    x1.score = 0.5;
    MatchResult x2 = x1;
    x2.studentId = "x2";
    QVERIFY(x2 < x1); // equal scores: smaller id ranks higher
    QVERIFY(x1 > x2);
    QVERIFY(!(x1 < x1));
}

QTEST_APPLESS_MAIN(TestScoring)
#include "tst_scoring.moc"
