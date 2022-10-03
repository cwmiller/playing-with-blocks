#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include "pd_api.h"
#include "titleScene.h"
#include "boardScene.h"
#include "game.h"
#include "asset.h"
#include "global.h"

#define PIECE_HEIGHT 30
#define PIECE_WIDTH 20

#define MATRIX_WIDTH 100
#define MATRIX_HEIGHT LCD_ROWS
#define MATRIX_START_X (LCD_COLUMNS / 2) - (MATRIX_WIDTH / 2)

#define MATRIX_GRID_COLS 10
#define MATRIX_GRID_ROWS 24
#define MATRIX_GRID_CELL_SIZE 10

#define MATRIX_GRID_TOP_X(col) (MATRIX_START_X + (col * MATRIX_GRID_CELL_SIZE))
#define MATRIX_GRID_LEFT_Y(row) (row * MATRIX_GRID_CELL_SIZE)

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

// Score is calculated based on the number of lines completed in one drop & the current difficulty
static int SCORING[4] = {
    40,
    100,
    300,
    1200
};

// Background
static LCDBitmap* background = NULL;

// Block pieces
static LCDBitmap* blockChessboard = NULL;
static LCDBitmap* blockEye = NULL;
static LCDBitmap* blockTargetOpen = NULL;
static LCDBitmap* blockTargetClosed = NULL;
static LCDBitmap* blockTracks = NULL;
static LCDBitmap* blockTracksReversed = NULL;

// Gameover is an animation of 4 steps
static LCDBitmap* gameOverOne = NULL;
static LCDBitmap* gameOverTwo = NULL;
static LCDBitmap* gameOverThree = NULL;
static LCDBitmap* gameOverFour = NULL;

// Public Pixel font used for all text
LCDFont* publicPixel = NULL;

// Music player
static FilePlayer* musicPlayer = NULL;

// Sound effects
static SamplePlayer* samplePlayer = NULL;

static AudioSample* whoopSound = NULL;
static AudioSample* kickSound = NULL;

// All piece types
typedef enum Piece {
    None = -1,
    O = 0,
    I = 1,
    S = 2,
    Z = 3,
    T = 4,
    L = 5,
    J = 6
} Piece;

// Each cell on the playerfield matrix can be filled with a block from a piece
// These blocks can either be remains from previous pieces, or the active piece
// being controlled by the player
typedef struct MatrixCell {
    // Indicates that this cell is occupied by the active player piece
    bool player;
    // Indicates that the cell is filled
    bool filled;
    Piece piece;
} MatrixCell;

typedef enum Status {
    // Lasts 1 frame, piece(s) are selected and the active piece is placed at the top of the screen
    Start,

    // Lasts 2 frames where the active piece is idle at the top of the screen
    ARE,

    // Piece is dropping and the player has control
    Dropping,

    // Piece has settled into a spot
    Settled,

    // Player has reached the top and the game must end
    GameOver
} Status;

typedef struct Position {
    int row;
    int col;
    int orientation;
} Position;

// Houses the 4 X/Y coordinates that make up a piece to be placed on the matrix
// Returned by the getPointsForPiece function
typedef struct MatrixPiecePoints {
    int points[4][2];
    int numPoints;
} MatrixPiecePoints;

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
    Status status;
    // Counts the number of frames for the current status
    unsigned int statusFrames;

    int initialDifficulty;
    int difficulty;
    int completedLines;

    int score;

    unsigned int gravityFrames;

    // Holds the state of each cell in the matrix
    MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS];

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
} SceneState;

// Each piece has 4 orientations which we designate as oritentation 0, 1, 2, and 3
// Most pieces live within a 3x3 grid they can rotate in
// However the I piece is a 4x4 grid and the O a 4x3 grid

static bool I_ORIENTATIONS[4][4][4] = {
    { { 0, 0, 0, 0 },  
      { 1, 1, 1, 1 },  
      { 0, 0, 0, 0 },  
      { 0, 0, 0, 0 } },
 
    { { 0, 0, 1, 0 },  
      { 0, 0, 1, 0 },  
      { 0, 0, 1, 0 },  
      { 0, 0, 1, 0 } },

    { { 0, 0, 0, 0 }, 
      { 0, 0, 0, 0 }, 
      { 1, 1, 1, 1 }, 
      { 0, 0, 0, 0 } },
      
    { { 0, 1, 0, 0 },
      { 0, 1, 0, 0 },
      { 0, 1, 0, 0 },
      { 0, 1, 0, 0 } }     
};

