#pragma once

#include "core/Student.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace teamforge {

struct SkillDistance
{
    std::string skill;
    int distance; // hops from the start skill
};

bool operator==(const SkillDistance& lhs, const SkillDistance& rhs);

// Undirected skill co-occurrence graph built from profiles: an edge joins two skills offered by
// the same student, weighted by how many students offer both. BFS over it finds related skills,
// e.g. what to look for when nobody offers a required skill directly. Distances count hops and
// ignore weights.
class SkillGraph
{
public:
    void addStudent(const Student& student);
    // `student` must be the same version that was added.
    void removeStudent(const Student& student);

    bool hasSkill(std::string_view skill) const;
    int edgeWeight(std::string_view a, std::string_view b) const; // 0 if not adjacent
    std::size_t skillCount() const { return adjacency_.size(); }
    std::size_t edgeCount() const;

    // Every skill within `maxDepth` hops of `skill` (excluding it), ordered by distance then name.
    std::vector<SkillDistance> relatedSkills(std::string_view skill, int maxDepth) const;
    // Fewest-hop path from `from` to `to`, both ends included; empty if unreachable.
    std::vector<std::string> shortestPath(std::string_view from, std::string_view to) const;

private:
    void link(const std::string& a, const std::string& b, int delta);

    // Inner map is ordered so BFS visits neighbours deterministically.
    std::unordered_map<std::string, std::map<std::string, int>> adjacency_;
    // Students offering each skill; the node is dropped when this reaches 0.
    std::unordered_map<std::string, int> studentCount_;
};

} // namespace teamforge
