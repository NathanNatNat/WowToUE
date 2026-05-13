/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "pkware.h"
#include "bitstream.h"

#include <array>
#include <stdexcept>
#include <format>

namespace mpq {

// LZ77 with Huffman coding
// ref: https://groups.google.com/g/comp.compression/c/M5P064or93o/m/W1ca1-ad6kgJ?pli=1
// ref: http://justsolve.archiveteam.org/wiki/PKWARE_DCL_Implode

enum CompressionType : uint8_t {
	Binary = 0, // Binary compression mode
	Ascii = 1,  // ASCII/text compression mode (not implemented)
};

class PKLibDecompress {
public:
	static constexpr std::array<uint8_t, 16> LEN_BITS = {{
		3, 2, 3, 3, 4, 4, 4, 5,
		5, 5, 5, 6, 6, 6, 7, 7
	}};

	static constexpr std::array<uint8_t, 16> LEN_CODE = {{
		5, 3, 1, 6, 10, 2, 12, 20,
		4, 24, 8, 48, 16, 32, 64, 0
	}};

	static constexpr std::array<uint8_t, 16> EX_LEN_BITS = {{
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 2, 3, 4, 5, 6, 7, 8
	}};

	static constexpr std::array<uint16_t, 16> LEN_BASE = {{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 10, 14, 22, 38, 70, 134, 262
	}};

	static constexpr std::array<uint8_t, 64> DIST_BITS = {{
		2, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
	}};

	static constexpr std::array<uint8_t, 64> DIST_CODE = {{
		3, 13, 5, 25, 9, 17, 1, 62, 30, 46, 14, 54, 22, 38, 6, 58,
		26, 42, 10, 50, 18, 34, 66, 2, 124, 60, 92, 28, 108, 44, 76, 12,
		116, 52, 84, 20, 100, 36, 68, 4, 120, 56, 88, 24, 104, 40, 72, 8,
		240, 112, 176, 48, 208, 80, 144, 16, 224, 96, 160, 32, 192, 64, 128, 0
	}};

	template<size_t N>
	static std::array<uint8_t, 256> generateDecodeTable(const std::array<uint8_t, N>& bits, const std::array<uint8_t, N>& codes) {
		std::array<uint8_t, 256> table{};

		for (int i = static_cast<int>(N) - 1; i >= 0; i--) {
			const uint8_t code = codes[i];
			const int step = 1 << bits[i];

			for (int j = code; j < 256; j += step)
				table[j] = static_cast<uint8_t>(i);
		}

		return table;
	}

	static const std::array<uint8_t, 256>& getPositionTable1() {
		static const auto table = generateDecodeTable(DIST_BITS, DIST_CODE);
		return table;
	}

	static const std::array<uint8_t, 256>& getPositionTable2() {
		static const auto table = generateDecodeTable(LEN_BITS, LEN_CODE);
		return table;
	}

	PKLibDecompress(std::span<const uint8_t> data)
		: compressionType(0), dictionarySizeBits(0), stream(data.subspan(0, 0)) {
		if (data.size() < 2)
			throw std::runtime_error("input too short for PKWare header");

		compressionType = data[0];
		dictionarySizeBits = data[1];

		if (compressionType != CompressionType::Binary && compressionType != CompressionType::Ascii)
			throw std::runtime_error(std::format("invalid compression type: {}", compressionType));

		if (dictionarySizeBits < 4 || dictionarySizeBits > 6)
			throw std::runtime_error(std::format("invalid dictionary size: {}", dictionarySizeBits));

		stream = BitStream(data.subspan(2));
	}

	std::vector<uint8_t> explode(size_t expectedSize) {
		std::vector<uint8_t> output(expectedSize);
		size_t out_pos = 0;

		while (true) {
			const int literal_len = decodeLiteral();

			if (literal_len == -1)
				break; // eos

			if (literal_len < 256) {
				if (out_pos >= expectedSize) // copy literal bytes directly
					break;

				output[out_pos++] = static_cast<uint8_t>(literal_len);
			} else {
				const int length = literal_len - 254; // convert to actual length
				const int distance = decodeDistance(length);

				if (distance == 0)
					break; // eos

				const int source_pos = static_cast<int>(out_pos) - distance;
				if (source_pos < 0 || out_pos + length > expectedSize)
					break; // invalid back-reference

				for (int i = 0; i < length; i++)
					output[out_pos++] = output[source_pos + i];
			}
		}

		if (out_pos == expectedSize)
			return output;

		output.resize(out_pos);
		return output;
	}

	int decodeLiteral() {
		const int flag = stream.readBits(1);

		if (flag == -1)
			return -1;

		if (flag == 0) {
			if (compressionType == CompressionType::Binary)
				return stream.readBits(8);

			throw std::runtime_error("ASCII/text compression mode is not implemented");
		} else {
			const int peek = stream.peekByte();
			if (peek == -1)
				return -1;

			const auto& posTable2 = getPositionTable2();
			const int index = posTable2[peek];
			const int bits_read = stream.readBits(LEN_BITS[index]);

			if (bits_read == -1)
				return -1;

			const int extra_bits = EX_LEN_BITS[index];
			int symbol_val = index;

			if (extra_bits != 0) {
				const int extra = stream.readBits(extra_bits);

				if (extra == -1 && static_cast<int>(LEN_BASE[index]) + extra != 270)
					return -1;

				symbol_val = static_cast<int>(LEN_BASE[index]) + extra;
			}

			return symbol_val + 256;
		}
	}

	int decodeDistance(int length) {
		if (!stream.ensureBits(8))
			return 0;

		const int peek = stream.peekByte();
		const auto& posTable1 = getPositionTable1();
		const int index = posTable1[peek];

		if (stream.readBits(DIST_BITS[index]) == -1)
			return 0;

		int distance;

		if (length == 2) {
			if (!stream.ensureBits(2)) // special case
				return 0;

			distance = (index << 2) | stream.readBits(2);
		} else {
			if (!stream.ensureBits(dictionarySizeBits))
				return 0;

			distance = (index << dictionarySizeBits) | stream.readBits(dictionarySizeBits);
		}

		return distance + 1; // not zero indexed
	}

private:
	uint8_t compressionType;
	uint8_t dictionarySizeBits;
	BitStream stream;
};

std::vector<uint8_t> pkware_dcl_explode(std::span<const uint8_t> compressedData, size_t expectedLength) {
	PKLibDecompress decompressor(compressedData);
	return decompressor.explode(expectedLength);
}

} // namespace mpq
