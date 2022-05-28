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

// Houses the 4 X/Y coordinates that make up a piece to be placed on the matrix
// Returned by the getPointsForPiece function
typedef struct MatrixPiecePoints {
    int points[4][2];
    int numPoints;
} MatrixPiecePoints;

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

    // Following four properties hold information about the active piece
    // being controlled by the player
    // Col/Row coords represent the top-left block within the orientation grid (can be negative if left column in orientation is empty blocks)

    Piece playerPiece;
    int playerOrientation;
    int playerCol;
    int playerRow;

    Piece standbyPiece;
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

static bool attemptPlayerPieceMove(SceneState* state, PDButtons buttons, bool enforceGravity, int* finalCol, int* finalRow, int* finalOrientation);
static bool shouldSettlePiece(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, int col, int row, int orientation);

static void clearMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]);
static void drawMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]);
static MatrixPiecePoints getPointsForPiece(Piece piece, int col, int row, int orientation);
static void addPiecePointsToMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, bool playerPiece, const MatrixPiecePoints* points);
static void removePiecePointsFromMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], const MatrixPiecePoints* points);
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

    switch (state->status) {
        case Start:
            updateSceneStart(state);
            break;

        case ARE:
            updateSceneAre(state);
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
    state->playerRow = 0;
    state->playerCol = 4;
    state->playerOrientation = 0;

    MatrixPiecePoints playerPoints = getPointsForPiece(state->playerPiece, state->playerCol, state->playerRow, state->playerOrientation);
    addPiecePointsToMatrix(state->matrix, state->playerPiece, true, &playerPoints);

    // Adjust difficulty every 10 lines
    if (state->completedLines % 10 == 0) {
        state->difficulty = difficultyForLines(state->initialDifficulty, state->completedLines);

        SYS->logToConsole("Reached %d lines. Diffiulty now %d", state->completedLines, state->difficulty);
    }

    // Set gravity based on current difficulty
    state->gravityFrames = gravityFramesForDifficulty(state->difficulty);
    
    // After a piece is selected, switch to ARE state
    state->status = ARE;
    state->statusFrames = 0;
}

// Called on frame update when in the "ARE" state status
// Only runs for 2 frames and is just to allow the piece to float before giving player control
static void updateSceneAre(SceneState* state) {
    if (++(state->statusFrames) == 2) {
        state->status = Dropping;
        state->statusFrames = 0;
    }
}

