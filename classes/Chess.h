#pragma once

#include "Bit.h"
#include "BitHolder.h"
#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include "MagicBitboards.h"
#include <cassert>
#include <cstdint>
#include <vector>
#include <string>

constexpr int pieceSize = 80;
constexpr int WHITE =  1;
constexpr int BLACK = -1;


enum AllBitBoards
{
    WHITE_PAWNS      = 0,
    WHITE_KNIGHTS    = 1,
    WHITE_BISHOPS    = 2,
    WHITE_ROOKS      = 3,
    WHITE_QUEENS     = 4,
    WHITE_KING       = 5,
    BLACK_PAWNS      = 6,
    BLACK_KNIGHTS    = 7,
    BLACK_BISHOPS    = 8,
    BLACK_ROOKS      = 9,
    BLACK_QUEENS     = 10,
    BLACK_KING       = 11,
    WHITE_ALL_PIECES = 12,
    BLACK_ALL_PIECES = 13,
    OCCUPANCY        = 14,
    EMPTY_SQUARES    = 15,
    e_numBitBoards   = 16
};

// Extended move flags
enum MoveFlag
{
    FLAG_NONE        = 0,
    FLAG_EN_PASSANT  = 1,
    FLAG_CASTLING    = 2,
    FLAG_PROMOTION   = 3
};

struct ChessMove
{
    uint8_t   from;
    uint8_t   to;
    uint8_t   piece;
    uint8_t   flag;        
    uint8_t   promoPiece;  

    ChessMove() : from(0), to(0), piece(NoPiece), flag(FLAG_NONE), promoPiece(Queen) {}
    ChessMove(int f, int t, ChessPiece p, MoveFlag fl = FLAG_NONE, ChessPiece promo = Queen)
        : from((uint8_t)f), to((uint8_t)t), piece((uint8_t)p), flag((uint8_t)fl), promoPiece((uint8_t)promo) {}

    bool operator==(const ChessMove& o) const {
        return from == o.from && to == o.to && piece == o.piece && flag == o.flag;
    }
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void        setUpBoard()                                               override;
    bool        canBitMoveFrom(Bit& bit, BitHolder& src)                   override;
    bool        canBitMoveFromTo(Bit& bit, BitHolder& src, BitHolder& dst) override;
    bool        actionForEmptyHolder(BitHolder& holder)                    override;
    void        clearBoardHighlights()                                     override;
    void        bitMovedFromTo(Bit& bit, BitHolder& src, BitHolder& dst)   override;
    void        stopGame()                                                 override;
    Player*     checkForWinner()                                           override;
    bool        checkForDraw()                                             override;
    std::string initialStateString()                                       override;
    std::string stateString()                                              override;
    void        setStateString(const std::string& s)                      override;
    Grid*       getGrid()                                                  override { return _grid; }

    bool gameHasAI() override { return true; }
    void updateAI()  override;

    void resetGame();
    void setHumanPlayer(int color);  // WHITE or BLACK
    void setBoardFlipped(bool flipped);
    bool isBoardFlipped() const { return _boardFlipped; }

private:
    Bit*    PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void    FENtoBoard(const std::string& fen);
    char    pieceNotation(int x, int y) const;
    void    applyMoveToBoard(const ChessMove& move);

    BitboardElement generateKnightMoveBitBoard(int square);
    BitboardElement generateKingMoveBitBoard(int square);

    void buildBitboards(const std::string& state);
    bool isInCheck(const std::string& state, int playerColor) const;

    std::vector<ChessMove> generatePseudoLegalMoves(const std::string& state, int currentPlayer);
    std::vector<ChessMove> generateAllMoves(const std::string& state, int currentPlayer);

    void generatePawnMoves  (std::vector<ChessMove>& moves, BitboardElement pawnBoard,
                             uint64_t occupancy, uint64_t opp_occupancy,
                             int currentPlayer, int enPassantSq);
    void generateKnightMoves(std::vector<ChessMove>& moves, BitboardElement knightBoard,
                             uint64_t occupancy);
    void generateKingMoves  (std::vector<ChessMove>& moves, BitboardElement kingBoard,
                             uint64_t occupancy);
    void generateBishopMoves(std::vector<ChessMove>& moves, BitboardElement bishopBoard,
                             uint64_t occupancy, uint64_t self_occupancy);
    void generateRookMoves  (std::vector<ChessMove>& moves, BitboardElement rookBoard,
                             uint64_t occupancy, uint64_t self_occupancy);
    void generateQueenMoves (std::vector<ChessMove>& moves, BitboardElement queenBoard,
                             uint64_t occupancy, uint64_t self_occupancy);
    void generateCastlingMoves(std::vector<ChessMove>& moves, const std::string& state,
                               int currentPlayer);

    struct MoveUndo {
        char fromChar, toChar, epChar;
        int  epSquare;
    };
    MoveUndo applyMoveToState(char* state, const ChessMove& move, int epSquare);
    void     unapplyMoveFromState(char* state, const ChessMove& move, const MoveUndo& undo);

    int negaMax(char* state, int depth, int alpha, int beta, int playerColor, int epSquare);
    int evaluate(const char* state);

    int             _currentPlayer;
    int             _humanPlayer;          
    bool            _boardFlipped;         
    int             _countState;
    int             _enPassantSquare;
    bool            _castlingRights[4];
    Grid*           _grid;
    BitboardElement _knightBitboards[64];
    BitboardElement _kingBitboards[64];
    std::vector<ChessMove> _moves;
    BitboardElement _bitboards[e_numBitBoards];
    int             _bitboardLookup[128];
};