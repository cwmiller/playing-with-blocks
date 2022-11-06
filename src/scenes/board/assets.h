#ifndef SCENES_BOARD_ASSETS_H
#define SCENES_BOARD_ASSETS_H

#include "pd_api.h"

typedef struct BoardSceneBitmapAssets {
    // Background image
    LCDBitmap* background;

    // Block piece cells
    LCDBitmap* blockChessboard;
    LCDBitmap* blockEye;
    LCDBitmap* blockBox;
    LCDBitmap* blockTargetOpen;
    LCDBitmap* blockTargetClosed;
    LCDBitmap* blockTracks;
    LCDBitmap* blockTracksReversed;

    // Gameover animation steps
    LCDBitmap* gameOverOne;
    LCDBitmap* gameOverTwo;
    LCDBitmap* gameOverThree;
    LCDBitmap* gameOverFour;
} BoardSceneBitmapAssets;

typedef struct BoardSceneSampleAssets {
    // Whoop used for piece rotation
    AudioSample* whoop;

    // Kick used for pieces settling & game over
    AudioSample* kick;

    // Perc used for line clear
    AudioSample* perc;
} BoardSceneSampleAssets;

// Load all bitmap assets for board scene
BoardSceneBitmapAssets* loadBitmapAssets(void);

// Load all audio sample assets for board scene
BoardSceneSampleAssets* loadSampleAssets(void);

#endif