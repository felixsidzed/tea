#pragma once

#include <format>
#include <cstdint>
#include <string.h>
#include <string_view>

namespace tea {
	class string {
	public:
		char* data;
		uint32_t size;

		string() : data(new char[1]), size(0) {
			data[0] = '\0';
		}

		string(const char* s) {
			if (!s) {
				data = new char[1];
				data[0] = '\0';
				size = 0;
			} else {
				size = (uint32_t)strlen(s);
				data = new char[size + 1];
				memcpy_s(data, size + 1, s, size);
				data[size] = '\0';
			}
		}

		string(const string& other) {
			size = other.size;
			data = new char[size + 1];
			memcpy_s(data, size + 1, other.data, size);
			data[size] = '\0';
		}

		string(string&& other) noexcept : data(other.data), size(other.size) {
			other.data = nullptr;
			other.size = 0;
		}

		string(const char* s, uint32_t si) {
			if (!s) {
				data = new char[1];
				data[0] = '\0';
				size = 0;
			} else {
				size = si;
				data = new char[size + 1];
				memcpy_s(data, size + 1, s, size);
				data[size] = '\0';
			}
		}

		string& operator=(const string& other) {
			if (this != &other) {
				delete[] data;
				size = other.size;
				data = new char[size + 1];
				memcpy_s(data, size + 1, other.data, size);
				data[size] = '\0';
			}
			return *this;
		}

		string& operator=(string&& other) noexcept {
			if (this != &other) {
				delete[] data;
				data = other.data;
				size = other.size;
				other.data = nullptr;
				other.size = 0;
			}
			return *this;
		}

		~string() {
			delete[] data;
		}

		operator const char*() const {
			return data;
		}

		bool empty() const { return size == 0; }

		char& operator[](uint32_t index) {
			if (index >= size)
				return data[0];
			return data[index];
		}

		const char& operator[](uint32_t index) const {
			if (index >= size)
				return data[0];
			return data[index];
		}

		string operator+(const string& rhs) const {
			uint32_t newSize = size + rhs.size;
			char* newData = new char[newSize + 1];
			strcpy_s(newData, (newSize + 1), data);
			strcat_s(newData, (newSize + 1), rhs.data);

			string result;
			delete[] result.data;
			result.data = newData;
			result.size = newSize;
			return result;
		}

		string& operator+=(const string& rhs) {
			uint32_t newSize = size + rhs.size;
			char* newData = new char[newSize + 1];
			strcpy_s(newData, (newSize + 1), data);
			strcat_s(newData, (newSize + 1), rhs.data);

			delete[] data;
			data = newData;
			size = newSize;
			return *this;
		}

		friend string operator+(const char* lhs, const string& rhs) {
			if (!lhs) lhs = "";
			uint32_t lhsSize = (uint32_t)strlen(lhs);
			uint32_t newSize = lhsSize + rhs.size;
			char* newData = new char[newSize + 1];
			strcpy_s(newData, newSize + 1, lhs);
			strcat_s(newData, newSize + 1, rhs.data);

			string result;
			delete[] result.data;
			result.data = newData;
			result.size = newSize;
			return result;
		}

		string& operator+=(char c) {
			char temp[2] = { c, '\0' };
			return (*this += temp);
		}

		string operator+(char c) const {
			string result(*this);
			result += c;
			return result;
		}

		bool operator==(const string& other) const {
			if (size != other.size) return false;
			if (data == nullptr || other.data == nullptr) return data == other.data;
			return !strcmp(data, other.data);
		}

		bool operator==(const char* other) const {
			if (size != strlen(other)) return false;
			if (data == nullptr || other == nullptr) return data == other;
			return !strcmp(data, other);
		}
	};
}

namespace std {
	template<>
	struct formatter<tea::string> : formatter<std::string_view> {
		auto format(const tea::string& s, std::format_context& ctx) const {
			std::string_view sv{s.data, s.size};
			return std::formatter<std::string_view>::format(sv, ctx);
		}
	};
}
