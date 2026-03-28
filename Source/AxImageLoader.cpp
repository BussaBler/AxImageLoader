#include "AxImageLoader.h"
#include "BitReader.h"
#include "HuffmanTree.h"
#include <array>
#include <fstream>
#include <iostream>
#include <numeric>

namespace AxImageLoader {
	// these are deflate spec constants
	const std::array<int, 29> lengthExtraBits = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
	3, 4, 4, 4, 4, 5, 5, 5, 5, 0
	};
	const std::array<int, 29> lengthBase = {
		3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43,
		51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
	};
	const std::array<int, 30> distanceExtraBits = {
		0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
		8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
	};
	const std::array<int, 30> distanceBase = {
		1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257,
		385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385,
		24577
	};
	const std::array<int, 19> codeLengthOrder = {
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
	};

	// PNG related structures and functions
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

	enum class ImageFormat {
		PNG,
		JPEG,
		UNKNOWN
	};

#pragma region PngFunctions
	static bool isPng(const std::vector<uint8_t>& data) {
		static const std::array<uint8_t, 8> pngSignature = { 137, 80, 78, 71, 13, 10, 26, 10 };
		return data.size() >= pngSignature.size() && std::equal(pngSignature.begin(), pngSignature.end(), data.begin());
	}

	static ImageFormat detectFormat(const std::vector<uint8_t>& fileData) {
		if (isPng(fileData)) {
			return ImageFormat::PNG;
		}

		return ImageFormat::UNKNOWN;
	}

	static void inflateBlockData(BitReader& bitReader, const HuffmanTree& litLengthTree, const HuffmanTree& distTree, std::vector<uint8_t>& output) {
		while (true) {
			int symbol = litLengthTree.decode(bitReader);
			if (symbol <= 255) {
				output.push_back(static_cast<uint8_t>(symbol));
			}
			else if (symbol == 256) {
				break;
			}
			else {
				symbol -= 257;
				int length = bitReader.readBits(lengthExtraBits[symbol]) + lengthBase[symbol];
				int distSymbol = distTree.decode(bitReader);
				int distance = bitReader.readBits(distanceExtraBits[distSymbol]) + distanceBase[distSymbol];

				if (distance > output.size()) {
					throw std::runtime_error("Invalid distance in DEFLATE data");
				}

				for (int i = 0; i < length; i++) {
					uint8_t byte = output[output.size() - distance];
					output.push_back(byte);
				}
			}
		}
	}

	static void inflateBlockNoCompression(BitReader& bitReader, std::vector<uint8_t>& output) {
		uint16_t len = bitReader.readBytes(2);
		uint16_t nlne = bitReader.readBytes(2);
		for (int i = 0; i < len; i++) {
			uint8_t byte = bitReader.readByte();
			output.push_back(byte);
		}
	}

	static HuffmanTree blListToHuffmanTree(const std::vector<int>& bitLength, const std::vector<int>& alphabet) {
		int maxBits = *std::max_element(bitLength.begin(), bitLength.end());

		std::vector<int> blCount(maxBits + 1, 0);
		for (int bits : bitLength) {
			if (bits != 0) {
				blCount[bits]++;
			}
		}

		std::vector<int> nextCode(maxBits + 1, 0);
		for (int bits = 2; bits <= maxBits; bits++) {
			nextCode[bits] = (nextCode[bits - 1] + blCount[bits - 1]) << 1;
		}

		HuffmanTree tree;
		size_t iterationSize = std::min(bitLength.size(), alphabet.size());
		for (size_t n = 0; n < iterationSize; n++) {
			int bits = bitLength[n];
			if (bits != 0) {
				int codeword = nextCode[bits];
				tree.insert(codeword, bits, alphabet[n]);
				nextCode[bits]++;
			}
		}
		return tree;
	}

	static void inflateBlockFixed(BitReader& bitReader, std::vector<uint8_t>& output) {
		std::vector<int> literalLengthBl(288);
		std::fill(literalLengthBl.begin(), literalLengthBl.begin() + 144, 8);
		std::fill(literalLengthBl.begin() + 144, literalLengthBl.begin() + 256, 9);
		std::fill(literalLengthBl.begin() + 256, literalLengthBl.begin() + 280, 7);
		std::fill(literalLengthBl.begin() + 280, literalLengthBl.end(), 8);

		std::vector<int> literalLengthAlphabet(286);
		std::iota(literalLengthAlphabet.begin(), literalLengthAlphabet.end(), 0);
		HuffmanTree literalLengthTree = blListToHuffmanTree(literalLengthBl, literalLengthAlphabet);
		std::vector<int> distanceBl(30, 5);
		std::vector<int> distanceAlphabet(30);
		std::iota(distanceAlphabet.begin(), distanceAlphabet.end(), 0);
		HuffmanTree distanceTree = blListToHuffmanTree(distanceBl, distanceAlphabet);
		inflateBlockData(bitReader, literalLengthTree, distanceTree, output);
	}

	static std::pair<HuffmanTree, HuffmanTree> decodeTrees(BitReader& bitReader) {
		int hlit = bitReader.readBits(5) + 257;
		int hdist = bitReader.readBits(5) + 1;
		int hclen = bitReader.readBits(4) + 4;

		std::vector<int> codeLengthTreeBl(19, 0);
		for (int i = 0; i < hclen; i++) {
			codeLengthTreeBl[codeLengthOrder[i]] = bitReader.readBits(3);
		}

		std::vector<int> codeLengthTreeAlphabet(19);
		std::iota(codeLengthTreeAlphabet.begin(), codeLengthTreeAlphabet.end(), 0);
		HuffmanTree codeLengthTree = blListToHuffmanTree(codeLengthTreeBl, codeLengthTreeAlphabet);

		std::vector<int> bl;
		bl.reserve(hlit + hdist);
		while (bl.size() < hlit + hdist) {
			int symbol = codeLengthTree.decode(bitReader);
			if (0 <= symbol && symbol <= 15) {
				bl.push_back(symbol);
			}
			else if (symbol == 16) {
				if (bl.empty()) {
					throw std::runtime_error("Invalid repeat code in code length alphabet");
				}
				int prevCodeLength = bl.back();
				int repeatCount = bitReader.readBits(2) + 3;
				bl.insert(bl.end(), repeatCount, prevCodeLength);
			}
			else if (symbol == 17) {
				int repeatCount = bitReader.readBits(3) + 3;
				bl.insert(bl.end(), repeatCount, 0);
			}
			else if (symbol == 18) {
				int repeatCount = bitReader.readBits(7) + 11;
				bl.insert(bl.end(), repeatCount, 0);
			}
			else {
				throw std::runtime_error("Invalid symbol in code length alphabet");
			}
		}

		std::vector<int> literalLengthBl(bl.begin(), bl.begin() + hlit);
		std::vector<int> distanceBl(bl.begin() + hlit, bl.end());

		std::vector<int> literalLengthAlphabet(286);
		std::iota(literalLengthAlphabet.begin(), literalLengthAlphabet.end(), 0);
		HuffmanTree literalLengthTree = blListToHuffmanTree(literalLengthBl, literalLengthAlphabet);

		std::vector<int> distanceAlphabet(30);
		std::iota(distanceAlphabet.begin(), distanceAlphabet.end(), 0);
		HuffmanTree distanceTree = blListToHuffmanTree(distanceBl, distanceAlphabet);

		return { std::move(literalLengthTree), std::move(distanceTree) };
	}

	static void inflateBlockDynamic(BitReader& bitReader, std::vector<uint8_t>& output) {
		auto trees = decodeTrees(bitReader);
		inflateBlockData(bitReader, trees.first, trees.second, output);
	}

	static uint8_t getSamplesPerPixel(uint8_t colorType) {
		switch (colorType) {
		case 0: return 1; // Grayscale
		case 2: return 3; // Truecolor
		case 3: return 1; // Indexed-color
		case 4: return 2; // Grayscale with alpha
		case 6: return 4; // Truecolor with alpha
		default:
			throw std::runtime_error("Unsupported PNG color type: " + std::to_string(colorType));
		}
	}

	static uint16_t inferChannels(uint8_t colorType, bool hasAlpha) {
		switch (colorType) {
		case 0: // Grayscale
			return hasAlpha ? 2 : 1;
		case 2: // Truecolor
			return hasAlpha ? 4 : 3;
		case 3: // Indexed-color
			return hasAlpha ? 4 : 3;
		case 4: // Grayscale with alpha
			return 2;
		case 6: // Truecolor with alpha
			return 4;
		default:
			throw std::runtime_error("Unsupported PNG color type: " + std::to_string(colorType));
			break;
		}
	}

	static uint8_t paethFilter(uint8_t a, uint8_t b, uint8_t c) {
		int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
		int pa = std::abs(p - static_cast<int>(a));
		int pb = std::abs(p - static_cast<int>(b));
		int pc = std::abs(p - static_cast<int>(c));
		if (pa <= pb && pa <= pc) return a;
		else if (pb <= pc) return b;
		else return c;
	}

	static void unfilterScanline(uint8_t* scanline, const uint8_t* prev, uint32_t rowBytes, uint32_t bpp, uint8_t filter) {
		switch (filter) {
		case 0: // None
			break;
		case 1: // Sub
			for (uint32_t i = bpp; i < rowBytes; i++) {
				scanline[i] = static_cast<uint8_t>(scanline[i] + scanline[i - bpp]);
			}
			break;
		case 2: // Up
			for (uint32_t i = 0; i < rowBytes; i++) {
				scanline[i] = static_cast<uint8_t>(scanline[i] + prev[i]);
			}
			break;
		case 3: // Average
			if (prev) {
				for (uint32_t i = 0; i < bpp; i++) {
					scanline[i] = static_cast<uint8_t>(scanline[i] + (prev[i] >> 1));
				}
				for (uint32_t i = bpp; i < rowBytes; i++) {
					scanline[i] = static_cast<uint8_t>(scanline[i] + ((scanline[i - bpp] + prev[i]) >> 1));
				}
			}
			else {
				for (uint32_t i = bpp; i < rowBytes; i++) {
					scanline[i] = static_cast<uint8_t>(scanline[i] + (scanline[i - bpp] >> 1));
				}
			}
			break;
		case 4: // Paeth
			if (prev) {
				for (uint32_t i = 0; i < bpp; i++) {
					scanline[i] = static_cast<uint8_t>(scanline[i] + paethFilter(0, prev[i], 0));
				}
				for (uint32_t i = bpp; i < rowBytes; i++) {
					scanline[i] = static_cast<uint8_t>(scanline[i] + paethFilter(scanline[i - bpp], prev[i], prev[i - bpp]));
				}
			}
			else {
				for (uint32_t i = bpp; i < rowBytes; i++) {
					scanline[i] = static_cast<uint8_t>(scanline[i] + paethFilter(scanline[i - bpp], 0, 0));
				}
			}
			break;
		default:
			throw std::runtime_error("Invalid PNG filter type: " + std::to_string(filter));
			break;
		}
	}

	static std::vector<uint32_t> unpackSamples(const std::vector<uint8_t>& imageData, uint32_t width, uint8_t samplesPerPixel, uint8_t bitsPerSample) {
		std::vector<uint32_t> samples(width * samplesPerPixel);
		BitReader bitReader(imageData, true);
		uint16_t bytesPerSample = std::max(1, (bitsPerSample + 7) / 8);
		for (uint32_t i = 0; i < width; i++) {
			for (uint8_t s = 0; s < samplesPerPixel; s++) {
				uint32_t index = i * samplesPerPixel + s;
				for (uint16_t b = 0; b < bytesPerSample; b++) {
					samples[index] = bitReader.readBits(bitsPerSample);
				}
			}
		}
		return samples;
	}

	static uint8_t sampleToRGBA8(uint32_t sample, uint8_t bitDepth) {
		if (bitDepth == 8) {
			return static_cast<uint8_t>(sample);
		}
		if (bitDepth == 16) {
			return static_cast<uint8_t>(sample >> 8);
		}
		const uint32_t maxSample = (1 << bitDepth) - 1;
		return static_cast<uint8_t>((sample * 255 + maxSample / 2) / maxSample);
	}

	static std::vector<uint8_t> inflate(BitReader& bitReader) {
		int bfinal = 0;
		std::vector<uint8_t> output;
		while (!bfinal) {
			bfinal = bitReader.readBit();
			int btype = bitReader.readBits(2);
			if (btype == 0) {
				inflateBlockNoCompression(bitReader, output);
			}
			else if (btype == 1) {
				inflateBlockFixed(bitReader, output);
			}
			else if (btype == 2) {
				inflateBlockDynamic(bitReader, output);
			}
			else {
				std::cout << "BTYPE: " << btype << std::endl;
				throw std::runtime_error("Invalid BTYPE in DEFLATE data");
			}
		}
		return output;
	}

	static std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressedData) {
		BitReader r(compressedData);
		uint8_t CMF = r.readByte();
		int CM = CMF & 15;
		if (CM != 8) {
			throw std::runtime_error("Invalid CM (only DEFLATE is supported)");
		}
		int CINFO = (CMF >> 4) & 15;
		if (CINFO > 7) {
			throw std::runtime_error("Invalid CINFO (window size > 32K)");
		}

		uint8_t FLG = r.readByte();
		if ((static_cast<uint16_t>(CMF) * 256 + FLG) % 31 != 0) {
			throw std::runtime_error("CMF+FLG checksum failed");
		}
		int FDICT = (FLG >> 5) & 1;
		if (FDICT) {
			throw std::runtime_error("Preset dictionary not supported");
		}

		std::vector<uint8_t> out = inflate(r);

		// Adler-32 checksum (ignored)
		uint32_t ADLER32 = r.readBytes(4);

		return out;
	}

	static std::vector<PngChunk> readChunks(const std::vector<uint8_t>& pngData) {
		std::vector<PngChunk> chunks;
		size_t offset = 8; // skip PNG signature

		while (offset + 8 <= pngData.size()) {
			PngChunk chunk;
			chunk.lenght = BitReader::combineBytes(pngData[offset], pngData[offset + 1], pngData[offset + 2], pngData[offset + 3]);
			chunk.name = std::string(reinterpret_cast<const char*>(&pngData[offset + 4]), 4);

			offset += 8;
			if (offset + chunk.lenght + 4 > pngData.size()) {
				throw std::runtime_error("Invalid PNG chunk length");
			}

			chunk.data = std::vector<uint8_t>(pngData.begin() + offset, pngData.begin() + offset + chunk.lenght);
			offset += chunk.lenght;
			chunk.crc = BitReader::combineBytes(pngData[offset], pngData[offset + 1], pngData[offset + 2], pngData[offset + 3]);
			offset += 4;
			chunks.push_back(chunk);
		}
		return chunks;
	}

	static std::vector<uint8_t> loadPNG(std::vector<uint8_t>& fileData, uint32_t& outWidth, uint32_t& outHeight, uint16_t& outChannels, uint16_t requiredChannels) {
		auto pngChunks = readChunks(fileData);
		uint32_t width = 0;
		uint32_t height = 0;
		uint8_t bitDepth = 0;
		uint8_t colorType = 0;
		std::vector<uint8_t> compressedImageData;
		std::vector<uint8_t> plteData;
		std::vector<uint8_t> trnsData;
		for (const auto& chunk : pngChunks) {
			if (chunk.name == "IHDR") {
				width = BitReader::combineBytes(chunk.data[0], chunk.data[1], chunk.data[2], chunk.data[3]);
				height = BitReader::combineBytes(chunk.data[4], chunk.data[5], chunk.data[6], chunk.data[7]);
				bitDepth = chunk.data[8];
				colorType = chunk.data[9];
				uint8_t interlaceMethod = chunk.data[12];
			}
			if (chunk.name == "IDAT") {
				compressedImageData.insert(compressedImageData.end(), chunk.data.begin(), chunk.data.end());
			}
			if (chunk.name == "PLTE") {
				plteData = chunk.data;
			}
			if (chunk.name == "tRNS") {
				trnsData = chunk.data;
			}
		}

		if (colorType == 3 && plteData.empty()) {
			throw std::runtime_error("PNG image uses indexed color but has no PLTE chunk");
		}

		PngPalette palette;
		palette.rgb = plteData;
		palette.a = trnsData;
		uint8_t samplesPerPixel = getSamplesPerPixel(colorType);
		outChannels = requiredChannels ? requiredChannels : inferChannels(colorType, palette.a.size() > 0 || colorType == 4 || colorType == 6);
		uint8_t bitsPerSample = bitDepth;
		uint16_t bitsPerPixel = samplesPerPixel * bitsPerSample;
		uint32_t bytesPerRow = (bitsPerPixel * width + 7) / 8;
		uint32_t bytesPerPixel = std::max(1, (bitsPerPixel + 7) / 8);

		std::vector<uint8_t> decompressedImageData = decompress(compressedImageData);
		std::vector<uint8_t> outPixels(width * height * outChannels);
		std::vector<uint8_t> prevScanline(bytesPerRow, 0), currScanline(bytesPerRow, 0);

		size_t offset = 0;
		for (uint32_t y = 0; y < height; y++) {
			if (offset + bytesPerRow + 1 > decompressedImageData.size()) {
				throw std::runtime_error("Decompressed image data is smaller than expected");
			}
			uint8_t filterType = decompressedImageData[offset++];
			std::memcpy(currScanline.data(), &decompressedImageData[offset], bytesPerRow);
			offset += bytesPerRow;
			unfilterScanline(currScanline.data(), prevScanline.data(), bytesPerRow, bytesPerPixel, filterType);
			auto samples = unpackSamples(currScanline, width, samplesPerPixel, bitsPerSample);

			for (uint32_t x = 0; x < width; x++) {
				uint8_t r = 0, g = 0, b = 0, a = 255;
				switch (colorType) {
				case 0: {
					uint8_t v = sampleToRGBA8(samples[x * samplesPerPixel + 0], bitDepth);
					r = g = b = v;
					break;
				}
				case 2: {
					r = sampleToRGBA8(samples[x * samplesPerPixel + 0], bitDepth);
					g = sampleToRGBA8(samples[x * samplesPerPixel + 1], bitDepth);
					b = sampleToRGBA8(samples[x * samplesPerPixel + 2], bitDepth);
					break;
				}
				case 3: {
					uint32_t index = samples[x * samplesPerPixel + 0];
					if (index * 3 + 2 >= palette.rgb.size()) {
						throw std::runtime_error("Palette index out of bounds in PNG image");
					}
					r = palette.rgb[index * 3 + 0];
					g = palette.rgb[index * 3 + 1];
					b = palette.rgb[index * 3 + 2];
					if (index < palette.a.size()) {
						a = palette.a[index];
					}
					break;
				}
				case 4: {
					uint8_t v = sampleToRGBA8(samples[x * samplesPerPixel + 0], bitDepth);
					r = g = b = v;
					a = sampleToRGBA8(samples[x * samplesPerPixel + 1], bitDepth);
					break;
				}
				case 6: {
					r = sampleToRGBA8(samples[x * samplesPerPixel + 0], bitDepth);
					g = sampleToRGBA8(samples[x * samplesPerPixel + 1], bitDepth);
					b = sampleToRGBA8(samples[x * samplesPerPixel + 2], bitDepth);
					a = sampleToRGBA8(samples[x * samplesPerPixel + 3], bitDepth);
					break;
				}
				default:
					throw std::runtime_error("Unsupported PNG color type: " + std::to_string(colorType));
				}

				size_t pixelIndex = (static_cast<size_t>(y) * width + x) * outChannels;
				switch (outChannels) {
				case 1: {
					uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
					outPixels[pixelIndex + 0] = gray;
					break;
				}
				case 2: {
					uint8_t gray = static_cast<uint8_t>(
						0.299f * r + 0.587f * g + 0.114f * b
						);
					outPixels[pixelIndex + 0] = gray;
					outPixels[pixelIndex + 1] = a;
					break;
				}
				case 3: {
					outPixels[pixelIndex + 0] = r;
					outPixels[pixelIndex + 1] = g;
					outPixels[pixelIndex + 2] = b;
					break;
				}
				case 4: {
					outPixels[pixelIndex + 0] = r;
					outPixels[pixelIndex + 1] = g;
					outPixels[pixelIndex + 2] = b;
					outPixels[pixelIndex + 3] = a;
					break;
				}
				default:
					throw std::runtime_error("Unsupported output channel count: " + std::to_string(outChannels));
				}
			}
			std::swap(prevScanline, currScanline);
		}
		outWidth = width;
		outHeight = height;
		return outPixels;
	}
