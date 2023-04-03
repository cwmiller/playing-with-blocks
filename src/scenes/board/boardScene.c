#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include "pd_api.h"
#include "../options/optionsScene.h"
#include "../title/titleScene.h"
#include "boardScene.h"
#include "assets.h"
#include "matrix.h"
#include "game.h"
#include "asset.h"
#include "global.h"
#include "text.h"
#include "form.h"
#include "rand.h"

#define PIECE_HEIGHT 30
#define PIECE_WIDTH 20

#define MAX_DIFFICULTY 20

#define DAS_CHARGE_DELAY 19
#define DAS_REPEAT_DELAY 7

#define SOFTDROP_GRAVITY 2

#define NEXT_BOX_X 38
#define NEXT_BOX_Y 25
#define NEXT_BOX_WIDTH 69
#define NEXT_BOX_HEIGHT 32

#define LEVEL_BOX_X 38
#define LEVEL_BOX_Y 93
#define LEVEL_BOX_WIDTH 69
#define LEVEL_BOX_HEIGHT 15

#define LINES_BOX_X 293
#define LINES_BOX_Y 93
#define LINES_BOX_WIDTH 69
#define LINES_BOX_HEIGHT 15

#define SCORE_BOX_X 293
#define SCORE_BOX_Y 25
#define SCORE_BOX_WIDTH 69
#define SCORE_BOX_HEIGHT 15

#define SEED_BOX_X 293
#define SEED_BOX_Y 161
#define SEED_BOX_WIDTH 69
#define SEED_BOX_HEIGHT 15

#define GAMEOVER_FONT_SIZE 18

#define BUTTON_Y SEED_BOX_Y
#define BUTTON_X MATRIX_GRID_LEFT_X(0) - (MATRIX_GRID_CELL_SIZE / 2)
#define BUTTON_HEIGHT (int)(MATRIX_GRID_CELL_SIZE * 2.5)
#define BUTTON_WIDTH MATRIX_WIDTH + MATRIX_GRID_CELL_SIZE

// Limit score to 999,999
#define MAX_SCORE 999999

// Score is calculated based on the number of lines completed in one drop & the current difficulty
static int SCORING[4] = {
    40,
    100,
    300,
    1200
};

#define ARE_FRAMES 2
#define LINECLEAR_FRAMES 77
#define TOPOUT_FRAMES 45

typedef enum Status {
    // Lasts 1 frame, piece(s) are selected and the active piece is placed at the top of the screen
    Start,

    // Lasts 2 frames where the active piece is idle at the top of the screen
    ARE,

    // Piece is dropping and the player has control
    Dropping,

    // Piece has settled into a spot
    Settled,

    // Player has completed at least one line that needs cleared out
    LineClear,

    // Player has reached the top and the game must end
    // Fills play area with blocks
    // Lasts 45 frames then switches to GameOver state
    TopOut,

    // Display game over text and buttons for restarting game
    GameOver
} Status;

// Houses a list of completed rows during a round
typedef struct CompletedRows {
    int rows[4];
    int numRows;
} CompletedRows;

// Houses the state of keys held for DAS
typedef struct DasState {
    // Current key being held (kButtonLeft, kButtonRight, zero)
    int key;
    // Whether the key has been pressed long enough to start repeating
    bool charged;
    // Current DAS frame count.
    int frames;
} DasState;

// Contains all the current state for the scene
typedef struct SceneState {
    // The random seed used by piece picker
    unsigned int seed;
    int initialDifficulty;
    bool music;
    bool sounds;

    PDMenuItem* musicMenuItem;
    PDMenuItem* soundsMenuItem;

    Status status;

    // Counts the number of frames for the current status
    unsigned int statusFrames;
    
    int difficulty;
    int completedLines;

    int score;

    unsigned int gravityFrames;

    // Holds the state of each cell in the matrix
    MatrixGrid matrix;

    // Current player piece being controlled
    Piece playerPiece;

    // Current position of the player piece. X/Y coords are the top-left block of a piece and can be negative
    Position playerPosition;

    Piece standbyPiece;

    DasState das;

    // Toggled when DOWN is pressed for a piece
    // Used to prevent soft-drop from continuing to the next piece
    bool softDropInitiated;
    // Keeps track of the row the soft drop started being held
    // Used for scoring
    int softDropStartingRow;

    // Toggle when UP is pressed
    bool hardDropInitiated;
    // Keeps track of the row where the hard drop begins
    // Used for scoring
    int hardDropStartingRow;

    // Tracks which rows were completed during LineClear state
    CompletedRows roundCompletedRows;

    // Form that is displayed on game over screen
    Form* gameOverForm;
} SceneState;

