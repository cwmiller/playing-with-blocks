#include <stdbool.h>
#include "global.h"
#include "form.h"
#include "text.h"

#define FORM_SEED_CHARACTER_NUM_OPTIONS 16
//#define FORM_FONT_SIZE 14

static char SEED_CHARACTERS[FORM_SEED_CHARACTER_NUM_OPTIONS] = {
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    'A',
    'B',
    'C',
    'D',
    'E',
    'F'
};

/*
 * Initializers
*/

static FormField* initField(FormFieldType type, int x, int y, int width, int height, const char* label, int labelFontSize, int valueFontSize, void* details) {
    FormField* field = SYS->realloc(NULL, sizeof(FormField));
    field->type = type;
    field->x = x;
    field->y = y;
    field->width = width;
    field->height = height;
    field->label = label;
    field->labelFontSize = labelFontSize;
    field->valueFontSize = valueFontSize;
    field->details = details;

    field->focused = false;
    field->focusFrameCount = 0;
    field->focusFlipFlop = false;

    return field;
}

// Initialize a Seed field, used to set a hexadecimal-based seed for the RNG
FormField* formInitSeedField(int x, int y, int width, int height, const char* label, char* value, int labelFontSize, int valueFontSize) {
    FormSeedField* field = SYS->realloc(NULL, sizeof(FormSeedField));
    field->value = value;
    field->editing = false;
    field->editingIndex = 0;

    return initField(kSeed, x, y, width, height, label, labelFontSize, valueFontSize, (void*)field);
}

// Initialize a Numerical field, used to increment/decrement a number
FormField* formInitNumericalField(int x, int y, int width, int height, const char* label, int* value, int minValue, int maxValue, int labelFontSize, int valueFontSize) {
    FormNumericalField* field = SYS->realloc(NULL, sizeof(FormNumericalField));
    field->value = value;
    field->minValue = minValue;
    field->maxValue = maxValue;
    field->editing = false;

    return initField(kNumerical, x, y, width, height, label, labelFontSize, valueFontSize, (void*)field);
}

// Initialize a Boolean field, used as a Yes/No field
FormField* formInitBooleanField(int x, int y, int width, int height, const char* label, bool* value, int labelFontSize, int valueFontSize) {
    FormBooleanField* field = SYS->realloc(NULL, sizeof(FormBooleanField));
    field->value = value;

    return initField(kBoolean, x, y, width, height, label, labelFontSize, valueFontSize, (void*)field);
}

// Initialize a Button field, which will call the handler function when pressed
FormField* formInitButtonField(int x, int y, int width, int height, const char* value, int labelFontSize, int valueFontSize, void* data, FormButtonFieldHandler handler) {
    FormButtonField* field = SYS->realloc(NULL, sizeof(FormButtonField));
    field->value = value;
    field->data = data;
    field->handler = handler;

    return initField(kButton, x, y, width, height, NULL, labelFontSize, valueFontSize, (void*)field);
}

/*
 * Drawing methods
*/

// Draw seed field contents
static bool seedFieldDraw(int x, int y, int width, int height, int fontSize, FormSeedField* field) {
    textDrawCentered(field->value, x, y, width, height, fontSize, kColorBlack);

    // If editing, draw a line under the character being edited
    if (field->editing) {
        // First find where the entire text block's top Y and left X start
        int tWidth = textWidth(field->value, FORM_SEED_FIELD_LENGTH, fontSize);
        int tHeight = textHeight(fontSize);

        int leftX = x + (width / 2) - (tWidth / 2);
        int topY = y + (height / 2) - (tHeight / 2);

        // Y will be simple since it doesn't change depending on which character is being edited
        int lineY = topY + tHeight + 2;

        // Now we need to find the X position where the character being edited begins.
        // This is done by creating a substring of characters up to that character's position.
        // Then we find the width of that substring and use it determine how far in the editing character is.
        char previousStr[FORM_SEED_FIELD_LENGTH + 1];

        // Copy characters into substring
        int i = 0;
        if (field->editingIndex > 0) {
            for (i = 0; i < field->editingIndex; i++) {
                previousStr[i] = field->value[i];
            }
        }

        // Null terminate sub-string just to be on the safe side
        previousStr[i] = '\0';

        // Get drawn width of previous text
        int precedingWidth = textWidth(previousStr, strlen(previousStr), fontSize);

        // We'll also need to get the length of just the character being editing so we know how long the line should be
        int characterWidth = textWidth(&field->value[field->editingIndex], 1, fontSize);

        int lineX = leftX + precedingWidth;

        GFX->drawLine(lineX, lineY, lineX + characterWidth, lineY, 2, kColorBlack);
    }

    return true;
}

