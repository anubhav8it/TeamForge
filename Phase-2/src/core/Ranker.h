#pragma once

#include "core/MatchResult.h"
#include "core/MatchingStrategy.h"
#include "core/Student.h"
#include "core/Team.h"

#include <cstddef>
#include <vector>

namespace teamforge {

// Scores each candidate with `strategy` and returns the best `k`, highest first. Current team
// members and null pointers are skipped.
//
// Keeps a size-k min-heap (std::priority_queue with std::greater): the weakest of the current
// best k sits on top and is evicted when a stronger candidate arrives. O(n log k) time and
// O(k) extra space, versus O(n log n) for sorting everyone.
std::vector<MatchResult> rankTopK(const std::vector<const Student *>& candidates, const Team& team,
                                  const MatchingStrategy& strategy, std::size_t k);

} // namespace teamforge