// How many frames per row a piece drops from gravity
static int DIFFICULTY_LEVELS[21] = {
    44,
    41,
    37,
    34,
    31,
    27,
    23,
    18,
    14,
    9,
    8,
    7,
    7,
    6,
    5,
    5,
    4,
    4,
    3,
    3,
    2
};

// Assets
static BoardSceneBitmapAssets* bitmapAssets = NULL;
static BoardSceneSampleAssets* sampleAssets  = NULL;

// Music player
static FilePlayer* musicPlayer = NULL;

// Sound effects
static SamplePlayer* samplePlayer = NULL;

// Function prototpes

static void initAudioPlayers(void);
static void loadAssets(void);

// Frame update handlers
static bool updateSceneStart(SceneState* state);
static bool updateSceneAre(SceneState* state);
static bool updateSceneDropping(SceneState* state);
static bool updateSceneSettled(SceneState* state);
static bool updateSceneLineClear(SceneState* state);
static bool updateSceneTopOut(SceneState* state);
static bool updateSceneGameOver(SceneState* state);

static void changeStatus(SceneState* state, Status status);

static void updateDasCounts(DasState* state, PDButtons buttons);
static int dasRepeatCheck(DasState* state);

static void drawMatrix(MatrixGrid matrix, bool forceFull);

static LCDBitmap* blockBitmapForPiece(Piece piece);

static Position determineDroppedPosition(const MatrixGrid matrix, Piece piece, Position pos);

static int difficultyForLines(int initialDifficulty, int completedLines);
static inline int gravityFramesForDifficulty(int difficulty);

static void drawAllBoxes(SceneState* state);
static void drawBoxText(const char* text, int x, int y, int width, int height);
static void drawBoxPiece(Piece piece, int x, int y, int width, int height);

static CompletedRows getCompletedRows(const MatrixGrid matrix);
static bool canSettlePiece(const MatrixGrid matrix, Piece piece, Position pos);

static void playMusic(SceneState* state);
static void stopMusic(void);
static bool isMusicPlaying(void);

static void playSample(SceneState* state, AudioSample* sample);

static int incrementScore(int current, int add);

static void handleMusicMenu(SceneState* state);
static void handleSoundMenu(SceneState* state);
static void handleEndGameMenu(SceneState* state);


// Handle when scene becomes active
static void initScene(Scene* scene) {
    SceneState* state = (SceneState*)scene->data;

    // Seed the random number generator
    rand_seed(state->seed);

    initAudioPlayers();
    loadAssets();

    // Clear screen 
    GFX->clear(kColorWhite);

    // Draw background
    if (bitmapAssets != NULL && bitmapAssets->background != NULL) {
        GFX->drawBitmap(bitmapAssets->background, 0, 0, kBitmapUnflipped);
    }

    matrixClear(state->matrix);
    drawMatrix(state->matrix, false);

    // Start playing music and loop forever
    playMusic(state);

    // Add menu items
    SYS->removeAllMenuItems();
    state->musicMenuItem = SYS->addCheckmarkMenuItem("Music", state->music ? 1 : 0, handleMusicMenu, state);
    state->soundsMenuItem = SYS->addCheckmarkMenuItem("Sound", state->sounds ? 1 : 0, handleSoundMenu, state);
    SYS->addMenuItem("End Game", handleEndGameMenu, state);
}

// Called on every frame while scene is active
static bool updateScene(Scene* scene) {
    SceneState* state = (SceneState*)scene->data;
    PDButtons buttons;

    SYS->getButtonState(&buttons, NULL, NULL);

    updateDasCounts(&(state->das), buttons);

    bool screenUpdated = false;

    switch (state->status) {
        case Start:
            screenUpdated = updateSceneStart(state);
            break;

        case ARE:
            screenUpdated = updateSceneAre(state);
            break;

        case Dropping:
            screenUpdated = updateSceneDropping(state);
            break;

        case Settled:
            screenUpdated = updateSceneSettled(state);
            break;

        case LineClear:
            screenUpdated = updateSceneLineClear(state);
            break;

        case TopOut:
            screenUpdated = updateSceneTopOut(state);
            break;

        case GameOver:
            screenUpdated = updateSceneGameOver(state);
            break;
    }

    //SYS->drawFPS(0, 0);

    return screenUpdated;
}

