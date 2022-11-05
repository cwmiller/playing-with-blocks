#include "matrix.h"
#include <stdbool.h>

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

// Returns a list of all visible X,Y matrix coordinates that a peice fills based on its orientation
MatrixPiecePoints matrixGetPointsForPiece(Piece piece, int col, int row, int orientation) {
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
void matrixAddPiecePoints(MatrixGrid matrix, Piece piece, bool playerPiece, const MatrixPiecePoints* points) {
    for (int i = 0; i < points->numPoints; i++) {
        const int* point = points->points[i];

        matrix[point[1]][point[0]].filled = true;
        matrix[point[1]][point[0]].player = playerPiece;
        matrix[point[1]][point[0]].piece = piece;
    }
}

// Clears matrix cells with visible points of a piece
void matrixRemovePiecePoints(MatrixGrid matrix, const MatrixPiecePoints* points) {
    for (int i = 0; i < points->numPoints; i++) {
        const int* point = points->points[i];

        matrix[point[1]][point[0]].filled = false;
        matrix[point[1]][point[0]].player = false;
        matrix[point[1]][point[0]].piece = None;
    }
}

// Returns whether or not the given X/Y points are are not already filled in the matrix
// Current player piece points are ignored
bool matrixPointsAvailable(const MatrixGrid matrix, const MatrixPiecePoints* points) {
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

// Remove specified rows from the matrix
void matrixRemoveRows(MatrixGrid matrix, int* rows, int totalRows) {
    for (int i = 0; i < totalRows; i++) {
        int row = rows[i];

        for (int targetRow = row; targetRow > 0; targetRow--) {
            int sourceRow = targetRow - 1;

            for (int col = 0; col < MATRIX_GRID_COLS; col++) {
                matrix[targetRow][col].filled = matrix[sourceRow][col].filled;
                matrix[targetRow][col].piece = matrix[sourceRow][col].piece;
            }
        }

        // Clear top row
        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
            matrix[0][col].filled = false;
            matrix[0][col].piece = None;
        }
    }
}

// Unsets the player attribute on all cells
void matrixClearPlayerIndicator(MatrixGrid matrix) {
  for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
            matrix[row][col].player = false;
        }
    }
}

// Clear all cells in the playfield matrix
void matrixClear(MatrixGrid matrix) {
    for (int row = 0; row < MATRIX_GRID_ROWS; row++) {
        for (int col = 0; col < MATRIX_GRID_COLS; col++) {
           matrix[row][col].filled = false;
           matrix[row][col].player = false;
           matrix[row][col].piece = None;
        }
    }
}