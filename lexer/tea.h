#pragma once

struct Configuration {
	void(*panic)(const char* message, ...);
};

namespace tea {
	extern Configuration configuration;
}

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