// Called on frame update when in the "Start" state status
// Only runs for 1 frame and sets the active player piece
static bool updateSceneStart(SceneState* state) {
    // On the first time this is called both player and standby pieces need to be picked
    if (state->standbyPiece != None) {
        state->playerPiece = state->standbyPiece;
    } else {
        state->playerPiece = rand_next() % 7;
    }

    // Randomly select the next piece in line
    state->standbyPiece = rand_next() % 7;

    // Place the player piece up top the matrix in the default orientation
    state->playerPosition.col = 4;
    state->playerPosition.row = 0;
    state->playerPosition.orientation = 0;

    Position playerPos = state->playerPosition;

    MatrixPiecePoints playerPoints = matrixGetPointsForPiece(state->playerPiece, playerPos.col, playerPos.row, playerPos.orientation);

    // A top out occurs when the player piece's starting position overlaps a piece on the board
    bool canPlotPoints = matrixPointsAvailable(state->matrix, &playerPoints);

    // Draw the new player piece even if it overwrites an existing piece
    matrixAddPiecePoints(state->matrix, state->playerPiece, true, &playerPoints);

    drawMatrix(state->matrix, false);

    if (!canPlotPoints) {
        changeStatus(state, TopOut);
    } else {
        // Adjust difficulty based off how many lines have been completed
        state->difficulty = difficultyForLines(state->initialDifficulty, state->completedLines);

        // Update all boxes
        drawAllBoxes(state);

        // Set gravity based on current difficulty
        state->gravityFrames = gravityFramesForDifficulty(state->difficulty);

        // Reset soft drop
        state->softDropInitiated = false;
        state->softDropStartingRow = 0;

        // Reset hard drop
        state->hardDropInitiated = false;
        state->hardDropStartingRow = 0;
        
        // After a piece is selected, switch to ARE state
        changeStatus(state, ARE);
    }

    return true;
}

// Called on frame update when in the "ARE" state status
// Only runs for 2 frames and is just to allow the piece to float before giving player control
static bool updateSceneAre(SceneState* state) {
    if (++(state->statusFrames) == ARE_FRAMES) {
        changeStatus(state, Dropping);
    }

    return false;
}

// Called on frame update when in the "Dropping" state status
// In this state the piece drops from gravity and is controllable by the player
static bool updateSceneDropping(SceneState* state) {
    bool enforceGravity = false;
    bool screenUpdated = false;

    PDButtons currentKeys;
    PDButtons pressedKeys;
    
    SYS->getButtonState(&currentKeys, &pressedKeys, NULL);

    // If DOWN is newly pressed, force soft drop gravity
    // Ignore if any other direction button is pressed too
    if ((pressedKeys & 0xF) == kButtonDown) {
        state->gravityFrames = SOFTDROP_GRAVITY;
        
        if (!state->softDropInitiated) {
            state->softDropInitiated = true;
            state->softDropStartingRow = state->playerPosition.row;
        }
    }

    // Enforce gravity when counter expires and reset it
    if (--state->gravityFrames == 0) {
        enforceGravity = true;

        // If DOWN is being held, override gravity for soft drop
        // Ignore if any other direction button is pressed too
        if (state->softDropInitiated && ((currentKeys & 0xF) == kButtonDown)) {
            state->gravityFrames = SOFTDROP_GRAVITY;
        } else {
            state->gravityFrames = gravityFramesForDifficulty(state->difficulty);
            state->softDropInitiated = false;
        }
    }

    // Allow DAS to move piece left or right
    int dasRepeatKey = dasRepeatCheck(&(state->das));

    if (enforceGravity || (pressedKeys > 0) || (dasRepeatKey > 0)) {
        // The current player piece position
        Position currentPos = state->playerPosition;

        // The position the player & gravity are trying to move the player piece to
        Position attemptedPos = currentPos;

        // The final position where the player piece will actually be moved to
        Position finalPos = attemptedPos;

        // A piece gets settled in place when it tries to fall into another piece or the floor
        int shouldSettle = false;

        // If UP is pressed, immediately drop piece and settle it
        if ((pressedKeys & kButtonUp) == kButtonUp) {
            finalPos = determineDroppedPosition(state->matrix, state->playerPiece, finalPos);
            shouldSettle = true;

            // Keep track of where the piece was when the soft drop was initiated so it can be scored after it settles
            state->hardDropInitiated = true;
            state->hardDropStartingRow = state->playerPosition.row;
        } else {
            // Adjust player movement attempt based on which button is pressed OR if DAS is in effect

            if ((dasRepeatKey | (pressedKeys & kButtonRight)) == kButtonRight) {
                attemptedPos.col++;
            } else if ((dasRepeatKey | (pressedKeys & kButtonLeft)) == kButtonLeft) {
                attemptedPos.col--;
            }

            // Pressing down with move the block down one row
            // The piece will also be pushed down if gravity is taking effect
            if (enforceGravity || ((pressedKeys & kButtonDown) == kButtonDown)) {
                attemptedPos.row++;
            }

            // Pressing A buttons adjusts orientation right
            if ((pressedKeys & kButtonA) == kButtonA) {
                if (++attemptedPos.orientation > 3) {
                    attemptedPos.orientation = 0;
                }
            }

            // Pressing B buttons adjusts orientation left
            if ((pressedKeys & kButtonB) == kButtonB) {
                if (--attemptedPos.orientation < 0) {
                    attemptedPos.orientation = 3;
                }
            }

            // Intersecting another block (or the bottom of the playfield) while the piece is moving down will cause it to settle in its current place
            if (attemptedPos.row > currentPos.row && canSettlePiece(state->matrix, state->playerPiece, currentPos)) {
                shouldSettle = true;
            } else {
                // The piece is still in play and we must determine if the piece can move to where it's being asked to go
                MatrixPiecePoints pointsForAttempt = matrixGetPointsForPiece(state->playerPiece, attemptedPos.col, attemptedPos.row, attemptedPos.orientation);

                // If any of the points for the attempted move are cells that are already filled, then the player can't move there.
                bool canPlotPoints = matrixPointsAvailable(state->matrix, &pointsForAttempt);

                // For a legal move, all 4 visible points of the piece must be plottable on the matrix and not already filled
                if (pointsForAttempt.numPoints == 4 && canPlotPoints) {
                    finalPos = attemptedPos;
                } else if (enforceGravity) {
                    // If the player couldn't move to a legal place but the piece needs to be moved by gravity, then just move it down a row
                    finalPos.row++;
                }
            }
        }

        // Play roation sound if rotation changed
        if (currentPos.orientation != finalPos.orientation) {
            if (sampleAssets != NULL && sampleAssets->whoop) {
                playSample(state, sampleAssets->whoop);
            }
        }

        // Update current player piece position if it has changed
        if (currentPos.col != finalPos.col || currentPos.row != finalPos.row || currentPos.orientation != finalPos.orientation) {
            MatrixPiecePoints currentPiecePoints = matrixGetPointsForPiece(state->playerPiece, state->playerPosition.col, state->playerPosition.row, state->playerPosition.orientation);
            matrixRemovePiecePoints(state->matrix, &currentPiecePoints);

            // Re-add piece to its new points and update state
            MatrixPiecePoints movedPiecePoints = matrixGetPointsForPiece(state->playerPiece, finalPos.col, finalPos.row, finalPos.orientation);
            matrixAddPiecePoints(state->matrix, state->playerPiece, true, &movedPiecePoints);

            state->playerPosition = finalPos;

            screenUpdated = true;
            drawMatrix(state->matrix, false);
        }

        if (shouldSettle) {
            changeStatus(state, Settled);
        }
    }

    return screenUpdated;
}

