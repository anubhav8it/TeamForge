#include "matching.hpp"

#include <algorithm>
#include <set>
#include <tuple>

namespace tf::matching {

size_t utf8_length(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::string normalize_skill(const std::string& name) {
    std::string out;
    bool space = false;
    for (unsigned char c : name) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
            space = !out.empty();
            continue;
        }
        if (space) { out += ' '; space = false; }
        out += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : char(c);
    }
    size_t len = utf8_length(out);
    return (len > 0 && len <= kMaxSkillLen) ? out : std::string();
}

std::vector<Row> explain(const Skills& skills, const Skills& required) {
    std::vector<Row> rows;
    for (const auto& [skill, need] : required) {
        auto it = skills.find(skill);
        int have = it == skills.end() ? 0 : it->second;
        rows.push_back({skill, need, have, have >= need ? "met" : have > 0 ? "partial" : "missing"});
    }
    // Highest required level first, then alphabetical.
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.required != b.required ? a.required > b.required : a.skill < b.skill;
    });
    return rows;
}

int score(const Skills& skills, const Skills& required) {
    long total = 0, got = 0;
    for (const auto& [skill, need] : required) {
        total += need;
        auto it = skills.find(skill);
        if (it != skills.end()) got += std::min(it->second, need);
    }
    if (total == 0) return 0;
    return int((200 * got + total) / (2 * total));   // rounded percentage
}

Skills team_levels(const std::vector<const Skills*>& members) {
    Skills best;
    for (const Skills* m : members)
        for (const auto& [skill, level] : *m)
            if (level > best[skill]) best[skill] = level;
    return best;
}

Coverage coverage(const std::vector<const Skills*>& members, const Skills& required) {
    Skills best = team_levels(members);
    Coverage c;
    c.rows = explain(best, required);
    c.percent = score(best, required);
    for (const auto& r : c.rows) if (r.status != "met") c.missing.push_back(r.skill);
    return c;
}

std::vector<std::string> suggest_team(const std::map<std::string, Skills>& candidates,
                                      const Skills& required, size_t max_size,
                                      const std::vector<std::string>& start, size_t min_size) {
    std::vector<std::string> team;
    Skills best;
    auto absorb = [&](const Skills& s) {
        for (const auto& [k, v] : s) if (v > best[k]) best[k] = v;
    };
    for (const auto& id : start) {
        auto it = candidates.find(id);
        if (it != candidates.end() && std::find(team.begin(), team.end(), id) == team.end()) {
            team.push_back(id);
            absorb(it->second);
        }
    }
    auto gain = [&](const Skills& s) {
        int g = 0;
        for (const auto& [skill, need] : required) {
            auto b = best.find(skill);
            int now = b == best.end() ? 0 : b->second;
            auto it = s.find(skill);
            int theirs = it == s.end() ? 0 : it->second;
            g += std::min(std::max(now, theirs), need) - std::min(now, need);
        }
        return g;
    };

    std::set<std::string> pool;
    for (const auto& [id, _] : candidates)
        if (std::find(team.begin(), team.end(), id) == team.end()) pool.insert(id);

    while (team.size() < max_size && !pool.empty()) {
        if (team.size() >= min_size && score(best, required) == 100) break;
        bool any_gain = false;
        for (const auto& id : pool) if (gain(candidates.at(id)) > 0) { any_gain = true; break; }

        std::string pick;
        std::tuple<int, int> best_key{-1, -1};
        for (const auto& id : pool) {   // std::set iterates in id order; later ids win ties
            const Skills& s = candidates.at(id);
            std::tuple<int, int> key = any_gain || team.empty()
                ? std::make_tuple(gain(s), score(s, required))
                : std::make_tuple(0, score(s, required));     // nobody adds coverage: fill by match
            if (key >= best_key) { best_key = key; pick = id; }
        }
        team.push_back(pick);
        pool.erase(pick);
        absorb(candidates.at(pick));
    }
    return team;
}

}  // namespace tf::matching
