#ifndef TEXT_H
#define TEXT_H

#define DEFAULT_FONT_SIZE 8

// Draw text on the screen at the given position
void textDraw(const char* str, int x, int y, int fontSize);

// Draw text on the screen in the given position centered within the bounds given
void textDrawCentered(const char* str, int x, int y, int width, int height, int fontSize);

#endif