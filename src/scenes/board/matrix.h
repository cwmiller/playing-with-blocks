#ifndef SCENES_BOARD_MATRIX_H
#define SCENES_BOARD_MATRIX_H

#include <stdbool.h>

#define MATRIX_WIDTH 100
#define MATRIX_HEIGHT LCD_ROWS
#define MATRIX_START_X (LCD_COLUMNS / 2) - (MATRIX_WIDTH / 2)

#define MATRIX_GRID_COLS 10
#define MATRIX_GRID_ROWS 24
#define MATRIX_GRID_CELL_SIZE 10

#define MATRIX_GRID_LEFT_X(col) (MATRIX_START_X + (col * MATRIX_GRID_CELL_SIZE))
#define MATRIX_GRID_TOP_Y(row) (row * MATRIX_GRID_CELL_SIZE)

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

typedef struct Position {
    int row;
    int col;
    int orientation;
} Position;

// Houses the 4 X/Y coordinates that make up a piece to be placed on the matrix
// Returned by the matrixGetPointsForPiece function
typedef struct MatrixPiecePoints {
    int points[4][2];
    int numPoints;
} MatrixPiecePoints;

// Each cell on the playerfield matrix can be filled with a block from a piece
// These blocks can either be remains from previous pieces, or the active piece
// being controlled by the player
typedef struct MatrixCell {
    // Indicates that the cell has updated since the last draw
    bool dirty;

    // Indicates that this cell is occupied by the active player piece
    bool player;

    // Indicates that the cell is filled
    bool filled;

    Piece piece;
} MatrixCell;

typedef MatrixCell MatrixGrid[MATRIX_GRID_ROWS][MATRIX_GRID_COLS];

// Returns a list of all visible X,Y matrix coordinates that a peice fills based on its orientation
MatrixPiecePoints matrixGetPointsForPiece(Piece piece, int col, int row, int orientation);

// Fills matrix cells with visible points of a piece
void matrixAddPiecePoints(MatrixGrid matrix, Piece piece, bool playerPiece, const MatrixPiecePoints* points);

// Clears matrix cells with visible points of a piece
void matrixRemovePiecePoints(MatrixGrid matrix, const MatrixPiecePoints* points);

// Remove specified rows from the matrix
void matrixRemoveRows(MatrixGrid matrix, int* rows, int totalRows);

// Returns whether or not the given X/Y points are are not already filled in the matrix
// Current player piece points are ignored
bool matrixPointsAvailable(const MatrixGrid matrix, const MatrixPiecePoints* points);

// Unsets the player attribute on all cells
void matrixClearPlayerIndicator(MatrixGrid matrix);

// Clear all cells in the playfield matrix
void matrixClear(MatrixGrid matrix);

#endif