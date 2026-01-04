#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#include <cstdarg>

int math_abs(int num) {
	return (num > 0 ? num : -num);
}

int math_sqrt(int num) {
	int res = 1;
	while (res * res <= num)
		res++;
	return res - 1;
}

int math_sum(int nargs, ...) {
	va_list va;
	va_start(va, nargs);

	int sum = 0;
	for (int i = 0; i < nargs; i++) {
		sum += va_arg(va, int);
	}

	va_end(va);
	return sum;
}

int math_max(int a, int b) {
	return a > b ? a : b;
}

int math_min(int a, int b) {
	return a < b ? a : b;
}

int math_clamp(int value, int min, int max) {
	return math_max(min, math_min(value, max));
}

int math_pow(int base, int exp) {
	if (!exp)
		return 1;

	int temp = math_pow(base, exp / 2);
	if ((exp % 2) == 0)
		return temp * temp;
	else
		return base * temp * temp;
}

static unsigned lcgseed = 0;

void math_srand(unsigned int seed) {
	lcgseed = seed;
}

int math_random(int min, int max) {
	lcgseed = (1664525 * lcgseed + 1013904223) % 4294967296u;
	return (int)((double)lcgseed / (double)4294967296u) * (max - min + 1) + min;
}

double math_ceil(double x) {
	int inum = (int)x;
	if (x == (double)inum)
		return inum;
	return inum + 1;
}

float math_ceilf(float x) {
	int inum = (int)x;
	if (x < 0 && (float)inum != x)
		return (float)(inum - 1);
	return (float)inum;
}

double math_floor(double x) {
	int inum = (int)x;
	if (x < 0 && (double)inum != x)
		return (double)(inum - 1);
	return (double)inum;
}

float math_floorf(float x) {
	int inum = (int)x;
	if (x < 0 && (float)inum != x)
		return (float)(inum - 1);
	return (float)inum;
}

#ifdef __cplusplus
}
#endif
