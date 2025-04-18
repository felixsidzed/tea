#ifdef _WIN32
#include <windows.h>

int _str__len(char* str) {
	if (!str) return 0;
	return lstrlenA(str);
}

#else
#error "'io' is not yet available on non-windows machines"
#endif
