#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace teamforge {

// Each factor is in [0, 1]. Definitions live in MatchingStrategy.h.
struct FactorScores
{
    double coverage = 0.0;
    double complementarity = 0.0;
    double experience = 0.0;
    double engagement = 0.0;
    double diversity = 0.0;
};

// One candidate's evaluation against a requirement and the team built so far. Keeps the factor
// breakdown and skill lists so the UI can explain a ranking, not just show a number.
struct MatchResult
{
    std::string studentId;
    std::string studentName;
    double score = 0.0; // [0, 1]
    FactorScores factors;
    std::vector<std::string> coveredSkills; // required skills met at the minimum level
    std::vector<std::string> gapsFilled;    // covered skills the team did not already cover
};

// `lhs < rhs` means lhs ranks below rhs: lower score, or on a tie the larger id (so equal scores
// list in id order). This is a strict weak ordering, so it works with sort and priority_queue.
bool operator<(const MatchResult& lhs, const MatchResult& rhs);
bool operator>(const MatchResult& lhs, const MatchResult& rhs);
std::ostream& operator<<(std::ostream& out, const MatchResult& result);

} // namespace teamforge
