/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

// ref: https://en.wikipedia.org/wiki/Bzip2

#include "bzip2.h"

#include <array>
#include <algorithm>
#include <stdexcept>
#include <format>

namespace mpq {

static constexpr std::array<int, 512> R_NUMS = {{
	619, 720, 127, 481, 931, 816, 813, 233, 566, 247, 985, 724, 205, 454, 863, 491,
	741, 242, 949, 214, 733, 859, 335, 708, 621, 574, 73, 654, 730, 472, 419, 436,
	278, 496, 867, 210, 399, 680, 480, 51, 878, 465, 811, 169, 869, 675, 611, 697,
	867, 561, 862, 687, 507, 283, 482, 129, 807, 591, 733, 623, 150, 238, 59, 379,
	684, 877, 625, 169, 643, 105, 170, 607, 520, 932, 727, 476, 693, 425, 174, 647,
	73, 122, 335, 530, 442, 853, 695, 249, 445, 515, 909, 545, 703, 919, 874, 474,
	882, 500, 594, 612, 641, 801, 220, 162, 819, 984, 589, 513, 495, 799, 161, 604,
	958, 533, 221, 400, 386, 867, 600, 782, 382, 596, 414, 171, 516, 375, 682, 485,
	911, 276, 98, 553, 163, 354, 666, 933, 424, 341, 533, 870, 227, 730, 475, 186,
	263, 647, 537, 686, 600, 224, 469, 68, 770, 919, 190, 373, 294, 822, 808, 206,
	184, 943, 795, 384, 383, 461, 404, 758, 839, 887, 715, 67, 618, 276, 204, 918,
	873, 777, 604, 560, 951, 160, 578, 722, 79, 804, 96, 409, 713, 940, 652, 934,
	970, 447, 318, 353, 859, 672, 112, 785, 645, 863, 803, 350, 139, 93, 354, 99,
	820, 908, 609, 772, 154, 274, 580, 184, 79, 626, 630, 742, 653, 282, 762, 623,
	680, 81, 927, 626, 789, 125, 411, 521, 938, 300, 821, 78, 343, 175, 128, 250,
	170, 774, 972, 275, 999, 639, 495, 78, 352, 126, 857, 956, 358, 619, 580, 124,
	737, 594, 701, 612, 669, 112, 134, 694, 363, 992, 809, 743, 168, 974, 944, 375,
	748, 52, 600, 747, 642, 182, 862, 81, 344, 805, 988, 739, 511, 655, 814, 334,
	249, 515, 897, 955, 664, 981, 649, 113, 974, 459, 893, 228, 433, 837, 553, 268,
	926, 240, 102, 654, 459, 51, 686, 754, 806, 760, 493, 403, 415, 394, 687, 700,
	946, 670, 656, 610, 738, 392, 760, 799, 887, 653, 978, 321, 576, 617, 626, 502,
	894, 679, 243, 440, 680, 879, 194, 572, 640, 724, 926, 56, 204, 700, 707, 151,
	457, 449, 797, 195, 791, 558, 945, 679, 297, 59, 87, 824, 713, 663, 412, 693,
	342, 606, 134, 108, 571, 364, 631, 212, 174, 643, 304, 329, 343, 97, 430, 751,
	497, 314, 983, 374, 822, 928, 140, 206, 73, 263, 980, 736, 876, 478, 430, 305,
	170, 514, 364, 692, 829, 82, 855, 953, 676, 246, 369, 970, 294, 750, 807, 827,
	150, 790, 288, 923, 804, 378, 215, 828, 592, 281, 565, 555, 710, 82, 896, 831,
	547, 261, 524, 462, 293, 465, 502, 56, 661, 821, 976, 991, 658, 869, 905, 758,
	745, 193, 768, 550, 608, 933, 378, 286, 215, 979, 792, 961, 61, 688, 793, 644,
	986, 403, 106, 366, 905, 644, 372, 567, 466, 434, 645, 210, 389, 550, 919, 135,
	780, 773, 635, 389, 707, 100, 626, 958, 165, 504, 920, 176, 193, 713, 857, 265,
	203, 50, 668, 108, 645, 990, 626, 197, 510, 357, 358, 850, 858, 364, 936, 638
}};

static constexpr int BASE_BLOCK_SIZE = 100000;
static constexpr int MAX_ALPHA_SIZE = 258;
static constexpr int MAX_CODE_LEN = 23;
static constexpr int RUNA = 0;
static constexpr int RUNB = 1;
static constexpr int N_GROUPS = 6;
static constexpr int G_SIZE = 50;
[[maybe_unused]] static constexpr int N_ITERS = 4;
static constexpr int MAX_SELECTORS = 2 + (900000 / G_SIZE);
[[maybe_unused]] static constexpr int NUM_OVERSHOOT_BYTES = 20;

static constexpr std::array<uint32_t, 256> CRC32_TABLE = {{
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
	0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
	0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
	0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
	0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
	0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
	0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
	0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
	0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
	0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
	0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
	0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
	0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
	0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
	0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
	0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
	0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
	0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
}};

static constexpr int START_BLOCK_STATE = 1;
static constexpr int RAND_PART_A_STATE = 2;
static constexpr int RAND_PART_B_STATE = 3;
static constexpr int RAND_PART_C_STATE = 4;
static constexpr int NO_RAND_PART_A_STATE = 5;
static constexpr int NO_RAND_PART_B_STATE = 6;
static constexpr int NO_RAND_PART_C_STATE = 7;

class StrangeCRC {
public:
	StrangeCRC() : globalCrc(-1) {}

