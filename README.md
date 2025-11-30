# AxImageLoader
A simple library for loading images. Intended for use in a game engine project. Currently supports PNG only.

## Status
- Minimal, single-purpose loader
- Works only with PNG images at the moment

## Usage
1. Add the library files to your project or link the library.
2. Include the public header(s) and call the loader to obtain image pixel data.

Example (pseudo-C++):
```cpp
#include "AxImageLoader.h"

Image img = AxImageLoader::LoadFromFile("assets/sprite.png");
// img.pixels, img.width, img.height, img.format
```

## Notes
- No other formats supported yet.
- Designed to be small and easy to integrate into my game engine (probably not the most optimal solution).
