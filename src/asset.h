#ifndef ASSETS_H
#define ASSETS_H

#include "pd_api.h"

// Load an image file into a Bitmap reference
LCDBitmap* assetLoadBitmap(const char* path);

// Load a font file into a Font reference
LCDFont* assetLoadFont(const char* path);

// Load an audio file into an AudioSample reference
AudioSample* assetLoadSample(const char* path);

#endif