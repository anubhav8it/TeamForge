#include "core/ProjectRequirement.h"

#include "core/Errors.h"
#include "core/Validation.h"

#include <ostream>

namespace teamforge {

namespace {

std::size_t checkedTeamSize(int minTeamSize, int maxTeamSize, const std::string& owner)
{
    if (minTeamSize < 1)
        throw ValidationError(owner + ": minimum team size must be at least 1");
    if (maxTeamSize < minTeamSize)
        throw ValidationError(owner + ": maximum team size must not be smaller than the minimum");
    return static_cast<std::size_t>(minTeamSize);
}

} // namespace

ProjectRequirement::ProjectRequirement(const std::string& id, const std::string& name,
                                       const SkillLevels& requiredSkills, int minTeamSize,
                                       int maxTeamSize, const std::string& type, const std::string& summary)
    : id_(requireNonEmpty(id, "Requirement id"))
    , name_(requireNonEmpty(name, "Requirement name"))
    , requiredSkills_(normalizeSkillLevels(requiredSkills, "Requirement '" + id_ + "'"))
    , minTeamSize_(checkedTeamSize(minTeamSize, maxTeamSize, "Requirement '" + id_ + "'"))
    , maxTeamSize_(static_cast<std::size_t>(maxTeamSize))
    , type_(trimmed(type))
    , summary_(trimmed(summary))
{
    if (requiredSkills_.empty())
        throw ValidationError("Requirement '" + id_ + "' must list at least one required skill");
}

int ProjectRequirement::minLevelFor(std::string_view skill) const
{
    const auto it = requiredSkills_.find(normalizeSkill(skill));
    return it == requiredSkills_.end() ? 0 : it->second;
}

std::vector<std::string> ProjectRequirement::skillNames() const
{
    std::vector<std::string> names;
    names.reserve(requiredSkills_.size());
    for (const auto& entry : requiredSkills_)
        names.push_back(entry.first);
    return names;
}

std::ostream& operator<<(std::ostream& out, const ProjectRequirement& requirement)
{
    return out << requirement.name() << " (" << requirement.id() << ", "
               << requirement.requiredSkills().size() << " skills, team "
               << requirement.minTeamSize() << "-" << requirement.maxTeamSize() << ")";
}

} // namespace teamforge
