// vip_bf unit tests — the engine-free part: the "health"/"armor"/"money" value parsing (including
// the original's two-character-erase quirk) and the round_min condition.
//
// Run by ../../test.sh (inside the Steam Runtime image, cwd = build/tests).

#include "bf_logic.hpp"

#include <cstdio>
#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;

void CheckEq(int actual, int expected, const std::string& what) {
	++g_checks;
	if (actual != expected) {
		++g_failures;
		std::printf("FAIL: %s (expected %d, got %d)\n", what.c_str(), expected, actual);
	}
}

void CheckValue(const char* raw, int expected) {
	CheckEq(vip_bf::ParseValue(raw), expected, std::string("value '") + raw + "'");
}

void CheckRound(int roundMin, bool pistol, int total, bool expected) {
	char buffer[128];
	std::snprintf(buffer, sizeof(buffer), "round_min=%d pistol=%d total=%d", roundMin, pistol ? 1 : 0, total);
	CheckEq(vip_bf::ShouldApply(roundMin, pistol, total) ? 1 : 0, expected ? 1 : 0, buffer);
}

} // namespace

int main() {
	std::printf("vip_bf tests\n");

	// Absolute values.
	CheckValue("150", 150);
	CheckValue("0", 0);
	CheckValue("", 0);
	CheckValue("abc", 0);
	CheckValue("100hp", 100);

	// "+\N": the original erases two characters, removing the '+' and the first digit.
	CheckValue("+50", 0);  // "+50" -> "0"
	CheckValue("+150", 50); // "+150" -> "50"
	CheckValue("+5", 0);    // "+5" -> ""
	CheckValue("+1000", 0);   // "+1000" -> "000"

	// The round gate of the original.
	CheckRound(0, false, 0, true);
	CheckRound(0, true, 0, true);
	CheckRound(1, false, 0, true);  // round_min == 1 counts from the first non-pistol round
	CheckRound(1, true, 5, false);
	CheckRound(2, false, 0, false);
	CheckRound(2, false, 1, false);
	CheckRound(2, false, 2, true);
	CheckRound(2, true, 5, false);
	CheckRound(3, false, 2, false);
	CheckRound(3, false, 3, true);
	CheckRound(5, false, 4, false);
	CheckRound(5, false, 9, true);

	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