static bool J_ORIENTATIONS[4][3][3] = {
    { { 1, 0, 0 },  
      { 1, 1, 1 },  
      { 0, 0, 0 } },
 
    { { 0, 1, 1 },  
      { 0, 1, 0 },  
      { 0, 1, 0 } },

    { { 0, 0, 0 }, 
      { 1, 1, 1 }, 
      { 0, 0, 1 } },
      
    { { 0, 1, 0 },
      { 0, 1, 0 },
      { 1, 1, 0 } }  
};

static bool L_ORIENTATIONS[4][3][3] = {
    { { 0, 0, 1 },  
      { 1, 1, 1 },  
      { 0, 0, 0 } },
 
    { { 0, 1, 0 },  
      { 0, 1, 0 },  
      { 0, 1, 1 } },

    { { 0, 0, 0 }, 
      { 1, 1, 1 }, 
      { 1, 0, 0 } },
      
    { { 1, 1, 0 },
      { 0, 1, 0 },
      { 0, 1, 0 } }  
};

static bool O_ORIENTATIONS[4][3][4] = {
    { { 0, 1, 1, 0 },  
      { 0, 1, 1, 0 },  
      { 0, 0, 0, 0 } },
 
    { { 0, 1, 1, 0 },  
      { 0, 1, 1, 0 },  
      { 0, 0, 0, 0 } },

    { { 0, 1, 1, 0 }, 
      { 0, 1, 1, 0 }, 
      { 0, 0, 0, 0 } },
      
    { { 0, 1, 1, 0 },
      { 0, 1, 1, 0 },
      { 0, 0, 0, 0 } }  
};

static bool S_ORIENTATIONS[4][3][3] = {
    { { 0, 1, 1 },  
      { 1, 1, 0 },  
      { 0, 0, 0 } },
 
    { { 0, 1, 0 },  
      { 0, 1, 1 },  
      { 0, 0, 1 } },

    { { 0, 0, 0 }, 
      { 0, 1, 1 }, 
      { 1, 1, 0 } },
      
    { { 1, 0, 0 },
      { 1, 1, 0 },
      { 0, 1, 0 } }  
};

static bool T_ORIENTATIONS[4][3][3] = {
    { { 0, 1, 0 },  
      { 1, 1, 1 },  
      { 0, 0, 0 } },
 
    { { 0, 1, 0 },  
      { 0, 1, 1 },  
      { 0, 1, 0 } },

    { { 0, 0, 0 }, 
      { 1, 1, 1 }, 
      { 0, 1, 0 } },
      
    { { 0, 1, 0 },
      { 1, 1, 0 },
      { 0, 1, 0 } }  
};

static bool Z_ORIENTATIONS[4][3][3] = {
    { { 1, 1, 0 },  
      { 0, 1, 1 },  
      { 0, 0, 0 } },
 
    { { 0, 0, 1 },  
      { 0, 1, 1 },  
      { 0, 1, 0 } },

    { { 0, 0, 0 }, 
      { 1, 1, 0 }, 
      { 0, 1, 1 } },
      
    { { 0, 1, 0 },
      { 1, 1, 0 },
      { 1, 0, 0 } }  
};

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

// Function prototpes

static void reset(SceneState* state);
static void loadAssets(void);
static bool updateSceneStart(SceneState* state);
static bool updateSceneAre(SceneState* state);
static bool updateSceneDropping(SceneState* state);
static bool updateSceneSettled(SceneState* state);
static bool updateSceneGameOver(SceneState* state);

static void changeStatus(SceneState* state, Status status);
static void updateDasCounts(DasState* state, PDButtons buttons);
static int dasRepeatCheck(DasState* state);

static bool canSettlePiece(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos);

static void clearMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]);
static void drawMatrix(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]);
static void blockBitmapForPiece(Piece piece, LCDBitmap** bitmap);
static MatrixPiecePoints getPointsForPiece(Piece piece, int col, int row, int orientation);
static void addPiecePointsToMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, bool playerPiece, const MatrixPiecePoints* points);
static void removePiecePointsFromMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points);
static bool arePointsAvailable(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points);
static Position determineDroppedPosition(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos);
static int difficultyForLines(int initialDifficulty, int completedLines);
static inline int gravityFramesForDifficulty(int difficulty);
static void drawAllBoxes(SceneState* state);

