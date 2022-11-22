#include "asset.h"
#include "global.h"
#include "text.h"
#include "pd_api.h"

#define FONT_PATH "fonts/public-pixel/PublicPixel-%dpt"

typedef struct LoadedFont {
   int points;
   LCDFont* font;
   struct LoadedFont* next;
} LoadedFont;

static LoadedFont *loadedHead = NULL;
static LoadedFont *loadedTail = NULL;

// Return a reference to a font based on the size
LCDFont* getFontForSize(int size) {
    // Return font if it's already loaded
    if (loadedHead != NULL) {
        for (LoadedFont* current = loadedHead; current != NULL; current = current->next) {
            if (current->points == size) {
                return current->font;
            }
        }
    }

    // If we're here then the font hasn't been loaded yet
    char* path;
    SYS->formatString(&path, FONT_PATH, size);

    LCDFont* font = assetLoadFont(path);

    // Add the newly loaded font to the list
    if (font != NULL) {
        LoadedFont* loadedFont = SYS->realloc(NULL, sizeof(LoadedFont));
        loadedFont->font = font;
        loadedFont->points = size;
        loadedFont->next = NULL;

        if (loadedTail == NULL) {
            loadedHead = loadedFont;
            loadedTail = loadedFont;
        } else {
            loadedTail->next = loadedFont;
            loadedTail = loadedFont;
        }
    }

    return font;
}

// Draw text on the screen at the given position
void textDraw(const char* str, int x, int y, int fontSize, TextColor textColor) {
    LCDFont* font = getFontForSize(fontSize);

    if (font != NULL) {
        GFX->setFont(font);
        GFX->setDrawMode(textColor == kTextColorBlack ? kDrawModeFillBlack : kDrawModeFillWhite);
        GFX->drawText(str, strlen(str), kASCIIEncoding, x, y);
        GFX->setDrawMode(kDrawModeCopy);
    }
}

// Draw text on the screen in the given position centered within the bounds given
void textDrawCentered(const char* str, int x, int y, int width, int height, int fontSize, TextColor textColor) {
    LCDFont* font = getFontForSize(fontSize);

    if (font != NULL) {
        // Determine the height and width of the rendered text so we can center it within the box
        int textWidth = GFX->getTextWidth(font, str, strlen(str), kASCIIEncoding, 0);
        int textHeight = GFX->getFontHeight(font);

        int centeredX = x + (width / 2) - (textWidth / 2);
        int centeredY = y + (height / 2) - (textHeight / 2);

        // Write the text
        GFX->setFont(font);
        GFX->setDrawMode(textColor == kTextColorBlack ? kDrawModeFillBlack : kDrawModeFillWhite);
        GFX->drawText(str, strlen(str), kASCIIEncoding, centeredX, centeredY);
        GFX->setDrawMode(kDrawModeCopy);
    }
}

// Get draw height of text in font used by the given font size
int textHeight(int fontSize) {
    LCDFont* font = getFontForSize(fontSize);

    return GFX->getFontHeight(font);
}

// Get draw width of text in font used by the given font size
int textWidth(const char *str, size_t len, int fontSize) {
    LCDFont* font = getFontForSize(fontSize);

    return GFX->getTextWidth(font, str, len, kASCIIEncoding, 0);
}