#pragma once

#include <format>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

#ifndef TEA_SSO_BUFFER_SIZE
	#define TEA_SSO_BUFFER_SIZE 15
#endif

namespace tea {
	class string {
		union {
			struct { char* ptr; size_t capacity; } heap;
			char sso[TEA_SSO_BUFFER_SIZE + 1];
		};
		size_t len = 0;

		void _setsso(const char* s, size_t n) {
			if (s && n) memcpy(sso, s, n);
			sso[n] = 0;
			len = n;
		}

		void _setlong(const char* s, size_t n, size_t cap) {
			heap.ptr = new char[cap + 1];
			heap.capacity = cap;
			if (s && n) memcpy(heap.ptr, s, n);
			heap.ptr[n] = 0;
			len = n;
		}

		void _free() {
			if (heap.ptr && len > TEA_SSO_BUFFER_SIZE)
				delete[] heap.ptr;
		}

	public:

		string() { sso[0] = 0; }

		string(const char* s) {
			size_t n = s ? strlen(s) : 0;
			if (n <= TEA_SSO_BUFFER_SIZE) _setsso(s, n);
			else _setlong(s, n, n);
		}

		string(const char ch) { _setsso(&ch, 1); }

		string(const string& other) {
			if (other.len > TEA_SSO_BUFFER_SIZE) _setlong(other.heap.ptr, other.len, other.len);
			else _setsso(other.sso, other.len);
		}

		string(string&& other) noexcept {
			len = other.len;
			if (len > TEA_SSO_BUFFER_SIZE) {
				heap.ptr = other.heap.ptr;
				heap.capacity = other.heap.capacity;
				other.len = 0; other.sso[0] = 0;
			} else
				memcpy(sso, other.sso, other.len + 1);
		}

		string(const char* s, size_t n) {
			if (n > TEA_SSO_BUFFER_SIZE) _setlong(s, n, n);
			else _setsso(s, n);
		}

		string(size_t n, char ch) {
			if (n <= TEA_SSO_BUFFER_SIZE) {
				memset(sso, ch, n);
				sso[n] = 0;
				len = n;
			} else {
				_setlong(nullptr, n, n);
				memset(heap.ptr, ch, n);
				heap.ptr[n] = 0;
			}
		}

		~string() { _free(); }

		string& operator=(const string& other) {
			if (this == &other) return *this;
			_free();
			if (other.len > TEA_SSO_BUFFER_SIZE) _setlong(other.heap.ptr, other.len, other.len);
			else _setsso(other.sso, other.len);
			return *this;
		}

		string& operator=(string&& other) noexcept {
			if (this == &other) return *this;
			_free();
			len = other.len;
			if (len > TEA_SSO_BUFFER_SIZE) {
				heap.ptr = other.heap.ptr;
				heap.capacity = other.heap.capacity;
				other.len = 0; other.sso[0] = 0;
			} else
				memcpy(sso, other.sso, other.len + 1);
			return *this;
		}

		size_t length() const { return len; };
		bool empty() const { return len == 0; }
		operator const char* () const { return data(); }
		char* data() { return len > TEA_SSO_BUFFER_SIZE ? heap.ptr : sso; }
		const char* data() const { return len > TEA_SSO_BUFFER_SIZE ? heap.ptr : sso; }

		char& operator[](size_t i) { return data()[i]; }
		const char& operator[](size_t i) const { return data()[i]; }

		void reserve(size_t n) {
			if (n <= TEA_SSO_BUFFER_SIZE || (len > TEA_SSO_BUFFER_SIZE && n <= heap.capacity)) return;
			char* newBuf = new char[n + 1];
			memcpy(newBuf, data(), len + 1);
			_free();
			heap.ptr = newBuf;
			heap.capacity = n;
		}

		string& append(const char* s, size_t n) {
			if (!n) return *this;

			size_t nlen = len + n;
			if (nlen <= TEA_SSO_BUFFER_SIZE) {
				memcpy(sso + len, s, n);
				sso[nlen] = 0;
				len = nlen;
				return *this;
			}

			size_t cap = (len > TEA_SSO_BUFFER_SIZE) ? heap.capacity : TEA_SSO_BUFFER_SIZE;
			if (nlen > cap) {
				size_t ncap = cap ? cap * 2 : TEA_SSO_BUFFER_SIZE;
				while (ncap < nlen)
					ncap *= 2;

				char* nbuf = new char[ncap + 1];
				memcpy(nbuf, data(), len);
				memcpy(nbuf + len, s, n);
				nbuf[nlen] = 0;

				if (len > TEA_SSO_BUFFER_SIZE)
					_free();

				heap.ptr = nbuf;
				heap.capacity = ncap;
			} else {
				memcpy(heap.ptr + len, s, n);
				heap.ptr[nlen] = 0;
			}

			len = nlen;
			return *this;
		}

		string& operator+=(const string& rhs) { return append(rhs.data(), rhs.len); }
		string& operator+=(char c) { return append(&c, 1); }

		string operator+(const string& rhs) const {
			string result;
			result.reserve(len + rhs.len);
			result.append(data(), len);
			result.append(rhs.data(), rhs.len);
			return result;
		}

		string operator+(char c) const { string r(*this); r += c; return r; }

		friend string operator+(const char* lhs, const string& rhs) {
			size_t llen = lhs ? strlen(lhs) : 0;
			string result;
			result.reserve(llen + rhs.len);
			result.append(lhs, llen);
			result.append(rhs.data(), rhs.len);
			return result;
		}

		bool operator==(const string& other) const {
			if (len != other.len) return false;
			return memcmp(data(), other.data(), len) == 0;
		}
		bool operator==(const char* other) const {
			if (!other) return len == 0;
			size_t n = strlen(other);
			if (len != n) return false;
			return memcmp(data(), other, n) == 0;
		}
	};
}

namespace std {
	template<>
	struct formatter<tea::string> : formatter<std::string_view> {
		auto format(const tea::string& s, std::format_context& ctx) const {
			return std::formatter<std::string_view>::format(std::string_view(s.data(), s.length()), ctx);
		}
	};

	template<>
	struct hash<tea::string> {
		size_t operator()(const tea::string& s) const noexcept {
			const char* d = s.data();
			size_t h = 1469598103934665603ull;
			for (size_t i = 0; i < s.length(); i++)
				h = (h ^ (uint8_t)d[i]) * 1099511628211ull;
			return h;
		}
	};
}