	void reset() {
		globalCrc = -1;
	}

	uint32_t value() const {
		return static_cast<uint32_t>(~globalCrc);
	}

	void update(int value) {
		int index = (globalCrc >> 24) ^ value;
		if (index < 0)
			index = 256 + index;

		globalCrc = static_cast<int32_t>((globalCrc << 8) ^ static_cast<int32_t>(CRC32_TABLE[index]));
	}

	void updateBuffer(std::span<const uint8_t> buf, int off = 0, int len = -1) {
		if (len < 0)
			len = static_cast<int>(buf.size());

		for (int i = 0; i < len; i++)
			update(buf[off + i]);
	}

private:
	int32_t globalCrc;
};

class BZip2Exception : public std::runtime_error {
public:
	explicit BZip2Exception(const std::string& message)
		: std::runtime_error(message) {}
};

class BufferInputStream {
public:
	explicit BufferInputStream(std::span<const uint8_t> data)
		: data_(data), position_(0) {}

	int readByte() {
		if (position_ >= data_.size())
			return -1;

		return data_[position_++];
	}

	size_t length() const {
		return data_.size();
	}

	size_t currentPosition() const {
		return position_;
	}

private:
	std::span<const uint8_t> data_;
	size_t position_;
};

class BZip2InputStream {
public:
	explicit BZip2InputStream(BufferInputStream& stream)
		: last(0), origPtr(0), blockSize100k(0), blockRandomised(false),
		  bsBuff(0), bsLive(0),
		  nInUse(0),
		  baseStream(stream), streamEnd(false),
		  currentChar(-1), currentState(START_BLOCK_STATE),
		  storedBlockCRC(0), storedCombinedCRC(0),
		  computedBlockCRC(0), computedCombinedCRC(0),
		  count(0), chPrev(0), ch2(0), tPos(0),
		  rNToGo(0), rTPos(0), i2(0), j2(0), z(0)
	{
		inUse.fill(false);
		seqToUnseq.fill(0);
		unseqToSeq.fill(0);
		selector.fill(0);
		selectorMtf.fill(0);
		unzftab.fill(0);
		minLens.fill(0);

		for (int i = 0; i < N_GROUPS; i++) {
			limit[i].fill(0);
			baseArray[i].fill(0);
			perm[i].fill(0);
		}

		initialize();
		initBlock();
		setupBlock();
	}

	int readByte() {
		if (streamEnd)
			return -1;

		const int current_char = currentChar;

		switch (currentState) {
			case RAND_PART_B_STATE:
				setupRandPartB();
				break;

			case RAND_PART_C_STATE:
				setupRandPartC();
				break;

			case NO_RAND_PART_B_STATE:
				setupNoRandPartB();
				break;

			case NO_RAND_PART_C_STATE:
				setupNoRandPartC();
				break;
		}

		return current_char;
	}