#pragma endregion

	std::expected<Image, std::string> loadImage(const std::filesystem::path& imagePath, uint16_t requiredChannels) {
		if (!std::filesystem::exists(imagePath)) {
			return std::unexpected("Image file does not exist: " + imagePath.string());
		}

		std::ifstream file(imagePath, std::ios::binary);
		if (!file) {
			return std::unexpected("Failed to open image file: " + imagePath.string());
		}

		file.seekg(0, std::ios::end);
		std::streamoff size = file.tellg();
		file.seekg(0, std::ios::beg);
		if (size <= 0) {
			return std::unexpected("Image file is empty: " + imagePath.string());
		}
		std::vector<uint8_t> fileData(static_cast<size_t>(size));
		file.read(reinterpret_cast<char*>(fileData.data()), size);
		if (!file) {
			return std::unexpected("Failed to read image file: " + imagePath.string());
		}

		AxImageLoader::ImageFormat format = detectFormat(fileData);
		Image image = {};
		switch (format) {
		case AxImageLoader::ImageFormat::PNG:
			image.data = loadPNG(fileData, image.width, image.height, image.channels, requiredChannels);
			break;
		case AxImageLoader::ImageFormat::JPEG:
			// TODO: implement JPEG loading
			return std::unexpected("JPEG loading not implemented yet");
			break;
		case AxImageLoader::ImageFormat::UNKNOWN:
			return std::unexpected("Unsupported image format: " + imagePath.string());
			break;
		default:
			return std::unexpected("Unsupported image format: " + imagePath.string());
			break;
		}

		return image;
	}
}