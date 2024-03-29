#include "titleScene.h"
#include "../board/boardScene.h"
#include "../options/optionsScene.h"
#include "global.h"
#include "asset.h"
#include "scene.h"
#include "game.h"

// Title screen image
static LCDBitmap* titleBitmap = NULL;

static void initScene(Scene* scene) {
    // Load title screen image
    titleBitmap = assetLoadBitmap("images/title.png");

    if (titleBitmap != NULL) {
        GFX->drawBitmap(titleBitmap, 0, 0, kBitmapUnflipped);
    }
}

static bool updateScene(Scene* scene) {
    PDButtons released;

    SYS->getButtonState(NULL, NULL, &released);

    // Press A button to start the game
    if ((released & kButtonA) == kButtonA) {
        gameChangeScene(optionsSceneCreate(true, true));

        return true;
    } else {
        return false;
    }
}

static void destroyScene(Scene* scene) {
    // Dispose of bitmap
    if (titleBitmap != NULL) {
        GFX->freeBitmap(titleBitmap);
    }

    // Dispose of scene
    pd->system->realloc(scene, 0);
}

// Create scene for title screen
Scene* titleSceneCreate(void) {
    Scene* scene = pd->system->realloc(NULL, sizeof(Scene));
    scene->name = "Title Screen";
    scene->init = initScene;
    scene->update = updateScene;
    scene->destroy = destroyScene;

    return scene;
}