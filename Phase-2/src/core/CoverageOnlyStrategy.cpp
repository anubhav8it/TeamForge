#include "core/CoverageOnlyStrategy.h"

namespace teamforge {

std::string CoverageOnlyStrategy::name() const
{
    return "Coverage only (baseline)";
}

MatchResult CoverageOnlyStrategy::evaluate(const Student& candidate, const Team& team) const
{
    MatchResult result = analyse(candidate, team);
    result.score = result.factors.coverage;
    return result;
}

} // namespace teamforge
