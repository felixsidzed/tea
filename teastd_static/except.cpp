#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <intrin.h>

	typedef struct eframe {
		const char* message;
		struct eframe* prev;
		_JUMP_BUFFER jb;
	} eframe;

	extern int __setjmp(_JUMP_BUFFER* buf);
	extern void __longjmp(_JUMP_BUFFER* buf, int value);

	static thread_local eframe* eframeTop = 0;

	extern void io_printf(const char* fmt, ...);

	[[noreturn]] void except_throw(const char* msg) {
		if (!msg) msg = "??";

		eframe* ef = eframeTop;
		if (!ef) {
			io_printf("unhandled exception at 0x%p: %s", _ReturnAddress(), msg);
			ExitThread(1); // TODO: debugbreak instead
		}

		ef->message = msg;
		__longjmp(&ef->jb, 1);
	}

	const char* except_pcall(void(*func)()) {
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

	bool except_xpcall(void(*func)(), bool(*handler)(const char*)) {
		const char* err = except_pcall(func);
		if (err && handler)
			return handler(err);
		return !err;
	}

	void assert(bool pred, const char* msg) {
		if (!pred) [[unlikely]]
			except_throw(msg);
	}

#else
#error windows only
#endif

#ifdef __cplusplus
}
#endif
