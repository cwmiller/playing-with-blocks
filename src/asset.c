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