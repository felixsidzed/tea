#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	typedef volatile unsigned mutex;

	typedef struct condvar_waiter {
		struct condvar_waiter* next;
		volatile unsigned notified;
	} condvar_waiter;

	typedef struct {
		mutex lock;
		condvar_waiter* waiters;
	} condvar;

	extern void* memory_alloc(size_t size);

	mutex* mutex_new() {
		mutex* mtx = (mutex*)memory_alloc(sizeof(mutex));
		_InterlockedExchange(mtx, 0);
		return mtx;
	}

	void mutex_lock(mutex* mtx) {
		while (_InterlockedCompareExchange(mtx, 1, 0))
			SwitchToThread();
	}

	void mutex_unlock(mutex* mtx) {
		_InterlockedExchange(mtx, 0);
	}

	condvar* mutex_cvnew() {
		condvar* cv = (condvar*)memory_alloc(sizeof(condvar));
		_InterlockedExchange(&cv->lock, 0);
		cv->waiters = nullptr;
		return cv;
	}

	void mutex_wait(condvar* cv, mutex* mtx) {
		condvar_waiter* waiter = (condvar_waiter*)memory_alloc(sizeof(condvar_waiter));
		waiter->next = nullptr;
		waiter->notified = false;

		while (_InterlockedCompareExchange(&cv->lock, 1, 0))
			SwitchToThread();

		waiter->next = cv->waiters;
		cv->waiters = waiter;

		_InterlockedExchange(&cv->lock, 0);
		mutex_unlock(mtx);

		while (!_InterlockedCompareExchange(&waiter->notified, 0, 0))
			SwitchToThread();

		mutex_lock(mtx);
	}

	void mutex_signal(condvar* cv) {
		while (_InterlockedCompareExchange(&cv->lock, 1, 0))
			SwitchToThread();

		if (cv->waiters) {
			condvar_waiter* waiter = cv->waiters;
			cv->waiters = waiter->next;
			_InterlockedExchange(&waiter->notified, 1);
		}

		_InterlockedExchange(&cv->lock, 0);
	}

	void mutex_broadcast(condvar* cv) {
		while (_InterlockedCompareExchange(&cv->lock, 1, 0))
			SwitchToThread();

		condvar_waiter* waiter = cv->waiters;
		while (waiter) {
			_InterlockedExchange(&waiter->notified, 1);
			waiter = waiter->next;
		}

		cv->waiters = nullptr;
		_InterlockedExchange(&cv->lock, 0);
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
