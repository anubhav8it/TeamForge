// Skill matching. Pure functions with no database or HTTP code, so they are
// easy to test and easy to swap for your own algorithm.
//
// Score rule: for each required skill with minimum level R and a person's
// level L, credit = min(L, R) (0 if missing). Match % = credit / sum(R) * 100.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace tf::matching {

using Skills = std::map<std::string, int>;   // skill name -> level 1..5

constexpr size_t kMaxSkillLen = 40;   // characters

// "  Machine   Learning " -> "machine learning". Returns "" if empty or too long.
std::string normalize_skill(const std::string& name);
size_t utf8_length(const std::string& s);

struct Row {
    std::string skill;
    int required = 0;
    int have = 0;
    std::string status;   // "met" | "partial" | "missing"
};

std::vector<Row> explain(const Skills& skills, const Skills& required);
int score(const Skills& skills, const Skills& required);
Skills team_levels(const std::vector<const Skills*>& members);

struct Coverage {
    int percent = 0;
    std::vector<Row> rows;
    std::vector<std::string> missing;
};
Coverage coverage(const std::vector<const Skills*>& members, const Skills& required);

// Greedy team suggestion. Keeps `start`, then repeatedly adds whoever closes
// the most remaining skill gap (ties: higher individual match). Stops when
// every skill is covered and the team has min_size people, or at max_size.
std::vector<std::string> suggest_team(const std::map<std::string, Skills>& candidates,
                                      const Skills& required, size_t max_size,
                                      const std::vector<std::string>& start = {},
                                      size_t min_size = 1);

}  // namespace tf::matching
