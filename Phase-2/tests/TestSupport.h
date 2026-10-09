#pragma once

#include "core/Interest.h"
#include "core/ProjectRequirement.h"
#include "core/Student.h"
#include "core/WeightedMatchingStrategy.h"

#include <QString>
#include <QTest>

#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

// QCOMPARE on doubles is relative-fuzzy and awkward near 0; scores need an absolute tolerance.
#define TF_COMPARE_NEAR(actual, expected)                                                        \
    QVERIFY2(std::abs((actual) - (expected)) < 1e-9,                                             \
             qPrintable(QStringLiteral("actual %1, expected %2")                                 \
                            .arg(static_cast<double>(actual), 0, 'g', 12)                        \
                            .arg(static_cast<double>(expected), 0, 'g', 12)))

namespace testsupport {

// Shared scenario. Expected scores for it are worked out by hand in tst_scoring.cpp.
// Required: c++ >= 3, dsa >= 3, qt >= 2, testing >= 2; team of 2-4.
inline teamforge::ProjectRequirement scenarioRequirement()
{
    return {"req-t", "Scenario", {{"C++", 3}, {"Qt", 2}, {"DSA", 3}, {"Testing", 2}}, 2, 4};
}

// Engagement for the scenario: only Gamma has expressed interest in it (engagement 1.0);
// everyone else is neutral (0.5).
inline const teamforge::InterestBook& scenarioInterest()
{
    static const teamforge::InterestBook book = [] {
        teamforge::InterestBook b;
        b.express("c", "req-t");
        return b;
    }();
    return book;
}

// The report's weights with the scenario's engagement.
inline teamforge::WeightedMatchingStrategy scenarioStrategy()
{
    teamforge::WeightedMatchingStrategy strategy;
    strategy.setEngagementSource(&scenarioInterest());
    return strategy;
}

// Covers c++ only (dsa 2 is below the minimum of 3).
inline teamforge::Student studentA()
{
    return {"a", "Alpha", "C++/OOP", {{"c++", 4}, {"dsa", 2}, {"git", 3}}};
}

// Covers c++ and qt, same role as A.
inline teamforge::Student studentB()
{
    return {"b", "Beta", "C++/OOP", {{"c++", 5}, {"qt", 3}}};
}

// Covers dsa and testing; has expressed interest in the scenario.
inline teamforge::Student studentC()
{
    return {"c", "Gamma", "DSA", {{"dsa", 4}, {"testing", 2}, {"python", 3}}};
}

// Covers qt and testing.
inline teamforge::Student studentD()
{
    return {"d", "Delta", "UI/Testing", {{"testing", 3}, {"qt", 3}}};
}

// Offers no required skill.
inline teamforge::Student studentE()
{
    return {"e", "Epsilon", "Backend", {{"java", 4}}};
}

inline std::vector<std::string> strings(std::initializer_list<const char *> items)
{
    return std::vector<std::string>(items.begin(), items.end());
}

} // namespace testsupport
