#pragma once

#include <new>
#include <cstdint>
#include <cstring>
#include <utility>
#include <type_traits>
#include <initializer_list>

#ifndef TEA_VECTOR_MIN_CAPACITY
#define TEA_VECTOR_MIN_CAPACITY 4
#endif

namespace tea {
	template<typename T>
	class vector {
	public:
		T* data;
		uint32_t size;
		uint32_t capacity;

		vector(uint32_t initCap = TEA_VECTOR_MIN_CAPACITY)
			: data(initCap ? (T*)::operator new(initCap * sizeof(T)) : nullptr), size(0), capacity(initCap) {
		}

		vector(vector&& other) noexcept : data(other.data), size(other.size), capacity(other.capacity) {
			other.data = nullptr;
			other.size = 0;
			other.capacity = 0;
		}

		template<typename U = T, typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
		vector(const std::initializer_list<T>& init)
			: data((T*)::operator new(init.size() * sizeof(T))), size((uint32_t)init.size()), capacity((uint32_t)init.size())
		{
			uint32_t i = 0;
			for (const T& item : init)
				new (&data[i++]) T(item);
		}

		vector(const T* odata, uint32_t osize)
			: data((T*)::operator new(osize * sizeof(T))), size(osize), capacity(osize)
		{
			copyRange(data, odata, osize);
		}

		vector(const vector& other)
			: data((T*)::operator new(other.size * sizeof(T))), size(other.size), capacity(other.size)
		{
			copyRange(data, other.data, other.size);
		}

		vector(uint32_t n, const T& value)
			: data((T*)::operator new(n * sizeof(T))), size(n), capacity(n)
		{
			for (uint32_t i = 0; i < n; i++)
				new (&data[i]) T(value);
		}

		~vector() {
			clear();
			::operator delete(data, capacity * sizeof(T));
		}

		template<typename U = T>
		typename std::enable_if<std::is_copy_constructible<U>::value>::type push(const T& value) {
			grow();
			new (&data[size]) T(value);
			size++;
		}

		void push(T&& value) {
			grow();
			new (&data[size]) T(std::move(value));
			size++;
		}

		template <typename... Args>
		T* emplace(Args&&... args) {
			grow();
			T* place = &data[size];
			new (place) T(std::forward<Args>(args)...);
			size++;
			return place;
		}

		void pop() { if (size) data[--size].~T(); }

		void clear() {
			if constexpr (!std::is_trivially_destructible_v<T>)
				for (uint32_t i = 0; i < size; i++) data[i].~T();
			size = 0;
		}

		bool empty() const { return size == 0; }

		T* find(const T& value) {
			for (uint32_t i = 0; i < size; i++)
				if (data[i] == value)
					return &data[i];
			return nullptr;
		}
		const T* find(const T& value) const {
			for (uint32_t i = 0; i < size; i++)
				if (data[i] == value)
					return &data[i];
			return nullptr;
		}

		T* begin() { return data; }
		T* end() { return data + size; }
		const T* begin() const { return data; }
		const T* end() const { return data + size; }

		T& operator[](uint32_t i) { return data[i]; }
		const T& operator[](uint32_t i) const { return data[i]; }

		vector& operator=(vector&& other) noexcept {
			if (this != &other) {
				clear();
				::operator delete(data, capacity * sizeof(T));
				data = other.data; size = other.size; capacity = other.capacity;
				other.data = nullptr; other.size = 0; other.capacity = 0;
			}
			return *this;
		}

		vector& operator=(const vector& other) {
			if (this != &other) {
				clear();
				if (capacity < other.size) {
					::operator delete(data, capacity * sizeof(T));
					data = (T*)::operator new(other.size * sizeof(T));
					capacity = other.size;
				}
				copyRange(data, other.data, other.size);
				size = other.size;
			}
			return *this;
		}

		void reserve(uint32_t extra) {
			uint64_t needed = (uint64_t)size + extra;
			if (needed <= capacity) return;
			uint32_t nc = capacity ? capacity : TEA_VECTOR_MIN_CAPACITY;
			while (nc < needed) nc *= 2;
			_realloc(nc);
		}

		template <typename... Args>
		T* emplace_front(Args&&... args) {
			grow();
			if (size > 0) {
				if constexpr (std::is_trivially_copyable_v<T>) {
					memmove(data + 1, data, size * sizeof(T));
				}
				else {
					for (uint32_t i = size; i > 0; i--) {
						new (&data[i]) T(std::move(data[i - 1]));
						data[i - 1].~T();
					}
				}
			}
			new (&data[0]) T(std::forward<Args>(args)...);
			size++;
			return &data[0];
		}

	private:
		static void copyRange(T* dst, const T* src, uint32_t n) {
			if constexpr (std::is_trivially_copyable_v<T>) {
				if (n) memcpy(dst, src, n * sizeof(T));
			}
			else {
				for (uint32_t i = 0; i < n; i++) new (&dst[i]) T(src[i]);
			}
		}

		void _realloc(uint32_t nc) {
			T* newData = (T*)::operator new(nc * sizeof(T));
			if constexpr (std::is_trivially_copyable_v<T>) {
				if (size) memcpy(newData, data, size * sizeof(T));
			}
			else {
				for (uint32_t i = 0; i < size; ++i) {
					new (&newData[i]) T(std::move(data[i]));
					data[i].~T();
				}
			}
			::operator delete(data, capacity * sizeof(T));
			capacity = nc;
			data = newData;
		}

		void grow() {
			if (size + 1 <= capacity) return;
			_realloc(capacity ? capacity * 2 : TEA_VECTOR_MIN_CAPACITY);
		}
	};
}
