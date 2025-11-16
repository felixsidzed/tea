#pragma once

#include "vector.h"

#include <utility>
#include <initializer_list>

namespace tea {
	/// <summary>
	/// Small ordered map
	/// </summary>
	/// <typeparam name="K">Key type</typeparam>
	/// <typeparam name="V">Value type</typeparam>
	template<typename K, typename V>
	class map {
	public:
		using element = std::pair<K, V>;

		vector<element> data;

		map() = default;
		map(std::initializer_list<element> init) {
			for (const auto& p : init)
				insert(p.first, p.second);
		}

		void insert(const K& key, const V& value) {
			if (auto* v = find(key))
				return;
			data.emplace(key, value);
		}

		void insert(K&& key, V&& value) {
			if (auto* v = find(key))
				return;
			data.emplace(std::move(key), std::move(value));
		}

		void insert(const K& key, V&& value) {
			if (auto* v = find(key))
				return;
			data.emplace(key, std::move(value));
		}

		V* find(const K& key) {
			for (uint32_t i = 0; i < data.size; i++)
				if (data[i].first == key)
					return &data[i].second;
			return nullptr;
		}

		const V* find(const K& key) const {
			for (uint32_t i = 0; i < data.size; i++)
				if (data[i].first == key)
					return &data[i].second;
			return nullptr;
		}

		bool contains(const K& key) const {
			return find(key) != nullptr;
		}

		void erase(const K& key) {
			for (uint32_t i = 0; i < data.size; ++i) {
				if (data[i].first == key) {
					data[i] = std::move(data[data.size - 1]);
					data.pop();
					return;
				}
			}
		}

		bool empty() const { return data.empty(); }

		uint32_t size() const { return data.size; }

		element* end() { return data.end(); }
		element* begin() { return data.begin(); }

		const element* end() const { return data.end(); }
		const element* begin() const { return data.begin(); }

		V& operator[](const K& key) {
			if (auto v = find(key))
				return *v;
			data.emplace(key, V{});
			return data[data.size - 1].second;
		}

		V& operator[](K&& key) {
			if (auto* v = find(key))
				return *v;
			data.emplace(std::move(key), V{});
			return data[data.size - 1].second;
		}
	};
}
