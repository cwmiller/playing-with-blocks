#include <stdbool.h>
#include "global.h"
#include "form.h"
#include "text.h"

#define FORM_SEED_CHARACTER_NUM_OPTIONS 16

#define BUTTON_CHARGE_DELAY 19
#define BUTTON_REPEAT_DELAY 7

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

typedef struct FormSeedField {
    char* value;
    // Indicates that the field is active and the current value is able to be edited
    bool isEditing;
    // Points to which index of the seed value is currently being edited
    int focusedIndex;
} FormSeedField;

typedef struct FormNumericalField {
    int* value;
    int minValue;
    int maxValue;
    // Indicates that the field is active and the current value is able to be edited
    bool isEditing;
} FormNumericalField;

typedef struct FormBooleanField {
    bool* value;
} FormBooleanField;

typedef struct FormButtonField {
    const char* value;
    void* data; // Allow for additional data to be attached to the button
    FormButtonFieldHandler handler;
} FormButtonField;

// Create a new form
Form* formCreate() {
    Form* form = SYS->realloc(NULL, sizeof(Form));
    form->fieldListHead = NULL;
    form->fieldListTail = NULL;
    form->focusFrameCount = 0;
    form->focusFlipFlop = false;

    form->btnRepeat.buttons = 0;
    form->btnRepeat.frames = 0;
    form->btnRepeat.isCharged = false;

    return form;
}

// Reset button repeat state
void formResetRepeat(Form* form) {
    form->btnRepeat.buttons = 0;
    form->btnRepeat.frames = 0;
    form->btnRepeat.isCharged = false;
}

// Add a field to a form
void formAddField(Form* form, FormField* field) {
    FormFieldListItem* listItem = SYS->realloc(NULL, sizeof(FormFieldListItem));
    listItem->field = field;
    listItem->next = NULL;

    // Is first item?
    if (form->fieldListHead == NULL) {
        form->fieldListHead = listItem;
        form->fieldListTail = listItem;

        // Make the first item the focused item
        form->focusedFieldItem = listItem;
    } else {
        // Add as tail
        FormFieldListItem* previousTail = form->fieldListTail;
        previousTail->next = listItem;
        form->fieldListTail = listItem;
    }
}

// Find the list item in the field list that contains the given field
static FormFieldListItem* fieldInList(FormFieldListItem* head, FormField* field) {
    for (FormFieldListItem* cur = head; cur != NULL; cur = cur->next) {
        if (cur->field == field) {
            return cur;
        }
    }

    return NULL;
}

// Change focus to a given field
void formFocus(Form* form, FormField* field) {
    FormFieldListItem* listItem = fieldInList(form->fieldListHead, field);

    if (listItem != NULL) {
        form->focusedFieldItem = listItem;

        // Changing focus resets button repeats
        formResetRepeat(form);
    }
}

// Create field for a form
static FormField* createField(FormFieldType type, Dimensions dimensions, const char* label, int labelFontSize, int valueFontSize, void* details) {
    FormField* field = SYS->realloc(NULL, sizeof(FormField));
    field->type = type;
    field->dimensions = dimensions;
    field->label = label;
    field->labelFontSize = labelFontSize;
    field->valueFontSize = valueFontSize;
    field->details = details;
    
    return field;
}

// Destroy and deallocate a form
static void destroyField(FormField* field) {
    // Dealloc field details
    SYS->realloc(field->details, 0);
    SYS->realloc(field, 0);
}

// Create a Seed field, used to set a hexadecimal-based seed for the RNG
FormField* formCreateSeedField(Dimensions dimensions, const char* label, char* value, int labelFontSize, int valueFontSize) {
    FormSeedField* seedField = SYS->realloc(NULL, sizeof(FormSeedField));
    seedField->value = value;
    seedField->isEditing = false;
    seedField->focusedIndex = 0;

    return createField(kSeed, dimensions, label, labelFontSize, valueFontSize, (void*)seedField);
}

// Create a Numerical field, used to increment/decrement a number
FormField* formCreateNumericalField(Dimensions dimensions, const char* label, int* value, int minValue, int maxValue, int labelFontSize, int valueFontSize) {
    FormNumericalField* field = SYS->realloc(NULL, sizeof(FormNumericalField));
    field->value = value;
    field->minValue = minValue;
    field->maxValue = maxValue;
    field->isEditing = false;

    return createField(kNumerical, dimensions, label, labelFontSize, valueFontSize, (void*)field);
}

