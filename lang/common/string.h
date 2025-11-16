#pragma once

#include <format>
#include <cstdint>
#include <string.h>
#include <string_view>

#ifndef TEA_SSO_BUFFER_SIZE
#define TEA_SSO_BUFFER_SIZE (sizeof(char*))
#endif

namespace tea {
	/// <summary>
	/// Small string for human-readable data
	/// </summary>
	class string {
		// the data is stored in `shortBuf` if it's less than TEA_SSO_BUFFER_SIZE
		// on heap, pointed to by `longBuf` otherwise
		union {
			char* longBuf = nullptr;
			char shortBuf[TEA_SSO_BUFFER_SIZE];
		};
		
		// length of the string
		size_t size = 0;

	public:
		string() {}

		// construt from raw string literal
		string(const char* s) {
			if (!s)
				return;
			size = strlen(s);
			if (size < TEA_SSO_BUFFER_SIZE) {
				memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
				memcpy_s(shortBuf, TEA_SSO_BUFFER_SIZE, s, size);
			} else {
				longBuf = new char[size + 1];
				memcpy_s(longBuf, size + 1, s, size);
				longBuf[size] = 0;
			}
		}

		// construct from char
		string(const char ch) {
			memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
			shortBuf[0] = ch;
			size = 1;
		}

		// clone constructor
		string(const string& other) {
			size = other.size;
			if (size < TEA_SSO_BUFFER_SIZE) {
				memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
				memcpy_s(shortBuf, TEA_SSO_BUFFER_SIZE, other.shortBuf, size);
			} else {
				longBuf = new char[size + 1];
				memcpy_s(longBuf, size + 1, other.longBuf, size);
				longBuf[size] = 0;
			}
		}

		// std::move
		string(string&& other) noexcept : longBuf(other.longBuf), size(other.size) {
			other.size = 0;
			other.longBuf = nullptr;
		}

		// construt from raw string literal (with size)
		string(const char* s, size_t si) {
			if (!s) {
				memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
				size = 0;
			} else {
				size = si;
				if (size < TEA_SSO_BUFFER_SIZE) {
					memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
					memcpy_s(shortBuf, TEA_SSO_BUFFER_SIZE, s, size);
				} else {
					longBuf = new char[size + 1];
					memcpy_s(longBuf, size + 1, s, size);
					longBuf[size] = 0;
				}
			}
		}

		~string() {
			if (size >= TEA_SSO_BUFFER_SIZE && longBuf)
				delete[] longBuf;
		}

		string& operator=(const string& other) {
			if (this != &other) {
				if (size >= TEA_SSO_BUFFER_SIZE)
					delete[] longBuf;
				size = other.size;
				if (size < TEA_SSO_BUFFER_SIZE) {
					memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
					memcpy_s(shortBuf, TEA_SSO_BUFFER_SIZE, other.shortBuf, size);
				} else {
					longBuf = new char[size + 1];
					memcpy_s(longBuf, size + 1, other.longBuf, size);
					longBuf[size] = 0;
				}
			}
			return *this;
		}

		string& operator=(string&& other) noexcept {
			if (this != &other) {
				if (size >= TEA_SSO_BUFFER_SIZE)
					delete[] longBuf;
				size = other.size;
				if (size < TEA_SSO_BUFFER_SIZE) {
					memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
					memcpy_s(shortBuf, TEA_SSO_BUFFER_SIZE, other.shortBuf, size);
				} else {
					longBuf = other.longBuf;
					other.longBuf = nullptr;
				}
				other.size = 0;
			}
			return *this;
		}

		size_t length() const { return size; };
		bool empty() const { return size == 0; }
		operator const char* () const { return data(); }
		char* data() const { return size >= TEA_SSO_BUFFER_SIZE ? longBuf : (char*)shortBuf; }

		char& operator[](size_t index) { return data()[index]; }
		const char& operator[](size_t index) const { return data()[index]; }