	int read(uint8_t* buffer, int offset, int count) {
		for (int i = 0; i < count; i++) {
			const int byte = readByte();
			if (byte == -1)
				return i;

			buffer[offset + i] = static_cast<uint8_t>(byte);
		}
		return count;
	}

private:
	void initialize() {
		const int c1 = bsGetUChar();
		const int c2 = bsGetUChar();
		const int c3 = bsGetUChar();
		const int c4 = bsGetUChar();

		if (c1 != 'B' || c2 != 'Z' || c3 != 'h' || c4 < '1' || c4 > '9') {
			streamEnd = true;
			throw BZip2Exception("invalid BZip2 header");
		}

		setDecompressStructureSizes(c4 - 0x30);
		computedCombinedCRC = 0;
	}

	void initBlock() {
		const int c1 = bsGetUChar();
		const int c2 = bsGetUChar();
		const int c3 = bsGetUChar();
		const int c4 = bsGetUChar();
		const int c5 = bsGetUChar();
		const int c6 = bsGetUChar();

		// check for end-of-stream marker (0x177245385090)
		if (c1 == 0x17 && c2 == 0x72 && c3 == 0x45 && c4 == 0x38 && c5 == 0x50 && c6 == 0x90) {
			complete();
			return;
		}

		// check for block marker (0x314159265359)
		if (c1 != 0x31 || c2 != 0x41 || c3 != 0x59 || c4 != 0x26 || c5 != 0x53 || c6 != 0x59)
			throw BZip2Exception("Bad BZip2 block header");

		storedBlockCRC = static_cast<uint32_t>(bsGetInt32());
		blockRandomised = bsR(1) == 1;

		getAndMoveToFrontDecode();

		mCrc.reset();
		currentState = START_BLOCK_STATE;
	}

	void endBlock() {
		computedBlockCRC = mCrc.value();

		const uint32_t stored_crc = storedBlockCRC;
		const uint32_t computed_crc = computedBlockCRC;

		if (stored_crc != computed_crc)
			throw BZip2Exception(std::format("BZip2 CRC error. Expected: 0x{:x}, Got: 0x{:x}", stored_crc, computed_crc));

		computedCombinedCRC = ((computedCombinedCRC << 1) | (computedCombinedCRC >> 31)) ^ computedBlockCRC;
	}

	void complete() {
		storedCombinedCRC = static_cast<uint32_t>(bsGetInt32());

		// Convert both to unsigned 32-bit for comparison
		const uint32_t stored_crc = storedCombinedCRC;
		const uint32_t computed_crc = computedCombinedCRC;

		if (stored_crc != computed_crc)
			throw BZip2Exception(std::format("BZip2 combined CRC error. Expected: 0x{:x}, Got: 0x{:x}", stored_crc, computed_crc));

		streamEnd = true;
	}

	void setDecompressStructureSizes(int newSize100k) {
		if (newSize100k < 0 || newSize100k > 9)
			throw BZip2Exception("Invalid block size");

		blockSize100k = newSize100k;

		if (newSize100k == 0)
			return;

		const int length = BASE_BLOCK_SIZE * newSize100k;
		ll8.resize(length, 0);
		tt.resize(length, 0);
	}

	void fillBuffer() {
		const int byte = baseStream.readByte();
		if (byte == -1)
			throw BZip2Exception("Unexpected end of BZip2 stream");

		bsBuff = (bsBuff << 8) | (byte & 0xFF);
		bsLive += 8;
	}

	int bsR(int n) {
		while (bsLive < n)
			fillBuffer();

		const int result = (bsBuff >> (bsLive - n)) & ((1 << n) - 1);
		bsLive -= n;
		return result;
	}

	int bsGetUChar() {
		return bsR(8);
	}

	int32_t bsGetInt32() {
		return static_cast<int32_t>(
			(((((0 << 8 | bsR(8)) << 8 | bsR(8)) << 8 | bsR(8)) << 8 | bsR(8)))
		);
	}

	int bsGetIntVS(int numBits) {
		return bsR(numBits);
	}

