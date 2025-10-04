#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	int _string__len(const char* str) {
		if (!str)
			return 0;
		return lstrlenA(str);
	}

	char* _string__itoa(int num, char* buffer) {
		int i = 0;
		int isNegative = 0;

		if (num == 0) {
			buffer[i++] = '0';
			buffer[i] = '\0';
			return buffer;
		}

		if (num < 0) {
			isNegative = 1;
			num = -num;
		}

		while (num != 0) {
			buffer[i++] = (num % 10) + '0';
			num /= 10;
		}

		if (isNegative) {
			buffer[i++] = '-';
		}

		buffer[i] = '\0';

		int start = 0;
		int end = i - 1;
		while (start < end) {
			char temp = buffer[start];
			buffer[start] = buffer[end];
			buffer[end] = temp;
			start++;
			end--;
		}

		return buffer;
	}

	char* _string__sub(char* str, int i, int j) {
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

	bool _string__eq(const char* s1, const char* s2) {
		bool equals = true;
		while (equals && *s1 && *s2) {
			equals = *s1 == *s2;
			s1++, s2++;
		}
		return equals;
	}

	char* _string__cat(int nargs, ...) {
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
