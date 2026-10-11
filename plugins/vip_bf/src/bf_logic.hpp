// vip_bf — engine-free part of the Base Functions module: the value parsing and the round gate
// of the original (vip_bf.cpp).
//
// Kept pure so both are unit-testable (tests/vip_bf_tests.cpp).

#pragma once

#include <cstdlib>
#include <string>
#include <string_view>

namespace vip_bf {

// The original recognised a "+N" value by its first character.
inline bool IsBonus(std::string_view value) {
	return !value.empty() && value.front() == '+';
}

// The original's "+N" parsing erases the first two characters: it removes the '+' *and the first
// digit*, so "+50" contributes 0, "+150" contributes 50 and "+5" contributes 0.
// The original behaves like this and the port keeps it.
inline int ParseBonus(std::string_view value) {
	std::string text(value);
	text.erase(0, 2);
	return std::atoi(text.c_str());
}

// atoi() of the whole value; anything non-numeric parses as 0 (the original's atoi).
inline int ParseValue(std::string_view value) {
	if (IsBonus(value)) {
		return ParseBonus(value);
	}
	return std::atoi(std::string(value).c_str());
}

// The original's round condition: round_min 1 counts from the first non-pistol round, a larger
// round_min from that many played rounds on but never on a pistol round, and 0 always applies.
inline bool ShouldApply(int roundMin, bool pistolRound, int totalRounds) {
	return (roundMin == 1 && !pistolRound) || (roundMin <= totalRounds && !pistolRound) || roundMin == 0;
}

} // namespace vip_bf
