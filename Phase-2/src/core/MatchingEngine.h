#pragma once

#include "core/MatchResult.h"
#include "core/MatchingStrategy.h"
#include "core/ProjectRequirement.h"
#include "core/Repository.h"
#include "core/SkillBST.h"
#include "core/SkillGraph.h"
#include "core/SkillIndex.h"
#include "core/Student.h"
#include "core/Team.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace teamforge {

// Facade the UI layer talks to. Owns the student repository and keeps the three skill
// structures (hash index, AVL tree, co-occurrence graph) in sync with it, then runs the
// pipeline: retrieve candidates -> score with a strategy -> rank top-k -> build a team.
class MatchingEngine
{
public:
    void addStudent(const Student& student);    // DuplicateError if the id exists
    void updateStudent(const Student& student); // NotFoundError if it does not
    void removeStudent(const std::string& studentId);

    const Repository<Student>& students() const { return students_; }
    const SkillIndex& skillIndex() const { return index_; }
    const SkillBST& skillTree() const { return skillTree_; }
    const SkillGraph& skillGraph() const { return graph_; }

    // Students offering at least one required skill (at any level), excluding team members.
    // Pointers stay valid until the student set changes.
    std::vector<const Student *> retrieveCandidates(const Team& team) const;

    std::vector<MatchResult> rank(const Team& team, const MatchingStrategy& strategy, std::size_t k) const;

    // Greedy team builder: repeatedly adds the top-ranked candidate until the team is full, or
    // it covers every required skill and meets the minimum size, or no candidates remain.
    // Fast and explainable, but not guaranteed optimal: an early pick can block a better set.
    Team suggestTeam(const std::string& teamId, const ProjectRequirement& requirement,
                     const MatchingStrategy& strategy) const;

    std::vector<SkillDistance> relatedSkills(std::string_view skill, int maxDepth) const;
    // Known skills starting with `prefix` (any spelling), ascending; all skills if blank.
    std::vector<std::string> skillsWithPrefix(std::string_view prefix) const;

    // Maintenance: rebuild one structure from scratch from the student repository. The add /
    // update / remove paths keep all three in sync, so these only matter for diagnostics
    // (the developer workspace) and must leave the structure equal to the incremental one.
    void rebuildSkillIndex();
    void rebuildSkillTree();
    void rebuildSkillGraph();

private:
    void indexStudent(const Student& student);
    void unindexStudent(const Student& student);

    Repository<Student> students_{"Student"};
    SkillIndex index_;
    SkillBST skillTree_;
    SkillGraph graph_;
};

} // namespace teamforge