// Called on frame update when in the "Settled" state status
// This state only runs for one frame and checks for completed lines, scores, and removes completed lines
static bool updateSceneSettled(SceneState* state) {
    bool screenUpdated = false;

    // Play sound
    if (sampleAssets != NULL && sampleAssets->kick != NULL) {
        playSample(state, sampleAssets->kick);
    }

    // Clear out player indicator
    matrixClearPlayerIndicator(state->matrix);

    // Get any completed rows
    // If there were any, then they will be cleared out in the LineClear state
    state->roundCompletedRows = getCompletedRows(state->matrix);

    // Score soft dropped pieces
    // Score is increased by the number of rows since soft drop was initiated
    if (state->softDropInitiated) {
        state->score = incrementScore(state->score, (state->playerPosition.row - state->softDropStartingRow));
    }

    // Score hard dropped pieces
    // Score is increased by the number of rows dropped * 2
    if (state->hardDropInitiated) {
        state->score = incrementScore(state->score, ((state->playerPosition.row - state->hardDropStartingRow) * 2));
    }

    // If there were any completed lines, go into LineClear state. Else reset to Start state
    changeStatus(state, state->roundCompletedRows.numRows > 0 ? LineClear : Start);

    return screenUpdated;
}

// Called on frame update when in the "LineClear" state
// Lasts for 77 frames
static bool updateSceneLineClear(SceneState* state) {
    // On last frame of LineClear, clear the completed lines and score it
    if (state->statusFrames++ == LINECLEAR_FRAMES) {
        matrixRemoveRows(state->matrix, (int*)state->roundCompletedRows.rows, state->roundCompletedRows.numRows);

        drawMatrix(state->matrix, true);

        // Score completed rows
        state->score = incrementScore(state->score, SCORING[state->roundCompletedRows.numRows - 1] * (state->difficulty + 1));
        state->completedLines += state->roundCompletedRows.numRows;

        changeStatus(state, Start);
    } else {
        // Every 10 frames flash the completed rows
        if (state->statusFrames % 20 == 0) {
            drawMatrix(state->matrix, true);
        } else if (state->statusFrames % 10 == 0) {
            for (int i = 0; i < state->roundCompletedRows.numRows; i++) {
                int row = state->roundCompletedRows.rows[i];

                GFX->fillRect(MATRIX_START_X, MATRIX_GRID_TOP_Y(row), MATRIX_WIDTH, MATRIX_GRID_CELL_SIZE, kColorWhite);
            }

            if (sampleAssets != NULL && sampleAssets->perc != NULL) {
                playSample(state, sampleAssets->perc);
            }
        } 
    }

    return true;
}

