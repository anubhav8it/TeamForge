#include "core/SkillGraph.h"

#include <algorithm>
#include <queue>

namespace teamforge {

bool operator==(const SkillDistance& lhs, const SkillDistance& rhs)
{
    return lhs.skill == rhs.skill && lhs.distance == rhs.distance;
}

namespace {

std::vector<std::string> skillKeys(const Student& student)
{
    std::vector<std::string> keys;
    keys.reserve(student.skillsOffered().size());
    for (const auto& entry : student.skillsOffered())
        keys.push_back(entry.first);
    return keys;
}

} // namespace

void SkillGraph::addStudent(const Student& student)
{
    const std::vector<std::string> skills = skillKeys(student);
    for (const std::string& skill : skills) {
        ++studentCount_[skill];
        adjacency_[skill]; // a student with one skill still adds the node
    }
    for (std::size_t i = 0; i < skills.size(); ++i) {
        for (std::size_t j = i + 1; j < skills.size(); ++j)
            link(skills[i], skills[j], +1);
    }
}

void SkillGraph::removeStudent(const Student& student)
{
    const std::vector<std::string> skills = skillKeys(student);
    for (std::size_t i = 0; i < skills.size(); ++i) {
        for (std::size_t j = i + 1; j < skills.size(); ++j)
            link(skills[i], skills[j], -1);
    }
    for (const std::string& skill : skills) {
        const auto it = studentCount_.find(skill);
        if (it == studentCount_.end())
            continue;
        if (--it->second == 0) {
            studentCount_.erase(it);
            adjacency_.erase(skill); // no student offers it, so it has no edges left
        }
    }
}

void SkillGraph::link(const std::string& a, const std::string& b, int delta)
{
    int& weight = adjacency_[a][b];
    weight += delta;
    if (weight <= 0) {
        adjacency_[a].erase(b);
        adjacency_[b].erase(a);
    } else {
        adjacency_[b][a] = weight;
    }
}

bool SkillGraph::hasSkill(std::string_view skill) const
{
    return adjacency_.count(normalizeSkill(skill)) != 0;
}

int SkillGraph::edgeWeight(std::string_view a, std::string_view b) const
{
    const auto node = adjacency_.find(normalizeSkill(a));
    if (node == adjacency_.end())
        return 0;
    const auto edge = node->second.find(normalizeSkill(b));
    return edge == node->second.end() ? 0 : edge->second;
}

std::size_t SkillGraph::edgeCount() const
{
    std::size_t endpoints = 0;
    for (const auto& node : adjacency_)
        endpoints += node.second.size();
    return endpoints / 2;
}

std::vector<SkillDistance> SkillGraph::relatedSkills(std::string_view skill, int maxDepth) const
{
    std::vector<SkillDistance> result;
    const std::string start = normalizeSkill(skill);
    if (maxDepth < 1 || adjacency_.count(start) == 0)
        return result;

    std::unordered_map<std::string, int> distance{{start, 0}};
    std::queue<std::string> frontier;
    frontier.push(start);
    while (!frontier.empty()) {
        const std::string current = frontier.front();
        frontier.pop();
        const int d = distance[current];
        if (d == maxDepth)
            continue;
        for (const auto& edge : adjacency_.at(current)) {
            const std::string& next = edge.first;
            if (distance.count(next) != 0)
                continue;
            distance[next] = d + 1;
            result.push_back({next, d + 1});
            frontier.push(next);
        }
    }
    // BFS already yields non-decreasing distance; sort names within each distance.
    std::stable_sort(result.begin(), result.end(), [](const SkillDistance& a, const SkillDistance& b) {
        return a.distance != b.distance ? a.distance < b.distance : a.skill < b.skill;
    });
    return result;
}

std::vector<std::string> SkillGraph::shortestPath(std::string_view from, std::string_view to) const
{
    const std::string start = normalizeSkill(from);
    const std::string goal = normalizeSkill(to);
    if (adjacency_.count(start) == 0 || adjacency_.count(goal) == 0)
        return {};

    std::unordered_map<std::string, std::string> parent{{start, start}};
    std::queue<std::string> frontier;
    frontier.push(start);
    while (!frontier.empty() && parent.count(goal) == 0) {
        const std::string current = frontier.front();
        frontier.pop();
        for (const auto& edge : adjacency_.at(current)) {
            if (parent.count(edge.first) != 0)
                continue;
            parent[edge.first] = current;
            frontier.push(edge.first);
        }
    }
    if (parent.count(goal) == 0)
        return {};

    std::vector<std::string> path{goal};
    while (path.back() != start)
        path.push_back(parent.at(path.back()));
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace teamforge
