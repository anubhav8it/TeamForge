#include "core/Motivation.h"

#include "core/Errors.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace teamforge {

namespace {

using C = QuoteCategory;

// The curated library: original quotes, ten words each, one highlight word in asterisks.
// Edit freely, but keep every category at three quotes or more so the history can rotate.
const std::vector<std::pair<std::string, QuoteCategory>>& builtInQuotes()
{
    static const std::vector<std::pair<std::string, QuoteCategory>> quotes = {
        {"Every expert started somewhere; your journey begins with one *skill*.", C::FirstSkill},
        {"Add your first skill and watch new *doors* begin opening.", C::FirstSkill},
        {"One skill on your profile quietly starts something much *bigger*.", C::FirstSkill},
        {"Name what you know today; some team needs exactly *that*.", C::FirstSkill},
        {"Small beginnings count; list your first skill and keep *going*.", C::FirstSkill},
        {"Your profile is a blank canvas; paint it with *skills*.", C::FirstSkill},

        {"Mastering one skill opens doors; combining skills creates new *possibilities*.", C::SingleSkill},
        {"A single focused skill can anchor an entire team's *project*.", C::SingleSkill},
        {"Depth in one area makes you easy to build *around*.", C::SingleSkill},
        {"Sharpen this skill today; tomorrow, let a second one *join*.", C::SingleSkill},
        {"One skill, practised with care, is a promise of *quality*.", C::SingleSkill},
        {"Focus builds momentum; your one skill is already a *contribution*.", C::SingleSkill},

        {"Deep expertise builds confidence; versatile skills help you adapt *faster*.", C::MultiSkill},
        {"Several skills give you more ways to fill team *gaps*.", C::MultiSkill},
        {"Different skills connect like bridges between ideas, teams and *people*.", C::MultiSkill},
        {"Range is a strength: switch roles whenever the project *shifts*.", C::MultiSkill},
        {"Combine what you know and new solutions start to *appear*.", C::MultiSkill},
        {"Many skills, one profile: bring a toolkit, not a *tool*.", C::MultiSkill},

        {"Keep learning; every new skill gives your ideas another *direction*.", C::Learning},
        {"Curiosity is a skill; let it guide your next *step*.", C::Learning},
        {"What you learn this semester becomes what you build *next*.", C::Learning},
        {"Good questions drive real progress; keep asking and keep *growing*.", C::Learning},
        {"Every project becomes a classroom when you're ready to *learn*.", C::Learning},
        {"The skills you want to learn today become tomorrow's *strengths*.", C::Learning},

        {"Your expertise becomes truly powerful when shared with ambitious *collaborators*.", C::Expertise},
        {"Strong skills shine brightest when you help others rise *too*.", C::Expertise},
        {"Experience is a gift; mentoring a teammate multiplies its *value*.", C::Expertise},
        {"Advanced skills set the bar; great teams raise it *together*.", C::Expertise},
        {"Your depth can unblock a whole team; share it *generously*.", C::Expertise},
        {"Hard-earned skills deserve a project that truly matches their *ambition*.", C::Expertise},

        {"Great teams transform individual strengths into *shared* achievements every day.", C::Collaboration},
        {"Your next great opportunity often begins with the right *collaboration*.", C::Collaboration},
        {"Bring your skills, share your ideas and build something *meaningful*.", C::Collaboration},
        {"Collaboration turns good ideas into projects that people actually *use*.", C::Collaboration},
        {"Two perspectives catch what one misses; that is collaboration's *power*.", C::Collaboration},
        {"Join with open ears; the best solutions are built *together*.", C::Collaboration},
        {"Your talent deserves a team that truly believes in *possibilities*.", C::Collaboration},

        {"Alone we code features; together we ship products people *love*.", C::Teamwork},
        {"A team is strongest when every member's skill has *space*.", C::Teamwork},
        {"Complementary skills make teams that are greater than their *parts*.", C::Teamwork},
        {"Show up, speak up, lift up the people beside *you*.", C::Teamwork},
        {"Trust grows with every commit, review and honest team *conversation*.", C::Teamwork},
        {"Teams win when each person knows their part and *delivers*.", C::Teamwork},

        {"Leaders set the vision, then make room for every *voice*.", C::Leadership},
        {"A clear brief is the first gift to your *team*.", C::Leadership},
        {"Great hosts choose complementary people, never just copies of *themselves*.", C::Leadership},
        {"Lead by listening; the best teams are built on *trust*.", C::Leadership},
        {"Define the goal, name the skills, then let talent *lead*.", C::Leadership},
        {"Every strong team starts with someone brave enough to *begin*.", C::Leadership},

        {"New ideas live where different skills meet and collide *productively*.", C::Innovation},
        {"Innovation is rarely solo; it needs curious minds working *together*.", C::Innovation},
        {"Bold prototypes beat perfect plans; build fast, learn faster, *improve*.", C::Innovation},
        {"Question the obvious; *breakthroughs* hide behind the words why not.", C::Innovation},
        {"Fresh perspectives turn ordinary problems into truly *extraordinary* student projects.", C::Innovation},
        {"Today's small experiment could become the tool everyone uses *tomorrow*.", C::Innovation},

        {"Build connections today; create extraordinary projects with your team *tomorrow*.", C::ProjectBuilding},
        {"Great skills become powerful when the right team comes *together*.", C::ProjectBuilding},
        {"Ship something small this week; momentum builds bigger things *later*.", C::ProjectBuilding},
        {"Every finished project is proof your team can do *more*.", C::ProjectBuilding},
        {"From first sketch to final demo, build *step* by step.", C::ProjectBuilding},

        {"Grow one project at a time; each one teaches *something*.", C::Growth},
        {"Your profile tells a story; keep adding chapters worth *reading*.", C::Growth},
        {"Progress beats perfection; today's profile is a strong starting *point*.", C::Growth},
        {"Careers grow from projects, people and the courage to *start*.", C::Growth},
        {"Each team you join adds new skills to your *story*.", C::Growth},
        {"Professional growth starts when you say yes to real *challenges*.", C::Growth},
    };
    return quotes;
}

bool isWordChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

// "possibilities." -> "possibilities"; keeps inner apostrophes and hyphens.
std::string stripPunctuation(std::string_view word)
{
    std::size_t first = 0;
    std::size_t last = word.size();
    while (first < last && !isWordChar(word[first]))
        ++first;
    while (last > first && !isWordChar(word[last - 1]))
        --last;
    return std::string(word.substr(first, last - first));
}

} // namespace

