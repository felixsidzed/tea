#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <intrin.h>

	void memory_free(void* buf) {
		HeapFree(GetProcessHeap(), 0, buf);
	}

	void* memory_alloc(size_t size) {
		return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	}

	void* memory_realloc(void* ptr, size_t nsize) {
		return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, nsize);
	}

	// TODO: Not sure how much faster this is
	void* memory_copy(char* dest, const char* src, unsigned n) {
		char* d = dest;

		while (n && ((uintptr_t)d & 15)) {
			*d++ = *src++;
			n--;
		}

		__m128i* dd = (__m128i*)d;
		const __m128i* ss = (const __m128i*)src;
		while (n >= 16) {
			_mm_storeu_si128(dd++, _mm_loadu_si128(ss++));
			n -= 16;
		}

		d = (char*)dd;
		src = (const char*)ss;
		while (n--)
			*d++ = *src++;

		return dest;
	}

	void* memory_set(unsigned char* dest, unsigned char src, size_t size) {
		__m128i v16 = _mm_set1_epi8(src);

		while (size && ((uintptr_t)dest & 15)) {
			*dest++ = src;
			size--;
		}

		__m128i* wp = (__m128i*)dest;
		while (size >= 16) {
			_mm_storeu_si128(wp++, v16);
			size -= 16;
		}

		dest = (unsigned char*)wp;
		while (size--)
			*dest++ = src;

		return dest;
	}

	int memory_cmp(const unsigned char* p1, const unsigned char* p2, size_t size) {
		while (size && ((uintptr_t)p1 & 15)) {
			if (*p1 != *p2) return *p1 - *p2;
			p1++;
			p2++;
			size--;
		}

		const __m128i* v1 = (const __m128i*)p1;
		const __m128i* v2 = (const __m128i*)p2;
		while (size >= 16) {
			__m128i a = _mm_loadu_si128(v1++);
			__m128i b = _mm_loadu_si128(v2++);
			__m128i cmp = _mm_cmpeq_epi8(a, b);
			if (_mm_movemask_epi8(cmp) != 0xFFFF) {
				const unsigned char* c1 = (const unsigned char*)(v1 - 1);
				const unsigned char* c2 = (const unsigned char*)(v2 - 1);
				for (int i = 0; i < 16; i++) {
					if (c1[i] != c2[i]) return c1[i] - c2[i];
				}
			}
			size -= 16;
		}

		p1 = (const unsigned char*)v1;
		p2 = (const unsigned char*)v2;
		while (size--) {
			if (*p1 != *p2) return *p1 - *p2;
			p1++;
			p2++;
		}

		return 0;
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
