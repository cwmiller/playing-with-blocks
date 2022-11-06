#include "assets.h"
#include "../../asset.h"
#include "global.h"

// Load bitmap assets
BoardSceneBitmapAssets* loadBitmapAssets() {
    BoardSceneBitmapAssets* assets = SYS->realloc(NULL, sizeof(BoardSceneBitmapAssets));

    if (assets != NULL) {
        assets->background = assetLoadBitmap("images/background.png");
        assets->blockChessboard = assetLoadBitmap("images/blocks/chessboard.png");
        assets->blockEye = assetLoadBitmap("images/blocks/eye.png");
        assets->blockBox = assetLoadBitmap("images/blocks/box.png");
        assets->blockTargetClosed = assetLoadBitmap("images/blocks/target-closed.png");
        assets->blockTargetOpen = assetLoadBitmap("images/blocks/target-open.png");
        assets->blockTracks = assetLoadBitmap("images/blocks/tracks.png");
        assets->blockTracksReversed = assetLoadBitmap("images/blocks/tracks-reversed.png");
        assets->gameOverOne = assetLoadBitmap("images/game-over/1.png");
        assets->gameOverTwo = assetLoadBitmap("images/game-over/2.png");
        assets->gameOverThree = assetLoadBitmap("images/game-over/3.png");
        assets->gameOverFour = assetLoadBitmap("images/game-over/4.png");
    }

    return assets;
}

// Load audio sample assets
BoardSceneSampleAssets* loadSampleAssets() {
    BoardSceneSampleAssets* assets = SYS->realloc(NULL, sizeof(BoardSceneSampleAssets));

    if (assets != NULL) {
        assets->kick = assetLoadSample("sounds/8-bit_kick14.wav");
        assets->perc = assetLoadSample("sounds/8-bit_perc.wav");
        assets->whoop = assetLoadSample("sounds/8-bit_whoop.wav");
    }

    return assets;
}