#include "core/ResumeAnalyzer.h"

#include "core/Skill.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <utility>

namespace teamforge {

int levelForYears(double years)
{
    if (years < 0)
        return 0;
    if (years <= 1) return 1; // Beginner
    if (years <= 2) return 2; // Basic
    if (years <= 4) return 3; // Intermediate
    if (years <= 5) return 4; // Advanced
    return 5;                 // Expert
}

// --- PhraseTrie ---------------------------------------------------------------------------------

PhraseTrie::PhraseTrie() : nodes_(1) {}

void PhraseTrie::insert(const std::vector<std::string>& words, int value)
{
    if (words.empty())
        return;
    int node = 0;
    for (const std::string& word : words) {
        const auto it = nodes_[static_cast<std::size_t>(node)].next.find(word);
        if (it != nodes_[static_cast<std::size_t>(node)].next.end()) {
            node = it->second;
        } else {
            const int created = static_cast<int>(nodes_.size());
            nodes_[static_cast<std::size_t>(node)].next.emplace(word, created);
            nodes_.emplace_back();
            node = created;
        }
    }
    if (nodes_[static_cast<std::size_t>(node)].value < 0)
        nodes_[static_cast<std::size_t>(node)].value = value;
}

std::pair<int, std::size_t> PhraseTrie::longestMatch(const std::vector<std::string>& words, std::size_t from) const
{
    std::pair<int, std::size_t> best{-1, 0};
    int node = 0;
    for (std::size_t i = from; i < words.size(); ++i) {
        const auto& next = nodes_[static_cast<std::size_t>(node)].next;
        const auto it = next.find(words[i]);
        if (it == next.end())
            break;
        node = it->second;
        if (nodes_[static_cast<std::size_t>(node)].value >= 0)
            best = {nodes_[static_cast<std::size_t>(node)].value, i - from + 1};
    }
    return best;
}

// --- text helpers -------------------------------------------------------------------------------

namespace {

constexpr std::size_t kMaxItems = 10;
constexpr std::size_t kMaxItemLength = 220;
constexpr std::size_t kMaxUnmatched = 20;
constexpr std::size_t kNearYears = 60; // characters between a skill and "N years" that still tie them

std::string lowerAscii(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string trim(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
        return {};
    const std::size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

// Leading list markers: ASCII ones and the usual bullet glyphs.
std::string stripBullet(const std::string& line)
{
    static const char *const bullets[] = {"\xE2\x80\xA2", "\xE2\x96\xAA", "\xE2\x97\x8F", "\xE2\x97\xA6", "\xE2\x96\xB6",
                                          "\xE2\x9E\xA2", "\xE2\x9C\x93", "\xE2\x9C\x94", "\xE2\x96\xA0", "\xE2\x96\xBA",
                                          "\xC2\xB7", "\xE2\x80\x93", "\xE2\x80\x94", "\xEF\x82\xB7", "-", "*", ">"};
    std::string s = trim(line);
    for (bool changed = true; changed && !s.empty();) {
        changed = false;
        for (const char *bullet : bullets) {
            if (startsWith(s, bullet)) {
                s = trim(s.substr(std::char_traits<char>::length(bullet)));
                changed = true;
            }
        }
    }
    return s;
}

std::string shorten(const std::string& text, std::size_t max = kMaxItemLength)
{
    if (text.size() <= max)
        return text;
    std::size_t cut = max;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
        --cut; // never split a UTF-8 character
    return trim(text.substr(0, cut)) + "\xE2\x80\xA6";
}

struct Token
{
    std::string text;
    std::size_t offset;
};

// Lower-case word tokens. Letters, digits and + # . / - & stay inside a token so "c++", "c#",
// "node.js", "html/css", "ci/cd", "scikit-learn" and "gd&t" survive; a trailing dot or dash
// (end of sentence) does not. Non-ASCII characters (dashes, bullets) separate tokens.
std::vector<Token> tokenize(const std::string& line)
{
    std::vector<Token> result;
    std::size_t i = 0;
    const auto inner = [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u < 0x80 && (std::isalnum(u) != 0 || c == '+' || c == '#' || c == '.' || c == '/' || c == '-' || c == '&');
    };
    while (i < line.size()) {
        if (!inner(line[i]) || line[i] == '/' || line[i] == '-' || line[i] == '&' || line[i] == '.') {
            // A token may start with "." (".net") but not with / - &.
            if (!(line[i] == '.' && i + 1 < line.size() && std::isalpha(static_cast<unsigned char>(line[i + 1])) != 0)) {
                ++i;
                continue;
            }
        }
        const std::size_t start = i;
        while (i < line.size() && inner(line[i]))
            ++i;
        std::string word = lowerAscii(line.substr(start, i - start));
        while (!word.empty() && (word.back() == '.' || word.back() == '-' || word.back() == '/' || word.back() == '&'))
            word.pop_back();
        if (!word.empty())
            result.push_back({word, start});
    }
    return result;
}

std::vector<std::string> words(const std::vector<Token>& tokens)
{
    std::vector<std::string> out;
    out.reserve(tokens.size());
    for (const Token& t : tokens)
        out.push_back(t.text);
    return out;
}

// --- sections -----------------------------------------------------------------------------------

enum class Section { None, Skills, Experience, Projects, Education, Certifications, Achievements, Other };

// "Work Experience:" -> "work experience"; "Awards & Honours" -> "awards and honours".
std::string headingKey(const std::string& line)
{
    std::string key;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
            key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == '&') {
            key += " and ";
        } else if (c == ' ' || c == '/' || c == '-' || c == '|') {
            key += ' ';
        } else if (c == ':') {
            continue;
        } else if (static_cast<unsigned char>(c) < 0x80 && !std::isspace(static_cast<unsigned char>(c))) {
            return {}; // digits or other punctuation: a sentence, not a heading
        }
    }
    std::string collapsed;
    for (const char c : key) {
        if (c == ' ' && (collapsed.empty() || collapsed.back() == ' '))
            continue;
        collapsed += c;
    }
    while (!collapsed.empty() && collapsed.back() == ' ')
        collapsed.pop_back();
    return collapsed;
}

Section headingSection(const std::string& key)
{
    static const std::map<std::string, Section> headings = [] {
        std::map<std::string, Section> m;
        for (const char *h : {"skills", "technical skills", "key skills", "core skills", "skill set", "skillset",
                              "core competencies", "competencies", "technologies", "technology stack", "tech stack",
                              "tools", "tools and technologies", "technical proficiency", "programming languages",
                              "languages and tools", "technical expertise", "areas of expertise", "expertise",
                              "it skills", "computer skills", "software skills", "technical summary", "soft skills",
                              "languages", "skills and tools", "skills and technologies", "technical skills and tools"})
            m[h] = Section::Skills;
        for (const char *h : {"experience", "work experience", "professional experience", "employment",
                              "employment history", "work history", "internships", "internship",
                              "internship experience", "relevant experience", "industry experience", "career history",
                              "professional history", "experience and internships", "work and internships"})
            m[h] = Section::Experience;
        for (const char *h : {"projects", "academic projects", "personal projects", "key projects", "major projects",
                              "project work", "selected projects", "notable projects", "project experience",
                              "technical projects", "project"})
            m[h] = Section::Projects;
        for (const char *h : {"education", "academic background", "academics", "academic qualifications",
                              "educational qualifications", "educational qualification", "qualifications",
                              "education and training", "academic details", "scholastic record", "academic profile"})
            m[h] = Section::Education;
        for (const char *h : {"certifications", "certification", "certificates", "licenses and certifications",
                              "licences and certifications", "courses", "courses and certifications",
                              "certifications and courses", "online courses", "training", "trainings",
                              "training and certifications", "certifications and trainings"})
            m[h] = Section::Certifications;
        for (const char *h : {"achievements", "awards", "honors", "honours", "accomplishments", "awards and achievements",
                              "achievements and awards", "honors and awards", "honours and awards", "awards and honors",
                              "awards and honours", "key achievements", "achievements and certifications"})
            m[h] = Section::Achievements;
        for (const char *h : {"summary", "professional summary", "profile", "profile summary", "career objective",
                              "objective", "about me", "about", "contact", "contact details", "contact information",
                              "personal details", "personal information", "interests", "hobbies", "hobbies and interests",
                              "languages known", "references", "declaration", "extracurricular activities",
                              "extra curricular activities", "activities", "volunteering", "volunteer experience",
                              "publications", "leadership", "positions of responsibility", "strengths"})
            m[h] = Section::Other;
        return m;
    }();
    const auto it = headings.find(key);
    return it == headings.end() ? Section::None : it->second;
}

int wordCount(const std::string& key)
{
    return key.empty() ? 0 : static_cast<int>(std::count(key.begin(), key.end(), ' ')) + 1;
}

// --- years and dates ----------------------------------------------------------------------------

struct YearsMention
{
    double years;
    std::size_t begin;
    std::size_t end;
};

std::vector<YearsMention> yearsMentions(const std::string& lower)
{
    static const std::regex years(R"((\d{1,2}(?:\.\d{1,2})?|one|two|three|four|five|six|seven|eight|nine|ten)\s*\+?\s*(?:years?|yrs?)\b)");
    static const std::regex months(R"((\d{1,2})\s*\+?\s*months?\b)");
    static const std::map<std::string, double> numbers{{"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5},
                                                       {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9}, {"ten", 10}};
    std::vector<YearsMention> found;
    const std::string line = lower.substr(0, 600);
    for (auto it = std::sregex_iterator(line.begin(), line.end(), years); it != std::sregex_iterator(); ++it) {
        const std::string n = (*it)[1].str();
        const auto word = numbers.find(n);
        const double value = word != numbers.end() ? word->second : std::stod(n);
        const auto begin = static_cast<std::size_t>(it->position(0));
        // "4-year degree" and "2024 years" are not experience.
        if (value > 0 && value <= 50 && !(begin > 0 && std::isdigit(static_cast<unsigned char>(line[begin - 1])) != 0))
            found.push_back({value, begin, begin + static_cast<std::size_t>(it->length(0))});
    }
    for (auto it = std::sregex_iterator(line.begin(), line.end(), months); it != std::sregex_iterator(); ++it) {
        const double value = std::stod((*it)[1].str()) / 12.0;
        const auto begin = static_cast<std::size_t>(it->position(0));
        if (value > 0 && value <= 50)
            found.push_back({value, begin, begin + static_cast<std::size_t>(it->length(0))});
    }
    return found;
}

struct Interval
{
    int first; // months since year 0, inclusive
    int last;
};

int monthIndex(int year, int month)
{
    return year * 12 + (month - 1);
}

int monthNumber(const std::string& name)
{
    static const char *const months[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
    for (int i = 0; i < 12; ++i)
        if (startsWith(name, months[i]))
            return i + 1;
    return 0;
}

int parseYear(const std::string& text, int currentYear)
{
    if (text.empty())
        return 0;
    if (text[0] == '\'') {
        const int two = std::stoi(text.substr(1));
        return two + (2000 + two > currentYear + 1 ? 1900 : 2000);
    }
    return std::stoi(text);
}

// "Jan 2023 – Dec 2023", "June 2022 - May 2023", "03/2021 to present", "2019 – 2021".
// `lower` is `original` in lower case (same byte positions); the label keeps the original case.
std::optional<std::pair<Interval, std::string>> dateRange(const std::string& lower, const std::string& original,
                                                          YearMonth today)
{
    static const std::regex range(
        R"((?:^|[^a-z0-9])(?:(jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)[a-z]*\.?\s*,?\s*|(\d{1,2})\s*/\s*)?(\d{4}|'\d{2}))"
        R"(\s*(?:-|\xE2\x80\x93|\xE2\x80\x94|to|till|until)\s*)"
        R"((?:(present|current|now|ongoing|today|date)|(?:(jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)[a-z]*\.?\s*,?\s*|(\d{1,2})\s*/\s*)?(\d{4}|'\d{2})))");
    std::smatch m;
    const std::string line = lower.substr(0, 400);
    if (!std::regex_search(line, m, range))
        return std::nullopt;
    const int startYear = parseYear(m[3].str(), today.year);
    int startMonth = m[1].matched ? monthNumber(m[1].str()) : m[2].matched ? std::stoi(m[2].str()) : 1;
    int endYear = 0;
    int endMonth = 12;
    if (m[4].matched) {
        endYear = today.year;
        endMonth = today.month;
    } else {
        endYear = parseYear(m[7].str(), today.year);
        endMonth = m[5].matched ? monthNumber(m[5].str()) : m[6].matched ? std::stoi(m[6].str()) : 12;
    }
    if (startMonth < 1 || startMonth > 12 || endMonth < 1 || endMonth > 12 || startYear < 1970
        || endYear > today.year + 1 || endYear < startYear)
        return std::nullopt;
    const int first = monthIndex(startYear, startMonth);
    const int last = std::min(monthIndex(endYear, endMonth), monthIndex(today.year, today.month));
    if (last < first || last - first > 50 * 12)
        return std::nullopt;
    std::string label = trim(original.substr(static_cast<std::size_t>(m.position(0)), static_cast<std::size_t>(m.length(0))));
    while (!label.empty() && std::isalnum(static_cast<unsigned char>(label[0])) == 0 && label[0] != '\'')
        label.erase(0, 1);
    return std::make_pair(Interval{first, last}, label);
}

// Sorts and merges overlapping or touching intervals; returns the months they cover. Two roles
// at the same time count once.
int coveredMonths(std::vector<Interval> intervals)
{
    if (intervals.empty())
        return 0;
    std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) { return a.first < b.first; });
    int total = 0;
    Interval current = intervals.front();
    for (std::size_t i = 1; i < intervals.size(); ++i) {
        if (intervals[i].first <= current.last + 1) {
            current.last = std::max(current.last, intervals[i].last);
        } else {
            total += current.last - current.first + 1;
            current = intervals[i];
        }
    }
    return total + current.last - current.first + 1;
}

double roundYears(double years)
{
    return std::round(years * 10.0) / 10.0;
}

std::string titleCase(const std::string& text)
{
    std::string out = lowerAscii(text);
    bool start = true;
    for (char& c : out) {
        if (start && std::isalpha(static_cast<unsigned char>(c)) != 0)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        start = c == ' ' || c == '-' || c == '\'' || c == '.';
    }
    return out;
}

// A name line: two to four words of letters, each capitalised (or the whole line in capitals).
std::string nameFrom(const std::string& line)
{
    if (line.size() > 48 || line.find('@') != std::string::npos)
        return {};
    std::vector<std::string> parts;
    std::string current;
    for (const char c : line + " ") {
        if (c == ' ') {
            if (!current.empty())
                parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (parts.size() < 2 || parts.size() > 4)
        return {};
    bool allCaps = true;
    for (const std::string& part : parts) {
        if (std::isupper(static_cast<unsigned char>(part[0])) == 0)
            return {};
        for (const char c : part) {
            if (std::isalpha(static_cast<unsigned char>(c)) == 0 && c != '.' && c != '\'' && c != '-')
                return {};
            if (std::islower(static_cast<unsigned char>(c)) != 0)
                allCaps = false;
        }
    }
    static const std::set<std::string> notNames{"curriculum", "vitae", "resume", "cv", "profile", "summary", "contact",
                                                "email", "phone", "address", "objective", "developer", "engineer",
                                                "analyst", "student", "designer", "intern", "manager"};
    for (const std::string& part : parts)
        if (notNames.count(lowerAscii(part)) != 0)
            return {};
    return allCaps ? titleCase(line) : line;
}

} // namespace

// --- RuleBasedResumeExtractor -------------------------------------------------------------------

std::vector<std::string> RuleBasedResumeExtractor::tokens(const std::string& line)
{
    return words(tokenize(line));
}

RuleBasedResumeExtractor::RuleBasedResumeExtractor(const std::vector<std::string>& knownSkills)
{
    // Skills that are also everyday words (or one or two letters): only trusted under a
    // skills heading, never in sentences ("we go", "testing ideas").
    static const std::set<std::string> proseWords{
        "c", "r", "go", "qt", "swift", "rust", "ruby", "dart", "julia", "unity", "excel", "cloud", "testing", "debugging",
        "documentation", "editing", "mentoring", "monitoring", "networking", "negotiation", "debate", "pitching",
        "prototyping", "storytelling", "photography", "illustration", "branding", "robotics", "electronics",
        "statistics", "algorithms", "databases", "agile", "notion", "transformers", "slam", "lora", "expo", "helm",
        "compilers", "fundraising", "entrepreneurship", "copywriting", "typography", "videography", "soldering",
        "scriptwriting", "markdown", "kanban", "scrum", "communication", "leadership", "research", "teamwork",
        "marketing", "writing", "management", "design", "sales", "finance"};
    for (const std::string& raw : knownSkills) {
        std::string skill;
        try {
            skill = normalizeSkill(raw);
        } catch (...) {
            continue;
        }
        if (!known_.insert(skill).second)
            continue;
        const auto parts = tokens(skill);
        if (parts.empty())
            continue;
        targets_.push_back({{skill}, proseWords.count(skill) == 0 && skill.size() > 2});
        trie_.insert(parts, static_cast<int>(targets_.size() - 1));
    }
    // Other spellings of known skills. Each is used only when its targets are known skills.
    static const std::vector<std::pair<std::string, std::vector<std::string>>> aliases{
        {"js", {"javascript"}}, {"reactjs", {"react"}}, {"react.js", {"react"}}, {"nodejs", {"node.js"}},
        {"node js", {"node.js"}}, {"node", {"node.js"}}, {"ts", {"typescript"}}, {"postgres", {"postgresql"}},
        {"k8s", {"kubernetes"}}, {"cpp", {"c++"}}, {"golang", {"go"}}, {"vue", {"vue.js"}}, {"vuejs", {"vue.js"}},
        {"nextjs", {"next.js"}}, {"expressjs", {"express.js"}}, {"express", {"express.js"}}, {"sklearn", {"scikit-learn"}},
        {"scikit learn", {"scikit-learn"}}, {"ms excel", {"excel"}}, {"microsoft excel", {"excel"}},
        {"html", {"html/css"}}, {"html5", {"html/css"}}, {"css", {"html/css"}}, {"css3", {"html/css"}},
        {"html5/css3", {"html/css"}}, {"ml", {"machine learning"}}, {"dl", {"deep learning"}},
        {"natural language processing", {"nlp"}}, {"cicd", {"ci/cd"}}, {"amazon web services", {"aws"}},
        {"gcp", {"google cloud"}}, {"google cloud platform", {"google cloud"}}, {"powerbi", {"power bi"}},
        {"sql server", {"microsoft sql server"}}, {"mssql", {"microsoft sql server"}}, {"mongo", {"mongodb"}},
        {"c/c++", {"c", "c++"}}, {"threejs", {"three.js"}}, {"tailwind", {"tailwind css"}},
        {"tensorflow.js", {"tensorflow"}}, {"open cv", {"opencv"}}, {"py torch", {"pytorch"}},
    };
    static const std::set<std::string> ambiguousAliases{"node", "ts", "vue", "express", "ml", "dl", "js", "mongo"};
    for (const auto& [alias, skills] : aliases) {
        std::vector<std::string> present;
        for (const std::string& s : skills)
            if (known_.count(s) != 0)
                present.push_back(s);
        if (present.empty() || present.size() != skills.size())
            continue;
        bool prose = ambiguousAliases.count(alias) == 0;
        for (const std::string& s : present)
            prose = prose && proseWords.count(s) == 0 && s.size() > 2;
        targets_.push_back({present, prose});
        trie_.insert(tokens(alias), static_cast<int>(targets_.size() - 1));
    }
}

ResumeAnalysis RuleBasedResumeExtractor::analyse(const std::string& text, YearMonth today) const
{
    ResumeAnalysis result;

    // Lines, without the blank ones.
    std::vector<std::string> lines;
    {
        std::string current;
        for (const char c : text + "\n") {
            if (c == '\n') {
                const std::string line = trim(current);
                if (!line.empty())
                    lines.push_back(line);
                current.clear();
            } else {
                current += c;
            }
        }
    }

    struct SkillFacts
    {
        SkillFacts(std::size_t firstSeen, std::string line) : order(firstSeen), evidence(std::move(line)) {}
        std::size_t order;
        std::string evidence;
        bool fromSkillsSection = false;
        double explicitYears = -1;
        std::string explicitEvidence;
        std::vector<Interval> roles;
        std::vector<std::string> roleLabels;
    };
    std::map<std::string, SkillFacts> facts;
    std::vector<Interval> allRoles;
    double explicitTotal = -1;
    std::string explicitTotalEvidence;
    std::vector<std::string> unmatched;
    std::set<std::string> unmatchedSeen;

    Section section = Section::None;
    bool seenHeading = false;
    std::optional<std::pair<Interval, std::string>> role; // the dated role being read (experience)
    std::vector<std::string>* items = nullptr;
    bool lastWasItem = false;

    for (std::size_t index = 0; index < lines.size(); ++index) {
        std::string line = lines[index];
        std::string content = line;

        // Headings, alone ("Projects") or with content ("Skills: Python, SQL").
        const std::size_t colon = line.find(':');
        const std::string key = headingKey(colon != std::string::npos ? line.substr(0, colon) : line);
        const Section heading = key.empty() || wordCount(key) > 5 ? Section::None : headingSection(key);
        if (heading != Section::None) {
            section = heading;
            seenHeading = true;
            role.reset();
            items = section == Section::Projects ? &result.projects : section == Section::Education ? &result.education
                  : section == Section::Certifications ? &result.certifications
                  : section == Section::Achievements ? &result.achievements : nullptr;
            lastWasItem = false;
            content = colon != std::string::npos ? trim(line.substr(colon + 1)) : std::string();
            if (content.empty())
                continue;
        }

        // Name and age: explicit labels anywhere, otherwise a name-like line near the top.
        const std::string lower = lowerAscii(content);
        if (result.name.empty() && startsWith(lower, "name") && colon != std::string::npos && heading == Section::None) {
            const std::string candidate = nameFrom(trim(line.substr(colon + 1)));
            if (!candidate.empty()) {
                result.name = candidate;
                result.nameEvidence = line;
                continue;
            }
        }
        if (result.name.empty() && !seenHeading && index < 8) {
            const std::string candidate = nameFrom(stripBullet(content));
            if (!candidate.empty()) {
                result.name = candidate;
                result.nameEvidence = line;
                continue;
            }
        }
        if (result.age < 0) {
            static const std::regex age(R"((?:^|[^a-z])age\s*[:\-]?\s*(\d{2})\b|\b(\d{2})\s*(?:years|yrs)\s*old\b)");
            std::smatch m;
            const std::string head = lower.substr(0, 200);
            if (std::regex_search(head, m, age)) {
                const int value = std::stoi(m[1].matched ? m[1].str() : m[2].str());
                if (value >= 14 && value <= 99) {
                    result.age = value;
                    result.ageEvidence = line;
                }
            }
        }

        // A dated role starts here (experience sections only).
        if (section == Section::Experience) {
            if (const auto range = dateRange(lower, content, today)) {
                role = range;
                allRoles.push_back(range->first);
            }
        }

        // Known skills in this line: longest match first, in one pass.
        const std::vector<Token> toks = tokenize(content);
        const std::vector<std::string> ws = words(toks);
        const std::vector<YearsMention> mentions = yearsMentions(lower);
        std::vector<std::size_t> skillOffsets;
        // A mention belongs to the skills in the same list item: "Python (4 years), Java" gives
        // Python four years and Java none.
        const auto tied = [&](std::size_t offset, const YearsMention& m) -> bool {
            const std::size_t from = std::min(offset, m.begin);
            const std::size_t to = std::max(offset, m.end);
            if (to - from > kNearYears + 30)
                return false;
            return lower.substr(from, to - from).find_first_of(",;|") == std::string::npos;
        };
        for (std::size_t i = 0; i < ws.size();) {
            const auto [value, used] = trie_.longestMatch(ws, i);
            if (value < 0) {
                ++i;
                continue;
            }
            const Target& target = targets_[static_cast<std::size_t>(value)];
            if (!target.proseSafe && section != Section::Skills) {
                i += used;
                continue;
            }
            const std::size_t offset = toks[i].offset;
            skillOffsets.push_back(offset);
            for (const std::string& skill : target.skills) {
                auto it = facts.find(skill);
                if (it == facts.end())
                    it = facts.emplace(skill, SkillFacts(facts.size(), shorten(line, 160))).first;
                SkillFacts& f = it->second;
                if (section == Section::Skills && !f.fromSkillsSection) {
                    f.fromSkillsSection = true;
                    f.evidence = shorten(line, 160); // the skills list is the clearest evidence
                }
                // "Python (4 years)", "3+ years of React": the nearest mention in the same item.
                const YearsMention *nearest = nullptr;
                std::size_t best = kNearYears + 1;
                for (const YearsMention& m : mentions) {
                    const std::size_t distance = offset >= m.end ? offset - m.end : m.begin >= offset ? m.begin - offset : 0;
                    if (distance < best && tied(offset, m)) {
                        best = distance;
                        nearest = &m;
                    }
                }
                if (nearest != nullptr && nearest->years > f.explicitYears) {
                    f.explicitYears = nearest->years;
                    f.explicitEvidence = shorten(line, 160);
                }
                if (role) {
                    f.roles.push_back(role->first);
                    f.roleLabels.push_back(role->second);
                }
            }
            i += used;
        }

        // "5 years of experience" not tied to a skill: overall experience.
        if (lower.find("experience") != std::string::npos) {
            for (const YearsMention& m : mentions) {
                const bool nearSkill = std::any_of(skillOffsets.begin(), skillOffsets.end(),
                                                   [&](std::size_t o) { return tied(o, m); });
                if (!nearSkill && m.years > explicitTotal) {
                    explicitTotal = m.years;
                    explicitTotalEvidence = shorten(line, 160);
                }
            }
        }

        // Skills-section entries TeamForge does not know: offered for review, never added silently.
        if (section == Section::Skills && unmatched.size() < kMaxUnmatched) {
            std::string list = content;
            const std::size_t label = list.find(':');
            if (label != std::string::npos && wordCount(headingKey(list.substr(0, label))) <= 3)
                list = list.substr(label + 1);
            std::string piece;
            const auto flush = [&] {
                std::string candidate;
                int depth = 0;
                for (const char c : piece) { // drop "(4 years)"
                    if (c == '(') ++depth;
                    else if (c == ')') depth = std::max(0, depth - 1);
                    else if (depth == 0) candidate += c;
                }
                candidate = stripBullet(trim(candidate));
                piece.clear();
                const std::vector<std::string> pieceWords = tokens(candidate);
                if (pieceWords.empty() || pieceWords.size() > 4 || candidate.size() > 40 || candidate.find('@') != std::string::npos
                    || lowerAscii(candidate).find("http") != std::string::npos || !yearsMentions(lowerAscii(candidate)).empty())
                    return;
                // Fully covered by known skills: nothing to review.
                bool allKnown = true;
                for (std::size_t i = 0; i < pieceWords.size();) {
                    const auto [value, used] = trie_.longestMatch(pieceWords, i);
                    if (value < 0) {
                        allKnown = false;
                        break;
                    }
                    i += used;
                }
                const std::string keyed = lowerAscii(candidate);
                if (allKnown || std::none_of(candidate.begin(), candidate.end(), [](char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }))
                    return;
                if (unmatchedSeen.insert(keyed).second && unmatched.size() < kMaxUnmatched)
                    unmatched.push_back(candidate);
            };
            for (std::size_t i = 0; i < list.size(); ++i) {
                const char c = list[i];
                const bool bullet = startsWith(list.substr(i), "\xE2\x80\xA2") || startsWith(list.substr(i), "\xC2\xB7");
                if (c == ',' || c == ';' || c == '|' || bullet) {
                    flush();
                    if (bullet)
                        i += startsWith(list.substr(i), "\xC2\xB7") ? 1 : 2;
                } else {
                    piece += c;
                }
            }
            flush();
        }

        // Projects, education, certifications, achievements: one item per line; a line that
        // continues the previous one (starts in lower case) is joined to it.
        if (items != nullptr) {
            const std::string item = stripBullet(content);
            if (!item.empty()) {
                const bool continuation = lastWasItem && !items->empty()
                    && (std::islower(static_cast<unsigned char>(item[0])) != 0
                        || items->back().back() == ',' || items->back().back() == '&');
                if (continuation)
                    items->back() = shorten(items->back() + " " + item);
                else if (items->size() < kMaxItems)
                    items->push_back(shorten(item));
                lastWasItem = true;
            }
        }
    }

    // Per-skill years: an explicit statement wins; otherwise the dated roles that mention the
    // skill, merged so overlapping roles count once. Total experience is never copied onto skills.
    std::vector<std::pair<std::size_t, ResumeSkill>> ordered;
    for (auto& [skill, f] : facts) {
        ResumeSkill s;
        s.skill = skill;
        s.evidence = f.evidence;
        if (f.explicitYears >= 0) {
            s.years = roundYears(f.explicitYears);
            s.yearsEvidence = f.explicitEvidence;
        } else if (!f.roles.empty()) {
            s.years = roundYears(coveredMonths(f.roles) / 12.0);
            std::vector<std::string> labels;
            for (const std::string& label : f.roleLabels)
                if (std::find(labels.begin(), labels.end(), label) == labels.end())
                    labels.push_back(label);
            std::string joined;
            for (const std::string& label : labels)
                joined += (joined.empty() ? "" : "; ") + label;
            s.yearsEvidence = "Roles mentioning it: " + joined;
        }
        s.suggestedLevel = levelForYears(s.years);
        ordered.emplace_back(f.order, std::move(s));
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& entry : ordered)
        result.skills.push_back(std::move(entry.second));

    if (explicitTotal >= 0) {
        result.totalYears = roundYears(explicitTotal);
        result.totalYearsEvidence = explicitTotalEvidence;
    } else if (!allRoles.empty()) {
        result.totalYears = roundYears(coveredMonths(allRoles) / 12.0);
        result.totalYearsEvidence = "Dated roles under the experience heading";
    }
    result.unmatchedSkills = std::move(unmatched);
    return result;
}

} // namespace teamforge
