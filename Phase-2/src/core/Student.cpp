#include "core/Student.h"

#include "core/Errors.h"
#include "core/Validation.h"

#include <ostream>

namespace teamforge {

Student::Student(const std::string& id, const std::string& name, const std::string& role,
                 const SkillLevels& skillsOffered, const std::vector<std::string>& skillsWanted,
                 const std::string& summary, const std::string& program)
    : id_(requireNonEmpty(id, "Student id"))
    , name_(requireNonEmpty(name, "Student name"))
    , role_(requireNonEmpty(role, "Student role"))
    , summary_(trimmed(summary))
    , program_(trimmed(program))
{
    const std::string owner = "Student '" + id_ + "'";
    skillsOffered_ = normalizeSkillLevels(skillsOffered, owner);
    if (skillsOffered_.empty())
        throw ValidationError(owner + " must offer at least one skill");
    for (const std::string& skill : skillsWanted)
        skillsWanted_.insert(normalizeSkill(skill));
}

int Student::levelIn(std::string_view skill) const
{
    const auto it = skillsOffered_.find(normalizeSkill(skill));
    return it == skillsOffered_.end() ? 0 : it->second;
}

bool Student::offers(std::string_view skill, int minLevel) const
{
    return levelIn(skill) >= minLevel;
}

bool operator==(const Student& lhs, const Student& rhs)
{
    return lhs.id() == rhs.id();
}

bool operator!=(const Student& lhs, const Student& rhs)
{
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& out, const Student& student)
{
    return out << student.name() << " (" << student.id() << ", " << student.role() << ")";
}

} // namespace teamforge
