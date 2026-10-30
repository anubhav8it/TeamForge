#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace teamforge {

// Motivational quotes for the forms (skill selection, onboarding, joining, project creation).
// Presentation only: nothing here reads or changes a match score, and nothing is generated at
// run time. Every quote is curated, exactly ten words long and checked when the library is built.
//
// Course concepts: the category index is a hash map (unordered_map), and the "recently shown"
// list is a bounded FIFO queue (deque), which is what stops the same quote coming straight back.

enum class QuoteCategory {
    FirstSkill,      // no skills entered yet
    SingleSkill,     // focusing on one skill
    MultiSkill,      // several skills: versatility
    Learning,        // learning interests, or only beginner / basic levels
    Expertise,       // most skills self-rated Advanced or Expert (level 4+)
    Collaboration,   // just before joining a team
    Teamwork,
    Leadership,      // hosts creating a project
    Innovation,
    ProjectBuilding,
    Growth           // profile basics, professional development
};

// Where the quote is shown; each moment draws from its own categories.
enum class QuoteMoment {
    Register,    // onboarding step 01: name and academic details
    Skills,      // choosing skills and levels: depends on the skills
    Join,        // the last thing before "Express interest"
    HostProject  // a host creating or editing a project
};

// Stable lower-case ids, used by the bridge and the QML theme ("first_skill", "teamwork", ...).
std::string_view categoryId(QuoteCategory category);
// Throws ValidationError for an unknown id.
QuoteMoment momentFromId(std::string_view id);

struct Quote
{
    std::string text;      // ten words, display form
    std::string highlight; // the one word the UI lights up (punctuation stripped)
    int highlightIndex;    // its position among the words, 0-based
    QuoteCategory category;
};

// What the skill rules look at: counts only, from the levels the participant entered themselves.
struct SkillSnapshot
{
    int skillCount = 0;
    int advancedCount = 0; // skills at level 4 (Advanced) or 5 (Expert)
    int maxLevel = 0;
    int learningCount = 0; // learning interests
};

class MotivationLibrary
{
public:
    static constexpr int kWordsPerQuote = 10;
    static constexpr std::size_t kHistory = 6; // recently shown quotes that are not picked again

    // The built-in library. `seed` only changes where the rotation starts (the app passes the
    // time, the tests a constant), so the order is reproducible for a given seed.
    explicit MotivationLibrary(unsigned seed = 0);
    // A custom library (tests). Each raw quote marks its highlight word with asterisks:
    // "Keep learning; every new skill gives your ideas another *direction*."
    // Throws ValidationError if a quote is not exactly ten words, has no single highlight or
    // repeats another quote.
    MotivationLibrary(const std::vector<std::pair<std::string, QuoteCategory>>& raw, unsigned seed);

    // Words are whitespace-separated tokens with at least one letter or digit, so "team's" and
    // "hard-earned" are one word each and a lone dash is none.
    static int wordCount(std::string_view text);

    // The rules, in order. Skills moment:
    //   no skills                                   -> FirstSkill
    //   at least half the skills at level 4+         -> Expertise
    //   every skill at level 1-2                     -> Learning
    //   one skill                                    -> SingleSkill (+ Learning with interests)
    //   two or more                                  -> MultiSkill  (+ Learning with interests)
    // Register -> Growth; Join -> Collaboration, Teamwork; HostProject -> Leadership,
    // ProjectBuilding, Innovation.
    static std::vector<QuoteCategory> categoriesFor(QuoteMoment moment, const SkillSnapshot& skills);

    // The quote for one showing of a form (`visit`, a token the caller renews each time the form
    // is shown). Within a visit the quote stays put while the categories stay the same, so typing
    // never reshuffles it; it changes when the categories change or on the next visit, and never
    // to a quote in the recent history while another one is available.
    const Quote& pick(QuoteMoment moment, const SkillSnapshot& skills, int visit);

    const std::vector<Quote>& quotes() const { return quotes_; }
    const std::vector<std::size_t>& quotesIn(QuoteCategory category) const;
    const std::deque<std::size_t>& recent() const { return recent_; }

private:
    struct Shown
    {
        std::vector<QuoteCategory> categories;
        std::size_t quote;
    };
    static constexpr std::size_t kVisitsKept = 32;

    void add(const std::string& raw, QuoteCategory category);
    std::size_t choose(const std::vector<std::size_t>& candidates, const Shown *previous);
    void remember(std::size_t quote);

    std::vector<Quote> quotes_;
    std::unordered_map<QuoteCategory, std::vector<std::size_t>> byCategory_;
    std::deque<std::size_t> recent_;       // oldest first, at most kHistory
    std::map<int, Shown> shownByVisit_;    // visit tokens only grow, so begin() is the oldest
    std::minstd_rand rotation_;
};

} // namespace teamforge
