#include "core/WeightedMatchingStrategy.h"

#include "core/Errors.h"

#include <cmath>

namespace teamforge {

void MatchWeights::validate() const
{
    const double all[] = {coverage, complementarity, experience, engagement, diversity};
    double sum = 0.0;
    for (double weight : all) {
        if (!(weight >= 0.0 && weight <= 1.0))
            throw ValidationError("Each match weight must be between 0 and 1");
        sum += weight;
    }
    if (std::abs(sum - 1.0) > 1e-9)
        throw ValidationError("Match weights must sum to 1, got " + std::to_string(sum));
}

WeightedMatchingStrategy::WeightedMatchingStrategy(const MatchWeights& weights)
    : weights_(weights)
{
    weights_.validate();
}

std::string WeightedMatchingStrategy::name() const
{
    return "Weighted";
}

MatchResult WeightedMatchingStrategy::evaluate(const Student& candidate, const Team& team) const
{
    MatchResult result = analyse(candidate, team);
    const FactorScores& f = result.factors;
    result.score = weights_.coverage * f.coverage + weights_.complementarity * f.complementarity
                   + weights_.experience * f.experience + weights_.engagement * f.engagement
                   + weights_.diversity * f.diversity;
    return result;
}

} // namespace teamforge
