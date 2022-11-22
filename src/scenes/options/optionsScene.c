#include "scenes/board/boardScene.h"
#include "optionsScene.h"
#include "global.h"
#include "scene.h"
#include "form.h"
#include "game.h"
#include "text.h"

typedef struct FieldListItem {
    FormField* field;
    struct FieldListItem* next;
} FieldListItem;

typedef struct FormValues {
    char* seed;
    int difficulty;
    bool music;
    bool sounds;
} FormValues;

typedef struct OptionsState {
    FieldListItem* fieldListHead;
    FieldListItem* fieldListTail;
    FieldListItem* focusedField;

    FormValues* formValues;
    
    int frameCount;
    int frameCountPerSecond;

    bool transitionToGame;
} OptionsState;

static FieldListItem* addField(OptionsState* state, FormField* field) {
    FieldListItem* listItem = SYS->realloc(NULL, sizeof(FieldListItem));
    listItem->field = field;
    listItem->next = NULL;

    if (state->fieldListTail == NULL) {
        state->fieldListHead = listItem;
        state->fieldListTail = listItem;
    } else {
        state->fieldListTail->next = listItem;
        state->fieldListTail = listItem;
    }

    return listItem;
}

// Blurs the currently focused field and focuses the previous one in the list
static void focusPreviousField(OptionsState* state) {
    if (state->focusedField != NULL) {
        FieldListItem* toBlur = state->focusedField;
        FieldListItem* toFocus = state->fieldListHead;

        for (FieldListItem* current = state->fieldListHead; current != NULL; current = current->next) {
            if (current->next == state->focusedField) {
                toFocus = current;
                break;
            }
        }

        // Notify the fields if focus changed
        if (toBlur->field != toFocus->field) {
            formBlurField(toBlur->field);
            formFocusField(toFocus->field);
        
            state->focusedField = toFocus;
        }
    }
}

// Blurs the currently focused field and focuses the next one in the list
static void focusNextField(OptionsState* state) {
    if (state->focusedField != NULL) {
        FieldListItem* toBlur = state->focusedField;
        FieldListItem* toFocus = state->focusedField;

        if (state->focusedField->next != NULL) {
            toFocus = state->focusedField->next;
        } else {
            toFocus = state->fieldListTail;
        }

        // Notify the fields if focus changed
        if (toBlur->field != toFocus->field) {
            formBlurField(toBlur->field);
            formFocusField(toFocus->field);
        
            state->focusedField = toFocus;
        }
    }
}

// Handle start button press
static void submitHandler(FormField* field) {
    FormButtonField* buttonField = (FormButtonField*)field->details;
    OptionsState* state = (OptionsState*)buttonField->data;

    state->transitionToGame = true;
}

// Called on first frame when scene switches
static void initScene(Scene* scene) {
    OptionsState* state = (OptionsState*)scene->data;

    // Screen uses an black background
    GFX->fillRect(0, 0, LCD_COLUMNS, LCD_ROWS, kColorBlack);
}

// Called on every frame
static bool updateScene(Scene* scene) {
    OptionsState* state = (OptionsState*)scene->data;
    PDButtons buttons;

    // Once Start is pressed, transition to board scene
    if (state->transitionToGame) {
        // Convert seed hex value to int
        unsigned int seed = (unsigned int)strtol(state->formValues->seed, NULL, 16);

        gameChangeScene(boardSceneCreate(seed, state->formValues->difficulty, state->formValues->music, state->formValues->sounds));

        return true;
    }

    SYS->getButtonState(NULL, &buttons, NULL);

    state->frameCount++;

    if (state->frameCountPerSecond++ == FPS) {
        state->frameCountPerSecond = 0;
    }

    // Send key presses to focused field
    // The field will handle the key press and return back if it's okay for us to process the presses to
    bool handleKeyPresses = formHandleButtons(state->focusedField->field, buttons);

    // Respond to button presses if the focused field didn't respond to the keypresses
    if (handleKeyPresses) {
        // Progress to next field if down or right is pressed
        if ((buttons & (kButtonDown | kButtonRight)) > 0) {
            focusNextField(state);
        // Progress to previous field if up or left is pressed
        } else if ((buttons & (kButtonUp | kButtonLeft)) > 0) {
            focusPreviousField(state);
        }
    }

    // Draw all fields
    if (state->fieldListHead != NULL) {
        for (FieldListItem* item = state->fieldListHead; item != NULL; item = item->next) {
            formDrawField(item->field);
        }
    }

    return true;
}

static void destroyScene(Scene* scene) {
    OptionsState *state = (OptionsState*)scene->data;

    // Free form values
    if (state->formValues != NULL) {
        if (state->formValues->seed != NULL) {
            SYS->realloc(state->formValues->seed, 0);
        }

        SYS->realloc(state->formValues, 0);
    }

    // Free field linked list
    if (state->fieldListHead != NULL) {
        FieldListItem* node = state->fieldListHead;

        while (node != NULL) {
            FieldListItem* tmp = node;
            node = node->next;

            SYS->realloc(tmp, 0);
        }
    }

    SYS->realloc(scene->data, 0);

    // Dispose of scene
    SYS->realloc(scene, 0);

    GFX->fillRect(0, 0, LCD_COLUMNS, LCD_ROWS, kColorWhite);
}

// Create scene for Options screen
Scene* optionsSceneCreate(void) {
    Scene* scene = SYS->realloc(NULL, sizeof(Scene));

    scene->name = "Options Screen";
    scene->init = initScene;
    scene->update = updateScene;
    scene->destroy = destroyScene;

    OptionsState* state = SYS->realloc(NULL, sizeof(OptionsState));
    scene->data = (void*)state;

    state->fieldListHead = NULL;
    state->fieldListTail = NULL;
    state->frameCount = 0;
    state->frameCountPerSecond = 0;

    FormValues *values = SYS->realloc(NULL, sizeof(FormValues));
    values->difficulty = 0;
    values->music = true;
    values->sounds = true;

    // Generate a random seed.. randomly
    srand(SYS->getSecondsSinceEpoch(NULL));

    SYS->formatString(&values->seed, "%02X%02X%02X%02X", 
        rand() % 256,
        rand() % 256,
        rand() % 256,
        rand() % 256);

    state->formValues = values;

    addField(state, formInitSeedField(75, 40, 140, 30, "Seed", values->seed));
    addField(state, formInitNumericalField(245, 40, 80, 30, "Level", &values->difficulty, 0, 20));

    addField(state, formInitBooleanField(75, 100, 80, 30, "Music", &values->music));
    addField(state, formInitBooleanField(245, 100, 80, 30, "Sound", &values->sounds));

    state->focusedField = addField(state, formInitButtonField((LCD_COLUMNS - 140) / 2, 160, 140, 30, "Start", state, submitHandler));


    state->focusedField->field->focused = true;

    state->transitionToGame = false;
    
    return scene;
}