#ifdef _WIN32
#include <windows.h>

int _str__len(char* str) {
	if (!str) return 0;
	return lstrlenA(str);
}

char* _str__itoa(int num) {
	static char buffer[20];
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

char* _str__sub(char* str, int i, int j) {
	if (i < 0 || j < 0 || i >= j)
		return "";

	int length = lstrlenA(str);
	if (i >= length)
		return NULL;

	if (j > length)
		j = length;

	char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (j - i + 1));
	if (buffer == NULL)
		return "ERR_MEM";

	for (int k = 0; k < (j - i); k++)
		buffer[k] = str[i + k];

	buffer[j - i] = '\0';

	return buffer;
}

#else
#error "'io' is not yet available on non-windows machines"
#endif
