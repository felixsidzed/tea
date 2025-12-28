#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#include <intrin.h>

	typedef struct eframe {
		const char* message;
		struct eframe* prev;
		_JUMP_BUFFER jb;
	} eframe;

	extern int __setjmp(_JUMP_BUFFER* buf);
	extern void __longjmp(_JUMP_BUFFER* buf, int value);

	static thread_local eframe* eframeTop = 0;

	extern void _io__printf(const char* fmt, ...);

	[[noreturn]] void _except__throw(const char* msg) {
		if (!msg) msg = "??";

		eframe* ef = eframeTop;
		if (!ef) {
			_io__printf("unhandled exception at %p: %s", _ReturnAddress(), msg);
			__debugbreak();
			__fastfail(1);
		}

		ef->message = msg;
		__longjmp(&ef->jb, 1);
	}

	const char* _except__pcall(void(*func)()) {
		eframe ef;
		ef.message = nullptr;
		ef.prev = eframeTop;

		eframeTop = &ef;

		if (__setjmp(&ef.jb)) {
			eframeTop = ef.prev;
			return ef.message;
		}

		func();

		eframeTop = ef.prev;
		return 0;
	}

	bool _except__xpcall(void(*func)(), bool(*handler)(const char*)) {
		const char* err = _except__pcall(func);
		if (err && handler)
			return handler(err);
		return !err;
	}

	void assert(bool pred, const char* msg) {
		if (!pred) [[unlikely]]
			_except__throw(msg);
	}

#else
#error windows only
#endif

#ifdef __cplusplus
}
#endif
