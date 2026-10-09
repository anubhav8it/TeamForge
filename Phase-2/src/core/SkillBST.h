#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace teamforge {

// Ordered multiset of skill names: each node keeps how many students offer that skill.
// AVL-balanced, so insert/remove/lookup stay O(log n) even when skills arrive in sorted order,
// which would degrade a plain BST to a linked list. Used for the sorted skill list and prefix
// search (autocomplete). Expects already-normalised names.
class SkillBST
{
public:
    SkillBST() = default;
    SkillBST(const SkillBST&) = delete;
    SkillBST& operator=(const SkillBST&) = delete;
    SkillBST(SkillBST&&) noexcept = default;
    SkillBST& operator=(SkillBST&&) noexcept = default;
    ~SkillBST() = default;

    // Adds the skill, or increments its count.
    void insert(const std::string& skill);
    // Decrements the count and deletes the node at zero. Returns false if the skill is absent.
    bool remove(const std::string& skill);

    bool contains(const std::string& skill) const { return findNode(skill) != nullptr; }
    int count(const std::string& skill) const;
    std::size_t size() const { return size_; } // distinct skills
    int height() const;

    // (skill, count) in ascending order.
    std::vector<std::pair<std::string, int>> inOrder() const;
    // Skills starting with `prefix`, ascending; visits only subtrees that can contain matches.
    std::vector<std::string> withPrefix(const std::string& prefix) const;

    // Checks BST ordering, stored heights and the AVL balance condition. For tests.
    bool isValidAvl() const;

private:
    struct Node
    {
        explicit Node(std::string k)
            : key(std::move(k))
        {
        }
        std::string key;
        int count = 1;
        int height = 1;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };
    using NodePtr = std::unique_ptr<Node>;

    static int heightOf(const Node *node);
    static void updateHeight(Node& node);
    static NodePtr rotateLeft(NodePtr node);
    static NodePtr rotateRight(NodePtr node);
    static NodePtr rebalance(NodePtr node);
    static NodePtr insertAt(NodePtr node, const std::string& skill, bool& added);
    static NodePtr removeAt(NodePtr node, const std::string& skill, bool& found, bool& erased);
    static NodePtr detachMin(NodePtr node, NodePtr& min);
    static void collectInOrder(const Node *node, std::vector<std::pair<std::string, int>>& out);
    static void collectPrefix(const Node *node, const std::string& prefix, std::vector<std::string>& out);
    static int checkAvl(const Node *node, const std::string *low, const std::string *high);

    const Node *findNode(const std::string& skill) const;

    NodePtr root_;
    std::size_t size_ = 0;
};

} // namespace teamforge
