#include "core/Team.h"

#include "core/Errors.h"
#include "core/Validation.h"

#include <algorithm>
#include <utility>

namespace teamforge {

Team::Team(const std::string& id, ProjectRequirement requirement)
    : id_(requireNonEmpty(id, "Team id"))
    , requirement_(std::move(requirement))
{
}

bool Team::contains(const std::string& studentId) const
{
    return std::any_of(members_.begin(), members_.end(),
                       [&](const Student& member) { return member.id() == studentId; });
}

void Team::addMember(const Student& student)
{
    if (contains(student.id()))
        throw DuplicateError("Student '" + student.id() + "' is already in team '" + id_ + "'");
    if (isFull()) {
        throw TeamConstraintError("Team '" + id_ + "' is full (maximum "
                                  + std::to_string(requirement_.maxTeamSize()) + " members)");
    }
    members_.push_back(student);
}

void Team::removeMember(const std::string& studentId)
{
    const auto it = std::find_if(members_.begin(), members_.end(),
                                 [&](const Student& member) { return member.id() == studentId; });
    if (it == members_.end())
        throw NotFoundError("Student '" + studentId + "' is not in team '" + id_ + "'");
    members_.erase(it);
}

Team& Team::operator+=(const Student& student)
{
    addMember(student);
    return *this;
}

bool Team::covers(std::string_view skill) const
{
    const int minLevel = requirement_.minLevelFor(skill);
    if (minLevel == 0)
        return false;
    return std::any_of(members_.begin(), members_.end(),
                       [&](const Student& member) { return member.levelIn(skill) >= minLevel; });
}

std::vector<std::string> Team::coveredSkills() const
{
    std::vector<std::string> result;
    for (const auto& entry : requirement_.requiredSkills()) {
        if (covers(entry.first))
            result.push_back(entry.first);
    }
    return result;
}

std::vector<std::string> Team::missingSkills() const
{
    std::vector<std::string> result;
    for (const auto& entry : requirement_.requiredSkills()) {
        if (!covers(entry.first))
            result.push_back(entry.first);
    }
    return result;
}

double Team::coverageRatio() const
{
    return static_cast<double>(coveredSkills().size())
           / static_cast<double>(requirement_.requiredSkills().size());
}

std::size_t Team::countRole(const std::string& role) const
{
    return static_cast<std::size_t>(std::count_if(
        members_.begin(), members_.end(), [&](const Student& member) { return member.role() == role; }));
}

bool Team::isComplete() const
{
    return members_.size() >= requirement_.minTeamSize() && missingSkills().empty();
}

void Team::validateForFinalization() const
{
    if (members_.size() < requirement_.minTeamSize()) {
        throw TeamConstraintError("Team '" + id_ + "' needs at least "
                                  + std::to_string(requirement_.minTeamSize()) + " members, has "
                                  + std::to_string(members_.size()));
    }
}

} // namespace teamforge