std::string_view categoryId(QuoteCategory category)
{
    switch (category) {
    case C::FirstSkill: return "first_skill";
    case C::SingleSkill: return "single_skill";
    case C::MultiSkill: return "multi_skill";
    case C::Learning: return "learning";
    case C::Expertise: return "expertise";
    case C::Collaboration: return "collaboration";
    case C::Teamwork: return "teamwork";
    case C::Leadership: return "leadership";
    case C::Innovation: return "innovation";
    case C::ProjectBuilding: return "project_building";
    case C::Growth: return "growth";
    }
    return "growth";
}

QuoteMoment momentFromId(std::string_view id)
{
    if (id == "register") return QuoteMoment::Register;
    if (id == "skills") return QuoteMoment::Skills;
    if (id == "join") return QuoteMoment::Join;
    if (id == "host_project") return QuoteMoment::HostProject;
    throw ValidationError("Unknown quote moment: " + std::string(id));
}

MotivationLibrary::MotivationLibrary(unsigned seed)
    : MotivationLibrary(builtInQuotes(), seed)
{
}

MotivationLibrary::MotivationLibrary(const std::vector<std::pair<std::string, QuoteCategory>>& raw, unsigned seed)
    : rotation_(seed == 0 ? 1u : seed)
{
    std::set<std::string> seen;
    for (const auto& [text, category] : raw) {
        add(text, category);
        if (!seen.insert(quotes_.back().text).second)
            throw ValidationError("Quote appears twice: " + quotes_.back().text);
    }
}

void MotivationLibrary::add(const std::string& raw, QuoteCategory category)
{
    const std::size_t open = raw.find('*');
    const std::size_t close = open == std::string::npos ? std::string::npos : raw.find('*', open + 1);
    if (open == std::string::npos || close == std::string::npos || raw.find('*', close + 1) != std::string::npos)
        throw ValidationError("Quote needs exactly one *highlighted* word: " + raw);

    std::string text = raw;
    text.erase(close, 1);
    text.erase(open, 1);
    const std::string highlight = stripPunctuation(raw.substr(open + 1, close - open - 1));
    if (highlight.empty() || highlight.find(' ') != std::string::npos)
        throw ValidationError("Quote highlight must be one word: " + raw);

    const int words = wordCount(text);
    if (words != kWordsPerQuote)
        throw ValidationError("Quote has " + std::to_string(words) + " words, not "
                              + std::to_string(kWordsPerQuote) + ": " + text);

    byCategory_[category].push_back(quotes_.size());
    quotes_.push_back({std::move(text), highlight, wordCount(std::string_view(raw).substr(0, open)), category});
}

