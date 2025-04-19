#ifdef _WIN32
#include <windows.h>

int _math__abs(int num) {
	return (num > 0 ? num : -num);
}

int _math__sqrt(int num) {
	int res = 1;
	while(res * res <= num){
		res++;
	}
	
	return res - 1;
}

int _math__sum(int count, ...) {
	va_list va;
	va_start(va, count);

	int sum = 0;
	for (int i = 0; i < count; i++) {
		sum += va_arg(va, int);
	}

	va_end(va);
	return sum;
}

#else
#error "'math' is not yet available on non-windows machines"
#endif
