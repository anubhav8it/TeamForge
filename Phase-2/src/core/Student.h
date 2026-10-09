#pragma once

#include "core/Skill.h"

#include <iosfwd>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace teamforge {

// A student's skill profile: what they offer (with proficiency 1-5), what they want to learn
// and their primary role (used for team balance). `summary` (one-line
// profile description) and `program` (academic details, e.g. "B.Tech CSE · 2nd year") are
// optional and for display only; they do not affect matching.
class Student
{
public:
    // Throws ValidationError on an empty id/name/role, no offered skills, or a bad skill level.
    Student(const std::string& id, const std::string& name, const std::string& role,
            const SkillLevels& skillsOffered, const std::vector<std::string>& skillsWanted = {},
            const std::string& summary = {}, const std::string& program = {});

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& role() const { return role_; }
    const std::string& summary() const { return summary_; }
    const std::string& program() const { return program_; }
    const SkillLevels& skillsOffered() const { return skillsOffered_; }
    const std::set<std::string>& skillsWanted() const { return skillsWanted_; }

    // Proficiency in `skill` (any spelling), or 0 if it is not offered.
    int levelIn(std::string_view skill) const;
    bool offers(std::string_view skill, int minLevel = kMinSkillLevel) const;

private:
    std::string id_;
    std::string name_;
    std::string role_;
    SkillLevels skillsOffered_;
    std::set<std::string> skillsWanted_;
    std::string summary_;
    std::string program_;
};

// Students are the same person when their ids match.
bool operator==(const Student& lhs, const Student& rhs);
bool operator!=(const Student& lhs, const Student& rhs);
std::ostream& operator<<(std::ostream& out, const Student& student);

} // namespace teamforge