static void drawBoxText(const char* text, LCDFont* font, int x, int y, int width, int height);
static void drawBoxPiece(Piece piece, int x, int y, int width, int height);

static void playMusic(void);
static void stopMusic(void);
static bool isMusicPlaying(void);

static void playSample(AudioSample* sample);

// Handle when scene becomes active
static void initScene(Scene* scene) {
    SceneState* state = (SceneState*)scene->data;

    // Seed the random number generator
    srand(SYS->getCurrentTimeMilliseconds());

    loadAssets();

    // Set Public Pixel font as default
    GFX->setFont(publicPixel);

    // Clear screen 
    GFX->clear(kColorWhite);

    // Draw background
    GFX->drawBitmap(background, 0, 0, kBitmapUnflipped);

    clearMatrix(state->matrix);
    drawMatrix(state->matrix);

    // Start playing music and loop forever
    playMusic();
}

static void reset(SceneState* state) {
    gameChangeScene(boardSceneCreate(state->initialDifficulty));
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

        case GameOver:
            screenUpdated = updateSceneGameOver(state);
            break;
    }

    SYS->drawFPS(0, 0);

    return screenUpdated;
}

// Called on frame update when in the "Start" state status
// Only runs for 1 frame and sets the active player piece
static bool updateSceneStart(SceneState* state) {
    // On the first time this is called both player and standby pieces need to be picked
    if (state->standbyPiece != None) {
        state->playerPiece = state->standbyPiece;
    } else {
        state->playerPiece = rand() % 7;
    }

    // Randomly select the next piece in line
    state->standbyPiece = rand() % 7;

    // Place the player piece up top the matrix in the default orientation
    state->playerPosition.col = 4;
    state->playerPosition.row = 0;
    state->playerPosition.orientation = 0;

    Position playerPos = state->playerPosition;

    MatrixPiecePoints playerPoints = getPointsForPiece(state->playerPiece, playerPos.col, playerPos.row, playerPos.orientation);

    // A game over occurs when the player piece's starting position overlaps a piece on the board
    bool canPlotPoints = arePointsAvailable(state->matrix, &playerPoints);

    // Draw the new player piece even if it overwrites an existing piece
    addPiecePointsToMatrix(state->matrix, state->playerPiece, true, &playerPoints);

    drawMatrix(state->matrix);

    if (!canPlotPoints) {
        changeStatus(state, GameOver);
    } else {
        // Adjust difficulty every 10 lines
        if (state->completedLines % 10 == 0) {
            state->difficulty = difficultyForLines(state->initialDifficulty, state->completedLines);
        }

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
    if (++(state->statusFrames) == 2) {
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
                MatrixPiecePoints pointsForAttempt = getPointsForPiece(state->playerPiece, attemptedPos.col, attemptedPos.row, attemptedPos.orientation);

                // If any of the points for the attempted move are cells that are already filled, then the player can't move there.
                bool canPlotPoints = arePointsAvailable(state->matrix, &pointsForAttempt);

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
            playSample(whoopSound);
        }

        // Update current player piece position if it has changed
        if (currentPos.col != finalPos.col || currentPos.row != finalPos.row || currentPos.orientation != finalPos.orientation) {
            MatrixPiecePoints currentPiecePoints = getPointsForPiece(state->playerPiece, state->playerPosition.col, state->playerPosition.row, state->playerPosition.orientation);
            removePiecePointsFromMatrix(state->matrix, &currentPiecePoints);

            // Re-add piece to its new points and update state
            MatrixPiecePoints movedPiecePoints = getPointsForPiece(state->playerPiece, finalPos.col, finalPos.row, finalPos.orientation);
            addPiecePointsToMatrix(state->matrix, state->playerPiece, true, &movedPiecePoints);

            state->playerPosition = finalPos;

            screenUpdated = true;
            drawMatrix(state->matrix);
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
    int completedLines = 0;
    bool screenUpdated = false;

    // Play sound
    playSample(kickSound);

    // Check for any completed rows
    for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        int completedCols = 0;

        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
            // Clear player indicator
            state->matrix[row][col].player = false;

            if (state->matrix[row][col].filled) {
                completedCols++;
            }
        }

        // All columns were filled for this row. Remove it by moving the row above it down
        if (completedCols == MATRIX_GRID_COLS) {
            // Update completed lines tally
            completedLines++;

            for (int targetRow = row; targetRow > 0; targetRow--) {
                int sourceRow = targetRow - 1;

                for (int col = 0; col < MATRIX_GRID_COLS; col++) {
                    state->matrix[targetRow][col].filled = state->matrix[sourceRow][col].filled;
                    state->matrix[targetRow][col].piece = state->matrix[sourceRow][col].piece;
                }
            }
            
            // Clear top row
            for (int col = 0; col < MATRIX_GRID_COLS; col++) {
                state->matrix[0][col].filled = false;
                state->matrix[0][col].piece = None;
            }
        }
    }

    // Score soft dropped pieces
    // Score is increased by the number of rows since soft drop was initiated
    if (state->softDropInitiated) {
        state->score += (state->playerPosition.row - state->softDropStartingRow);
    }

    // Score hard dropped pieces
    // Score is increased by the number of rows dropped * 2
    if (state->hardDropInitiated) {
        state->score += ((state->playerPosition.row - state->hardDropStartingRow) * 2);
    }

    // Score completed lines
    if (completedLines > 0) {
        // Score is completedLinesScore * (difficulty + 1)
        state->score += SCORING[completedLines - 1] * (state->difficulty + 1);

        state->completedLines += completedLines;

        drawMatrix(state->matrix);
        screenUpdated = true;
    }

    // Set state back to Start so the next piece is selected and put into play
    changeStatus(state, Start);

    return screenUpdated;
}

