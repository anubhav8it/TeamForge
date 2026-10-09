#pragma once

#include "core/Errors.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace teamforge {

// In-memory store of entities keyed by id. T must provide `const std::string& id() const`.
// Backed by std::map (balanced tree, O(log n)), so values() comes back in id order and
// output stays deterministic.
template <typename T>
class Repository
{
public:
    explicit Repository(std::string entityName = "Item")
        : entityName_(std::move(entityName))
    {
    }

    // Throws DuplicateError if the id is taken.
    void add(T item)
    {
        std::string key = item.id();
        if (items_.count(key) != 0)
            throw DuplicateError(entityName_ + " '" + key + "' already exists");
        items_.emplace(std::move(key), std::move(item));
    }

    // Replaces the stored item with the same id. Throws NotFoundError if there is none.
    void update(T item)
    {
        const auto it = items_.find(item.id());
        if (it == items_.end())
            throw NotFoundError(entityName_ + " '" + item.id() + "' not found");
        it->second = std::move(item);
    }

    void remove(const std::string& id)
    {
        if (items_.erase(id) == 0)
            throw NotFoundError(entityName_ + " '" + id + "' not found");
    }

    const T& get(const std::string& id) const
    {
        const T *item = find(id);
        if (item == nullptr)
            throw NotFoundError(entityName_ + " '" + id + "' not found");
        return *item;
    }

    // nullptr if absent. The pointer stays valid until that item is removed or updated.
    const T *find(const std::string& id) const
    {
        const auto it = items_.find(id);
        return it == items_.end() ? nullptr : &it->second;
    }

    bool contains(const std::string& id) const { return items_.count(id) != 0; }
    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    std::vector<T> values() const
    {
        std::vector<T> result;
        result.reserve(items_.size());
        for (const auto& entry : items_)
            result.push_back(entry.second);
        return result;
    }

    template <typename Predicate>
    std::vector<T> filter(Predicate keep) const
    {
        std::vector<T> result;
        for (const auto& entry : items_) {
            if (keep(entry.second))
                result.push_back(entry.second);
        }
        return result;
    }

private:
    std::string entityName_;
    std::map<std::string, T> items_;
};

} // namespace teamforge