// Draw Numerical field contents
static bool numericalFieldDraw(int x, int y, int width, int height, int fontSize, FormNumericalField* field) {
    // Format current number value as a string
    char *str;

    SYS->formatString(&str, "%d", *field->value);

    textDrawCentered(str, x, y, width, height, fontSize, kColorBlack);

    // Draw underline under value if editing
    if (field->editing) {
        int tWidth = textWidth(str, strlen(str), fontSize);
        int tHeight = textHeight(fontSize);

        int leftX = x + (width / 2) - (tWidth / 2);
        int topY = y + (height / 2) - (tHeight / 2);

        GFX->drawLine(leftX, topY + tHeight + 2, leftX + tWidth, topY + tHeight + 2, 2, kColorBlack);
    }

    return true;
}

// Draw boolean field contents
static bool booleanFieldDraw(int x, int y, int width, int height, int fontSize, FormBooleanField* field) {
    textDrawCentered(*field->value ? "On" : "Off", x, y, width, height, fontSize, kColorBlack);

    return true;
}

// Draw button field contents
static bool buttonFieldDraw(int x, int y, int width, int height, int fontSize, FormButtonField* field) {
    textDrawCentered(field->value, x, y, width, height, fontSize, kColorBlack);

    return true;
}

/*
 * Event methods
*/

// Handle keypress on Seed field
static bool seedFieldKeyPress(FormField* field, PDButtons buttons) {
    FormSeedField* seedField = (FormSeedField*)field->details;

    // Pressing A will toggle editing mode
    if ((buttons & kButtonA) == kButtonA) {
        seedField->editing = !seedField->editing;

        // Reset edit index
        seedField->editingIndex = 0;

        // Always suppress button presses for parent when switching editing modes
        return false;
    }

    if (seedField->editing) {
        // Adjust which character is being editing using left and right buttons
        if ((buttons & kButtonRight) == kButtonRight) {
            if (++seedField->editingIndex >= FORM_SEED_FIELD_LENGTH) {
                seedField->editingIndex = FORM_SEED_FIELD_LENGTH - 1;
            }
        } else if ((buttons & kButtonLeft) == kButtonLeft) {
            if (--seedField->editingIndex < 0) {
                seedField->editingIndex = 0;
            }
        // Adjust character above caret when up or down is pressed
        } else if ((buttons & (kButtonUp | kButtonDown)) > 0) {
            char cur = seedField->value[seedField->editingIndex];

            // Find current character in option index
            int idx = 0;
            for (int i = 0; i < FORM_SEED_CHARACTER_NUM_OPTIONS; i++) {
                if (cur == SEED_CHARACTERS[i]) {
                    idx = i;
                    break;
                }
            }

            if ((buttons & kButtonUp) == kButtonUp) {
                if (++idx >= FORM_SEED_CHARACTER_NUM_OPTIONS) {
                    idx = 0;
                }
            } else if ((buttons & kButtonDown) == kButtonDown) {
                if (--idx < 0) {
                    idx = FORM_SEED_CHARACTER_NUM_OPTIONS - 1;
                }
            }

            // Replace character in string
            seedField->value[seedField->editingIndex] = SEED_CHARACTERS[idx];
        }
        
        return false;
    }

    return true;
}

