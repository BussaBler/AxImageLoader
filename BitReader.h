#pragma once
#include <vector>
#include <stdexcept>
#include <cstdint>

class BitReader {
public:
	BitReader(const std::vector<uint8_t>& data, bool isReversed = false) : mem(data), pos(0), b(0), numBits(0), reversed(isReversed) {}
	~BitReader() = default;

	uint8_t readByte();
	int readBit();
	uint32_t readBits(int n);
	uint32_t readBytes(int n);
	static uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);

private:
	const std::vector<uint8_t>& mem;
	size_t pos;
	uint8_t b;
	int numBits;
	bool reversed; // If set to true, bits are read MSB first
};