// Called on frame update when in the "TopOut" state
static bool updateSceneTopOut(SceneState* state) {
    bool screenUpdated = false;

    // Stop music if it's playing
    if (isMusicPlaying()) {
        stopMusic();
    }

    // Display 4 chunks of blocks that cover the playfield over 45 frames
    // On last frame switch to GameOver state
    if (state->statusFrames <= TOPOUT_FRAMES) {
        if ((state->statusFrames % 15) == 0) {
            int i = state->statusFrames / 15;
            int startRow = (MATRIX_GRID_ROWS - 1) - (i * (MATRIX_GRID_ROWS / 4));
            int endRow = startRow - (MATRIX_GRID_ROWS / 4);

            for (int row = startRow; row > endRow; row--) {
                int y = MATRIX_GRID_TOP_Y(row);

                for (int col = 0; col < MATRIX_GRID_COLS; col++) {
                    int x = MATRIX_GRID_LEFT_X(col);

                    GFX->drawBitmap(bitmapAssets->column, x, y, kBitmapUnflipped);
                    screenUpdated = true;
                }
            }


            playSample(state, sampleAssets->kick);
        }

        state->statusFrames++;
    } else {
        changeStatus(state, GameOver);
    }

    return screenUpdated;
}

// Called on frame update in the "GameOver" state
// Blacks out the playarea and displays buttons to try again or start new game
static bool updateSceneGameOver(SceneState* state) {
    float endPct = (float)state->statusFrames / (float)(LCD_ROWS / 10);

    if (endPct <= 1) {
        int endY = (int)(sin((endPct * 3.14159) / 2) * LCD_ROWS);

        GFX->fillRect(MATRIX_GRID_LEFT_X(0) - MATRIX_GRID_CELL_SIZE, 0, MATRIX_GRID_CELL_SIZE * 12, endY, kColorBlack);

        state->statusFrames++;
    } else {
        int gameOverX = MATRIX_GRID_LEFT_X(0);
        int gameOverY = NEXT_BOX_Y + NEXT_BOX_HEIGHT;
        int gameOverWidth = MATRIX_GRID_CELL_SIZE * 10;
        int gameOverHeight = MATRIX_GRID_CELL_SIZE * 3;

        int txtHeight = textHeight(GAMEOVER_FONT_SIZE);
        int gameTxtWidth = textWidth("Game", strlen("Game"), GAMEOVER_FONT_SIZE);
        int overTxtWidth = textWidth("Over", strlen("Over"), GAMEOVER_FONT_SIZE);
        int GaTxtWidth = textWidth("Ga", strlen("Ga"), GAMEOVER_FONT_SIZE);

        int fullLengthWidth = textWidth("Gameer", strlen("Gameer"), GAMEOVER_FONT_SIZE);

        int txtX = gameOverX + (gameOverWidth / 2) - (fullLengthWidth / 2);
        int txtY = gameOverY + (gameOverHeight / 2) - (txtHeight * 2) / 2;

        textDraw("Game", txtX, txtY, GAMEOVER_FONT_SIZE, kColorWhite);
        textDraw("Over", txtX + GaTxtWidth, txtY + txtHeight, GAMEOVER_FONT_SIZE, kColorWhite);

        formUpdate(state->gameOverForm);
    }

    return true;
}

// Determine where a piece would sit if it dropped straight down
static Position determineDroppedPosition(const MatrixGrid matrix, Piece piece, Position pos) {
    for (int row = pos.row; row < MATRIX_GRID_ROWS; row++) {
        pos.row = row;
        if (canSettlePiece(matrix, piece, pos)) {
            break;
        }
    }

    return pos;
}

// Change current status and rest status frame counter
static void changeStatus(SceneState* state, Status status) {
    state->status = status;
    state->statusFrames = 0;
}

