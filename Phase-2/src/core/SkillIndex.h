#pragma once

#include "core/Student.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace teamforge {

// Inverted index skill -> ids of students offering it (at any level). Candidate retrieval is
// average O(1) per required skill instead of scanning every profile.
class SkillIndex
{
public:
    void add(const Student& student);
    // `student` must be the same version that was added.
    void remove(const Student& student);

    std::set<std::string> studentsWith(std::string_view skill) const;
    // Union over `skills`: everyone who offers at least one of them.
    std::set<std::string> studentsWithAny(const std::vector<std::string>& skills) const;

    bool hasSkill(std::string_view skill) const;
    std::size_t skillCount() const { return bySkill_.size(); }

private:
    std::unordered_map<std::string, std::set<std::string>> bySkill_;
};

} // namespace teamforge