// Called on frame update when in the "Dropping" state status
// In this state the piece drops from gravity and is controllable by the player
static void updateSceneDropping(SceneState* state) {
    bool enforceGravity = false;

    if (--state->gravityFrames == 0) {
        enforceGravity = true;
        state->gravityFrames = gravityFramesForDifficulty(state->difficulty);
    }

    PDButtons buttons;
    int finalCol;
    int finalRow;
    int finalOrientation;

    SYS->getButtonState(NULL, &buttons, NULL);

    if (attemptPlayerPieceMove(state, buttons, enforceGravity, &finalCol, &finalRow, &finalOrientation)) {
        // Remove the piece from its old position
        MatrixPiecePoints currentPiecePoints = getPointsForPiece(state->playerPiece, state->playerCol, state->playerRow, state->playerOrientation);
        removePiecePointsFromMatrix(state->matrix, &currentPiecePoints);

        // Re-add piece to its new points and update state
        MatrixPiecePoints movedPiecePoints = getPointsForPiece(state->playerPiece, finalCol, finalRow, finalOrientation);
        addPiecePointsToMatrix(state->matrix, state->playerPiece, true, &movedPiecePoints);

        state->playerCol = finalCol;
        state->playerRow = finalRow;
        state->playerOrientation = finalOrientation;

        // If player piece now touches the top of another piece OR the bottom of the playfield
        // then move to Settled status
        if (shouldSettlePiece(state->matrix, state->playerPiece, state->playerCol, state->playerRow, state->playerOrientation)) {
            state->status = Settled;
            state->statusFrames = 0;
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
    state->status = Start;
    state->statusFrames = 0;
}

// Attempts to move the active player piece during the Dropping status
// Returns whether or not the piece did move
static bool attemptPlayerPieceMove(SceneState* state, PDButtons buttons, bool enforceGravity, int* finalCol, int* finalRow, int* finalOrientation) {
    // Return value. Indicates that the player piece was moved
    bool moved = false;

    // If any buttons are pressed and/or gravity is being enforced then we need to make adjustments to the player piece
    // We can ignore if only UP is pressed since UP has no affect on the piece
    if (enforceGravity || ((buttons > 0) && (buttons != kButtonUp))) {
        int attemptedCol = state->playerCol;
        int attemptedRow = state->playerRow;
        int attemptedOrientation = state->playerOrientation;

        *finalCol = state->playerCol;
        *finalRow = state->playerRow;
        *finalOrientation = state->playerOrientation;

        // Adjust player movement attempt based on which button is pressed

        if ((buttons & kButtonRight) == kButtonRight) {
            attemptedCol++;
        } else if ((buttons & kButtonLeft) == kButtonLeft) {
            attemptedCol--;
        }

        // Pressing down with move the block down one row
        // The piece will also be pushed down if gravity takes effect
        if (enforceGravity || ((buttons & kButtonDown) == kButtonDown)) {
            attemptedRow++;
        }

        // Pressing A buttons adjusts orientation right
        if ((buttons & kButtonA) == kButtonA) {
            if (++attemptedOrientation > 3) {
                attemptedOrientation = 0;
            }
        }

        // Pressing B buttons adjusts orientation left
        if ((buttons & kButtonB) == kButtonB) {
            if (--attemptedOrientation < 0) {
                attemptedOrientation = 3;
            }
        }

        MatrixPiecePoints pointsForAttempt = getPointsForPiece(state->playerPiece, attemptedCol, attemptedRow, attemptedOrientation);

        // If any of the points for the attempted move are cells that are already filled, then the player can't move there.
        bool pointsAreFree = true;

        for (int i = 0; i < pointsForAttempt.numPoints; i++) {
            const int* point = pointsForAttempt.points[i];
            const int pointCol = point[0];
            const int pointRow = point[1];

            if (state->matrix[pointRow][pointCol].filled && !state->matrix[pointRow][pointCol].player) {
                pointsAreFree = false;
            }
        }

        // For a legal move, all 4 visible points of the piece must be plottable on the matrix and not already filled
        if (pointsForAttempt.numPoints == 4 && pointsAreFree) {
            *finalRow = attemptedRow;
            *finalCol = attemptedCol;
            *finalOrientation = attemptedOrientation;
        } else if (enforceGravity) {
            // If the player couldn't move to a legal place but the piece needs to be moved by gravity, then just move it down a row
            *finalRow++;
        }

        if (*finalRow != state->playerRow || *finalCol != state->playerCol || *finalOrientation != state->playerOrientation) {     
            moved = true;
        }
    }

    return moved;
}

// Returns if the given piece sits on another piece or the floor and should be fixed in place
static bool shouldSettlePiece(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS], Piece piece, int col, int row, int orientation) {
    bool shouldSettle = false;
    MatrixPiecePoints points = getPointsForPiece(piece, col, row, orientation);

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
    state->playerOrientation = 0;
    state->playerCol = 0;
    state->playerRow = 0;
    state->standbyPiece = None;

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
static void drawMatrix(MatrixCell matrix[MATRIX_GRID_ROWS][MATRIX_GRID_COLS]) {
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

// Calculates what the difficulty should be for the given number of completed lines
static int difficultyForLines(int initialDifficulty, int completedLines) {
    // Round lines to lower 10
    completedLines = floor(completedLines / 10) * 10;

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