// Create a Boolean field, used as a Yes/No field
FormField* formCreateBooleanField(Dimensions dimensions, const char* label, bool* value, int labelFontSize, int valueFontSize) {
    FormBooleanField* field = SYS->realloc(NULL, sizeof(FormBooleanField));
    field->value = value;

    return createField(kBoolean, dimensions, label, labelFontSize, valueFontSize, (void*)field);
}

// Create a Button field, which will call the handler function when pressed
FormField* formCreateButtonField(Dimensions dimensions, const char* value, int labelFontSize, int valueFontSize, void* data, FormButtonFieldHandler handler) {
    FormButtonField* field = SYS->realloc(NULL, sizeof(FormButtonField));
    field->value = value;
    field->data = data;
    field->handler = handler;

    return createField(kButton, dimensions, NULL, labelFontSize, valueFontSize, (void*)field);
}

// Draw seed field contents
static void seedFieldDraw(int x, int y, int width, int height, int fontSize, FormSeedField* field) {
    textDrawCentered(field->value, x, y, width, height, fontSize, kColorBlack);

    // If editing, draw a line under the character being edited
    if (field->isEditing) {
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
        if (field->focusedIndex > 0) {
            for (i = 0; i < field->focusedIndex; i++) {
                previousStr[i] = field->value[i];
            }
        }

        // Null terminate sub-string just to be on the safe side
        previousStr[i] = '\0';

        // Get drawn width of previous text
        int precedingWidth = textWidth(previousStr, strlen(previousStr), fontSize);

        // We'll also need to get the length of just the character being editing so we know how long the line should be
        int characterWidth = textWidth(&field->value[field->focusedIndex], 1, fontSize);

        int lineX = leftX + precedingWidth;

        GFX->drawLine(lineX, lineY, lineX + characterWidth, lineY, 2, kColorBlack);
    }
}

// Draw Numerical field contents
static void numericalFieldDraw(int x, int y, int width, int height, int fontSize, FormNumericalField* field) {
    // Format current number value as a string
    char *str;

    SYS->formatString(&str, "%d", *field->value);

    textDrawCentered(str, x, y, width, height, fontSize, kColorBlack);

    // Draw underline under value if editing
    if (field->isEditing) {
        int tWidth = textWidth(str, strlen(str), fontSize);
        int tHeight = textHeight(fontSize);

        int leftX = x + (width / 2) - (tWidth / 2);
        int topY = y + (height / 2) - (tHeight / 2);

        GFX->drawLine(leftX, topY + tHeight + 2, leftX + tWidth, topY + tHeight + 2, 2, kColorBlack);
    }
}

// Draw boolean field contents
static void booleanFieldDraw(int x, int y, int width, int height, int fontSize, FormBooleanField* field) {
    textDrawCentered(*field->value ? "On" : "Off", x, y, width, height, fontSize, kColorBlack);
}

// Draw button field contents
static void buttonFieldDraw(int x, int y, int width, int height, int fontSize, FormButtonField* field) {
    textDrawCentered(field->value, x, y, width, height, fontSize, kColorBlack);
}

// Draw a form field
static void formDrawField(FormField* field, bool isHighlighted) {
    Dimensions* dim = &field->dimensions;

    GFX->fillRect(dim->x, dim->y, dim->width, dim->height, kColorWhite);
    GFX->drawRect(dim->x + 1, dim->y + 1, dim->width - 2, dim->height - 2, kColorBlack);

    // Draws thick border to denote focus
    GFX->drawRect(dim->x + 2, dim->y + 2, dim->width - 4, dim->height - 4, isHighlighted ? kColorBlack : kColorWhite);
    GFX->drawRect(dim->x + 3, dim->y + 3, dim->width - 6, dim->height - 6, isHighlighted ? kColorBlack : kColorWhite);

    // Draw the label above it if specified
    if (field->label != NULL) {
        int labelHeight = textHeight(field->labelFontSize);
        textDraw(field->label, dim->x + 3, dim->y - labelHeight - 1, field->labelFontSize, kColorWhite);
    }

    // Draw field contents
    switch (field->type) {
        case kSeed:
            seedFieldDraw(dim->x, dim->y, dim->width, dim->height, field->valueFontSize, (FormSeedField*)field->details);
            break;
        case kNumerical:
            numericalFieldDraw(dim->x, dim->y, dim->width, dim->height, field->valueFontSize, (FormNumericalField*)field->details);
            break;
        case kBoolean:
            booleanFieldDraw(dim->x, dim->y, dim->width, dim->height, field->valueFontSize, (FormBooleanField*)field->details);
            break;
        case kButton:
            buttonFieldDraw(dim->x, dim->y, dim->width, dim->height, field->valueFontSize, (FormButtonField*)field->details);
            break;
    }
}

