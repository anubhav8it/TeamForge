#include "core/Skill.h"

#include "core/Errors.h"

#include <cctype>

namespace teamforge {

std::string normalizeSkill(std::string_view name)
{
    std::string result;
    result.reserve(name.size());
    bool pendingSpace = false;
    for (char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isspace(u)) {
            pendingSpace = !result.empty();
            continue;
        }
        if (pendingSpace) {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(static_cast<char>(std::tolower(u)));
    }
    if (result.empty())
        throw ValidationError("Skill name must not be empty");
    return result;
}

SkillLevels normalizeSkillLevels(const SkillLevels& raw, std::string_view owner)
{
    SkillLevels result;
    for (const auto& [name, level] : raw) {
        std::string key = normalizeSkill(name);
        if (level < kMinSkillLevel || level > kMaxSkillLevel) {
            throw ValidationError(std::string(owner) + ": level for '" + key + "' must be between "
                                  + std::to_string(kMinSkillLevel) + " and "
                                  + std::to_string(kMaxSkillLevel) + ", got "
                                  + std::to_string(level));
        }
        if (!result.emplace(key, level).second)
            throw ValidationError(std::string(owner) + ": skill '" + key + "' is listed more than once");
    }
    return result;
}

} // namespace teamforge