// Called on each frame. Updates the DAS counters
static void updateDasCounts(DasState* state, PDButtons buttons) {
    // Track if/how long the right or left button is held for DAS
    if ((buttons & (kButtonLeft | kButtonRight)) > 0) {
        int pressedKey = (buttons & kButtonLeft) == kButtonLeft
            ? kButtonLeft
            : kButtonRight;

        // Reset DAS count if different key pressed
        // Else increment
        if (pressedKey != state->key) {
            state->key = pressedKey;
            state->frames = 1;
            state->charged = false;
        } else {
            state->frames++;

            if (!state->charged && state->frames == DAS_CHARGE_DELAY) {
                state->charged = true;
                state->frames = 0;
            }
        }
        
    } else {
        // Reset everything if left or right isn't currently pressed
        state->key = 0;
        state->frames = 0;
        state->charged = false;
    }
}

// Check the DAS state if a key can be repeated.
// Resets frame count if a repeat is available
static int dasRepeatCheck(DasState* state) {
    if (state->charged) {
        if (state->frames >= DAS_REPEAT_DELAY) {
            state->frames = 0;

            return state->key;
        }
    }

    return 0;
}

// Called before scene is transitioned away
static void destroyScene(Scene* scene) {
    SceneState* state = (SceneState*)scene->data;

    // Stop music if it's playing
    if (isMusicPlaying()) {
        stopMusic();
    }

    // Dispose of form
    formDestroy(state->gameOverForm);

    // Dispose of scene
    SYS->realloc(scene->data, 0);
    SYS->realloc(scene, 0);

    // Remove the menu items
    SYS->removeAllMenuItems();
}

// Handle Replay button
// Starts a game with the same settings (incl. seed) as the previous game
static void replayHandler(void* data) {
    SceneState* state = (SceneState*)data;

    gameChangeScene(boardSceneCreate(state->seed, state->initialDifficulty, state->music, state->sounds));
}

// Handle New Game button
// Goes back to Options screen to start a new game
static void newGameHandler(void* data) {
    gameChangeScene(optionsSceneCreate());
}

// Create scene for Board scene
Scene* boardSceneCreate(unsigned int seed, int initialDifficulty, bool music, bool sounds) {
    Scene* scene = SYS->realloc(NULL, sizeof(Scene));

    // Initialize scene state to default values
    SceneState* state = SYS->realloc(NULL, sizeof(SceneState));
    state->seed = seed;
    state->initialDifficulty = initialDifficulty;
    state->music = music;
    state->sounds = sounds;
    state->musicMenuItem = NULL;
    state->soundsMenuItem = NULL;
    state->difficulty = initialDifficulty;
    state->completedLines = 0;
    state->score = 0;
    state->gravityFrames = gravityFramesForDifficulty(initialDifficulty);
    state->status = Start;
    state->statusFrames = 0;
    state->playerPiece = None;
    state->playerPosition.col = 0;
    state->playerPosition.row = 0;
    state->playerPosition.orientation = 0;
    state->standbyPiece = None;
    state->das.charged = false;
    state->das.frames = 0;
    state->das.key = 0;
    state->softDropInitiated = false;
    state->softDropStartingRow = 0;
    state->hardDropInitiated = false;
    state->hardDropStartingRow = 0;

    // Create replay/new game forms
    Form* form = formCreate();
    state->gameOverForm = form;
    
    formAddField(form, formCreateButtonField((Dimensions){ .x = BUTTON_X, .y = BUTTON_Y, .width = BUTTON_WIDTH, .height = BUTTON_HEIGHT }, "Replay", 12, 12, state, replayHandler));
    formAddField(form, formCreateButtonField((Dimensions){ .x = BUTTON_X, .y = BUTTON_Y + BUTTON_HEIGHT + 12, .width = BUTTON_WIDTH, .height = BUTTON_HEIGHT }, "New Game", 12, 12, state, newGameHandler));
    
    matrixClear(state->matrix);

    scene->name = "Board";
    scene->init = initScene;
    scene->update = updateScene;
    scene->destroy = destroyScene;
    scene->data = (void*)state;

    return scene;
}

static void initAudioPlayers(void) {
    // FilePlayer for music
    if (musicPlayer == NULL) {
        musicPlayer = SND->fileplayer->newPlayer();
        if (!SND->fileplayer->loadIntoPlayer(musicPlayer, "sounds/its-raining-pixels")) {
            SYS->logToConsole("Error loading music");
        }
    }

    // Sample player for Audio Samples
    if (samplePlayer == NULL) {
        samplePlayer = SND->sampleplayer->newPlayer();
    }
}

// Preloads all assets
static void loadAssets(void) {
    if (bitmapAssets == NULL) {
        bitmapAssets = loadBitmapAssets();
    }

    if (sampleAssets == NULL) {
        sampleAssets = loadSampleAssets();
    }
}