// Called on frame update when in the "GameOver" state
static bool updateSceneGameOver(SceneState* state) {
    bool screenUpdated = false;

    // Stop music if it's playing
    if (isMusicPlaying()) {
        stopMusic();
    }

    // Display 4 blocks that cover the playfield over a second
    if (state->statusFrames < 60) {
        if ((state->statusFrames % 15) == 0) {
            LCDBitmap* step = NULL;

            switch (state->statusFrames / 15) {
                case 0: 
                    step = gameOverOne;
                    break;
                case 1: 
                    step = gameOverTwo;
                    break;
                case 2: 
                    step = gameOverThree;
                    break;
                case 3: 
                    step = gameOverFour;
                    break;
            }

            if (step != NULL) {
                playSample(kickSound);
                GFX->drawBitmap(step, 0, 0, kBitmapUnflipped);
                screenUpdated = true;
            }
        }

        state->statusFrames++;
    } else {
        // Pressing the A button will reset the game
        PDButtons keys;
        SYS->getButtonState(NULL, &keys, NULL);

        if ((keys & kButtonA) == kButtonA) {
            reset(state);
        }
    }

    return screenUpdated;
}

// Returns if the given piece sits on top another piece or the floor
static bool canSettlePiece(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos) {
    bool shouldSettle = false;
    MatrixPiecePoints points = getPointsForPiece(piece, pos.col, pos.row, pos.orientation);

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
    // Dispose of scene
    SYS->realloc(scene->data, 0);
    SYS->realloc(scene, 0);
}

// Create scene for Board scene
Scene* boardSceneCreate(int initialDifficulty) {
    Scene* scene = SYS->realloc(NULL, sizeof(Scene));

    // Initialize scene state to default values
    SceneState* state = SYS->realloc(NULL, sizeof(SceneState));
    state->initialDifficulty = initialDifficulty;
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

    clearMatrix(state->matrix);

    scene->name = "Board";
    scene->init = initScene;
    scene->update = updateScene;
    scene->destroy = destroyScene;
    scene->data = (void*)state;

    return scene;
}

