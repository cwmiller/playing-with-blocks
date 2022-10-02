#ifndef ASSETS_H
#define ASSETS_H

#include "pd_api.h"

LCDBitmap* assetLoadBitmap(const char* path);
LCDFont* assetLoadFont(const char* path);
AudioSample* assetLoadSample(const char* path);

#endif