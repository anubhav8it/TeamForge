#include "core/Errors.h"
#include "core/Motivation.h"

#include <QtTest>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace teamforge;

// The motivational quote library: the ten-word rule, the skill rules and the no-repeat queue.
class TestMotivation : public QObject
{
    Q_OBJECT

private slots:
    void wordCountRules();
    void builtInLibraryIsValid();
    void invalidQuotesAreRejected();
    void skillRulesPickCategories();
    void entryAloneNeverMeansExpert();
    void momentsHaveTheirOwnCategories();
    void quoteStaysWithinAVisit();
    void quoteChangesWithTheRule();
    void noImmediateRepeatAcrossVisits();
    void historyIsBounded();
    void unknownMomentThrows();
};

void TestMotivation::wordCountRules()
{
    QCOMPARE(MotivationLibrary::wordCount("Mastering one skill opens doors; combining skills creates new possibilities."), 10);
    QCOMPARE(MotivationLibrary::wordCount("Hard-earned skills, team's work"), 4); // hyphen and apostrophe stay one word
    QCOMPARE(MotivationLibrary::wordCount("  spaced   out \t words "), 3);
    QCOMPARE(MotivationLibrary::wordCount("before - after"), 2); // a lone dash is not a word
    QCOMPARE(MotivationLibrary::wordCount(""), 0);
}

void TestMotivation::builtInLibraryIsValid()
{
    const MotivationLibrary library;
    QVERIFY(library.quotes().size() >= 50);

    std::set<std::string> texts;
    std::map<QuoteCategory, int> perCategory;
    for (const Quote& quote : library.quotes()) {
        QCOMPARE(MotivationLibrary::wordCount(quote.text), MotivationLibrary::kWordsPerQuote);
        QVERIFY2(quote.text.find('*') == std::string::npos, quote.text.c_str());
        QVERIFY2(quote.text.find(quote.highlight) != std::string::npos, quote.text.c_str());
        QVERIFY(quote.highlightIndex >= 0 && quote.highlightIndex < MotivationLibrary::kWordsPerQuote);
        QVERIFY2(texts.insert(quote.text).second, quote.text.c_str());
        ++perCategory[quote.category];
    }
    // Every category is used, with enough quotes for the history to rotate.
    for (QuoteCategory category : {QuoteCategory::FirstSkill, QuoteCategory::SingleSkill, QuoteCategory::MultiSkill,
                                   QuoteCategory::Learning, QuoteCategory::Expertise, QuoteCategory::Collaboration,
                                   QuoteCategory::Teamwork, QuoteCategory::Leadership, QuoteCategory::Innovation,
                                   QuoteCategory::ProjectBuilding, QuoteCategory::Growth}) {
        QVERIFY2(perCategory[category] >= 3, std::string(categoryId(category)).c_str());
        QCOMPARE(library.quotesIn(category).size(), std::size_t(perCategory[category]));
    }
}

void TestMotivation::invalidQuotesAreRejected()
{
    using Raw = std::vector<std::pair<std::string, QuoteCategory>>;
    // Nine words: the master prompt's own example, one word short.
    QVERIFY_THROWS_EXCEPTION(ValidationError,
        MotivationLibrary(Raw{{"Your expertise becomes powerful when shared with ambitious *collaborators*.", QuoteCategory::Expertise}}, 1));
    // No highlight, two highlights.
    QVERIFY_THROWS_EXCEPTION(ValidationError,
        MotivationLibrary(Raw{{"Every expert started somewhere; your journey begins with one skill.", QuoteCategory::FirstSkill}}, 1));
    QVERIFY_THROWS_EXCEPTION(ValidationError,
        MotivationLibrary(Raw{{"Every *expert* started somewhere; your journey begins with one *skill*.", QuoteCategory::FirstSkill}}, 1));
    // The same quote twice.
    QVERIFY_THROWS_EXCEPTION(ValidationError,
        MotivationLibrary(Raw{{"Every expert started somewhere; your journey begins with one *skill*.", QuoteCategory::FirstSkill},
                              {"Every expert started somewhere; your journey begins with one *skill*.", QuoteCategory::Growth}}, 1));
}

void TestMotivation::skillRulesPickCategories()
{
    using V = std::vector<QuoteCategory>;
    const auto rule = [](int count, int advanced, int maxLevel, int learning) {
        return MotivationLibrary::categoriesFor(QuoteMoment::Skills, {count, advanced, maxLevel, learning});
    };
    QVERIFY(rule(0, 0, 0, 0) == V{QuoteCategory::FirstSkill});
    QVERIFY(rule(1, 0, 3, 0) == V{QuoteCategory::SingleSkill});
    QVERIFY((rule(1, 0, 3, 2) == V{QuoteCategory::SingleSkill, QuoteCategory::Learning}));
    QVERIFY(rule(4, 1, 4, 0) == V{QuoteCategory::MultiSkill});      // one of four at 4+: not mostly advanced
    QVERIFY((rule(4, 1, 4, 1) == V{QuoteCategory::MultiSkill, QuoteCategory::Learning}));
    QVERIFY(rule(4, 2, 5, 0) == V{QuoteCategory::Expertise});       // half at 4+
    QVERIFY(rule(1, 1, 5, 0) == V{QuoteCategory::Expertise});
    QVERIFY(rule(3, 0, 2, 0) == V{QuoteCategory::Learning});        // all at Beginner / Basic
}

