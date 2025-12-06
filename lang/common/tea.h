#pragma once

#include "string.h"
#include "vector.h"

#if defined(__clang__) && defined(__has_warning)
	#if __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
		#define TEA_FALLTHROUGH [[clang::fallthrough]]
	#else
		#define TEA_FALLTHROUGH
	#endif
#elif defined(__GNUC__) && __GNUC__ >= 7
	#define TEA_FALLTHROUGH [[gnu::fallthrough]]
#else
	#define TEA_FALLTHROUGH [[fallthrough]]
#endif

#if defined(__clang__)
	#if __has_attribute(noreturn)
		#define TEA_NORETURN [[noreturn]] void
	#else
	#define TEA_NORETURN __attribute__((noreturn)) void
	#endif
#elif defined(__GNUC__)
	#if __GNUC__ >= 4
		#define TEA_NORETURN [[noreturn]] void
	#else
		#define TEA_NORETURN __attribute__((noreturn)) void
	#endif
#else
	#define TEA_NORETURN [[noreturn]] void
#endif

#ifdef _MSC_VER
#include <intrin.h>

#define TEA_RETURNADDR _ReturnAddress()
#else
#define TEA_RETURNADDR __builtin_return_address(0)
#endif

#ifdef _MSC_VER
#define TEA_LIKELY(pred) pred
#define TEA_UNLIKELY(pred) pred
#define TEA_UNREACHABLE() __assume(false)
#else
#define TEA_LIKELY(pred) __builtin_expect(pred, 1)
#define TEA_UNLIKELY(pred) __builtin_expect(pred, 0)
#define TEA_UNREACHABLE() __builtin_unreachable()
#endif

#define TEA_PANIC tea::configuration.panic

namespace tea {
	struct CompilerFlags {
	public:
		enum Flag : uint8_t {
			None = 0,
			DumpMIR = 1,
			DumpFinalIR = 2
		};

		uint8_t value;

		constexpr CompilerFlags() : value(0) {}
		constexpr CompilerFlags(Flag f) : value((uint8_t)f) {}

		void set(Flag f) { value |= (uint8_t)f; }
		void clear(Flag f) { value &= ~(uint8_t)f; }
		bool has(Flag f) const { return (value & (uint8_t)f) != 0; }
		void toggle(Flag f) { value ^= (uint8_t)f; }

		CompilerFlags operator|(CompilerFlags other) const {
			CompilerFlags result;
			result.value = value | other.value;
			return result;
		}

		CompilerFlags& operator|=(CompilerFlags other) {
			value |= other.value;
			return *this;
		}
	};

	constexpr CompilerFlags operator|(CompilerFlags::Flag a, CompilerFlags::Flag b) {
		return CompilerFlags((CompilerFlags::Flag)(
			(uint8_t)a | (uint8_t)b
		));
	}

	struct Configuration {
		void(*panic)(const char* message, ...);
	};

	extern Configuration configuration;

	void compile(
		const tea::string& source,
		const tea::vector<const char*>& importLookup,
		const char* outFile,
		const char* triple = nullptr,
		const CompilerFlags& flags = CompilerFlags::None,
		uint8_t optLevel = 2
	);
}