// Draws all cells in the playfield matrix to the screen
// forceFull will force drawing the whole grid if true, else will only draw blocks marked as dirty
static void drawMatrix(MatrixGrid matrix, bool forceFull) {
    for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
            if (forceFull || matrix[row][col].dirty) {
                int x = MATRIX_GRID_LEFT_X(col);
                int y = MATRIX_GRID_TOP_Y(row);
                
                if (matrix[row][col].filled) {
                    LCDBitmap* block = blockBitmapForPiece(matrix[row][col].piece);

                    if (block != NULL) {
                        GFX->drawBitmap(block, x, y, kBitmapUnflipped);
                    }
                } else {
                    GFX->fillRect(x, y, MATRIX_GRID_CELL_SIZE, MATRIX_GRID_CELL_SIZE, kColorWhite);
                }

                matrix[row][col].dirty = false;
            }
        }
    }
}

// Get reference to bitmap for a block used by a piece
static LCDBitmap* blockBitmapForPiece(Piece piece) {
    LCDBitmap* bitmap = NULL;

    if (bitmapAssets != NULL) {
        switch (piece) {
            case None:
                break;
            case L:
                bitmap = bitmapAssets->blockTracks;
                break;
            case O:
                bitmap = bitmapAssets->blockBox;
                break;
            case S:
                bitmap = bitmapAssets->blockTargetOpen;
                break;
            case Z:
                bitmap = bitmapAssets->blockTargetClosed;
                break;
            case I:
                bitmap = bitmapAssets->blockChessboard;
                break;
            case T:
                bitmap = bitmapAssets->blockEye;
                break;
            case J:
                bitmap = bitmapAssets->blockTracksReversed;
                break;
        }
    }

    return bitmap;
}

// Calculates what the difficulty should be for the given number of completed lines
static int difficultyForLines(int initialDifficulty, int completedLines) {
    // Round lines to lower 10
    completedLines = (int)(floor(completedLines / 10) * 10);

    int difficulty = (completedLines / 10);

    return difficulty > initialDifficulty
        ? difficulty
        : initialDifficulty;
}

// Get number of frames until gravity drops a piece one frame
static inline int gravityFramesForDifficulty(int difficulty) {
    if (difficulty < 0 || difficulty > MAX_DIFFICULTY) {
        difficulty = MAX_DIFFICULTY;
    }

    return DIFFICULTY_LEVELS[difficulty];
}

// Draws the level, score, lines, and next piece boxes from current state
static void drawAllBoxes(SceneState* state) {
    // Update text boxes
    char* scoreTxt;
    char* levelTxt;
    char* linesTxt;
    char* seedTxt;

    SYS->formatString(&scoreTxt, "%d", state->score);
    SYS->formatString(&levelTxt, "%d", state->difficulty);
    SYS->formatString(&linesTxt, "%d", state->completedLines);
    SYS->formatString(&seedTxt, "%08X", state->seed);

    drawBoxText(scoreTxt, SCORE_BOX_X, SCORE_BOX_Y, SCORE_BOX_WIDTH, SCORE_BOX_HEIGHT);
    drawBoxText(levelTxt, LEVEL_BOX_X, LEVEL_BOX_Y, LEVEL_BOX_WIDTH, LEVEL_BOX_HEIGHT);
    drawBoxText(linesTxt, LINES_BOX_X, LINES_BOX_Y, LINES_BOX_WIDTH, LINES_BOX_HEIGHT);
    drawBoxText(seedTxt, SEED_BOX_X, SEED_BOX_Y, SEED_BOX_WIDTH, SEED_BOX_HEIGHT);

    // Update piece displays in Next box
    drawBoxPiece(state->standbyPiece, NEXT_BOX_X, NEXT_BOX_Y, NEXT_BOX_WIDTH, NEXT_BOX_HEIGHT);

    SYS->realloc(scoreTxt, 0);
    SYS->realloc(levelTxt, 0);
    SYS->realloc(linesTxt, 0);
    SYS->realloc(seedTxt, 0);
}

// Draw a line of text within a bounded box.
// Centers the text within the box width and height
static void drawBoxText(const char* text, int x, int y, int width, int height) {
    // Clear the box
    GFX->fillRect(x, y, width, height, kColorWhite);

    textDrawCentered(text, x, y, width, height, DEFAULT_FONT_SIZE, kTextColorBlack);
}

