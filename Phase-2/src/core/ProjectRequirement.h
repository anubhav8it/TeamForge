#pragma once

#include "core/Skill.h"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace teamforge {

// What a project or hackathon needs: required skills with a minimum level each and team-size
// limits. `type` is a free-text
// category such as "Hackathon" and `summary` a one-line tagline; both are descriptive only and
// do not affect matching.
class ProjectRequirement
{
public:
    // Throws ValidationError on an empty id/name, no required skills, a bad level, or
    // size limits that are not 1 <= minTeamSize <= maxTeamSize. `type` and `summary` may be empty.
    ProjectRequirement(const std::string& id, const std::string& name,
                       const SkillLevels& requiredSkills, int minTeamSize, int maxTeamSize,
                       const std::string& type = {}, const std::string& summary = {});

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& type() const { return type_; }
    const std::string& summary() const { return summary_; }
    const SkillLevels& requiredSkills() const { return requiredSkills_; }
    std::size_t minTeamSize() const { return minTeamSize_; }
    std::size_t maxTeamSize() const { return maxTeamSize_; }

    bool needsSkill(std::string_view skill) const { return minLevelFor(skill) > 0; }
    // Minimum level for `skill` (any spelling), or 0 if it is not required.
    int minLevelFor(std::string_view skill) const;
    std::vector<std::string> skillNames() const;

private:
    std::string id_;
    std::string name_;
    SkillLevels requiredSkills_;
    std::size_t minTeamSize_;
    std::size_t maxTeamSize_;
    std::string type_;
    std::string summary_;
};

std::ostream& operator<<(std::ostream& out, const ProjectRequirement& requirement);

} // namespace teamforge
