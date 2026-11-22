#pragma once

#include "core/MatchResult.h"
#include "core/Skill.h"
#include "core/Student.h"
#include "core/WeightedMatchingStrategy.h"

#include <string>
#include <vector>

namespace teamforge {

// Team recruitment: how well one registered person would fit one Community team, explained.
//
// The score is the project's existing weighted matching (WeightedMatchingStrategy: coverage 40%,
// complementarity 25%, experience 15%, engagement 10%, diversity 10%), computed against the
// team's required skills and the members it already has. Nothing is random and nothing comes from
// a language model: the same profile and team always give the same report, and the report lists
// each factor's contribution so the number can be checked.
//
// Rules that keep it fair:
//  * Missing information is not proof of inability. A required skill that is not in the profile is
//    reported as "not shown in the profile", never as "cannot". A skill named on the Community
//    profile without a level counts at the team's minimum level (benefit of the doubt) and is
//    flagged "level not stated". A profile with no skills at all gets "not enough information"
//    instead of a low score.
//  * Evidence is kept apart from claims: a skill backed by the person's projects, certifications
//    or resume (skill years) is marked as such; levels are otherwise self-assessed. TeamForge does
//    not verify skills, and the report says so.
//  * The report is advice for the team leader, who always decides.

enum class Evidence { Projects, Declared, LevelNotStated };
enum class FitBand { Strong, Good, Partial, Low, NotEnoughInformation };
enum class CoverageState { Covered, Partial, Missing };

std::string evidenceName(Evidence evidence); // "projects", "declared", "unrated"
std::string bandName(FitBand band);          // "strong", "good", "partial", "low", "unknown"
std::string bandLabel(FitBand band);         // "Strong match", ..., "Not enough information"
std::string coverageName(CoverageState state); // "covered", "partial", "missing"
// Same thresholds as the app's match colours: >= 0.70 strong, >= 0.50 good, >= 0.30 partial.
FitBand bandFor(double score);

// One registered person, as far as their profiles say.
struct CandidateProfile
{
    std::string id;                          // participant profile id (or the account id)
    std::string name;
    std::string role;                        // their stated main role, e.g. "Backend"
    SkillLevels levels;                      // skills with self-assessed levels 1-5
    std::vector<std::string> unratedSkills;  // named on the Community profile, no level
    ProfileDetails details;                  // projects, education, certifications, skill years
    std::vector<std::string> links;          // portfolio / GitHub
    bool expressedInterest = false;          // applied, or came through an invitation link
};

// One Community team: what it needs and who is in it already.
struct TeamNeeds
{
    std::string name;
    SkillLevels required;                    // skill -> minimum level
    std::size_t maxMembers = 6;
    std::vector<Student> members;            // members' participant profiles (those that have one)
    std::vector<std::string> openRoles;
};

struct SkillFit
{
    std::string skill;
    int required = 0;     // minimum level the team asks for
    int level = 0;        // the person's level (the minimum when the level is not stated)
    Evidence evidence = Evidence::Declared;
    bool fillsGap = false; // no member covers it yet
};

struct SkillCoverage
{
    std::string skill;
    int required = 0;
    int bestLevel = 0;                      // best level among the members, 0 if none has it
    CoverageState state = CoverageState::Missing;
    std::vector<std::string> coveredBy;     // members at or above the minimum level
};

struct CompatibilityReport
{
    double score = 0.0;   // [0, 1]
    int percent = 0;
    FitBand band = FitBand::NotEnoughInformation;
    bool enoughInformation = false;
    FactorScores factors;
    std::vector<SkillFit> matching;          // meets the minimum level
    std::vector<SkillFit> belowLevel;        // has the skill, below the minimum
    std::vector<std::string> notShown;       // required, not in the profile
    std::vector<std::string> strengths;
    std::string suggestedRole;
    std::string roleReason;
    std::vector<std::string> improvements;
    std::vector<std::string> explanation;    // how the score was made, factor by factor
};

// Per required skill: covered by a member, partly (someone has it below the minimum), or missing.
std::vector<SkillCoverage> teamCoverage(const TeamNeeds& team);

// Throws nothing for missing data (it is reported instead). A team without required skills gives
// enoughInformation = false with an explanation.
CompatibilityReport evaluateCompatibility(const CandidateProfile& candidate, const TeamNeeds& team,
                                          const MatchWeights& weights = MatchWeights{});

} // namespace teamforge