// Draw all fields on a form
static void formDrawAllFields(Form *form) {
    if (form->fieldListHead != NULL) {
        for (FormFieldListItem* item = form->fieldListHead; item != NULL; item = item->next) {
            // Switch highlight every 15 frames when focused
            bool isHighlighted = false;

            if (form->focusedFieldItem == item) {
                if (form->focusFrameCount++ == 15) {
                    form->focusFrameCount = 0;
                    form->focusFlipFlop = !form->focusFlipFlop;
                }

                isHighlighted = !form->focusFlipFlop;
            }

            formDrawField(item->field, isHighlighted);
        }
    }
}

// Handle buttons on Seed field
static bool seedFieldHandleButtons(FormField* field, PDButtons buttons) {
    FormSeedField* seedField = (FormSeedField*)field->details;

    // Pressing A will toggle editing mode
    if ((buttons & kButtonA) == kButtonA) {
        seedField->isEditing = !seedField->isEditing;

        // Reset edit index
        seedField->focusedIndex = 0;

        // Always suppress button presses for parent when switching editing modes
        return false;
    }

    if (seedField->isEditing) {
        // Adjust which character is being editing using left and right buttons
        if ((buttons & kButtonRight) == kButtonRight) {
            if (++seedField->focusedIndex >= FORM_SEED_FIELD_LENGTH) {
                seedField->focusedIndex = FORM_SEED_FIELD_LENGTH - 1;
            }
        } else if ((buttons & kButtonLeft) == kButtonLeft) {
            if (--seedField->focusedIndex < 0) {
                seedField->focusedIndex = 0;
            }
        // Adjust character above caret when up or down is pressed
        } else if ((buttons & (kButtonUp | kButtonDown)) > 0) {
            char cur = seedField->value[seedField->focusedIndex];

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
            seedField->value[seedField->focusedIndex] = SEED_CHARACTERS[idx];
        }
        
        return false;
    }

    return true;
}

// Handle buttons on Numerical field
static bool numericalFieldHandleButtons(FormField* field, PDButtons buttons) {
    FormNumericalField* numericalField = (FormNumericalField*)field->details;

    // Pressing A will toggle editing mode
    if ((buttons & kButtonA) == kButtonA) {
        numericalField->isEditing = !numericalField->isEditing;

        return false;
    // Increment value when Up is pressed and in editing mode
    } else if (numericalField->isEditing && ((buttons & kButtonUp) == kButtonUp)) {
        if (++*numericalField->value > numericalField->maxValue) {
            *numericalField->value = numericalField->minValue;
        }
    // Decrement value when Down is pressed and in editing mode
    } else if (numericalField->isEditing && ((buttons & kButtonDown) == kButtonDown)) {
        if (--*numericalField->value < numericalField->minValue) {
            *numericalField->value = numericalField->maxValue;
        }
    }

    // Don't allow parent to handle button presses if in editing mode
    return !numericalField->isEditing;
}

// Handle buttons on Boolean field
static bool booleanFieldHandleButtons(FormField* field, PDButtons buttons) {
    // Pressing A will flip value
     if ((buttons & kButtonA) == kButtonA) {
        FormBooleanField* booleanField = (FormBooleanField*)field->details;
        *booleanField->value = !(*booleanField->value);

        return false;
     }

     return true;
}

