#include "core/MatchingStrategy.h"

namespace teamforge {

MatchResult MatchingStrategy::analyse(const Student& candidate, const Team& team) const
{
    const ProjectRequirement& requirement = team.requirement();

    MatchResult result;
    result.studentId = candidate.id();
    result.studentName = candidate.name();

    double levelSum = 0.0;
    int requiredSkillsOffered = 0;
    for (const auto& [skill, minLevel] : requirement.requiredSkills()) {
        const int level = candidate.levelIn(skill);
        if (level == 0)
            continue;
        ++requiredSkillsOffered;
        levelSum += static_cast<double>(level) / kMaxSkillLevel;
        if (level >= minLevel) {
            result.coveredSkills.push_back(skill);
            if (!team.covers(skill))
                result.gapsFilled.push_back(skill);
        }
    }

    FactorScores& f = result.factors;
    const auto covered = static_cast<double>(result.coveredSkills.size());
    f.coverage = covered / static_cast<double>(requirement.requiredSkills().size());
    f.complementarity = covered > 0.0 ? static_cast<double>(result.gapsFilled.size()) / covered : 0.0;
    f.experience = requiredSkillsOffered > 0 ? levelSum / requiredSkillsOffered : 0.0;
    f.engagement = engagement_ != nullptr ? engagement_->engagement(candidate.id(), requirement.id())
                                          : kNeutralEngagement;
    f.diversity = 1.0 / (1.0 + static_cast<double>(team.countRole(candidate.role())));
    return result;
}

} // namespace teamforge
