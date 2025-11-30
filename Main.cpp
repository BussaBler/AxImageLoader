#include "AxImageLoader.h"
#include <string>
#include <zlib.h>
#include <iostream>

int main() {
	//std::string msg;
	//for (int i = 0; i < 10000; ++i) {
	//	msg += "Hello, World! ";
	//}

 //   z_stream strm{};
 //   if (deflateInit2(&strm,
 //       9,               // level
 //       Z_DEFLATED,
 //       15,              // zlib wrapper (CMF/FLG)
 //       8,               // memLevel
 //       Z_DEFAULT_STRATEGY) != Z_OK) { // avoid Z_FIXED
 //       std::cerr << "zlib deflateInit2 failed\n";
 //   }

 //   std::vector<uint8_t> zbuf(compressBound(msg.size()));
 //   strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(msg.data()));
 //   strm.avail_in = static_cast<uInt>(msg.size());
 //   strm.next_out = zbuf.data();
 //   strm.avail_out = static_cast<uInt>(zbuf.size());

 //   int ret = deflate(&strm, Z_FINISH);
 //   if (ret != Z_STREAM_END) {
 //       std::cerr << "zlib deflate failed\n";
 //       deflateEnd(&strm);
 //   }
 //   zbuf.resize(strm.total_out);
 //   deflateEnd(&strm);
 //   auto decompressed = AxImageLoader::decompressData(zbuf);
	//std::string result(decompressed.begin(), decompressed.end());
	//std::cout << "Decompressed message: " << result << std::endl;
	uint32_t width, height;
	uint16_t channels;
	auto a = AxImageLoader::loadImage("test.png", width, height, channels, 4);
	std::cout << "Image loaded: " << width << "x" << height << " Channels: " << channels << std::endl;
}