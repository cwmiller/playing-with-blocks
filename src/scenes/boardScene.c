#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include "pd_api.h"
#include "titleScene.h"
#include "asset.h"
#include "global.h"

#define PIECE_HEIGHT 30
#define PIECE_WIDTH 20

#define MATRIX_WIDTH 100
#define MATRIX_START_X (LCD_COLUMNS / 2) - (MATRIX_WIDTH / 2)

#define MATRIX_GRID_COLS 10
#define MATRIX_GRID_ROWS 24
#define MATRIX_GRID_SIZE 10

#define MATRIX_GRID_TOP_X(col) (MATRIX_START_X + (col * MATRIX_GRID_SIZE))
#define MATRIX_GRID_LEFT_Y(row) (row * MATRIX_GRID_SIZE)

#define MAX_DIFFICULTY 20

#define DAS_CHARGE_DELAY 23
#define DAS_REPEAT_DELAY 9

#define SOFTDROP_GRAVITY 3

static LCDBitmap* background = NULL;
static LCDBitmap* blockChessboard = NULL;
static LCDBitmap* blockEye = NULL;
static LCDBitmap* blockKnot = NULL;
static LCDBitmap* blockTargetOpen = NULL;
static LCDBitmap* blockTargetClosed = NULL;
static LCDBitmap* blockTracks = NULL;
static LCDBitmap* blockTracksReversed = NULL;

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
    Settled
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
    53,
    49,
    45,
    41,
    37,
    33,
    28,
    22,
    17,
    11,
    10,
    9,
    8,
    7,
    6,
    6,
    5,
    5,
    4,
    4,
    3
};

// Function prototpes

static void loadBitmaps(void);
static void updateSceneStart(SceneState* state);
static void updateSceneAre(SceneState* state);
static void updateSceneDropping(SceneState* state);
static void updateSceneSettled(SceneState* state);

static void changeStatus(SceneState* state, Status status);
static void updateDasCounts(DasState* state, PDButtons buttons);
static int dasRepeatCheck(DasState* state);

static bool shouldSettlePiece(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos);

static void clearMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]);
static void drawMatrix(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]);
static MatrixPiecePoints getPointsForPiece(Piece piece, int col, int row, int orientation);
static void addPiecePointsToMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, bool playerPiece, const MatrixPiecePoints* points);
static void removePiecePointsFromMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points);
static bool arePointsAvailable(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points);
static Position determineDroppedPosition(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos);
static int difficultyForLines(int initialDifficulty, int completedLines);
static inline int gravityFramesForDifficulty(int difficulty);

// Handle when scene becomes active
static void initScene(Scene* scene) {
    SceneState* state = (SceneState*)scene->data;

    // Seed the random number generator
    srand(SYS->getCurrentTimeMilliseconds());

    loadBitmaps();

    // Clear screen 
    GFX->clear(kColorWhite);

    // Draw background
    GFX->drawBitmap(background, 0, 0, kBitmapUnflipped);

    clearMatrix(state->matrix);
    drawMatrix(state->matrix);
}

// Called on every frame while scene is active
static void updateScene(Scene* scene) {
    SceneState* state = (SceneState*)scene->data;
    PDButtons buttons;

    SYS->getButtonState(&buttons, NULL, NULL);

    updateDasCounts(&(state->das), buttons);

    switch (state->status) {
        case Start:
            updateSceneStart(state);
            break;

        case ARE:
            updateSceneAre(state
            );
            break;

        case Dropping:
            updateSceneDropping(state);
            break;

        case Settled:
            updateSceneSettled(state);
            break;
    }

    drawMatrix(state->matrix);

    SYS->drawFPS(0, 0);
}

// Called on frame update when in the "Start" state status
// Only runs for 1 frame and sets the active player piece
static void updateSceneStart(SceneState* state) {
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
    addPiecePointsToMatrix(state->matrix, state->playerPiece, true, &playerPoints);

    // Adjust difficulty every 10 lines
    if (state->completedLines % 10 == 0) {
        state->difficulty = difficultyForLines(state->initialDifficulty, state->completedLines);

        SYS->logToConsole("Reached %d lines. Diffiulty now %d", state->completedLines, state->difficulty);
    }

    // Set gravity based on current difficulty
    state->gravityFrames = gravityFramesForDifficulty(state->difficulty);

    // Reset soft drop
    state->softDropInitiated = false;
    
    // After a piece is selected, switch to ARE state
    changeStatus(state, ARE);
}

