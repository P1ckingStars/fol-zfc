#pragma once

#include <concepts>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

namespace logic::util {

// Concept: T has a get_key() const member function returning Key
template<typename T, typename Key>
concept HasGetKey = requires(const T& t) {
    { t.get_key() } -> std::convertible_to<Key>;
};

// Concept: H is a hash functor for Key returning size_t
template<typename H, typename Key>
concept KeyHasher = requires(const H& h, const Key& k) {
    { h(k) } -> std::convertible_to<size_t>;
};

// Registry for items with exact-match deduplication (no conflict possible)
template<typename T, typename Id, typename Hash>
class Registry {
public:
    Id register_item(T item) {
        auto it = item2id_.find(item);
        if (it != item2id_.end()) {
            return it->second;
        }
        Id id = next_id_++;
        id2item_.emplace(id, item);
        item2id_.emplace(std::move(item), id);
        return id;
    }

    const T& get(Id id) const {
        return id2item_.at(id);
    }

    std::optional<Id> find(const T& item) const {
        auto it = item2id_.find(item);
        return it != item2id_.end() ? std::optional{it->second} : std::nullopt;
    }

    bool contains(Id id) const {
        return id2item_.find(id) != id2item_.end();
    }

private:
    std::unordered_map<Id, T> id2item_;
    std::unordered_map<T, Id, Hash> item2id_;
    Id next_id_ = 1;
};

// Registry with key-based conflict detection
// T must have get_key() member function returning Key
// Items with same key but different values are conflicts
template<typename T, typename Id, typename Key, typename KeyHash>
    requires HasGetKey<T, Key> && KeyHasher<KeyHash, Key>
class KeyedRegistry {
public:
    struct ConflictError : std::runtime_error {
        const T* existing;
        ConflictError(const T* e, const std::string& msg)
            : std::runtime_error(msg), existing(e) {}
    };

    // Returns ID on success, throws ConflictError if same key but different item
    Id register_item(T item) {
        Key key = item.get_key();
        auto it = key2id_.find(key);
        if (it != key2id_.end()) {
            const T& existing = id2item_.at(it->second);
            if (existing == item) {
                return it->second;
            }
            throw ConflictError(&existing, "Conflicting registration: same key with different values");
        }
        Id id = next_id_++;
        id2item_.emplace(id, item);
        key2id_.emplace(std::move(key), id);
        return id;
    }

    // Non-throwing version
    std::variant<Id, const T*> try_register(T item) {
        Key key = item.get_key();
        auto it = key2id_.find(key);
        if (it != key2id_.end()) {
            const T& existing = id2item_.at(it->second);
            if (existing == item) {
                return it->second;
            }
            return &existing;
        }
        Id id = next_id_++;
        id2item_.emplace(id, item);
        key2id_.emplace(std::move(key), id);
        return id;
    }

    const T& get(Id id) const {
        return id2item_.at(id);
    }

    std::optional<Id> find_by_key(const Key& key) const {
        auto it = key2id_.find(key);
        return it != key2id_.end() ? std::optional{it->second} : std::nullopt;
    }

    bool contains(Id id) const {
        return id2item_.find(id) != id2item_.end();
    }

private:
    std::unordered_map<Id, T> id2item_;
    std::unordered_map<Key, Id, KeyHash> key2id_;
    Id next_id_ = 1;
};

}  // namespace logic::util
