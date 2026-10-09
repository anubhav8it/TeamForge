#include "core/SkillIndex.h"

namespace teamforge {

void SkillIndex::add(const Student& student)
{
    for (const auto& entry : student.skillsOffered())
        bySkill_[entry.first].insert(student.id());
}

void SkillIndex::remove(const Student& student)
{
    for (const auto& entry : student.skillsOffered()) {
        const auto it = bySkill_.find(entry.first);
        if (it == bySkill_.end())
            continue;
        it->second.erase(student.id());
        if (it->second.empty())
            bySkill_.erase(it);
    }
}

std::set<std::string> SkillIndex::studentsWith(std::string_view skill) const
{
    const auto it = bySkill_.find(normalizeSkill(skill));
    return it == bySkill_.end() ? std::set<std::string>{} : it->second;
}

std::set<std::string> SkillIndex::studentsWithAny(const std::vector<std::string>& skills) const
{
    std::set<std::string> result;
    for (const std::string& skill : skills) {
        const auto it = bySkill_.find(normalizeSkill(skill));
        if (it != bySkill_.end())
            result.insert(it->second.begin(), it->second.end());
    }
    return result;
}

bool SkillIndex::hasSkill(std::string_view skill) const
{
    return bySkill_.count(normalizeSkill(skill)) != 0;
}

} // namespace teamforge
