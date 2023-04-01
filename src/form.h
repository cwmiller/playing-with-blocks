#ifndef FORM_H
#define FORM_H

#include "pd_api.h"

// Length of characters of a Seed field (WITHOUT NULL)
#define FORM_SEED_FIELD_LENGTH 8

typedef enum FormFieldType {
    kSeed,
    kNumerical,
    kBoolean,
    kButton
} FormFieldType;

typedef struct Dimensions {
    int x;
    int y;
    int width;
    int height;
} Dimensions;

typedef struct FormField {
    FormFieldType type;
    Dimensions dimensions;

    const char* label;

    int labelFontSize;
    int valueFontSize;

    // Holds data specific to the field type
    void* details;
} FormField;

typedef struct FormFieldListItem {
    FormField* field;
    struct FormFieldListItem* next;
} FormFieldListItem;


// Houses the state of buttons held for button repeats
typedef struct ButtonRepeatState {
    // Current buttons being held (only holds diretction buttons)
    PDButtons buttons;

    // Whether the button has been pressed long enough to start repeating
    bool isCharged;

    // Current frame count 
    int frames;
} ButtonRepeatState;

typedef struct Form {
    FormFieldListItem* fieldListHead;
    FormFieldListItem* fieldListTail;
    FormFieldListItem* focusedFieldItem;

    ButtonRepeatState btnRepeat;

    int focusFrameCount;
    bool focusFlipFlop;
} Form;

// Create a new form
Form* formCreate();

// Create a Seed field, used to set a hexadecimal-based seed for the RNG
FormField* formCreateSeedField(Dimensions dimensions, const char* label, char* value, int labelFontSize, int valueFontSize);

// Create a Numerical field, used to increment/decrement a number
FormField* formCreateNumericalField(Dimensions dimensions, const char* label, int* value, int minValue, int maxValue, int labelFontSize, int valueFontSize);

// Create a Boolean field, used as a Yes/No field
FormField* formCreateBooleanField(Dimensions dimensions, const char* label, bool* value, int labelFontSize, int valueFontSize);

typedef void (*FormButtonFieldHandler)(void* data);

// Create a Button field, which will call the handler function when pressed
FormField* formCreateButtonField(Dimensions dimensions, const char* value, int labelFontSize, int valueFontSize, void* data, FormButtonFieldHandler handler);

// Add a field to a form
void formAddField(Form* form, FormField* field);

// Change focus to a given field
void formFocus(Form* form, FormField* field);

// Update/draw form. Should be called every frame
void formUpdate(Form* form);

// Destroy and deallocate a form
void formDestroy(Form* form);

#endif