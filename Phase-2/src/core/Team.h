#pragma once

#include "core/ProjectRequirement.h"
#include "core/Student.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace teamforge {

// A (possibly partial) team for one requirement. Membership rules are enforced on every change:
// no duplicate members and never more than the requirement's maximum size.
class Team
{
public:
    Team(const std::string& id, ProjectRequirement requirement);

    const std::string& id() const { return id_; }
    const ProjectRequirement& requirement() const { return requirement_; }
    const std::vector<Student>& members() const { return members_; }
    std::size_t size() const { return members_.size(); }
    bool isFull() const { return members_.size() >= requirement_.maxTeamSize(); }
    bool contains(const std::string& studentId) const;

    // Throws DuplicateError if already a member, TeamConstraintError if the team is full.
    void addMember(const Student& student);
    // Throws NotFoundError if not a member.
    void removeMember(const std::string& studentId);
    Team& operator+=(const Student& student);

    // A required skill is covered when some member has it at or above the minimum level.
    bool covers(std::string_view skill) const;
    std::vector<std::string> coveredSkills() const;
    std::vector<std::string> missingSkills() const;
    double coverageRatio() const;
    std::size_t countRole(const std::string& role) const;

    // Within size limits and every required skill covered.
    bool isComplete() const;
    // Throws TeamConstraintError if the team is below the minimum size. Missing skills are
    // reported by missingSkills() but do not block finalisation: students make the final call.
    void validateForFinalization() const;

private:
    std::string id_;
    ProjectRequirement requirement_;
    std::vector<Student> members_;
};

} // namespace teamforge
