#include "core.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

// Platform-specific font file
#ifdef __ANDROID__
const char* FONT_FILE = "fonts/arial.ttf";
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__NT__)
const char* FONT_FILE = "assets\\fonts\\sf-plasmatica-open.ttf";
#elif defined(__APPLE__) || defined(__MACH__)
const char* FONT_FILE = "assets/fonts/sf-plasmatica-open.ttf";
#else
const char* FONT_FILE = "assets/fonts/sf-plasmatica-open.ttf";
#endif