#pragma once

#include <cstdint>

namespace tea {
	struct EmissionFlags {
	public:
		enum Flag : uint8_t {
			None = 0,
			Constant = 1,
			Callee = 2,
		};

		uint8_t value;

		constexpr EmissionFlags() : value(0) {}
		constexpr EmissionFlags(Flag f) : value((uint8_t)f) {}

		void set(Flag f) { value |= (uint8_t)f; }
		void clear(Flag f) { value &= ~(uint8_t)f; }
		bool has(Flag f) const { return (value & (uint8_t)f) != 0; }
		void toggle(Flag f) { value ^= (uint8_t)f; }

		EmissionFlags operator|(EmissionFlags other) const {
			EmissionFlags result;
			result.value = value | other.value;
			return result;
		}

		EmissionFlags& operator|=(EmissionFlags other) {
			value |= other.value;
			return *this;
		}
	};

	constexpr EmissionFlags operator|(EmissionFlags::Flag a, EmissionFlags::Flag b) {
		return EmissionFlags((EmissionFlags::Flag)(
			(uint8_t)a | (uint8_t)b
		));
	}
}