// Draw a piece within a bounded box
// Piece is centered within the box width and height
static void drawBoxPiece(Piece piece, int x, int y, int width, int height) {
    // Clear the box
    GFX->fillRect(x, y, width, height, kColorWhite);

    LCDBitmap *block = blockBitmapForPiece(piece);

    if (block != NULL) {
        MatrixPiecePoints piecePoints = matrixGetPointsForPiece(piece, 0, 0, 0);

        // Find the max X and Y points to deteremine the width and height of the piece
        int maxX = 0;
        int maxY = 0;

        for (int i = 0; i < piecePoints.numPoints; i++) {
            const int* point = piecePoints.points[i];

            if (point[0] > maxX) {
                maxX = point[0];
            }

            if (point[1] > maxY) {
                maxY = point[1];
            }
        }

        int pieceWidth = (maxX + 1) * MATRIX_GRID_CELL_SIZE;
        int pieceHeight = (maxY + 1) * MATRIX_GRID_CELL_SIZE;

        // Using the dimensions and the box and the piece, find the center point so we can offset the image to be in the center
        int offsetX = x + (width / 2) - (pieceWidth / 2);
        int offsetY = y + (height / 2) - (pieceHeight / 2);

        for (int i = 0; i < piecePoints.numPoints; i++) {
            const int* point = piecePoints.points[i];

            int blockX = offsetX + (MATRIX_GRID_CELL_SIZE * point[0]);
            int blockY = offsetY + (MATRIX_GRID_CELL_SIZE * point[1]);

            GFX->drawBitmap(block, blockX, blockY, kBitmapUnflipped);
        }
    }
}

// Returns if the given piece sits on top another piece or the floor
static bool canSettlePiece(const MatrixGrid matrix, Piece piece, Position pos) {
    bool shouldSettle = false;
    MatrixPiecePoints points = matrixGetPointsForPiece(piece, pos.col, pos.row, pos.orientation);

    for (int i = 0; i < points.numPoints; i++) {
        const int* point = points.points[i];
        const int pointCol = point[0];
        const int pointRow = point[1];
        
        const int rowBelow = pointRow + 1;

        // Settle piece if point is at the bottom row of the playfield
        if (pointRow == (MATRIX_GRID_ROWS - 1)) {
            shouldSettle = true;
        } else {
            // Settle piece if point is directly above another filled cell that isn't part of the player piece
            if (matrix[rowBelow][pointCol].filled && !matrix[rowBelow][pointCol].player) {
                shouldSettle = true;
            }
        }
    }

    return shouldSettle;
}


// Retrieves the rows that have been completed by the player.
static CompletedRows getCompletedRows(const MatrixGrid matrix) {
    CompletedRows completedRows = {
        .numRows = 0,
        .rows = { 0, 0, 0, 0 }
    };

    for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        int completedCols = 0;

        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
            if (matrix[row][col].filled) {
                completedCols++;
            }
        }
        
        if (completedCols == MATRIX_GRID_COLS) {
            completedRows.rows[completedRows.numRows++] = row;
        }
    }

    return completedRows;
}


// Start playing background music
static void playMusic(SceneState* state) {
    if (state->music && musicPlayer != NULL) {
        SND->fileplayer->play(musicPlayer, 0);
    }
}

// Stops playing background music
static void stopMusic() {
    if (musicPlayer != NULL) {
        SND->fileplayer->stop(musicPlayer);
    }
}

// Returns if the background music is currently playing
static bool isMusicPlaying() {
    return SND->fileplayer->isPlaying(musicPlayer);
}

// Play an audio sample
// Any current playing sample will be muted
static void playSample(SceneState* state, AudioSample* sample) {
    if (state->sounds && (samplePlayer != NULL) && (sample != NULL)) {
        if (SND->sampleplayer->isPlaying(samplePlayer)) {
            SND->sampleplayer->stop(samplePlayer);
        }

        SND->sampleplayer->setSample(samplePlayer, sample);
        SND->sampleplayer->play(samplePlayer, 1, 0);
    }
}

// Increment score by an amount
// Enforces max score restriction
static int incrementScore(int current, int add){
    int new = current + add;

    if (new > MAX_SCORE) {
        new = MAX_SCORE;
    }

    return new;
}

// Handle when the Music menu item toggles
static void handleMusicMenu(SceneState* state) {
    if (state->musicMenuItem != NULL) {
        state->music = SYS->getMenuItemValue(state->musicMenuItem) == 1;

        if (state->music) {
            playMusic(state);
        } else {
            stopMusic();
        }
    }
}

// Handle when the Sounds menu item toggles
static void handleSoundMenu(SceneState* state) {
    if (state->soundsMenuItem != NULL) {
        state->sounds = SYS->getMenuItemValue(state->soundsMenuItem) == 1;
    }
}

// Handle when the End Game menu item is activated
static void handleEndGameMenu(SceneState* state) {
    gameChangeScene(optionsSceneCreate());
}