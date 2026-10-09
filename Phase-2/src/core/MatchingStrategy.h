#pragma once

#include "core/Interest.h"
#include "core/MatchResult.h"
#include "core/Student.h"
#include "core/Team.h"

#include <string>

namespace teamforge {

// Strategy interface for scoring one candidate against a team's requirement. Concrete
// strategies differ only in how they combine the factors; analyse() computes the factors once,
// so every strategy measures the same things.
//
// Factor definitions (all in [0, 1]); R = required skills, "covers" = level >= minimum level:
//   coverage        |required skills the candidate covers| / |R|
//   complementarity of the skills the candidate covers, the fraction the team does not already
//                   cover (fills a gap = 1, duplicates = 0); 0 if the candidate covers none
//   experience      mean of level/5 over the required skills the candidate offers at any level;
//                   0 if they offer none
//   engagement      the candidate's interest in this project, from the EngagementSource:
//                   1 once they have expressed interest, 0.5 (neutral) before that or when the
//                   strategy has no source
//
// Availability (weekly time slots) was part of the Phase-I model and was removed from the
// product; engagement took its 10% weight. Nothing else about the factors changed.
//   diversity       1 / (1 + number of team members with the candidate's primary role)
class MatchingStrategy
{
public:
    virtual ~MatchingStrategy() = default;

    virtual std::string name() const = 0;

    // Scores `candidate` for team.requirement() given the members already in `team`.
    virtual MatchResult evaluate(const Student& candidate, const Team& team) const = 0;

    // Where the engagement factor comes from (not owned; may be null = neutral for everyone).
    void setEngagementSource(const EngagementSource *source) { engagement_ = source; }
    const EngagementSource *engagementSource() const { return engagement_; }

protected:
    // Fills everything in MatchResult except `score`.
    MatchResult analyse(const Student& candidate, const Team& team) const;

private:
    const EngagementSource *engagement_ = nullptr;
};

} // namespace teamforge