		string operator+(const string& rhs) const {
			size_t newSize = size + rhs.size;

			string result;
			result.size = newSize;

			if (newSize < TEA_SSO_BUFFER_SIZE) {
				memset(result.shortBuf, 0, TEA_SSO_BUFFER_SIZE);
				memcpy_s(result.shortBuf, TEA_SSO_BUFFER_SIZE, data(), size);
				strcat_s(result.shortBuf, TEA_SSO_BUFFER_SIZE, rhs.data());
			} else {
				result.longBuf = new char[newSize + 1];
				memset(result.longBuf, 0, newSize + 1);
				memcpy_s(result.longBuf, newSize + 1, data(), size);
				strcat_s(result.longBuf, newSize + 1, rhs.data());
			}
			return result;
		}

		string& operator+=(const string& rhs) {
			size_t newSize = size + rhs.size;

			if (newSize < TEA_SSO_BUFFER_SIZE) {
				char buffer[TEA_SSO_BUFFER_SIZE];
				memset(buffer, 0, TEA_SSO_BUFFER_SIZE);
				memcpy_s(buffer, TEA_SSO_BUFFER_SIZE, data(), size);
				strcat_s(buffer, TEA_SSO_BUFFER_SIZE, rhs.data());

				if (size >= TEA_SSO_BUFFER_SIZE)
					delete[] longBuf;

				memset(shortBuf, 0, TEA_SSO_BUFFER_SIZE);
				memcpy_s(shortBuf, TEA_SSO_BUFFER_SIZE, buffer, newSize);
			} else {
				char* newBuf = new char[newSize + 1];
				memset(newBuf, 0, newSize + 1);
				memcpy_s(newBuf, newSize + 1, data(), size);
				strcat_s(newBuf, newSize + 1, rhs.data());

				if (size >= TEA_SSO_BUFFER_SIZE)
					delete[] longBuf;
				longBuf = newBuf;
			}

			size = newSize;
			return *this;
		}

		friend string operator+(const char* lhs, const string& rhs) {
			if (!lhs) lhs = "";
			size_t lhsSize = strlen(lhs);
			size_t newSize = lhsSize + rhs.size;

			string result;
			result.size = newSize;

			if (newSize < TEA_SSO_BUFFER_SIZE) {
				memset(result.shortBuf, 0, TEA_SSO_BUFFER_SIZE);
				memcpy_s(result.shortBuf, TEA_SSO_BUFFER_SIZE, lhs, rhs.size);
				strcat_s(result.shortBuf, TEA_SSO_BUFFER_SIZE, rhs.data());
			} else {
				result.longBuf = new char[newSize + 1];
				memcpy_s(result.longBuf, newSize + 1, lhs, rhs.size);
				strcat_s(result.longBuf, newSize + 1, rhs.data());
			}

			return result;
		}

		string& operator+=(char c) {
			char tmp[2] = { c, 0 };
			return (*this += tmp);
		}

		string operator+(char c) const {
			string result(*this);
			result += c;
			return result;
		}

		bool operator==(const string& other) const {
			if (size != other.size) return false;
			return strcmp(data(), other.data()) == 0;
		}

		bool operator==(const char* other) const {
			if (!other) return size == 0;
			if (size != strlen(other)) return false;
			return strcmp(data(), other) == 0;
		}
	};
}

namespace std {
	template<>
	struct formatter<tea::string> : formatter<std::string_view> {
		auto format(const tea::string& s, std::format_context& ctx) const {
			std::string_view sv{ s.data(), s.length() };
			return std::formatter<std::string_view>::format(sv, ctx);
		}
	};

	template<>
	struct hash<tea::string> {
		size_t operator()(const tea::string& s) const noexcept {
			const char* data = s.data();
			size_t h = 1469598103934665603ull;
			for (size_t i = 0; i < s.length(); i++)
				h = (h ^ (uint8_t)data[i]) * 1099511628211ull;
			return h;
		}
	};
}