	void hbCreateDecodeTables(std::array<int32_t, MAX_ALPHA_SIZE>& limit,
							  std::array<int32_t, MAX_ALPHA_SIZE>& base,
							  std::array<int32_t, MAX_ALPHA_SIZE>& perm_arr,
							  const std::array<int, MAX_ALPHA_SIZE>& length,
							  int minLen, int maxLen, int alphaSize) {
		int pp = 0;

		for (int i = minLen; i <= maxLen; i++) {
			for (int j = 0; j < alphaSize; j++) {
				if (length[j] == i) {
					perm_arr[pp] = j;
					pp++;
				}
			}
		}

		for (int i = 0; i < MAX_CODE_LEN; i++)
			base[i] = 0;

		for (int i = 0; i < alphaSize; i++)
			base[length[i] + 1]++;

		for (int i = 1; i < MAX_CODE_LEN; i++)
			base[i] += base[i - 1];

		for (int i = 0; i < MAX_CODE_LEN; i++)
			limit[i] = 0;

		int vec = 0;
		for (int i = minLen; i <= maxLen; i++) {
			const int nb = base[i + 1] - base[i];
			vec += nb;
			limit[i] = vec - 1;
			vec <<= 1;
		}

		for (int i = minLen + 1; i <= maxLen; i++)
			base[i] = ((limit[i - 1] + 1) << 1) - base[i];
	}

	void recvDecodingTables() {
		std::array<std::array<int, MAX_ALPHA_SIZE>, N_GROUPS> len{};

		std::array<bool, 16> in_use_16{};
		for (int i = 0; i < 16; i++)
			in_use_16[i] = bsR(1) == 1;

		for (int i = 0; i < 16; i++) {
			if (in_use_16[i]) {
				for (int j = 0; j < 16; j++)
					inUse[i * 16 + j] = bsR(1) == 1;
			} else {
				for (int j = 0; j < 16; j++)
					inUse[i * 16 + j] = false;
			}
		}

		makeMaps();
		const int alpha_size = nInUse + 2;

		const int n_groups = bsR(3);
		const int n_selectors = bsR(15);

		for (int i = 0; i < n_selectors; i++) {
			int j = 0;
			while (bsR(1) == 1)
				j++;

			selectorMtf[i] = static_cast<uint8_t>(j);
		}

		// undo mtf
		std::array<uint8_t, N_GROUPS> pos{};
		for (int i = 0; i < n_groups; i++)
			pos[i] = static_cast<uint8_t>(i);

		for (int i = 0; i < n_selectors; i++) {
			const int v = selectorMtf[i];
			const uint8_t tmp = pos[v];
			for (int j = v; j > 0; j--)
				pos[j] = pos[j - 1];

			pos[0] = tmp;
			selector[i] = tmp;
		}

		for (int t = 0; t < n_groups; t++) {
			int curr = bsR(5);
			for (int i = 0; i < alpha_size; i++) {
				while (bsR(1) == 1) {
					if (bsR(1) == 0) {
						curr++;
					} else {
						curr--;
					}
				}
				len[t][i] = curr;
			}
		}

		for (int t = 0; t < n_groups; t++) {
			int min_len = 32;
			int max_len = 0;

			for (int i = 0; i < alpha_size; i++) {
				max_len = std::max(max_len, len[t][i]);
				min_len = std::min(min_len, len[t][i]);
			}

			hbCreateDecodeTables(limit[t], baseArray[t], perm[t], len[t], min_len, max_len, alpha_size);
			minLens[t] = min_len;
		}
	}

	void makeMaps() {
		nInUse = 0;
		for (int i = 0; i < 256; i++) {
			if (inUse[i]) {
				seqToUnseq[nInUse] = static_cast<uint8_t>(i);
				unseqToSeq[i] = static_cast<uint8_t>(nInUse);
				nInUse++;
			}
		}
	}

