#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

namespace logic::util {

// Forward declarations
template<typename T> class RegistryBase;
template<typename T> class Handle;

// Base class for all registries storing type T
// Provides the get_by_id interface that Handle uses
template<typename T>
class RegistryBase {
    friend class Handle<T>;

protected:
    virtual const T& get_by_id(size_t id) const = 0;

    // Create a handle - accessible to derived registry classes
    Handle<T> make_handle(size_t id) const {
        return Handle<T>(id, this);
    }

    // Access handle's id - accessible to derived registry classes
    static size_t get_id(const Handle<T>& h) { return h.id_; }

public:
    virtual ~RegistryBase() = default;
};

// Handle wraps an ID and registry reference.
// ID is only accessible by Registry classes - external code uses handles.
template<typename T>
class Handle {
    friend class RegistryBase<T>;

    size_t id_;
    const RegistryBase<T>* registry_;

    Handle(size_t id, const RegistryBase<T>* reg) : id_(id), registry_(reg) {}

public:
    Handle() : id_(0), registry_(nullptr) {}

    const T& get() const { return registry_->get_by_id(id_); }
    const T& operator*() const { return get(); }
    const T* operator->() const { return &get(); }

    bool valid() const { return id_ != 0 && registry_ != nullptr; }
    explicit operator bool() const { return valid(); }

    bool operator==(const Handle& other) const {
        return id_ == other.id_ && registry_ == other.registry_;
    }

    bool operator!=(const Handle& other) const {
        return !(*this == other);
    }

    // For use in hash maps - only accessible by std::hash specialization
    size_t hash_value() const { return id_; }
};

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
template<typename T, typename Hash>
class Registry : public RegistryBase<T> {
public:
    using handle_type = Handle<T>;

    handle_type register_item(T item) {
        auto it = item2id_.find(item);
        if (it != item2id_.end()) {
            return this->make_handle(it->second);
        }
        size_t id = next_id_++;
        id2item_.emplace(id, item);
        item2id_.emplace(std::move(item), id);
        return this->make_handle(id);
    }

    std::optional<handle_type> find(const T& item) const {
        auto it = item2id_.find(item);
        return it != item2id_.end() ? std::optional{this->make_handle(it->second)} : std::nullopt;
    }

    bool contains(const handle_type& h) const {
        return h.valid() && &h.get() == &get_by_id(this->get_id(h));
    }

    // Apply a function to all items (mutable access)
    // Used for rebinding handles after copy
    template<typename Fn>
    void for_each_mut(Fn&& fn) {
        for (auto& [id, item] : id2item_) {
            fn(item);
        }
    }

    // Rebuild item2id_ from id2item_
    // Call this after modifying items (e.g., rebinding handles after copy)
    void rebuild_index() {
        item2id_.clear();
        for (const auto& [id, item] : id2item_) {
            item2id_.emplace(item, id);
        }
    }

    // Const iteration over all (id, item) pairs
    template<typename Fn>
    void for_each(Fn&& fn) const {
        for (const auto& [id, item] : id2item_) {
            fn(id, item);
        }
    }

    // Current next ID (for delta computation)
    size_t next_id() const { return next_id_; }

protected:
    const T& get_by_id(size_t id) const override {
        return id2item_.at(id);
    }

private:
    std::unordered_map<size_t, T> id2item_;
    std::unordered_map<T, size_t, Hash> item2id_;
    size_t next_id_ = 1;
};

// Registry with key-based conflict detection
// T must have get_key() member function returning Key
// Items with same key but different values are conflicts
template<typename T, typename Id, typename Key, typename KeyHash>
    requires HasGetKey<T, Key> && KeyHasher<KeyHash, Key>
class KeyedRegistry : public RegistryBase<T> {
public:
    using handle_type = Handle<T>;

    struct ConflictError : std::runtime_error {
        const T* existing;
        ConflictError(const T* e, const std::string& msg)
            : std::runtime_error(msg), existing(e) {}
    };

    // Returns handle on success, throws ConflictError if same key but different item
    handle_type register_item(T item) {
        Key key = item.get_key();
        auto it = key2id_.find(key);
        if (it != key2id_.end()) {
            const T& existing = id2item_.at(it->second);
            if (existing == item) {
                return this->make_handle(it->second);
            }
            throw ConflictError(&existing, "Conflicting registration: same key with different values");
        }
        size_t id = next_id_++;
        id2item_.emplace(id, item);
        key2id_.emplace(std::move(key), id);
        return this->make_handle(id);
    }

    // Non-throwing version
    std::variant<handle_type, const T*> try_register(T item) {
        Key key = item.get_key();
        auto it = key2id_.find(key);
        if (it != key2id_.end()) {
            const T& existing = id2item_.at(it->second);
            if (existing == item) {
                return this->make_handle(it->second);
            }
            return &existing;
        }
        size_t id = next_id_++;
        id2item_.emplace(id, item);
        key2id_.emplace(std::move(key), id);
        return this->make_handle(id);
    }

    std::optional<handle_type> find_by_key(const Key& key) const {
        auto it = key2id_.find(key);
        return it != key2id_.end() ? std::optional{this->make_handle(it->second)} : std::nullopt;
    }

    bool contains(const handle_type& h) const {
        return h.valid() && &h.get() == &get_by_id(this->get_id(h));
    }

    // Const iteration over all (id, item) pairs
    template<typename Fn>
    void for_each(Fn&& fn) const {
        for (const auto& [id, item] : id2item_) {
            fn(id, item);
        }
    }

    // Current next ID (for delta computation)
    size_t next_id() const { return next_id_; }

protected:
    const T& get_by_id(size_t id) const override {
        return id2item_.at(id);
    }

private:
    std::unordered_map<size_t, T> id2item_;
    std::unordered_map<Key, size_t, KeyHash> key2id_;
    size_t next_id_ = 1;
};

// Simple indexed store without deduplication
// Each add() creates a new entry with unique ID
// Use for items where each instance is unique (e.g., proof steps)
template<typename T, typename Id>
class IndexedStore : public RegistryBase<T> {
public:
    using handle_type = Handle<T>;

    handle_type add(T item) {
        size_t id = next_id_++;
        items_.emplace(id, std::move(item));
        return this->make_handle(id);
    }

    T& get_mut(const handle_type& h) {
        return items_.at(this->get_id(h));
    }

    bool contains(const handle_type& h) const {
        return h.valid() && &h.get() == &get_by_id(this->get_id(h));
    }

    size_t size() const {
        return items_.size();
    }

    bool empty() const {
        return items_.empty();
    }

    // Iterator support for range-based for loops
    auto begin() const { return items_.begin(); }
    auto end() const { return items_.end(); }

protected:
    const T& get_by_id(size_t id) const override {
        return items_.at(id);
    }

private:
    std::unordered_map<size_t, T> items_;
    size_t next_id_ = 1;
};

}  // namespace logic::util

// Hash specialization for Handle - must be in std namespace
namespace std {
template<typename T>
struct hash<logic::util::Handle<T>> {
    size_t operator()(const logic::util::Handle<T>& h) const {
        return h.hash_value();
    }
};
}  // namespace std
