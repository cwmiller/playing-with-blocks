#include "asset.h"
#include "global.h"

LCDBitmap* assetLoadBitmap(const char* path) {
    LCDBitmap* bitmap = NULL;
    const char* error = NULL;

    bitmap = GFX->loadBitmap(path, &error);

    if (error != NULL) {
        SYS->logToConsole("Error loading asset at path '%s': %s", path, error);
    }

    return bitmap;
}

LCDFont* assetLoadFont(const char* path) {
    LCDFont* font = NULL;
    const char* error = NULL;

    font = GFX->loadFont(path, &error);

    if (error != NULL) {
        SYS->logToConsole("Error loading asset at path '%s': %s", path, error);
    }

    return font;
}

AudioSample* assetLoadSample(const char* path) {
    AudioSample* sample = SND->sample->load(path);

    if (sample == NULL) {
        SYS->logToConsole("Error loading asset at path '%s'", path);
    }

    return sample;
}