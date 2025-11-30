#include "AxImageLoader.h"

const std::array<int, 29> AxImageLoader::lengthExtraBits = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
	3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
const std::array<int, 29> AxImageLoader::lengthBase = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43,
	51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
const std::array<int, 30> AxImageLoader::distanceExtraBits = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
	8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
const std::array<int, 30> AxImageLoader::distanceBase = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257,
	385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385,
	24577
};
const std::array<int, 19> AxImageLoader::codeLengthOrder = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static inline void writeLe16(std::ofstream& f, uint16_t v) {
	f.put(char(v & 0xFF));
	f.put(char((v >> 8) & 0xFF));
}
static inline void writeLe32(std::ofstream& f, uint32_t v) {
	f.put(char(v & 0xFF));
	f.put(char((v >> 8) & 0xFF));
	f.put(char((v >> 16) & 0xFF));
	f.put(char((v >> 24) & 0xFF));
 }
static inline void write_le32s(std::ofstream& f, int32_t v) {
	writeLe32(f, static_cast<uint32_t>(v));
}

static void writeBMP32(const std::string& path, uint32_t width, uint32_t height, const std::vector<uint8_t>& rgba) {
	if (rgba.size() != size_t(width) * size_t(height) * 4)
		throw std::runtime_error("writeBMP32: rgba size mismatch");

	std::ofstream f(path, std::ios::binary);
	if (!f) throw std::runtime_error("writeBMP32: cannot open file");

	const uint32_t rowStride = width * 4; // 32 bits/pixel or 4 bytes/pixel
	const uint32_t pixelDataSize = rowStride * height;
	const uint32_t fileHeaderSize = 14;
	const uint32_t infoHeaderSize = 40;
	const uint32_t pixelDataOff = fileHeaderSize + infoHeaderSize;
	const uint32_t fileSize = pixelDataOff + pixelDataSize;

	// BITMAPFILEHEADER (14 bytes)
	f.put('B'); f.put('M');                // bfType
	writeLe32(f, fileSize);                // bfSize
	writeLe16(f, 0); writeLe16(f, 0);      // bfReserved1/2
	writeLe32(f, pixelDataOff);            // bfOffBits

	// BITMAPINFOHEADER (40 bytes)
	writeLe32(f, infoHeaderSize);          // biSize
	writeLe32(f, width);                   // biWidth
	write_le32s(f, -int32_t(height));      // biHeight (negative = top-down)
	writeLe16(f, 1);                       // biPlanes
	writeLe16(f, 32);                      // biBitCount
	writeLe32(f, 0);                       // biCompression = BI_RGB (no masks)
	writeLe32(f, pixelDataSize);           // biSizeImage
	writeLe32(f, 2835);                    // biXPelsPerMeter
	writeLe32(f, 2835);                    // biYPelsPerMeter
	writeLe32(f, 0);                       // biClrUsed
	writeLe32(f, 0);                       // biClrImportant

	// Invert RGBA to BGRA
	for (uint32_t y = 0; y < height; y++) {
		const uint8_t* row = &rgba[size_t(y) * width * 4];
		for (uint32_t x = 0; x < width; x++) {
			const uint8_t r = row[x * 4 + 0];
			const uint8_t g = row[x * 4 + 1];
			const uint8_t b = row[x * 4 + 2];
			const uint8_t a = row[x * 4 + 3]; // Alpha channel maybe ignored
			f.put(char(b)); f.put(char(g)); f.put(char(r)); f.put(char(a));
		}
	}
	if (!f) throw std::runtime_error("writeBMP32: write error");
}