	void getAndMoveToFrontDecode() {
		std::array<uint8_t, 256> yy{};
		const int block_size = BASE_BLOCK_SIZE * blockSize100k;

		origPtr = bsGetIntVS(24);
		recvDecodingTables();

		const int EOB = nInUse + 1;
		int group_no = -1;
		int group_pos = 0;

		// unzftab
		for (int i = 0; i <= 255; i++)
			unzftab[i] = 0;

		// yy (MTF list)
		for (int i = 0; i <= 255; i++)
			yy[i] = static_cast<uint8_t>(i);

		last = -1;

		if (group_pos == 0) {
			group_no++;
			group_pos = G_SIZE;
		}

		group_pos--;

		const int zt = selector[group_no];
		int zn = minLens[zt];
		int zvec = bsR(zn);

		while (zvec > limit[zt][zn]) {
			if (zn > 20)
				throw BZip2Exception("huffman code length exceeds maximum");

			zn++;
			while (bsLive < 1)
				fillBuffer();

			const int zj = (bsBuff >> (bsLive - 1)) & 1;
			bsLive--;
			zvec = (zvec << 1) | zj;
		}

		if (zvec - baseArray[zt][zn] < 0 || zvec - baseArray[zt][zn] >= MAX_ALPHA_SIZE)
			throw BZip2Exception("huffman decode error");

		int next_sym = perm[zt][zvec - baseArray[zt][zn]];

		while (next_sym != EOB) {
			if (next_sym == RUNA || next_sym == RUNB) {
				int es = -1;
				int N = 1;

				do {
					if (next_sym == RUNA) {
						es += N;
					} else if (next_sym == RUNB) {
						es += 2 * N;
					}

					N <<= 1;

					if (group_pos == 0) {
						group_no++;
						group_pos = G_SIZE;
					}

					group_pos--;

					const int zt2 = selector[group_no];
					int zn2 = minLens[zt2];
					int zvec2 = bsR(zn2);

					while (zvec2 > limit[zt2][zn2]) {
						zn2++;
						while (bsLive < 1)
							fillBuffer();

						const int zj2 = (bsBuff >> (bsLive - 1)) & 1;
						bsLive--;
						zvec2 = (zvec2 << 1) | zj2;
					}

					next_sym = perm[zt2][zvec2 - baseArray[zt2][zn2]];
				} while (next_sym == RUNA || next_sym == RUNB);

				es++;
				const uint8_t ch = seqToUnseq[yy[0]];
				unzftab[ch] += es;

				for (int i = 0; i < es; i++) {
					last++;
					ll8[last] = ch;
				}

				if (last >= block_size)
					throw BZip2Exception("block overrun");
			} else {
				// reg symbol
				last++;
				if (last >= block_size)
					throw BZip2Exception("block overrun");

				const uint8_t tmp = yy[next_sym - 1];
				unzftab[seqToUnseq[tmp]]++;
				ll8[last] = seqToUnseq[tmp];

				// move to front
				for (int j = next_sym - 1; j > 0; j--)
					yy[j] = yy[j - 1];

				yy[0] = tmp;

				if (group_pos == 0) {
					group_no++;
					group_pos = G_SIZE;
				}

				group_pos--;

				const int zt3 = selector[group_no];
				int zn3 = minLens[zt3];
				int zvec3 = bsR(zn3);

				while (zvec3 > limit[zt3][zn3]) {
					zn3++;
					while (bsLive < 1)
						fillBuffer();

					const int zj3 = (bsBuff >> (bsLive - 1)) & 1;
					bsLive--;
					zvec3 = (zvec3 << 1) | zj3;
				}

				next_sym = perm[zt3][zvec3 - baseArray[zt3][zn3]];
			}
		}
	}

	void setupBlock() {
		if (ll8.empty() || tt.empty())
			throw BZip2Exception("block not initialized");

		// Build inverse BWT pointer array
		std::array<int32_t, 257> cftab{};
		cftab[0] = 0;

		for (int i = 1; i <= 256; i++)
			cftab[i] = unzftab[i - 1];

		for (int i = 1; i <= 256; i++)
			cftab[i] += cftab[i - 1];

		for (int i = 0; i <= last; i++) {
			const uint8_t ch = ll8[i];
			tt[cftab[ch]] = i;
			cftab[ch]++;
		}

		tPos = tt[origPtr];
		count = 0;
		i2 = 0;
		ch2 = 256;

		if (blockRandomised) {
			rNToGo = 0;
			rTPos = 0;
			setupRandPartA();
		} else {
			setupNoRandPartA();
		}
	}

	void setupRandPartA() {
		if (i2 <= last) {
			chPrev = ch2;
			ch2 = ll8[tPos];
			tPos = tt[tPos];

			if (rNToGo == 0) {
				rNToGo = R_NUMS[rTPos];
				rTPos++;
				if (rTPos == 512)
					rTPos = 0;
			}
			rNToGo--;
			ch2 ^= (rNToGo == 1) ? 1 : 0;

			i2++;
			currentChar = ch2;
			currentState = RAND_PART_B_STATE;
			mCrc.update(ch2);
		} else {
			endBlock();
			initBlock();
			setupBlock();
		}
	}

