#pragma once

#include <new>
#include <cstdint>
#include <initializer_list>

#define TEA_VECTOR_CHUNK_SIZE 4

namespace tea {
	/// <summary>
	/// Small vector for storing same-type elements
	/// </summary>
	/// <typeparam name="T">Element type</typeparam>
	template<typename T>
	class vector {
	public:
		// pointer to the actual element array on heap
		T* data;

		// amount of elements stored
		uint32_t size;
		// amount of elements that can be stored before needing to grow the buffer
		uint32_t capacity;

		vector(vector&& other) noexcept : data(other.data), size(other.size), capacity(other.capacity) {
			other.data = nullptr;
			other.size = 0;
			other.capacity = 0;
		}
		vector(uint32_t size = TEA_VECTOR_CHUNK_SIZE) : size(0), capacity(size), data((T*)::operator new(size * sizeof(T))) {};

		template<typename U = T, typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
		vector(const std::initializer_list<T>& init) :
			size((uint32_t)init.size()), capacity((uint32_t)init.size()), data((T*)::operator new(init.size() * sizeof(T)))
		{
			uint32_t i = 0;
			for (const T& item : init) {
				new (&data[i]) T(item);
				i++;
			}
		}

		vector(const T* otherData, uint32_t otherSize)
			: size(otherSize), capacity(otherSize), data((T*)::operator new(otherSize * sizeof(T)))
		{
			for (uint32_t i = 0; i < otherSize; i++)
				new (&data[i]) T(otherData[i]);
		}

		vector(const vector& other)
			: size(other.size), capacity(other.size), data((T*)::operator new(other.size * sizeof(T)))
		{
			for (uint32_t i = 0; i < other.size; i++)
				new (&data[i]) T(other.data[i]);
		}

		~vector() {
			clear();
			::operator delete(data, capacity * sizeof(T));
		}

		template<typename U = T>
		typename std::enable_if<std::is_copy_constructible<U>::value>::type
			push(const T& value) {
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

		void pop() {
			if (size > 0) {
				size--;
				data[size].~T();
			}
		}

		void clear() {
			for (uint32_t i = 0; i < size; i++)
				data[i].~T();
			size = 0;
		}

		bool empty() const { return size == 0; }

		T* find(const T& value) {
			for (uint32_t i = 0; i < size; i++) {
				if (data[i] == value)
					return &data[i];
			}
			return nullptr;
		}

		const T* find(const T& value) const {
			for (uint32_t i = 0; i < size; i++) {
				if (data[i] == value)
					return &data[i];
			}
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

				data = other.data;
				size = other.size;
				capacity = other.capacity;

				other.data = nullptr;
				other.size = 0;
				other.capacity = 0;
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

				for (uint32_t i = 0; i < other.size; i++) {
					new (&data[i]) T(other.data[i]);
				}
				size = other.size;
			}
			return *this;
		}

		void reserve(uint32_t ns) {
			if (ns <= capacity)
				return;

			T* newData = (T*)::operator new(ns * sizeof(T));
			for (uint32_t i = 0; i < size; ++i) {
				new (&newData[i]) T(std::move(data[i]));
				data[i].~T();
			}
			::operator delete(data, capacity * sizeof(T));

			capacity = ns;
			data = newData;
		}

		template <typename... Args>
		T* emplace_front(Args&&... args) {
			grow();

			if (size > 0) {
				for (uint32_t i = size; i > 0; i--) {
					new (&data[i]) T(std::move(data[i - 1]));
					data[i - 1].~T();
				}
			}

			new (&data[0]) T(std::forward<Args>(args)...);
			size++;

			return &data[0];
		}

	private:
		void grow() {
			if (size + 1 <= capacity)
				return;

			uint32_t nc = capacity + TEA_VECTOR_CHUNK_SIZE;
			T* newData = (T*)::operator new(nc * sizeof(T));
			for (uint32_t i = 0; i < size; ++i) {
				new (&newData[i]) T(std::move(data[i]));
				data[i].~T();
			}
			::operator delete(data, capacity * sizeof(T));

			capacity = nc;
			data = newData;
		}
	};
}
