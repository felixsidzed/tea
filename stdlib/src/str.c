typedef struct String String;

typedef struct {
	void(*dtor)(String*);
	char(*at)(String*, int);
	String*(*sub)(String*, int, int);
	String*(*cat)(String*, int, ...);
} __String_vtable_t;

typedef struct String {
	__String_vtable_t* __vptr__;
	int refcount;

	char* raw;
	long length;
} String;

#ifdef _WIN32
#include <windows.h>

extern BOOL _mem__free(void* buf);
extern void* _mem__alloc(int size);
extern void* _mem__copy(void* dest, const void* src, int n);

extern void _core__throw(const char* message);
extern BOOL _io__printf(const char* fmt, ...);

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

BOOL _str__eq(char* s1, char* s2) {
	char* p1 = s1;
	char* p2 = s2;
	BOOL equals = TRUE;
	while (equals && *s1 && *s2) {
		equals = *s1 == *s2;
		s1++;
		s2++;
	}
	return equals;
}

char* _str__cat(int count, ...) {
	va_list va;
	int totalLength = 0;

	va_start(va, count);
	for (int i = 0; i < count; i++) {
		char* s = va_arg(va, char*);
		totalLength += lstrlenA(s);
	}
	va_end(va);

	char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalLength + 1);
	if (buffer == NULL)
		return "ERR_MEM";

	va_start(va, count);
	int i = 0;
	for (int j = 0; j < count; j++) {
		char* s = va_arg(va, char*);
		while (*s) {
			buffer[i++] = *s++;
		}
	}
	va_end(va);

	buffer[totalLength] = '\0';
	return buffer;
}

int _str__atoi(const char* str) {
	int result = 0;
	int sign = 1;
	int i = 0;

	while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r' || str[i] == '\f' || str[i] == '\v')
		i++;

	if (str[i] == '+' || str[i] == '-') {
		if (str[i] == '-') {
			sign = -1;
		}
		i++;
	}

	while (str[i] >= '0' && str[i] <= '9') {
		result = result * 10 + (str[i] - '0');
		i++;
	}

	return sign * result;
}

void __String_dtor(String* this) {
	if (--this->refcount <= 0) {
		this->length = 0;
		_mem__free(this->raw);
		_mem__free(this);
	}
}

char __String_at(String* this, int idx) {
	if (idx > this->length) return 0;
	return this->raw[idx];
}

String* __String_sub(String* this, int i, int j) {
	char* result = _str__sub(this->raw, i, j);
	_mem__free(this->raw);
	this->raw = result;
	this->length = j - i;
	return this;
}

String* __String_cat(String* this, int count, ...) {
	va_list va;
	int totalLength = 0;

	va_start(va, count);
	for (int i = 0; i < count; i++) {
		String* s = va_arg(va, String*);
		totalLength += s->length;
	}
	va_end(va);

	char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalLength + 1);
	if (buffer == NULL)
		return this;

	va_start(va, count);
	int i = 0;
	for (int j = 0; j < count; j++) {
		char* s = va_arg(va, char*);
		while (*s) {
			buffer[i++] = *s++;
		}
	}
	va_end(va);

	buffer[totalLength] = '\0';

	_mem__free(this->raw);
	this->raw = buffer;
	return this;
}

static __String_vtable_t __String__vtable = {
	.dtor = __String_dtor,
	.at = __String_at,
	.sub = __String_sub,
	.cat = __String_cat
};

String* __String__constructor(char* raw) {
	String* this = _mem__alloc(sizeof(String));
	if (!this) return NULL;

	this->__vptr__ = &__String__vtable;
	if (raw) {
		const int len = lstrlenA(raw);
		char* allocated = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
		if (allocated) _mem__copy(allocated, raw, len);
		else allocated = raw;
		this->raw = allocated;
		this->length = len;
	} else {
		this->raw = "";
		this->length = 0;
	}

	return this;
};

#else
#error "'str' is not yet available on non-windows machines"
#endif
