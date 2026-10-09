#include "core/MatchingEngine.h"

#include "core/Ranker.h"
#include "core/Validation.h"

namespace teamforge {

void MatchingEngine::addStudent(const Student& student)
{
    students_.add(student);
    indexStudent(student);
}

void MatchingEngine::updateStudent(const Student& student)
{
    const Student& previous = students_.get(student.id());
    unindexStudent(previous);
    students_.update(student);
    indexStudent(student);
}

void MatchingEngine::removeStudent(const std::string& studentId)
{
    unindexStudent(students_.get(studentId));
    students_.remove(studentId);
}

void MatchingEngine::indexStudent(const Student& student)
{
    index_.add(student);
    graph_.addStudent(student);
    for (const auto& entry : student.skillsOffered())
        skillTree_.insert(entry.first);
}

void MatchingEngine::unindexStudent(const Student& student)
{
    index_.remove(student);
    graph_.removeStudent(student);
    for (const auto& entry : student.skillsOffered())
        skillTree_.remove(entry.first);
}

std::vector<const Student *> MatchingEngine::retrieveCandidates(const Team& team) const
{
    std::vector<const Student *> candidates;
    for (const std::string& id : index_.studentsWithAny(team.requirement().skillNames())) {
        if (!team.contains(id))
            candidates.push_back(students_.find(id));
    }
    return candidates;
}

std::vector<MatchResult> MatchingEngine::rank(const Team& team, const MatchingStrategy& strategy,
                                              std::size_t k) const
{
    return rankTopK(retrieveCandidates(team), team, strategy, k);
}

Team MatchingEngine::suggestTeam(const std::string& teamId, const ProjectRequirement& requirement,
                                 const MatchingStrategy& strategy) const
{
    Team team(teamId, requirement);
    while (!team.isFull()) {
        if (team.size() >= requirement.minTeamSize() && team.missingSkills().empty())
            break;
        const std::vector<MatchResult> best = rank(team, strategy, 1);
        if (best.empty())
            break;
        team.addMember(students_.get(best.front().studentId));
    }
    return team;
}

std::vector<SkillDistance> MatchingEngine::relatedSkills(std::string_view skill, int maxDepth) const
{
    return graph_.relatedSkills(skill, maxDepth);
}

std::vector<std::string> MatchingEngine::skillsWithPrefix(std::string_view prefix) const
{
    return skillTree_.withPrefix(trimmed(prefix).empty() ? std::string() : normalizeSkill(prefix));
}

void MatchingEngine::rebuildSkillIndex()
{
    SkillIndex fresh;
    for (const Student& student : students_.values())
        fresh.add(student);
    index_ = std::move(fresh);
}

void MatchingEngine::rebuildSkillTree()
{
    SkillBST fresh;
    for (const Student& student : students_.values()) {
        for (const auto& entry : student.skillsOffered())
            fresh.insert(entry.first);
    }
    skillTree_ = std::move(fresh);
}

void MatchingEngine::rebuildSkillGraph()
{
    SkillGraph fresh;
    for (const Student& student : students_.values())
        fresh.addStudent(student);
    graph_ = std::move(fresh);
}

} // namespace teamforge