// Preloads all bitmaps from image files
static void loadAssets(void) {
    // Background
    if (background == NULL) {
        background = assetLoadBitmap("images/background.png");
    }

    // Block pieces

    if (blockChessboard == NULL) {
        blockChessboard = assetLoadBitmap("images/blocks/chessboard.png");
    }

    if (blockEye == NULL) {
        blockEye = assetLoadBitmap("images/blocks/eye.png");
    }

    if (blockTargetClosed == NULL) {
        blockTargetClosed = assetLoadBitmap("images/blocks/target-closed.png");
    }

    if (blockTargetOpen == NULL) {
        blockTargetOpen = assetLoadBitmap("images/blocks/target-open.png");
    }

    if (blockTracks == NULL) {
        blockTracks = assetLoadBitmap("images/blocks/tracks.png");
    }

    if (blockTracksReversed == NULL) {
        blockTracksReversed = assetLoadBitmap("images/blocks/tracks-reversed.png");
    }

    // Gameover steps

    if (gameOverOne == NULL) {
        gameOverOne = assetLoadBitmap("images/game-over/1.png");
    }

    if (gameOverTwo == NULL) {
        gameOverTwo = assetLoadBitmap("images/game-over/2.png");
    }

    if (gameOverThree == NULL) {
        gameOverThree = assetLoadBitmap("images/game-over/3.png");
    }

    if (gameOverFour == NULL) {
        gameOverFour = assetLoadBitmap("images/game-over/4.png");
    }

    // Font
    if (publicPixel == NULL) {
        publicPixel = assetLoadFont("fonts/public-pixel/PublicPixel-8pt");
    }

    // FilePlayer for music
    if (musicPlayer == NULL) {
        musicPlayer = SND->fileplayer->newPlayer();

        SND->fileplayer->loadIntoPlayer(musicPlayer, "sounds/Its-Raining-Pixels.mp3");
    }

    // Audio Samples
    if (samplePlayer == NULL) {
        samplePlayer = SND->sampleplayer->newPlayer();
    }

    if (whoopSound == NULL) {
        whoopSound = assetLoadSample("sounds/8-bit_whoop.wav");
    }

    if (kickSound == NULL) {
        kickSound = assetLoadSample("sounds/8-bit_kick14.wav");
    }
}

// Clear all cells in the playfield matrix
static void clearMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]) {
    for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
           matrix[row][col].filled = false;
           matrix[row][col].player = false;
           matrix[row][col].piece = None;
        }
    }
}

// Draws all cells in the playfield matrix to the screen
static void drawMatrix(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]) {
    for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
            int x = MATRIX_GRID_TOP_X(col);
            int y = MATRIX_GRID_LEFT_Y(row);
            
            if (matrix[row][col].filled) {
                LCDBitmap* block = NULL;

                blockBitmapForPiece(matrix[row][col].piece, &block);

                if (block != NULL) {
                    GFX->drawBitmap(block, x, y, kBitmapUnflipped);
                }
            } else {
                GFX->fillRect(x, y, MATRIX_GRID_CELL_SIZE, MATRIX_GRID_CELL_SIZE, kColorWhite);
            }
        }
    }
}

// Get reference to bitmap for a block used by a piece
static void blockBitmapForPiece(Piece piece, LCDBitmap** bitmap) {
    switch (piece) {
        case None:
            break;
        case L:
            *bitmap = blockTargetOpen;
            break;
        case O:
            *bitmap = blockChessboard;
            break;
        case S:
            *bitmap = blockEye;
            break;
        case Z:
            *bitmap = blockTargetClosed;
            break;
        case I:
            *bitmap = blockChessboard;
            break;
        case T:
            *bitmap = blockTracks;
            break;
        case J:
            *bitmap = blockTracksReversed;
            break;
    }
}

// Returns a list of all visible X,Y matrix coordinates that a peice fills based on its orientation
static MatrixPiecePoints getPointsForPiece(Piece piece, int col, int row, int orientation) {
    MatrixPiecePoints allPoints = {
        .numPoints = 0,
        .points = { { 0, 0 } }
    };

    int pieceCols = 3;
    int pieceRows = 3;

    if (piece == I) {
        pieceCols = 4;
        pieceRows = 4;
    } else if (piece == O) {
        pieceCols = 4;
    }

    for (int pieceRow = 0; pieceRow < pieceRows; pieceRow++) {
        for (int pieceCol = 0; pieceCol < pieceCols; pieceCol++) {
            bool enableCell = false;

            switch (piece) {
                case None:
                    break;
                case O:
                    enableCell = O_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;
                case I:
                    enableCell = I_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;
                case S:
                    enableCell = S_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;
                case Z:
                    enableCell = Z_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;
                case T:
                    enableCell = T_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;
                case L:
                    enableCell = L_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;
                case J:
                    enableCell = J_ORIENTATIONS[orientation][pieceRow][pieceCol];
                    break;                
            }

            if (enableCell) {
                int plotRow = row + pieceRow;
                int plotCol = col + pieceCol;
                
                // Ensure cell is within bounds
                if ((plotRow >= 0) && (plotRow < MATRIX_GRID_ROWS) && (plotCol >= 0) && (plotCol < MATRIX_GRID_COLS)) {
                    allPoints.points[allPoints.numPoints][0] = plotCol;
                    allPoints.points[allPoints.numPoints++][1] = plotRow;
                }
            }
        }
    }

    return allPoints;
} 

