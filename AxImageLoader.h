#pragma once
#include "HuffmanTree.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <numeric>
#include <filesystem>

class AxImageLoader {
public:
	static std::vector<uint8_t> loadImage(const std::filesystem::path& imagePath, uint32_t& outWidth, uint32_t& outHeight, uint16_t& outChannel, uint16_t requiredChannels = 0);

private:
	// Things for PNG loading
	struct PngChunk {
		uint32_t lenght;
		std::string name;
		std::vector<uint8_t> data;
		uint32_t crc;

		friend std::ostream& operator<<(std::ostream& os, const PngChunk& chunk) {
			os << "Chunk Name: " << chunk.name << ", Length: " << chunk.lenght << ", CRC: " << chunk.crc;
			return os;
		}
	};
	struct PngPalette {
		std::vector<uint8_t> rgb;
		std::vector<uint8_t> a;
	};

	static std::vector<uint8_t> loadPNG(std::vector<uint8_t>& fileData, uint32_t& outWidth, uint32_t& outHeight, uint16_t& outChannel, uint16_t requiredChannels);
	static std::vector<PngChunk> readChunks(const std::vector<uint8_t>& pngData);
	static std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressedData);
	static std::vector<uint8_t> inflate(BitReader& bitReader);
	static void inflateBlockNoCompression(BitReader& bitReader, std::vector<uint8_t>& output);
	static void inflateBlockFixed(BitReader& bitReader, std::vector<uint8_t>& output);
	static void inflateBlockDynamic(BitReader& bitReader, std::vector<uint8_t>& output);
	static void inflateBlockData(BitReader& bitReader, const HuffmanTree& litLengthTree, const HuffmanTree& distTree, std::vector<uint8_t>& output);
	static std::pair<HuffmanTree, HuffmanTree> decodeTrees(BitReader& bitReader);
	/*
	* @brief Constructs a Huffman tree from the given bit lengths and alphabet.
	*/
	static HuffmanTree blListToHuffmanTree(const std::vector<int>& bitLength, const std::vector<int>& alphabet);
	static uint8_t getSamplesPerPixel(uint8_t colorType);
	static uint16_t inferChannels(uint8_t colorType, bool hasAlpha);
	static inline uint8_t paethFilter(uint8_t a, uint8_t b, uint8_t c) {
		int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
		int pa = std::abs(p - static_cast<int>(a));
		int pb = std::abs(p - static_cast<int>(b));
		int pc = std::abs(p - static_cast<int>(c));
		if (pa <= pb && pa <= pc) return a;
		else if (pb <= pc) return b;
		else return c;
	}
	static void unfilterScanline(uint8_t* scanline, const uint8_t* prev, uint32_t rowBytes,uint32_t bpp, uint8_t filter);
	static std::vector<uint32_t> unpackSamples(const std::vector<uint8_t>&, uint32_t width, uint8_t samplesPerPixel, uint8_t bitDepth);
	static uint8_t sampleToRGBA8(uint32_t sample, uint8_t bitDepth);

private:
	// --- DEFLATE constants ---
	static const std::array<int, 29> lengthExtraBits;
	static const std::array<int, 29> lengthBase;
	static const std::array<int, 30> distanceExtraBits;
	static const std::array<int, 30> distanceBase;
	static const std::array<int, 19> codeLengthOrder;
};

