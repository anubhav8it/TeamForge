#pragma once

#include "core/MatchingStrategy.h"

namespace teamforge {

// Baseline for evaluation: ranks purely by required-skill coverage, the way students compare
// profiles by hand. Comparing it with WeightedMatchingStrategy shows what the other factors add.
class CoverageOnlyStrategy final : public MatchingStrategy
{
public:
    std::string name() const override;
    MatchResult evaluate(const Student& candidate, const Team& team) const override;
};

} // namespace teamforge
