#include "core/MatchResult.h"

#include <iomanip>
#include <ostream>

namespace teamforge {

bool operator<(const MatchResult& lhs, const MatchResult& rhs)
{
    if (lhs.score != rhs.score)
        return lhs.score < rhs.score;
    return lhs.studentId > rhs.studentId;
}

bool operator>(const MatchResult& lhs, const MatchResult& rhs)
{
    return rhs < lhs;
}

std::ostream& operator<<(std::ostream& out, const MatchResult& result)
{
    std::ios savedFormat(nullptr);
    savedFormat.copyfmt(out);
    const FactorScores& f = result.factors;
    out << std::fixed << std::setprecision(3) << result.studentName << " (" << result.studentId
        << ") score=" << result.score << " [coverage=" << f.coverage
        << " complementarity=" << f.complementarity << " experience=" << f.experience
        << " engagement=" << f.engagement << " diversity=" << f.diversity << "]";
    out.copyfmt(savedFormat);
    return out;
}

} // namespace teamforge