	void setupRandPartB() {
		if (ch2 != chPrev) {
			currentState = RAND_PART_A_STATE;
			count = 1;
			setupRandPartA();
		} else {
			count++;
			if (count >= 4) {
				z = ll8[tPos];
				tPos = tt[tPos];

				if (rNToGo == 0) {
					rNToGo = R_NUMS[rTPos];
					rTPos++;
					if (rTPos == 512)
						rTPos = 0;
				}
				rNToGo--;
				z ^= (rNToGo == 1) ? 1 : 0;

				j2 = 0;
				currentState = RAND_PART_C_STATE;
				setupRandPartC();
			} else {
				currentState = RAND_PART_A_STATE;
				setupRandPartA();
			}
		}
	}

	void setupRandPartC() {
		if (j2 < z) {
			currentChar = ch2;
			mCrc.update(ch2);
			j2++;
		} else {
			currentState = RAND_PART_A_STATE;
			i2++;
			count = 0;
			setupRandPartA();
		}
	}

	void setupNoRandPartA() {
		if (i2 <= last) {
			chPrev = ch2;
			ch2 = ll8[tPos];
			tPos = tt[tPos];
			i2++;

			currentChar = ch2;
			currentState = NO_RAND_PART_B_STATE;
			mCrc.update(ch2);
		} else {
			endBlock();
			initBlock();
			setupBlock();
		}
	}

	void setupNoRandPartB() {
		if (ch2 != chPrev) {
			currentState = NO_RAND_PART_A_STATE;
			count = 1;
			setupNoRandPartA();
		} else {
			count++;
			if (count >= 4) {
				z = ll8[tPos];
				tPos = tt[tPos];
				currentState = NO_RAND_PART_C_STATE;
				j2 = 0;
				setupNoRandPartC();
			} else {
				currentState = NO_RAND_PART_A_STATE;
				setupNoRandPartA();
			}
		}
	}

	void setupNoRandPartC() {
		if (j2 < z) {
			currentChar = ch2;
			mCrc.update(ch2);
			j2++;
		} else {
			currentState = NO_RAND_PART_A_STATE;
			i2++;
			count = 0;
			setupNoRandPartA();
		}
	}

	int last;
	int origPtr;
	int blockSize100k;
	bool blockRandomised;

	int bsBuff;
	int bsLive;

	StrangeCRC mCrc;

	std::array<bool, 256> inUse;
	int nInUse;
	std::array<uint8_t, 256> seqToUnseq;
	std::array<uint8_t, 256> unseqToSeq;

	std::array<uint8_t, MAX_SELECTORS> selector;
	std::array<uint8_t, MAX_SELECTORS> selectorMtf;

	std::vector<int32_t> tt;
	std::vector<uint8_t> ll8;

	std::array<int32_t, 256> unzftab;
	std::array<std::array<int32_t, MAX_ALPHA_SIZE>, N_GROUPS> limit;
	std::array<std::array<int32_t, MAX_ALPHA_SIZE>, N_GROUPS> baseArray;
	std::array<std::array<int32_t, MAX_ALPHA_SIZE>, N_GROUPS> perm;
	std::array<int32_t, N_GROUPS> minLens;

	BufferInputStream& baseStream;
	bool streamEnd;

	int currentChar;
	int currentState;

	uint32_t storedBlockCRC;
	uint32_t storedCombinedCRC;
	uint32_t computedBlockCRC;
	uint32_t computedCombinedCRC;

	int count;
	int chPrev;
	int ch2;
	int tPos;
	int rNToGo;
	int rTPos;
	int i2;
	int j2;
	int z;
};

std::vector<uint8_t> bzip2_decompress(std::span<const uint8_t> compressed_data, size_t expected_length) {
	BufferInputStream input_stream(compressed_data);
	BZip2InputStream decompressor(input_stream);

	std::vector<uint8_t> output_chunks;
	size_t total_length = 0;

	const size_t chunk_size = expected_length > 0 ? std::min(expected_length, static_cast<size_t>(65536)) : 65536;

	while (true) {
		std::vector<uint8_t> chunk(chunk_size, 0);
		size_t bytes_read = 0;

		// Read into chunk
		for (size_t i = 0; i < chunk_size; i++) {
			const int byte = decompressor.readByte();
			if (byte == -1)
				break;

			chunk[i] = static_cast<uint8_t>(byte);
			bytes_read++;
		}

		if (bytes_read == 0)
			break;

		output_chunks.insert(output_chunks.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(bytes_read));
		total_length += bytes_read;

		if (bytes_read < chunk_size)
			break;
	}

	return output_chunks;
}

} // namespace mpq
