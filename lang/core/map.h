#pragma once

#include "vector.h"

#include <utility>
#include <initializer_list>
#include <functional>

namespace tea {
	// TODO: maybe not hash key sometimes
	template<typename K, typename V>
	class map {
	public:
		using element = std::pair<K, V>;

		vector<element> data;
		vector<int> buckets;
		uint32_t bmask = 0;

	private:
		static size_t hashKey(const K& k) { return std::hash<K>{}(k); }

		void ensure() { if (buckets.size == 0) rehash(8); }

		void rehash(uint32_t newBucketCount) {
			uint32_t n = 1;
			while (n < newBucketCount) n *= 2;

			vector<int> newBuckets(n, -1);
			uint32_t mask = n - 1;

			for (uint32_t i = 0; i < data.size; i++) {
				uint32_t idx = (uint32_t)(hashKey(data[i].first) & mask);
				while (newBuckets[idx] != -1)
					idx = (idx + 1) & mask;
				newBuckets[idx] = (int)i;
			}

			buckets = std::move(newBuckets);
			bmask = mask;
		}

		void grow() {
			if ((uint64_t)(data.size + 1) * 10 > (uint64_t)(bmask + 1) * 7)
				rehash((bmask + 1) * 2);
		}

		int findBucket(const K& key) const {
			if (buckets.size == 0) return -1;
			uint32_t idx = (uint32_t)(hashKey(key) & bmask);
			uint32_t start = idx;
			while (buckets[idx] != -1) {
				if (data[buckets[idx]].first == key) return (int)idx;
				idx = (idx + 1) & bmask;
				if (idx == start) break;
			}
			return -1;
		}

		void insertBucket(uint32_t dataIdx) {
			uint32_t idx = (uint32_t)(hashKey(data[dataIdx].first) & bmask);
			while (buckets[idx] != -1) idx = (idx + 1) & bmask;
			buckets[idx] = (int)dataIdx;
		}

	public:
		map() = default;
		map(std::initializer_list<element> init) {
			for (const auto& p : init)
				insert(p.first, p.second);
		}

		void insert(const K& key, const V& value) {
			if (contains(key)) return;
			ensure(); grow();
			data.emplace(key, value);
			insertBucket(data.size - 1);
		}
		void insert(K&& key, V&& value) {
			if (contains(key)) return;
			ensure(); grow();
			data.emplace(std::move(key), std::move(value));
			insertBucket(data.size - 1);
		}
		void insert(const K& key, V&& value) {
			if (contains(key)) return;
			ensure(); grow();
			data.emplace(key, std::move(value));
			insertBucket(data.size - 1);
		}

		V* find(const K& key) {
			int b = findBucket(key);
			return b < 0 ? nullptr : &data[buckets[b]].second;
		}
		const V* find(const K& key) const {
			int b = findBucket(key);
			return b < 0 ? nullptr : &data[buckets[b]].second;
		}

		bool contains(const K& key) const {
			return find(key) != nullptr;
		}

		void erase(const K& key) {
			int b = findBucket(key);
			if (b < 0) return;

			uint32_t remi = buckets[b];
			uint32_t lastDataIdx = data.size - 1;
			buckets[b] = -1;

			if (remi != lastDataIdx) {
				uint32_t idx = (uint32_t)(hashKey(data[lastDataIdx].first) & bmask);
				while (buckets[idx] != (int)lastDataIdx)
					idx = (idx + 1) & bmask;
				buckets[idx] = (int)remi;
				data[remi] = std::move(data[lastDataIdx]);
			}
			data.pop();

			uint32_t next = ((uint32_t)b + 1) & bmask;
			while (buckets[next] != -1) {
				int bleedbleedbleed = buckets[next];
				buckets[next] = -1;
				insertBucket((uint32_t)bleedbleedbleed);
				next = (next + 1) & bmask;
			}
		}

		bool empty() const { return data.empty(); }
		uint32_t size() const { return data.size; }

		element* begin() { return data.begin(); }
		element* end() { return data.end(); }
		const element* begin() const { return data.begin(); }
		const element* end() const { return data.end(); }

		V& operator[](const K& key) {
			if (auto v = find(key))
				return *v;
			ensure(); grow();
			data.emplace(key, V{});
			insertBucket(data.size - 1);
			return data[data.size - 1].second;
		}
		V& operator[](K&& key) {
			if (auto* v = find(key))
				return *v;
			ensure(); grow();
			data.emplace(std::move(key), V{});
			insertBucket(data.size - 1);
			return data[data.size - 1].second;
		}

		void clear() {
			buckets.clear();
			data.clear();
			bmask = 0;
		}
	};
}