// Determine if any of the currently held down buttons should trigger a repeat
static PDButtons formGetRepeatedButtons(Form* form, PDButtons currentButtons) {
    // Only directional buttons can be repeated
    currentButtons &= (kButtonUp | kButtonRight | kButtonDown | kButtonLeft);
    PDButtons repeatButtons = 0;

    // If currently pressed buttons are different than the last ones, reset the counter
    if (form->btnRepeat.buttons != currentButtons) {
        formResetRepeat(form);
    }

    form->btnRepeat.buttons = currentButtons;

    if (currentButtons > 0) {
        form->btnRepeat.frames++;

        if (!form->btnRepeat.isCharged && form->btnRepeat.frames == BUTTON_CHARGE_DELAY) {
            form->btnRepeat.isCharged = true;
            form->btnRepeat.frames = 0;
        } else if (form->btnRepeat.isCharged && form->btnRepeat.frames == BUTTON_REPEAT_DELAY) {
            repeatButtons = currentButtons;

            form->btnRepeat.frames = 0;
        }
    }

    return repeatButtons;
}


// Handle buttons on Button field
static bool buttonFieldHandleButtons(FormField* field, PDButtons buttons) {
    // If A gets pressed, execute handler function
    if ((buttons & kButtonA) == kButtonA) {
        ((FormButtonField*)field->details)->handler(((FormButtonField*)field->details)->data);

        return false;
    }

    return true;
}

// Handle button presses on a field
static bool formHandleButtons(Form* form, PDButtons pressed, PDButtons current) {
    bool bubble = true;
    FormField* field = form->focusedFieldItem->field;

    PDButtons buttons = pressed | formGetRepeatedButtons(form, current);

    switch (field->type) {
        case kSeed:
            bubble = seedFieldHandleButtons(field, buttons);
            break;
        case kNumerical:
            bubble = numericalFieldHandleButtons(field, buttons);
            break;
        case kBoolean:
            bubble = booleanFieldHandleButtons(field, buttons);
            break;
        case kButton:
            bubble = buttonFieldHandleButtons(field, buttons);
            break;
    }

    return bubble;
}

// Blurs the currently focused field and focuses the previous one in the list
static void focusPreviousField(Form* form) {
    if (form->focusedFieldItem != NULL) {
        FormFieldListItem* toFocus = form->fieldListHead;

        for (FormFieldListItem* current = form->fieldListHead; current != NULL; current = current->next) {
            if (current->next == form->focusedFieldItem) {
                toFocus = current;
                break;
            }
        }

        form->focusedFieldItem = toFocus;

        // Reset highlight counter
        form->focusFlipFlop = false;
        form->focusFrameCount = 0;
    }
}

// Blurs the currently focused field and focuses the next one in the list
static void focusNextField(Form* form) {
    if (form->focusedFieldItem != NULL) {
        FormFieldListItem* toFocus = form->focusedFieldItem;

        if (form->focusedFieldItem->next != NULL) {
            toFocus = form->focusedFieldItem->next;
        } else {
            toFocus = form->fieldListTail;
        }

        form->focusedFieldItem = toFocus;

        // Reset highlight counter
        form->focusFlipFlop = false;
        form->focusFrameCount = 0;
    }
}

// Update/draw a form. Should be called every frame
void formUpdate(Form *form) {
    PDButtons current;
    PDButtons pressed;

    SYS->getButtonState(&current, &pressed, NULL);

    // The current focused field is allowed to process all button presses
    // It tells us when we can do other things with button presses, such as move focus
    bool allowButtonPresses = false;

    // Send button presses to the focused field
    if (form->focusedFieldItem != NULL) {
        allowButtonPresses = formHandleButtons(form, pressed, current);
    }

    if (allowButtonPresses) {
        // Progress to next field if down or right is pressed
        if ((pressed & (kButtonDown | kButtonRight)) > 0) {
            focusNextField(form);
        // Progress to previous field if up or left is pressed
        } else if ((pressed & (kButtonUp | kButtonLeft)) > 0) {
            focusPreviousField(form);
        }
    }

    formDrawAllFields(form);
}

// Destroy and deallocate a form
void formDestroy(Form* form) {
    // Dealloc all fields
    if (form->fieldListHead != NULL) {
        FormFieldListItem* next = form->fieldListHead;

        while (next != NULL) {
            FormFieldListItem* tmp = next;
            next = tmp->next;
            destroyField(tmp->field);

            SYS->realloc(tmp, 0);
        }
    }

    // Dealloc the form itself
    SYS->realloc(form, 0);
}