#include "game.h"
#include "scene.h"
#include "scenes/title/titleScene.h"
#include "global.h"
#include "pd_api.h"

typedef enum {
    Running,
    Paused,
    SceneTransition
} RunStatus;

typedef struct {
    RunStatus status;
    Scene* currentScene;
    Scene* nextScene;
} GameState;

// Global variable exposed in global.h
PlaydateAPI* pd = NULL;

// Private function prototypes
static int gameUpdate(void*);

// Private variables
static GameState* gameState = NULL;

// Initializes game state and loop
void gameInit(PlaydateAPI* playdate) {
    // Initialize global Playdate variable
    pd = playdate;

    // Setup game loop
    pd->display->setRefreshRate(FPS);
    SYS->setUpdateCallback(gameUpdate, NULL);

    // Initial game state structure
    gameState = SYS->realloc(NULL, sizeof(GameState));
    gameState->status = SceneTransition;
    gameState->currentScene = NULL;
    gameState->nextScene = titleSceneCreate();
}

// Main game loop called on each frame
static int gameUpdate(void* userdata) {
    (void)userdata;

    switch (gameState->status) {
        case Running:
            gameState->currentScene->update(gameState->currentScene);
            break;

        case Paused:
            // TODO
            break;

        case SceneTransition:
            // Dispose of previous scene (if it exists)
            // It's assumed the destroy callback will dipose of the Scene struct
            if (gameState->currentScene != NULL) {
                SYS->logToConsole("Destroying scene '%s'", gameState->currentScene->name);

                gameState->currentScene->destroy(gameState->currentScene);
            }

            // Move pending scene to current scene and call its initializer
            if (gameState->nextScene != NULL) {
                SYS->logToConsole("Switching to scene '%s'", gameState->nextScene->name);

                gameState->currentScene = gameState->nextScene;
                gameState->nextScene = NULL;

                gameState->currentScene->init(gameState->currentScene);
            }

            gameState->status = Running;
            break;
    }

    return 1;
}

// Transition to a new scene. Current scene will be terminated and the new one will be displayed on next frame. 
void gameChangeScene(Scene* scene) {
    // Do not allow a scene change if one is already in progress
    if (gameState->status != SceneTransition) {
        SYS->logToConsole("Received transition request to scene '%s'", scene->name);

        gameState->nextScene = scene;
        gameState->status = SceneTransition;
    } else {
        SYS->logToConsole("Attempted to transition to new scene '%s' while already in a transition.");
    }
}