// Fills matrix cells with visible points of a piece
static void addPiecePointsToMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, bool playerPiece, const MatrixPiecePoints* points) {
    for (int i = 0; i < points->numPoints; i++) {
        const int* point = points->points[i];

        matrix[point[1]][point[0]].filled = true;
        matrix[point[1]][point[0]].player = playerPiece;
        matrix[point[1]][point[0]].piece = piece;
    }
}

// Clears matrix cells with visible points of a piece
static void removePiecePointsFromMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points) {
    for (int i = 0; i < points->numPoints; i++) {
        const int* point = points->points[i];

        matrix[point[1]][point[0]].filled = false;
        matrix[point[1]][point[0]].player = false;
        matrix[point[1]][point[0]].piece = None;
    }
}

// Returns whether or not the given X/Y points are are not already filled in the matrix
// Current player piece points are ignored
static bool arePointsAvailable(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points) {
    for (int i = 0; i < points->numPoints; i++) {
        const int* point = points->points[i];
        const int pointCol = point[0];
        const int pointRow = point[1];

        if (matrix[pointRow][pointCol].filled && !matrix[pointRow][pointCol].player) {
            return false;
        }
    }

    return true;
}

// Determine where a piece would sit if it dropped straight down
static Position determineDroppedPosition(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos) {
    for (int row = pos.row; row < MATRIX_GRID_ROWS; row++) {
        pos.row = row;
        if (canSettlePiece(matrix, piece, pos)) {
            break;
        }
    }

    return pos;
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

    SYS->formatString(&scoreTxt, "%d", state->score);
    SYS->formatString(&levelTxt, "%d", state->difficulty);
    SYS->formatString(&linesTxt, "%d", state->completedLines);

    drawBoxText(scoreTxt, publicPixel, SCORE_BOX_X, SCORE_BOX_Y, SCORE_BOX_WIDTH, SCORE_BOX_HEIGHT);
    drawBoxText(levelTxt, publicPixel, LEVEL_BOX_X, LEVEL_BOX_Y, LEVEL_BOX_WIDTH, LEVEL_BOX_HEIGHT);
    drawBoxText(linesTxt, publicPixel, LINES_BOX_X, LINES_BOX_Y, LINES_BOX_WIDTH, LINES_BOX_HEIGHT);

    // Update piece displays in Next box
    drawBoxPiece(state->standbyPiece, NEXT_BOX_X, NEXT_BOX_Y, NEXT_BOX_WIDTH, NEXT_BOX_HEIGHT);

    SYS->realloc(scoreTxt, 0);
    SYS->realloc(levelTxt, 0);
    SYS->realloc(linesTxt, 0);
}

// Draw a line of text within a bounded box.
// Centers the text within the box width and height
static void drawBoxText(const char* text, LCDFont* font, int x, int y, int width, int height) {
    GFX->setFont(font);

    // Determine the height and width of the rendered text so we can center it within the box
    int textWidth = GFX->getTextWidth(font, text, strlen(text), kASCIIEncoding, 0);
    int textHeight = GFX->getFontHeight(font);

    int centeredX = x + (width / 2) - (textWidth / 2);
    int centeredY = y + (height /2) - (textHeight /2);

    // Clear the box
    GFX->fillRect(x, y, width, height, kColorWhite);
    
    // Write the text
    GFX->drawText(text, strlen(text), kASCIIEncoding, centeredX, centeredY);
}

// Draw a piece within a bounded box
// Piece is centered within the box width and height
static void drawBoxPiece(Piece piece, int x, int y, int width, int height) {
    // Clear the box
    GFX->fillRect(x, y, width, height, kColorWhite);

    LCDBitmap *block = NULL;

    blockBitmapForPiece(piece, &block);

    if (block != NULL) {
        MatrixPiecePoints piecePoints = getPointsForPiece(piece, 0, 0, 0);

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

// Start playing background music
static void playMusic() {
    if (musicPlayer != NULL) {
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
static void playSample(AudioSample* sample) {
    if (samplePlayer != NULL && sample != NULL) {
        if (SND->sampleplayer->isPlaying(samplePlayer)) {
            SND->sampleplayer->stop(samplePlayer);
        }

        SND->sampleplayer->setSample(samplePlayer, sample);
        SND->sampleplayer->play(samplePlayer, 1, 0);
    }
}