#pragma once
#include <filesystem>
#include <expected>
#include <vector>

namespace AxImageLoader {
	struct Image {
		std::vector<uint8_t> data;
		uint32_t width;
		uint32_t height;
		uint16_t channels;
	};
	std::expected<Image, std::string> loadImage(const std::filesystem::path& imagePath, uint16_t requiredChannels = 0);
}