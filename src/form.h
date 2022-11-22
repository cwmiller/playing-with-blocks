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

typedef struct FormField {
    FormFieldType type;
    const char* label;
    int x;
    int y;
    int width;
    int height;
    void* details;
    int frameCount;

    // Used to track focus state and frames for animation
    bool focused;
    int focusFrameCount;
    bool focusFlipFlop;
} FormField;

typedef struct FormSeedField {
    char* value;
    // Indicates that the field is active and the current value is able to be edited
    bool editing;
    // Points to which index of the seed value is currently being edited
    int editingIndex;
} FormSeedField;

typedef struct FormNumericalField {
    int* value;
    int minValue;
    int maxValue;
    // Indicates that the field is active and the current value is able to be edited
    bool editing;
} FormNumericalField;

typedef struct FormBooleanField {
    bool* value;
} FormBooleanField;

typedef void (*FormButtonFieldHandler)(FormField* field);

typedef struct FormButtonField {
    const char* value;
    void* data; // Allow for additional data to be attached to the button
    FormButtonFieldHandler handler;
} FormButtonField;

/*
 * Initializers
 */

// Initialize a Seed field, used to set a hexadecimal-based seed for the RNG
FormField* formInitSeedField(int x, int y, int width, int height, const char* label, char* value);

// Initialize a Numerical field, used to increment/decrement a number
FormField* formInitNumericalField(int x, int y, int width, int height, const char* label, int* value, int minValue, int maxValue);

// Initialize a Boolean field, used as a Yes/No field
FormField* formInitBooleanField(int x, int y, int width, int height, const char* label, bool* value);

// Initialize a Button field, which will call the handler function when pressed
FormField* formInitButtonField(int x, int y, int width, int height, const char* value, void* data, FormButtonFieldHandler handler);

/*
 * Events
 */

// Called on a field when a user focuses it 
void formFocusField(FormField* field);

// Called on a field when a user changes focus away from it
void formBlurField(FormField* field);

// Send current keypresses to field
// Returns whether or not the keypresses should bubble up and be processed
bool formHandleButtons(FormField* field, PDButtons buttons);

/*
 * Drawing
 */

// Draw the given field, called on every frame
bool formDrawField(FormField* field);

#endif