#pragma once

#include "core/Skill.h"

#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace teamforge {

// Optional profile details a participant confirmed, usually from their resume (core/ResumeAnalyzer.h).
// Display only: matching never reads them. Lists hold one entry per project, qualification,
// certification or achievement; `skillYears` is experience per offered skill and `totalYears`
// overall experience (-1 = not given).
struct ProfileDetails
{
    std::vector<std::string> projects;
    std::vector<std::string> education;
    std::vector<std::string> certifications;
    std::vector<std::string> achievements;
    std::map<std::string, double> skillYears;
    double totalYears = -1;

    bool empty() const;
};

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

    const ProfileDetails& details() const { return details_; }
    // Trims entries and drops empty ones; keeps at most 20 per list, 300 characters each. Years
    // for skills the student does not offer are dropped (skills can change). Throws
    // ValidationError for years below 0 or above 60.
    void setDetails(const ProfileDetails& details);

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
    ProfileDetails details_;
};

// Students are the same person when their ids match.
bool operator==(const Student& lhs, const Student& rhs);
bool operator!=(const Student& lhs, const Student& rhs);
std::ostream& operator<<(std::ostream& out, const Student& student);

} // namespace teamforge
