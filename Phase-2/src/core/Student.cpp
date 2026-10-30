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

bool ProfileDetails::empty() const
{
    return projects.empty() && education.empty() && certifications.empty() && achievements.empty()
           && skillYears.empty() && totalYears < 0;
}

namespace {

constexpr std::size_t kMaxDetailItems = 20;
constexpr std::size_t kMaxDetailLength = 300;
constexpr double kMaxYears = 60;

std::vector<std::string> cleanItems(const std::vector<std::string>& items)
{
    std::vector<std::string> result;
    for (const std::string& item : items) {
        std::string text = trimmed(item);
        if (text.size() > kMaxDetailLength) {
            std::size_t cut = kMaxDetailLength;
            while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                --cut; // keep UTF-8 characters whole
            text.resize(cut);
        }
        if (!text.empty() && result.size() < kMaxDetailItems)
            result.push_back(std::move(text));
    }
    return result;
}

void checkYears(double years, const std::string& what)
{
    if (!(years >= 0 && years <= kMaxYears))
        throw ValidationError(what + " must be between 0 and 60 years");
}

} // namespace

void Student::setDetails(const ProfileDetails& details)
{
    ProfileDetails clean;
    clean.projects = cleanItems(details.projects);
    clean.education = cleanItems(details.education);
    clean.certifications = cleanItems(details.certifications);
    clean.achievements = cleanItems(details.achievements);
    for (const auto& [skill, years] : details.skillYears) {
        const std::string key = normalizeSkill(skill);
        checkYears(years, "Experience in '" + key + "'");
        if (skillsOffered_.count(key) != 0)
            clean.skillYears[key] = years;
    }
    if (details.totalYears >= 0)
        checkYears(details.totalYears, "Total experience");
    clean.totalYears = details.totalYears < 0 ? -1 : details.totalYears;
    details_ = std::move(clean);
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
