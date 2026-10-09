#pragma once

#include "core/MatchingStrategy.h"

namespace teamforge {

// Defaults are the Phase-I report's weights, with engagement in place of the removed
// availability factor (same 10%).
struct MatchWeights
{
    double coverage = 0.40;
    double complementarity = 0.25;
    double experience = 0.15;
    double engagement = 0.10;
    double diversity = 0.10;

    // Throws ValidationError unless every weight is in [0, 1] and they sum to 1.
    void validate() const;
};

// score = sum(weight_i * factor_i), so it stays in [0, 1].
class WeightedMatchingStrategy final : public MatchingStrategy
{
public:
    explicit WeightedMatchingStrategy(const MatchWeights& weights = MatchWeights{});

    std::string name() const override;
    MatchResult evaluate(const Student& candidate, const Team& team) const override;

    const MatchWeights& weights() const { return weights_; }

private:
    MatchWeights weights_;
};

} // namespace teamforge
