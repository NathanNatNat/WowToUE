/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <unordered_map>
#include <variant>
#include <vector>
#include <string>

/**
 * A map that supports multiple values per key.
 * When a single value is stored for a key, it is stored directly.
 * When additional values are added for the same key, they are grouped into a vector.
 *
 * Mirrors the JS MultiMap class that extends Map with multi-value set() semantics.
 */
template <typename Key, typename Value>
class MultiMap {
public:
	using SingleOrMulti = std::variant<Value, std::vector<Value>>;

	/**
	 * Construct a new multi-value map.
	 */
	MultiMap() = default;

	/**
	 * Set a value for a specific key in this map.
	 * If the key already exists with a single value, converts to a vector.
	 * If the key already exists with a vector, appends to it.
	 * @param key
	 * @param value
	 */
	void set(const Key& key, const Value& value) {
		auto it = _data.find(key);
		if (it != _data.end()) {
			auto& entry = it->second;
			if (auto* vec = std::get_if<std::vector<Value>>(&entry)) {
				vec->push_back(value);
			} else {
				Value existing = std::get<Value>(entry);
				entry = std::vector<Value>{std::move(existing), value};
			}
		} else {
			_data.emplace(key, SingleOrMulti{value});
		}
	}

	/**
	 * Get the value(s) for a specific key.
	 * @param key
	 * @return Pointer to the variant, or nullptr if not found.
	 */
	const SingleOrMulti* get(const Key& key) const {
		auto it = _data.find(key);
		if (it != _data.end())
			return &it->second;
		return nullptr;
	}

	/**
	 * Check if a key exists in this map.
	 * @param key
	 * @return true if the key exists
	 */
	bool has(const Key& key) const {
		return _data.contains(key);
	}

	/**
	 * Delete a key from this map.
	 * @param key
	 * @return true if the key was found and removed
	 */
	bool erase(const Key& key) {
		return _data.erase(key) > 0;
	}

	/**
	 * Get the number of keys in this map.
	 * @return size
	 */
	std::size_t size() const {
		return _data.size();
	}

	/**
	 * Clear all entries from this map.
	 */
	void clear() {
		_data.clear();
	}

	/**
	 * Iterate over all key-value entries.
	 */
	auto begin() const { return _data.begin(); }
	auto end() const { return _data.end(); }
	auto begin() { return _data.begin(); }
	auto end() { return _data.end(); }

private:
	std::unordered_map<Key, SingleOrMulti> _data;
};
