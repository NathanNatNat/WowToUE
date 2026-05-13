/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DXTDecoder.h"

#include <algorithm>
#include <array>

namespace dxt {

static std::array<double, 3> unpack_565(int v) {
	return {
		((v >> 11) & 0x1F) * (255.0 / 31.0),
		((v >> 5) & 0x3F) * (255.0 / 63.0),
		(v & 0x1F) * (255.0 / 31.0)
	};
}

std::vector<uint8_t> decode_dxt(const uint8_t* data, int width, int height, int dxt_type) {
	std::vector<uint8_t> out(width * height * 4, 0);
	const int block_bytes = dxt_type == DXT_TYPE_DXT1 ? 8 : 16;
	const int blocks_x = std::max(1, (width + 3) >> 2);
	const int blocks_y = std::max(1, (height + 3) >> 2);

	int pos = 0;

	for (int by = 0; by < blocks_y; by++) {
		for (int bx = 0; bx < blocks_x; bx++) {
			int color_ofs = pos;
			if (dxt_type != DXT_TYPE_DXT1)
				color_ofs += 8;

			const int c0 = data[color_ofs] | (data[color_ofs + 1] << 8);
			const int c1 = data[color_ofs + 2] | (data[color_ofs + 3] << 8);

			const auto r0 = unpack_565(c0);
			const auto r1 = unpack_565(c1);

			std::array<double, 16> colors{};
			colors[0] = r0[0]; colors[1] = r0[1]; colors[2] = r0[2]; colors[3] = 255;
			colors[4] = r1[0]; colors[5] = r1[1]; colors[6] = r1[2]; colors[7] = 255;

			if (dxt_type == DXT_TYPE_DXT1 && c0 <= c1) {
				colors[8] = (r0[0] + r1[0]) / 2;
				colors[9] = (r0[1] + r1[1]) / 2;
				colors[10] = (r0[2] + r1[2]) / 2;
				colors[11] = 255;
				colors[12] = 0; colors[13] = 0; colors[14] = 0; colors[15] = 0;
			} else {
				colors[8] = (2 * r0[0] + r1[0]) / 3;
				colors[9] = (2 * r0[1] + r1[1]) / 3;
				colors[10] = (2 * r0[2] + r1[2]) / 3;
				colors[11] = 255;
				colors[12] = (r0[0] + 2 * r1[0]) / 3;
				colors[13] = (r0[1] + 2 * r1[1]) / 3;
				colors[14] = (r0[2] + 2 * r1[2]) / 3;
				colors[15] = 255;
			}

			// decode color indices
			std::array<int, 16> idx_data{};
			for (int i = 0; i < 4; i++) {
				const int packed = data[color_ofs + 4 + i];
				idx_data[i * 4] = packed & 0x3;
				idx_data[i * 4 + 1] = (packed >> 2) & 0x3;
				idx_data[i * 4 + 2] = (packed >> 4) & 0x3;
				idx_data[i * 4 + 3] = (packed >> 6) & 0x3;
			}

			// decode alpha
			std::array<double, 16> alpha{};
			if (dxt_type == DXT_TYPE_DXT3) {
				for (int i = 0; i < 8; i++) {
					const int q = data[pos + i];
					alpha[i * 2] = (q & 0x0F) | ((q & 0x0F) << 4);
					alpha[i * 2 + 1] = (q & 0xF0) | ((q & 0xF0) >> 4);
				}
			} else if (dxt_type == DXT_TYPE_DXT5) {
				const int a0 = data[pos];
				const int a1 = data[pos + 1];
				std::array<double, 8> a_lut{};
				a_lut[0] = a0;
				a_lut[1] = a1;

				if (a0 <= a1) {
					for (int i = 1; i < 5; i++)
						a_lut[i + 1] = ((5 - i) * a0 + i * a1) / 5.0;
					a_lut[6] = 0;
					a_lut[7] = 255;
				} else {
					for (int i = 1; i < 7; i++)
						a_lut[i + 1] = ((7 - i) * a0 + i * a1) / 7.0;
				}

				int a_pos = 2;
				int a_idx = 0;
				for (int i = 0; i < 2; i++) {
					int value = 0;
					for (int j = 0; j < 3; j++)
						value |= data[pos + a_pos++] << (8 * j);
					for (int j = 0; j < 8; j++)
						alpha[a_idx++] = a_lut[(value >> (3 * j)) & 0x7];
				}
			}

			// write pixels
			for (int py = 0; py < 4; py++) {
				for (int px = 0; px < 4; px++) {
					const int sx = bx * 4 + px;
					const int sy = by * 4 + py;

					if (sx >= width || sy >= height)
						continue;

					const int src = idx_data[py * 4 + px] * 4;
					const int dst = (sy * width + sx) * 4;
					out[dst] = static_cast<uint8_t>(colors[src]);
					out[dst + 1] = static_cast<uint8_t>(colors[src + 1]);
					out[dst + 2] = static_cast<uint8_t>(colors[src + 2]);

					if (dxt_type == DXT_TYPE_DXT1)
						out[dst + 3] = static_cast<uint8_t>(colors[src + 3]);
					else
						out[dst + 3] = static_cast<uint8_t>(alpha[py * 4 + px]);
				}
			}

			pos += block_bytes;
		}
	}

	return out;
}

} // namespace dxt
