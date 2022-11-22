#ifndef TEXT_H
#define TEXT_H

#define DEFAULT_FONT_SIZE 8

typedef enum TextColor {
    kTextColorBlack,
    kTextColorWhite
} TextColor;

// Draw text on the screen at the given position
void textDraw(const char* str, int x, int y, int fontSize, TextColor textColor);

// Draw text on the screen in the given position centered within the bounds given
void textDrawCentered(const char* str, int x, int y, int width, int height, int fontSize, TextColor textColor);

// Get draw height of text in font used by the given font size
int textHeight(int fontSize);

// Get draw width of text in font used by the given font size
int textWidth(const char *str, size_t len, int fontSize);

#endif