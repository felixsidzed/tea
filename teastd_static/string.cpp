#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	int string_len(const char* str) {
		if (!str)
			return 0;
		return lstrlenA(str);
	}

	bool string_itoa(int num, char* buf, size_t size) {
		if (!buf || !size)
			return false;

		int i = 0;
		bool isNegative = 0;

		if (num == 0) {
			if (size < 2)
				return false;
			buf[i++] = '0';
			buf[i] = '\0';
			return true;
		}

		if (num < 0) {
			isNegative = true;
			if (num == -2147483648)
				return false;
			num = -num;
		}

		while (num) {
			if (i + 1 >= size)
				return false;
			buf[i++] = (num % 10) + '0';
			num /= 10;
		}

		if (isNegative) {
			if (i + 1 >= size)
				return false;
			buf[i++] = '-';
		}

		buf[i] = '\0';

		for (size_t start = 0, end = i - 1; start < end; start++, end--) {
			char tmp = buf[start];
			buf[start] = buf[end];
			buf[end] = tmp;
		}

		return true;
	}

	char* string_sub(char* str, int i, int j) {
		if (i < 0 || j < 0 || i >= j)
			return nullptr;

		int length = lstrlenA(str);
		if (i >= length)
			return NULL;

		if (j > length)
			j = length;

		char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (j - i + 1));
		if (buffer == NULL)
			return nullptr; // TODO: throw an exception

		for (int k = 0; k < (j - i); k++)
			buffer[k] = str[i + k];

		buffer[j - i] = '\0';

		return buffer;
	}

	bool string_eq(const char* s1, const char* s2) {
		while (*s1 && *s2) {
			if (*s1 != *s2)
				return false;
			s1++, s2++;
		}
		return *s1 == *s2;
	}

	char* string_cat(int nargs, ...) {
		va_list va;
		int total = 0;

		va_start(va, nargs);
		for (int i = 0; i < nargs; i++) {
			const char* s = va_arg(va, const char*);
			if (s)
				total += lstrlenA(s);
		}
		va_end(va);

		char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total + 1);
		if (buffer == NULL)
			return nullptr; // TODO: throw an exception

		va_start(va, nargs);
		int i = 0;
		for (int j = 0; j < nargs; j++) {
			const char* s = va_arg(va, const char*);
			if (!s) continue;

			while (*s)
				buffer[i++] = *s++;
		}
		va_end(va);

		buffer[total] = '\0';
		return buffer;
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
