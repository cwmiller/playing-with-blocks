#include "scenes/board/boardScene.h"
#include "optionsScene.h"
#include "global.h"
#include "scene.h"
#include "form.h"
#include "game.h"
#include "rand.h"
#include "text.h"

typedef struct FormValues {
    char seed[FORM_SEED_FIELD_LENGTH + 1];
    int difficulty;
    bool music;
    bool sounds;
} FormValues;

typedef struct OptionsState {
    Form* form;
    FormValues* formValues;
    
    int frameCount;
    int frameCountPerSecond;

    bool transitionToGame;
} OptionsState;

// Handle start button press
static void submitHandler(void* data) {
    // We've attached the current scene state to the button
    OptionsState* state = (OptionsState*)data;

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

    // Once Start is pressed, transition to board scene
    if (state->transitionToGame) {
        state->transitionToGame = false;

        // Convert seed hex value to int
        unsigned int seed = (unsigned int)strtoul(state->formValues->seed, NULL, 16);

        Scene* boardScene = boardSceneCreate(
            seed, 
            state->formValues->difficulty, 
            state->formValues->music, 
            state->formValues->sounds
        );

        gameChangeScene(boardScene);
    } else {
        // Draw form
        if (state->form != NULL) {
            formUpdate(state->form);
        }
    }

    return true;
}

static void destroyScene(Scene* scene) {
    OptionsState *state = (OptionsState*)scene->data;

    // Free form values
    if (state->formValues != NULL) {
        SYS->realloc(state->formValues, 0);
    }

    // Free form
    formDestroy(state->form);

    // Dispose of scene
    SYS->realloc(scene->data, 0);
    SYS->realloc(scene, 0);

    GFX->fillRect(0, 0, LCD_COLUMNS, LCD_ROWS, kColorWhite);
}

void generateSeed(char* dest) {
    // Generate a random seed.. randomly
    rand_seed(SYS->getSecondsSinceEpoch(NULL));

    char* src;

    SYS->formatString(&src, "%02X%02X%02X%02X", 
        rand_next() % 256,
        rand_next() % 256,
        rand_next() % 256,
        rand_next() % 256);

    strcpy_s(dest, FORM_SEED_FIELD_LENGTH + 1, src);

    SYS->realloc(src, 0);
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

    // Create form

    FormValues *values = SYS->realloc(NULL, sizeof(FormValues));
    values->difficulty = 0;
    values->music = true;
    values->sounds = true;

    generateSeed(values->seed);

    state->form = formCreate();
    state->formValues = values;

    formAddField(state->form, formCreateSeedField((Dimensions){ .x = 75, .y = 54, .width = 140, .height = 30 }, "Seed", values->seed, 14, 14));
    formAddField(state->form, formCreateNumericalField((Dimensions){ .x = 245, .y = 54, .width = 80, .height = 30 }, "Level", &values->difficulty, 0, 20, 14, 14));

    formAddField(state->form, formCreateBooleanField((Dimensions){ .x = 75, .y = 114, .width = 80, .height = 30 }, "Music", &values->music, 14, 14));
    formAddField(state->form, formCreateBooleanField((Dimensions) { .x = 245, .y = 114, .width = 80, .height = 30 }, "SFX", &values->sounds, 14, 14));

    FormField* submitBtn = formCreateButtonField((Dimensions) { .x = (LCD_COLUMNS - 140) / 2, .y = 174, .width = 140, .height = 30 }, "Play!", 14, 14, state, submitHandler);

    formAddField(state->form, submitBtn);

    formFocus(state->form, submitBtn);

    state->transitionToGame = false;
    
    return scene;
}