// Handle keypress on Numerical field
static bool numericalFieldKeyPress(FormField* field, PDButtons buttons) {
    FormNumericalField* numericalField = (FormNumericalField*)field->details;

    // Pressing A will toggle editing mode
    if ((buttons & kButtonA) == kButtonA) {
        numericalField->editing = !numericalField->editing;

        return false;
    // Increment value when Up is pressed and in editing mode
    } else if (numericalField->editing && ((buttons & kButtonUp) == kButtonUp)) {
        if (++*numericalField->value > numericalField->maxValue) {
            *numericalField->value = numericalField->minValue;
        }
    // Decrement value when Down is pressed and in editing mode
    } else if (numericalField->editing && ((buttons & kButtonDown) == kButtonDown)) {
        if (--*numericalField->value < numericalField->minValue) {
            *numericalField->value = numericalField->maxValue;
        }
    }

    // Don't allow parent to handle key presses if in editing mode
    return !numericalField->editing;
}

// Handle keypress on Boolean field
static bool booleanFieldKeyPress(FormField* field, PDButtons buttons) {
    // Pressing A will flip value
     if ((buttons & kButtonA) == kButtonA) {
        FormBooleanField* booleanField = (FormBooleanField*)field->details;
        *booleanField->value = !(*booleanField->value);

        return false;
     }

     return true;
}

// Handle keypress on Button field
static bool buttonFieldKeyPress(FormField* field, PDButtons buttons) {
    // If A gets pressed, execute handler function
    if ((buttons & kButtonA) == kButtonA) {
        ((FormButtonField*)field->details)->handler(field);

        return false;
    }

    return true;
}

// Called on a field when a user focuses it 
void formFocusField(FormField* field) {
    field->focused = true;
}

// Called on a field when a user changes focus away from it
void formBlurField(FormField* field) {
    field->focused = false;
}

// Send current keypresses to field
// Returns whether or not the keypresses should bubble up and be processed
bool formHandleButtons(FormField* field, PDButtons buttons) {
    bool ret = true;

    switch (field->type) {
        case kSeed:
            ret = seedFieldKeyPress(field, buttons);
            break;
        case kNumerical:
            ret = numericalFieldKeyPress(field, buttons);
            break;
        case kBoolean:
            ret = booleanFieldKeyPress(field, buttons);
            break;
        case kButton:
            ret = buttonFieldKeyPress(field, buttons);
            break;
    }

    return ret;
}

// Draw the given field, called on every frame
bool formDrawField(FormField* field) {
    if (field->frameCount++ == FPS) {
        field->frameCount = 0;
    }

    bool highlight = false;

    // Switch highlight every 15 frames when focused
    if (field->focused) {
        if (field->focusFrameCount++ == 15) {
            field->focusFrameCount = 0;
            field->focusFlipFlop = !field->focusFlipFlop;
        }

        highlight = !field->focusFlipFlop;
    } else {
        field->focusFrameCount = 0;
        field->focusFlipFlop = false;
    }

    GFX->fillRect(field->x, field->y, field->width, field->height, kColorWhite);
    GFX->drawRect(field->x + 1, field->y + 1, field->width - 2, field->height - 2, kColorBlack);

    // Draws thick border to denote focus
    GFX->drawRect(field->x + 2, field->y + 2, field->width - 4, field->height - 4, highlight ? kColorBlack : kColorWhite);
    GFX->drawRect(field->x + 3, field->y + 3, field->width - 6, field->height - 6, highlight ? kColorBlack : kColorWhite);

    // Draw the label above it if specified
    if (field->label != NULL) {
        int labelHeight = textHeight(field->labelFontSize);
        textDraw(field->label, field->x + 3, field->y - labelHeight - 1, field->labelFontSize, kColorWhite);
    }

    // Draw field contents
    switch (field->type) {
        case kSeed:
            seedFieldDraw(field->x, field->y, field->width, field->height, field->valueFontSize, (FormSeedField*)field->details);
            break;
        case kNumerical:
            numericalFieldDraw(field->x, field->y, field->width, field->height, field->valueFontSize, (FormNumericalField*)field->details);
            break;
        case kBoolean:
            booleanFieldDraw(field->x, field->y, field->width, field->height, field->valueFontSize, (FormBooleanField*)field->details);
            break;
        case kButton:
            buttonFieldDraw(field->x, field->y, field->width, field->height, field->valueFontSize, (FormButtonField*)field->details);
            break;
    }

    return true;
}