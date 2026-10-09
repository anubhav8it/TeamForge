#include "core/Ranker.h"

#include <algorithm>
#include <functional>
#include <queue>

namespace teamforge {

std::vector<MatchResult> rankTopK(const std::vector<const Student *>& candidates, const Team& team,
                                  const MatchingStrategy& strategy, std::size_t k)
{
    std::vector<MatchResult> ranked;
    if (k == 0)
        return ranked;

    std::priority_queue<MatchResult, std::vector<MatchResult>, std::greater<MatchResult>> best;
    for (const Student *candidate : candidates) {
        if (candidate == nullptr || team.contains(candidate->id()))
            continue;
        best.push(strategy.evaluate(*candidate, team));
        if (best.size() > k)
            best.pop();
    }

    ranked.reserve(best.size());
    while (!best.empty()) {
        ranked.push_back(best.top());
        best.pop();
    }
    std::reverse(ranked.begin(), ranked.end());
    return ranked;
}

} // namespace teamforge
