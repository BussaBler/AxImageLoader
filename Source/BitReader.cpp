#include "BitReader.h"

uint8_t BitReader::readByte() {
	numBits = 0;
    if (pos >= mem.size()) {
		throw std::out_of_range("BitReader: readByte out of bounds");
	}
	return mem[pos++];
}

int BitReader::readBit() {
	if (reversed) {
		if (numBits <= 0) {
			b = readByte();
			numBits = 8;
		}
		numBits--;
		int bit = (b >> 7) & 1;
		b <<= 1;
		return bit;
	}

	if (numBits <= 0) {
		b = readByte();
		numBits = 8;
	}
	numBits--;
	int bit = b & 1;
	b >>= 1;
	return bit;
}

uint32_t BitReader::readBits(int n) {
	uint32_t out = 0;
	if (reversed) {
		for (int i = n - 1; i >= 0; i--) {
			out |= (readBit() << i);
		}
		return out;
	}

	for (int i = 0; i < n; i++) {
		out |= (readBit() << i);
	}
	return out;
}

uint32_t BitReader::readBytes(int n) {
	uint32_t out = 0;
	for (int i = 0; i < n; i++) {
		out |= (static_cast<uint32_t>(readByte()) << (i * 8));
	}
	return out;
}

uint32_t BitReader::combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
	return (static_cast<uint32_t>(byte1) << 24) |
		   (static_cast<uint32_t>(byte2) << 16) |
		   (static_cast<uint32_t>(byte3) << 8) |
		   (static_cast<uint32_t>(byte4));
}