void TestMotivation::entryAloneNeverMeansExpert()
{
    // Skills are added at the default level 3; adding more never reaches the expertise rule.
    for (int count = 1; count <= 12; ++count) {
        const auto categories = MotivationLibrary::categoriesFor(QuoteMoment::Skills, {count, 0, 3, 0});
        QVERIFY(std::find(categories.begin(), categories.end(), QuoteCategory::Expertise) == categories.end());
    }
}

void TestMotivation::momentsHaveTheirOwnCategories()
{
    const SkillSnapshot skills{5, 5, 5, 0}; // ignored outside the skills moment
    QVERIFY(MotivationLibrary::categoriesFor(QuoteMoment::Register, skills) == std::vector{QuoteCategory::Growth});
    QVERIFY((MotivationLibrary::categoriesFor(QuoteMoment::Join, skills)
             == std::vector{QuoteCategory::Collaboration, QuoteCategory::Teamwork}));
    QVERIFY((MotivationLibrary::categoriesFor(QuoteMoment::HostProject, skills)
             == std::vector{QuoteCategory::Leadership, QuoteCategory::ProjectBuilding, QuoteCategory::Innovation}));

    MotivationLibrary library(7);
    const Quote& join = library.pick(QuoteMoment::Join, {}, 1);
    QVERIFY(join.category == QuoteCategory::Collaboration || join.category == QuoteCategory::Teamwork);
}

void TestMotivation::quoteStaysWithinAVisit()
{
    MotivationLibrary library(3);
    const std::string first = library.pick(QuoteMoment::Skills, {2, 0, 3, 0}, 1).text;
    // More skills, same rule (MultiSkill): the quote must not reshuffle while the user types.
    for (int count = 3; count <= 8; ++count)
        QCOMPARE(library.pick(QuoteMoment::Skills, {count, 0, 3, 0}, 1).text, first);
}

void TestMotivation::quoteChangesWithTheRule()
{
    MotivationLibrary library(3);
    const Quote& none = library.pick(QuoteMoment::Skills, {0, 0, 0, 0}, 1);
    QVERIFY(none.category == QuoteCategory::FirstSkill);
    const Quote& one = library.pick(QuoteMoment::Skills, {1, 0, 3, 0}, 1);
    QVERIFY(one.category == QuoteCategory::SingleSkill);
    const Quote& two = library.pick(QuoteMoment::Skills, {2, 0, 3, 0}, 1);
    QVERIFY(two.category == QuoteCategory::MultiSkill);
}

void TestMotivation::noImmediateRepeatAcrossVisits()
{
    for (unsigned seed : {1u, 2u, 99u, 12345u}) {
        MotivationLibrary library(seed);
        std::string previous;
        for (int visit = 1; visit <= 40; ++visit) {
            const std::string text = library.pick(QuoteMoment::Join, {}, visit).text;
            QVERIFY2(text != previous, text.c_str());
            previous = text;
        }
    }
    // The pool (7 + 6 quotes) is larger than the history, so the last kHistory picks are distinct.
    MotivationLibrary library(5);
    std::vector<std::string> picks;
    for (int visit = 1; visit <= 30; ++visit)
        picks.push_back(library.pick(QuoteMoment::Join, {}, visit).text);
    for (std::size_t i = 0; i + MotivationLibrary::kHistory <= picks.size(); ++i) {
        const std::set<std::string> window(picks.begin() + long(i), picks.begin() + long(i + MotivationLibrary::kHistory));
        QCOMPARE(window.size(), MotivationLibrary::kHistory);
    }
}

void TestMotivation::historyIsBounded()
{
    MotivationLibrary library(11);
    for (int visit = 1; visit <= 100; ++visit)
        library.pick(visit % 2 == 0 ? QuoteMoment::HostProject : QuoteMoment::Register, {}, visit);
    QVERIFY(library.recent().size() <= MotivationLibrary::kHistory);
}

void TestMotivation::unknownMomentThrows()
{
    QVERIFY(momentFromId("skills") == QuoteMoment::Skills);
    QVERIFY_THROWS_EXCEPTION(ValidationError, momentFromId("dashboard"));
}

QTEST_APPLESS_MAIN(TestMotivation)
#include "tst_motivation.moc"