// Called on frame update when in the "ARE" state status
// Only runs for 2 frames and is just to allow the piece to float before giving player control
static void updateSceneAre(SceneState* state) {
    if (++(state->statusFrames) == 2) {
        changeStatus(state, Dropping);
    }
}

// Called on frame update when in the "Dropping" state status
// In this state the piece drops from gravity and is controllable by the player
static void updateSceneDropping(SceneState* state) {
    bool enforceGravity = false;

    PDButtons currentKeys;
    PDButtons pressedKeys;
    
    SYS->getButtonState(&currentKeys, &pressedKeys, NULL);

    // If DOWN is newly pressed, force soft drop gravity
    // Ignore if any other direction button is pressed too
    if ((pressedKeys & 0xF) == kButtonDown) {
        state->gravityFrames = SOFTDROP_GRAVITY;
        state->softDropInitiated = true;
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

        // If UP is pressed, immediately drop piece
        if ((pressedKeys & kButtonUp) == kButtonUp) {
            finalPos = determineDroppedPosition(state->matrix, state->playerPiece, finalPos);
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

        // Update current player piece position if it has changed
        if (currentPos.col != finalPos.col || currentPos.row != finalPos.row || currentPos.orientation != finalPos.orientation) {
            MatrixPiecePoints currentPiecePoints = getPointsForPiece(state->playerPiece, state->playerPosition.col, state->playerPosition.row, state->playerPosition.orientation);
            removePiecePointsFromMatrix(state->matrix, &currentPiecePoints);

            // Re-add piece to its new points and update state
            MatrixPiecePoints movedPiecePoints = getPointsForPiece(state->playerPiece, finalPos.col, finalPos.row, finalPos.orientation);
            addPiecePointsToMatrix(state->matrix, state->playerPiece, true, &movedPiecePoints);

            state->playerPosition = finalPos;

            if (shouldSettlePiece(state->matrix, state->playerPiece, finalPos)) {
                changeStatus(state, Settled);
            }
        }
    }
}

// Called on frame update when in the "Settled" state status
// This state only runs for one frame and checks for completed lines, scores, and removes completed lines
static void updateSceneSettled(SceneState* state) {
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
            state->completedLines++;

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

    // Set state back to Start so the next piece is selected and put into play
    changeStatus(state, Start);
}

// Returns if the given piece sits on another piece or the floor and should be fixed in place
static bool shouldSettlePiece(const MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, Position pos) {
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

    clearMatrix(state->matrix);

    scene->name = "Board";
    scene->init = initScene;
    scene->update = updateScene;
    scene->destroy = destroyScene;
    scene->data = (void*)state;

    return scene;
}

// Preloads all bitmaps from image files
static void loadBitmaps(void) {
    if (background == NULL) {
        background = assetLoadBitmap("images/background.png");
    }

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

                switch (matrix[row][col].piece) {
                    case None:
                        break;
                    case L:
                        block = blockTargetOpen;
                        break;
                    case O:
                        block = blockChessboard;
                        break;
                    case S:
                        block = blockEye;
                        break;
                    case Z:
                        block = blockTargetClosed;
                        break;
                    case I:
                        block = blockChessboard;
                        break;
                    case T:
                        block = blockTracks;
                        break;
                    case J:
                        block = blockTracksReversed;
                        break;
                }

                if (block != NULL) {
                    GFX->drawBitmap(block, x, y, kBitmapUnflipped);
                }
            } else {
                GFX->fillRect(x, y, MATRIX_GRID_SIZE, MATRIX_GRID_SIZE, kColorWhite);
            }
        }
    }
}

// Returns a list of all visible X,Y matrix coordinates that a peice fills based on its orientation
static MatrixPiecePoints getPointsForPiece(Piece piece, int col, int row, int orientation) {
    MatrixPiecePoints allPoints = {
        .numPoints = 0,
        .points = { 0, 0 }
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
                if (plotRow < 0 || plotRow >= MATRIX_GRID_ROWS || plotCol < 0 || plotCol >= MATRIX_GRID_COLS) {
                    SYS->logToConsole("Attempted to plot invalid point %d,%d for piece %d", plotCol, plotRow, piece);
                } else {
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
        if (shouldSettlePiece(matrix, piece, pos)) {
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