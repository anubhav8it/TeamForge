#include "core/SkillBST.h"

#include <algorithm>
#include <cstdlib>

namespace teamforge {

void SkillBST::insert(const std::string& skill)
{
    bool added = false;
    root_ = insertAt(std::move(root_), skill, added);
    if (added)
        ++size_;
}

bool SkillBST::remove(const std::string& skill)
{
    bool found = false;
    bool erased = false;
    root_ = removeAt(std::move(root_), skill, found, erased);
    if (erased)
        --size_;
    return found;
}

int SkillBST::count(const std::string& skill) const
{
    const Node *node = findNode(skill);
    return node == nullptr ? 0 : node->count;
}

int SkillBST::height() const
{
    return heightOf(root_.get());
}

std::vector<std::pair<std::string, int>> SkillBST::inOrder() const
{
    std::vector<std::pair<std::string, int>> result;
    result.reserve(size_);
    collectInOrder(root_.get(), result);
    return result;
}

std::vector<std::string> SkillBST::withPrefix(const std::string& prefix) const
{
    std::vector<std::string> result;
    collectPrefix(root_.get(), prefix, result);
    return result;
}

bool SkillBST::isValidAvl() const
{
    return checkAvl(root_.get(), nullptr, nullptr) >= 0;
}

const SkillBST::Node *SkillBST::findNode(const std::string& skill) const
{
    const Node *node = root_.get();
    while (node != nullptr) {
        if (skill < node->key)
            node = node->left.get();
        else if (node->key < skill)
            node = node->right.get();
        else
            return node;
    }
    return nullptr;
}

int SkillBST::heightOf(const Node *node)
{
    return node == nullptr ? 0 : node->height;
}

void SkillBST::updateHeight(Node& node)
{
    node.height = 1 + std::max(heightOf(node.left.get()), heightOf(node.right.get()));
}

SkillBST::NodePtr SkillBST::rotateRight(NodePtr node)
{
    NodePtr pivot = std::move(node->left);
    node->left = std::move(pivot->right);
    updateHeight(*node);
    pivot->right = std::move(node);
    updateHeight(*pivot);
    return pivot;
}

SkillBST::NodePtr SkillBST::rotateLeft(NodePtr node)
{
    NodePtr pivot = std::move(node->right);
    node->right = std::move(pivot->left);
    updateHeight(*node);
    pivot->left = std::move(node);
    updateHeight(*pivot);
    return pivot;
}

SkillBST::NodePtr SkillBST::rebalance(NodePtr node)
{
    updateHeight(*node);
    const int balance = heightOf(node->left.get()) - heightOf(node->right.get());
    if (balance > 1) {
        // Left-right case: rotate the child first so a single right rotation fixes it.
        if (heightOf(node->left->left.get()) < heightOf(node->left->right.get()))
            node->left = rotateLeft(std::move(node->left));
        return rotateRight(std::move(node));
    }
    if (balance < -1) {
        if (heightOf(node->right->right.get()) < heightOf(node->right->left.get()))
            node->right = rotateRight(std::move(node->right));
        return rotateLeft(std::move(node));
    }
    return node;
}

SkillBST::NodePtr SkillBST::insertAt(NodePtr node, const std::string& skill, bool& added)
{
    if (!node) {
        added = true;
        return std::make_unique<Node>(skill);
    }
    if (skill < node->key) {
        node->left = insertAt(std::move(node->left), skill, added);
    } else if (node->key < skill) {
        node->right = insertAt(std::move(node->right), skill, added);
    } else {
        ++node->count;
        return node;
    }
    return rebalance(std::move(node));
}

SkillBST::NodePtr SkillBST::detachMin(NodePtr node, NodePtr& min)
{
    if (!node->left) {
        NodePtr right = std::move(node->right);
        min = std::move(node);
        return right;
    }
    node->left = detachMin(std::move(node->left), min);
    return rebalance(std::move(node));
}

SkillBST::NodePtr SkillBST::removeAt(NodePtr node, const std::string& skill, bool& found, bool& erased)
{
    if (!node)
        return nullptr;
    if (skill < node->key) {
        node->left = removeAt(std::move(node->left), skill, found, erased);
    } else if (node->key < skill) {
        node->right = removeAt(std::move(node->right), skill, found, erased);
    } else {
        found = true;
        if (node->count > 1) {
            --node->count;
            return node;
        }
        erased = true;
        if (!node->left)
            return std::move(node->right);
        if (!node->right)
            return std::move(node->left);
        // Two children: the in-order successor (minimum of the right subtree) takes its place.
        NodePtr successor;
        NodePtr rest = detachMin(std::move(node->right), successor);
        successor->left = std::move(node->left);
        successor->right = std::move(rest);
        node = std::move(successor);
    }
    return rebalance(std::move(node));
}

void SkillBST::collectInOrder(const Node *node, std::vector<std::pair<std::string, int>>& out)
{
    if (node == nullptr)
        return;
    collectInOrder(node->left.get(), out);
    out.emplace_back(node->key, node->count);
    collectInOrder(node->right.get(), out);
}

void SkillBST::collectPrefix(const Node *node, const std::string& prefix, std::vector<std::string>& out)
{
    if (node == nullptr)
        return;
    const bool hasPrefix = node->key.compare(0, prefix.size(), prefix) == 0;
    // Keys with the prefix form one contiguous range starting at `prefix`. A key greater than
    // `prefix` without it lies past that range, so its right subtree cannot match.
    if (node->key > prefix)
        collectPrefix(node->left.get(), prefix, out);
    if (hasPrefix)
        out.push_back(node->key);
    if (node->key < prefix || hasPrefix)
        collectPrefix(node->right.get(), prefix, out);
}

int SkillBST::checkAvl(const Node *node, const std::string *low, const std::string *high)
{
    if (node == nullptr)
        return 0;
    if ((low != nullptr && !(*low < node->key)) || (high != nullptr && !(node->key < *high)))
        return -1;
    const int left = checkAvl(node->left.get(), low, &node->key);
    const int right = checkAvl(node->right.get(), &node->key, high);
    if (left < 0 || right < 0 || std::abs(left - right) > 1)
        return -1;
    const int height = 1 + std::max(left, right);
    return height == node->height ? height : -1;
}

} // namespace teamforge
