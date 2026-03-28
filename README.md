# AxImageLoader

## Introduction
A library for loading images. Intended for use in my game engine project. Currently supports PNG only.

## Build System
This library uses the SCons as it's build system.

> [!NOTE]
> This lib requires version ISO C++23 or newer

Reader usage:
```cpp
#include "AxImageLoader.h"

struct Image {
	std::vector<uint8_t> data;
	uint32_t width;
	uint32_t height;
	uint16_t channels;
};

std::expected<Image, std::string> loadImage(const std::filesystem::path& imagePath, uint16_t requiredChannels = 0);
```
