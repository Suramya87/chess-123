#pragma once

#include "Game.h"
#include "Grid.h"
#include <vector>
#include <cstdint>

constexpr int pieceSize = 80;

enum ChessPiece
{
    NoPiece,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

struct BitMove {
    uint8_t from;
    uint8_t to;
    uint8_t piece;
    
    BitMove(int from, int to, int piece)
        : from(from), to(to), piece(piece) { }
        
    BitMove() : from(0), to(0), piece(NoPiece) { }
    
    bool operator==(const BitMove& other) const {
        return from == other.from && 
               to == other.to && 
               piece == other.piece;
    }
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override;
    
    // Move generation
    std::vector<BitMove> generateAllMoves(int playerNumber);

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;
    
    // Bitboard helpers
    uint64_t getOccupancyBitboard() const;
    uint64_t getFriendlyBitboard(int playerNumber) const;
    uint64_t getEnemyBitboard(int playerNumber) const;
    uint64_t getBitboardForPieceType(int pieceType, int playerNumber) const;
    
    // Move generation for each piece type
    std::vector<BitMove> generatePawnMoves(int playerNumber);
    std::vector<BitMove> generateKnightMoves(int playerNumber);
    std::vector<BitMove> generateBishopMoves(int playerNumber);
    std::vector<BitMove> generateRookMoves(int playerNumber);
    std::vector<BitMove> generateQueenMoves(int playerNumber);
    std::vector<BitMove> generateKingMoves(int playerNumber);
    
    // Helper functions
    bool isValidSquare(int x, int y) const;
    bool isOccupied(int x, int y) const;
    bool isEnemyPiece(int x, int y, int playerNumber) const;
    bool isFriendlyPiece(int x, int y, int playerNumber) const;
    int getPieceType(Bit* bit) const;
    int getPieceTypeAt(int x, int y) const;
    int squareToIndex(int x, int y) const;
    void indexToSquare(int index, int& x, int& y) const;
    bool isLegalMove(const BitMove& move, int playerNumber);

    Grid* _grid;
};