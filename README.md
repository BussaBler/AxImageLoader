# AxImageLoader

## Introduction
A library for loading images. Intended for use in my game engine project. Currently supports PNG only.

## Build System
This library uses the SCons as it's build system.

Example:
```cpp
#include "AxImageLoader.h"

std::vector<uint8_t> img = AxImageLoader::loadImage("assets/sprite.png", outWidth, outHeight, outChannels, requiredChannels);
```
