#include "asset.h"
#include "global.h"
#include "text.h"
#include "pd_api.h"

#define FONT_PATH "fonts/public-pixel/PublicPixel-%dpt"

static LCDFont* fontPoints[8] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

// Return a reference to a font based on the size
LCDFont* getFontForSize(int size) {
    // Check if font for the given size has already been loaded
    if (size < 1 || size > 8) {
        SYS->logToConsole("Invalid font size %d, defaulting to %d", size, DEFAULT_FONT_SIZE);
        size = DEFAULT_FONT_SIZE;
    }

    // Decrement by one to convert to index
    int sizeIndex = size - 1;

    // If font not loaded yet, load it from disk
    if (fontPoints[sizeIndex] == NULL) {
        char* path;
        SYS->formatString(&path, FONT_PATH, size);

        SYS->logToConsole("Loading font");

        fontPoints[sizeIndex] = assetLoadFont(path);

        SYS->realloc(path, 0);
    }

    return fontPoints[sizeIndex];
}

// Draw text on the screen at the given position
void textDraw(const char* str, int x, int y, int fontSize) {
    LCDFont* font = getFontForSize(fontSize);

    if (font != NULL) {
        GFX->setFont(font);
        GFX->drawText(str, strlen(str), kASCIIEncoding, x, y);
    }
}

// Draw text on the screen in the given position centered within the bounds given
void textDrawCentered(const char* str, int x, int y, int width, int height, int fontSize) {
    LCDFont* font = getFontForSize(fontSize);

    if (font != NULL) {
        // Determine the height and width of the rendered text so we can center it within the box
        int textWidth = GFX->getTextWidth(font, str, strlen(str), kASCIIEncoding, 0);
        int textHeight = GFX->getFontHeight(font);

        int centeredX = x + (width / 2) - (textWidth / 2);
        int centeredY = y + (height /2) - (textHeight /2);

        // Write the text
        GFX->setFont(font);
        GFX->drawText(str, strlen(str), kASCIIEncoding, centeredX, centeredY);
    }
}