std::vector<uint8_t> AxImageLoader::loadImage(const std::filesystem::path& imagePath, uint32_t& outWidth, uint32_t& outHeight, uint16_t& outChannels, uint16_t requiredChannels) {
	if (!std::filesystem::exists(imagePath)) {
		throw std::runtime_error("Image file does not exist: " + imagePath.string());
	}
	
	std::ifstream file(imagePath, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Failed to open image file: " + imagePath.string());
	}

	file.seekg(0, std::ios::end);
	std::streamoff size = file.tellg();
	file.seekg(0, std::ios::beg);
	if (size <= 0) {
		throw std::runtime_error("Image file is empty: " + imagePath.string());
	}
	std::vector<uint8_t> fileData(static_cast<size_t>(size));
	file.read(reinterpret_cast<char*>(fileData.data()), size);
	if (!file) {
		throw std::runtime_error("Failed to read image file: " + imagePath.string());
	}

	auto rgba = loadPNG(fileData, outWidth, outHeight, outChannels, requiredChannels);
	
	writeBMP32("output.bmp", outWidth, outHeight, rgba);
}

std::vector<uint8_t> AxImageLoader::loadPNG(std::vector<uint8_t>& fileData, uint32_t& outWidth, uint32_t& outHeight, uint16_t& outChannels, uint16_t requiredChannels) {
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

std::vector<AxImageLoader::PngChunk> AxImageLoader::readChunks(const std::vector<uint8_t>& pngData) {
	std::vector<PngChunk> chunks;
	size_t offset = 8; // Skip PNG signature

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

std::vector<uint8_t> AxImageLoader::decompress(const std::vector<uint8_t>& compressedData) {
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

std::vector<uint8_t> AxImageLoader::inflate(BitReader& bitReader) {
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

void AxImageLoader::inflateBlockNoCompression(BitReader& bitReader, std::vector<uint8_t>& output) {
	uint16_t len = bitReader.readBytes(2);
	uint16_t nlne = bitReader.readBytes(2);
	for (int i = 0; i < len; i++) {
		uint8_t byte = bitReader.readByte();
		output.push_back(byte);
	}
}

void AxImageLoader::inflateBlockFixed(BitReader& bitReader, std::vector<uint8_t>& output) {
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

void AxImageLoader::inflateBlockDynamic(BitReader& bitReader, std::vector<uint8_t>& output) {
	auto trees = decodeTrees(bitReader);
	inflateBlockData(bitReader, trees.first, trees.second, output);
}

void AxImageLoader::inflateBlockData(BitReader& bitReader, const HuffmanTree& litLengthTree, const HuffmanTree& distTree, std::vector<uint8_t>& output) {
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

			for (int i = 0 ; i < length; i++) {
				uint8_t byte = output[output.size() - distance];
				output.push_back(byte);
			}
		}
	}
}

std::pair<HuffmanTree, HuffmanTree> AxImageLoader::decodeTrees(BitReader& bitReader) {
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
	while (bl.size () < hlit + hdist) {
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

HuffmanTree AxImageLoader::blListToHuffmanTree(const std::vector<int>& bitLength, const std::vector<int>& alphabet) {
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

uint8_t AxImageLoader::getSamplesPerPixel(uint8_t colorType) {
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

uint16_t AxImageLoader::inferChannels(uint8_t colorType, bool hasAlpha) {
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

void AxImageLoader::unfilterScanline(uint8_t* scanline, const uint8_t* prev, uint32_t rowBytes, uint32_t bpp, uint8_t filter) {
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
				for	(uint32_t i = bpp; i < rowBytes; i++) {
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

std::vector<uint32_t> AxImageLoader::unpackSamples(const std::vector<uint8_t>& imageData, uint32_t width, uint8_t samplesPerPixel, uint8_t bitsPerSample) {
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

uint8_t AxImageLoader::sampleToRGBA8(uint32_t sample, uint8_t bitDepth) {
	if (bitDepth == 8) {
		return static_cast<uint8_t>(sample);
	}
	if (bitDepth == 16) {
		return static_cast<uint8_t>(sample >> 8);
	}
	const uint32_t maxSample = (1 << bitDepth) - 1;
	return static_cast<uint8_t>((sample * 255 + maxSample / 2) / maxSample);
}