int MotivationLibrary::wordCount(std::string_view text)
{
    int count = 0;
    bool inToken = false;
    bool tokenHasWordChar = false;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const bool space = i == text.size() || std::isspace(static_cast<unsigned char>(text[i])) != 0;
        if (space) {
            if (inToken && tokenHasWordChar)
                ++count;
            inToken = false;
            tokenHasWordChar = false;
        } else {
            inToken = true;
            tokenHasWordChar = tokenHasWordChar || isWordChar(text[i]);
        }
    }
    return count;
}

std::vector<QuoteCategory> MotivationLibrary::categoriesFor(QuoteMoment moment, const SkillSnapshot& skills)
{
    switch (moment) {
    case QuoteMoment::Register:
        return {C::Growth};
    case QuoteMoment::Join:
        return {C::Collaboration, C::Teamwork};
    case QuoteMoment::HostProject:
        return {C::Leadership, C::ProjectBuilding, C::Innovation};
    case QuoteMoment::Skills:
        break;
    }
    if (skills.skillCount <= 0)
        return {C::FirstSkill};
    // Self-rated levels only, and only when most skills are rated 4+: entering a skill never
    // makes anyone an "expert".
    if (skills.advancedCount > 0 && skills.advancedCount * 2 >= skills.skillCount)
        return {C::Expertise};
    if (skills.maxLevel <= 2)
        return {C::Learning};
    std::vector<QuoteCategory> categories{skills.skillCount == 1 ? C::SingleSkill : C::MultiSkill};
    if (skills.learningCount > 0)
        categories.push_back(C::Learning);
    return categories;
}

const std::vector<std::size_t>& MotivationLibrary::quotesIn(QuoteCategory category) const
{
    static const std::vector<std::size_t> none;
    const auto found = byCategory_.find(category);
    return found == byCategory_.end() ? none : found->second;
}

const Quote& MotivationLibrary::pick(QuoteMoment moment, const SkillSnapshot& skills, int visit)
{
    std::vector<QuoteCategory> categories = categoriesFor(moment, skills);
    const auto found = shownByVisit_.find(visit);
    const Shown *previous = found == shownByVisit_.end() ? nullptr : &found->second;
    if (previous != nullptr && previous->categories == categories)
        return quotes_[previous->quote];

    std::vector<std::size_t> candidates;
    for (QuoteCategory category : categories) {
        const auto& inCategory = quotesIn(category);
        candidates.insert(candidates.end(), inCategory.begin(), inCategory.end());
    }
    if (candidates.empty())
        throw ValidationError("No quotes for this moment");

    const std::size_t quote = choose(candidates, previous);
    remember(quote);
    shownByVisit_[visit] = {std::move(categories), quote};
    while (shownByVisit_.size() > kVisitsKept)
        shownByVisit_.erase(shownByVisit_.begin());
    return quotes_[quote];
}

std::size_t MotivationLibrary::choose(const std::vector<std::size_t>& candidates, const Shown *previous)
{
    const auto isRecent = [this](std::size_t quote) {
        return std::find(recent_.begin(), recent_.end(), quote) != recent_.end();
    };
    const std::size_t replacing = previous != nullptr ? previous->quote : candidates.size() + quotes_.size();

    // Rotate from a seeded start through the candidates; take the first one not shown lately.
    const std::size_t start = static_cast<std::size_t>(rotation_()) % candidates.size();
    for (std::size_t step = 0; step < candidates.size(); ++step) {
        const std::size_t quote = candidates[(start + step) % candidates.size()];
        if (quote != replacing && !isRecent(quote))
            return quote;
    }
    // Everything was shown lately: the least recently shown one (the queue's front), but never
    // the quote being replaced while there is another choice.
    for (std::size_t quote : recent_) {
        if (quote != replacing && std::find(candidates.begin(), candidates.end(), quote) != candidates.end())
            return quote;
    }
    return candidates[start];
}

void MotivationLibrary::remember(std::size_t quote)
{
    recent_.erase(std::remove(recent_.begin(), recent_.end(), quote), recent_.end());
    recent_.push_back(quote);
    while (recent_.size() > kHistory)
        recent_.pop_front();
}

} // namespace teamforge
