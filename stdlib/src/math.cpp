#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#include <cstdarg>

int _math__abs(int num) {
	return (num > 0 ? num : -num);
}

int _math__sqrt(int num) {
	int res = 1;
	while (res * res <= num)
		res++;
	return res - 1;
}

int _math__sum(int nargs, ...) {
	va_list va;
	va_start(va, nargs);

	int sum = 0;
	for (int i = 0; i < nargs; i++) {
		sum += va_arg(va, int);
	}

	va_end(va);
	return sum;
}

#ifdef __cplusplus
}
#endif
