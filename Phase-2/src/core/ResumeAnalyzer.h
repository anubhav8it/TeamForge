#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace teamforge {

// Resume analysis: plain resume text (core/DocumentText.h) -> suggested profile details. Every
// value it returns was read from the resume and carries the line it came from ("evidence"), so
// the person can check it; anything the resume does not say is left empty, never guessed. The
// person reviews and corrects everything before it is saved.

// TeamForge level (1-5) for years of experience in one skill:
//   0-1 year -> 1 Beginner, more than 1-2 -> 2 Basic, more than 2-4 -> 3 Intermediate,
//   more than 4-5 -> 4 Advanced, more than 5 -> 5 Expert. Unknown (negative) -> 0.
int levelForYears(double years);

struct YearMonth
{
    int year;
    int month; // 1-12
};

struct ResumeSkill
{
    std::string skill;         // TeamForge's normalised name for it
    std::string evidence;      // the resume line it was found on
    double years = -1;         // experience in this skill; -1 when the resume does not say
    std::string yearsEvidence; // the statement or the dated roles the years come from
    int suggestedLevel = 0;    // levelForYears(years); 0 when years is unknown
};

struct ResumeAnalysis
{
    std::string name;
    std::string nameEvidence;
    int age = -1; // only when written out ("Age: 21"); never inferred
    std::string ageEvidence;
    std::vector<ResumeSkill> skills;           // skills TeamForge knows, in resume order
    std::vector<std::string> unmatchedSkills;  // listed under a skills heading, unknown to TeamForge
    double totalYears = -1;                    // overall experience; -1 when unknown
    std::string totalYearsEvidence;
    std::vector<std::string> projects;
    std::vector<std::string> education;
    std::vector<std::string> certifications;
    std::vector<std::string> achievements;
};

// The analysis strategy. Rule-based today; another provider (for example a language model on a
// server) can implement the same interface later without the bridge or the UI changing.
class ResumeExtractor
{
public:
    virtual ~ResumeExtractor() = default;
    // `today` resolves "Present" in date ranges.
    virtual ResumeAnalysis analyse(const std::string& text, YearMonth today) const = 0;
    virtual std::string name() const = 0;
};

// Word-level trie over skill names ("machine learning" = two edges), for longest-match lookup
// of every known skill in one pass over the text: O(total tokens x longest skill), instead of
// searching the text once per skill.
class PhraseTrie
{
public:
    PhraseTrie();
    void insert(const std::vector<std::string>& words, int value);
    // The longest phrase starting at words[from]: {value, words used}, or {-1, 0}.
    std::pair<int, std::size_t> longestMatch(const std::vector<std::string>& words, std::size_t from) const;
    std::size_t size() const { return nodes_.size(); }

private:
    struct Node
    {
        std::unordered_map<std::string, int> next;
        int value = -1;
    };
    std::vector<Node> nodes_;
};

class RuleBasedResumeExtractor : public ResumeExtractor
{
public:
    // `knownSkills`: every skill TeamForge knows (catalogue, profiles, projects), any spelling.
    // A few common spellings ("ReactJS", "k8s", "sklearn") are mapped onto them; a mapping is
    // used only when its target is one of the known skills, so no skill is invented.
    explicit RuleBasedResumeExtractor(const std::vector<std::string>& knownSkills);

    ResumeAnalysis analyse(const std::string& text, YearMonth today) const override;
    std::string name() const override { return "rules"; }

    // Lower-case tokens of a line as the matcher sees them ("Node.js, C++" -> node.js, c++).
    static std::vector<std::string> tokens(const std::string& line);

private:
    struct Target
    {
        std::vector<std::string> skills; // usually one; "c/c++" maps to two
        bool proseSafe;                  // false for words like "go" or "testing": skills sections only
    };
    std::vector<Target> targets_;
    PhraseTrie trie_;
    std::unordered_set<std::string> known_;
};

} // namespace teamforge
