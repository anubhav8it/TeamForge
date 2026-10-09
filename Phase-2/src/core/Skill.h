#pragma once

#include <map>
#include <string>
#include <string_view>

namespace teamforge {

constexpr int kMinSkillLevel = 1;
constexpr int kMaxSkillLevel = 5;

// Normalised skill name -> level. For a student it is proficiency; for a requirement it is the
// minimum level that counts as covering the skill.
using SkillLevels = std::map<std::string, int>;

// Canonical key used everywhere skills are compared: trimmed, inner whitespace collapsed to one
// space, ASCII lowercase. "  Data   Analysis" -> "data analysis". Throws ValidationError if empty.
std::string normalizeSkill(std::string_view name);

// Normalises every key and checks each level is within [kMinSkillLevel, kMaxSkillLevel].
// Throws ValidationError (prefixed with `owner`) on a bad level or two spellings of one skill.
SkillLevels normalizeSkillLevels(const SkillLevels& raw, std::string_view owner);

} // namespace teamforge
