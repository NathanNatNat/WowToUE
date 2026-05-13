/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	Based off original works by Robert John Jenkins Junior (Port by bryc).
	https://en.wikipedia.org/wiki/Jenkins_hash_function
	License: MIT
 */
#include "jenkins96.h"

namespace casc {

std::pair<uint32_t, uint32_t> jenkins96(std::span<const uint8_t> k, uint32_t init, uint32_t init2) {
	uint32_t len = static_cast<uint32_t>(k.size());
	uint32_t o = 0;
	uint32_t a = 0xDEADBEEFu + len + init;
	uint32_t b = 0xDEADBEEFu + len + init;
	uint32_t c = 0xDEADBEEFu + len + init + init2;

	while (len > 12) {
		a += static_cast<uint32_t>(k[o])   | static_cast<uint32_t>(k[o+1]) << 8 | static_cast<uint32_t>(k[o+2])  << 16 | static_cast<uint32_t>(k[o+3])  << 24;
		b += static_cast<uint32_t>(k[o+4]) | static_cast<uint32_t>(k[o+5]) << 8 | static_cast<uint32_t>(k[o+6])  << 16 | static_cast<uint32_t>(k[o+7])  << 24;
		c += static_cast<uint32_t>(k[o+8]) | static_cast<uint32_t>(k[o+9]) << 8 | static_cast<uint32_t>(k[o+10]) << 16 | static_cast<uint32_t>(k[o+11]) << 24;

		a -= c; a ^= (c << 4)  | (c >> 28); c = c + b;
		b -= a; b ^= (a << 6)  | (a >> 26); a = a + c;
		c -= b; c ^= (b << 8)  | (b >> 24); b = b + a;
		a -= c; a ^= (c << 16) | (c >> 16); c = c + b;
		b -= a; b ^= (a << 19) | (a >> 13); a = a + c;
		c -= b; c ^= (b << 4)  | (b >> 28); b = b + a;

		len -= 12;
		o += 12;
	}

	if (len > 0) { // final mix only if len > 0
		switch (len) { // incorporate trailing bytes before fmix
			case 12: c += static_cast<uint32_t>(k[o+11]) << 24; break;
			case 11: c += static_cast<uint32_t>(k[o+10]) << 16; break;
			case 10: c += static_cast<uint32_t>(k[o+9]) << 8;   break;
			case 9:  c += static_cast<uint32_t>(k[o+8]);         break;
			case 8:  b += static_cast<uint32_t>(k[o+7]) << 24;  break;
			case 7:  b += static_cast<uint32_t>(k[o+6]) << 16;  break;
			case 6:  b += static_cast<uint32_t>(k[o+5]) << 8;   break;
			case 5:  b += static_cast<uint32_t>(k[o+4]);         break;
			case 4:  a += static_cast<uint32_t>(k[o+3]) << 24;  break;
			case 3:  a += static_cast<uint32_t>(k[o+2]) << 16;  break;
			case 2:  a += static_cast<uint32_t>(k[o+1]) << 8;   break;
			case 1:  a += static_cast<uint32_t>(k[o]);           break;
		}

		c ^= b; c -= (b << 14) | (b >> 18);
		a ^= c; a -= (c << 11) | (c >> 21);
		b ^= a; b -= (a << 25) | (a >> 7);
		c ^= b; c -= (b << 16) | (b >> 16);
		a ^= c; a -= (c << 4)  | (c >> 28);
		b ^= a; b -= (a << 14) | (a >> 18);
		c ^= b; c -= (b << 24) | (b >> 8);
	}

	// use c as 32-bit hash; add b for 64-bit hash. a is not mixed well.
	return { b, c };
}

} // namespace casc