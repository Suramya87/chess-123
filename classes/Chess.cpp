#include "Chess.h"
#include "Bit.h"
#include "BitHolder.h"
#include "Bitboard.h"
#include "ChessSquare.h"
#include "Game.h"
#include "MagicBitboards.h"
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <algorithm>

Chess::Chess()
{
    _grid = new Grid(8, 8);

    static bool magicReady = false;
    if (!magicReady) { initMagicBitboards(); magicReady = true; }

    for (int i = 0; i < 64; i++) {
        _knightBitboards[i] = generateKnightMoveBitBoard(i);
        _kingBitboards[i]   = generateKingMoveBitBoard(i);
    }

    for (int i = 0; i < 128; i++) { _bitboardLookup[i] = EMPTY_SQUARES; }
    _bitboardLookup['P'] = WHITE_PAWNS;
    _bitboardLookup['N'] = WHITE_KNIGHTS;
    _bitboardLookup['B'] = WHITE_BISHOPS;
    _bitboardLookup['R'] = WHITE_ROOKS;
    _bitboardLookup['Q'] = WHITE_QUEENS;
    _bitboardLookup['K'] = WHITE_KING;
    _bitboardLookup['p'] = BLACK_PAWNS;
    _bitboardLookup['n'] = BLACK_KNIGHTS;
    _bitboardLookup['b'] = BLACK_BISHOPS;
    _bitboardLookup['r'] = BLACK_ROOKS;
    _bitboardLookup['q'] = BLACK_QUEENS;
    _bitboardLookup['k'] = BLACK_KING;
    _bitboardLookup['0'] = EMPTY_SQUARES;

    _currentPlayer    = WHITE;
    _humanPlayer      = WHITE;
    _boardFlipped     = false;
    _countState       = 0;
    _enPassantSquare  = -1;
    _castlingRights[0] = _castlingRights[1] = true;
    _castlingRights[2] = _castlingRights[3] = true;
}

Chess::~Chess()
{
    delete _grid;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    _currentPlayer   = WHITE;
    _enPassantSquare = -1;
    _castlingRights[0] = _castlingRights[1] = true;
    _castlingRights[2] = _castlingRights[3] = true;

    int aiPlayer = (_humanPlayer == WHITE) ? 1 : 0;
    setAIPlayer(aiPlayer);

    _moves = generateAllMoves(stateString(), _currentPlayer);
    startGame();
}

void Chess::FENtoBoard(const std::string& fen)
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->setBit(nullptr);
    });

    int row = 7, col = 0;
    for (char ch : fen) {
        if (ch == ' ') break;
        if (ch == '/') { row--; col = 0; }
        else if (std::isdigit(ch)) { col += ch - '0'; }
        else {
            ChessPiece piece = Pawn;
            switch (std::toupper(ch)) {
                case 'R': piece = Rook;   break;
                case 'N': piece = Knight; break;
                case 'B': piece = Bishop; break;
                case 'Q': piece = Queen;  break;
                case 'K': piece = King;   break;
                default:  piece = Pawn;   break;
            }
            int playerNumber = std::isupper(ch) ? 0 : 1;
            Bit* bit = PieceForPlayer(playerNumber, piece);
            ChessSquare* square = _grid->getSquare(col, row);
            bit->setPosition(square->getPosition());
            bit->setParent(square);
            bit->setGameTag(std::isupper(ch) ? piece : piece + 128);
            square->setBit(bit);
            col++;
        }
    }
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = {
        "pawn.png", "knight.png", "bishop.png",
        "rook.png", "queen.png",  "king.png"
    };
    Bit* bit = new Bit();
    std::string spritePath = std::string(playerNumber == 0 ? "w_" : "b_") + pieces[piece - 1];
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    return bit;
}

char Chess::pieceNotation(int x, int y) const
{
    const char* wpieces = { "0PNBRQK" };
    const char* bpieces = { "0pnbrqk" };
    Bit* bit = _grid->getSquare(x, y)->bit();
    if (!bit) return '0';
    return bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag() - 128];
}

void Chess::applyMoveToBoard(const ChessMove& move)
{
    int fromX = move.from % 8, fromY = move.from / 8;
    int toX   = move.to   % 8, toY   = move.to   / 8;
    ChessSquare* srcSq = _grid->getSquare(fromX, fromY);
    ChessSquare* dstSq = _grid->getSquare(toX,   toY);
    Bit* bit = srcSq->bit();

    if (move.flag == FLAG_EN_PASSANT) {
        int capY = (_currentPlayer == WHITE) ? toY - 1 : toY + 1;
        ChessSquare* capSq = _grid->getSquare(toX, capY);
        if (capSq->bit()) capSq->destroyBit();
    }
    if (move.flag == FLAG_CASTLING) {
        int rookFromX, rookToX;
        if (toX == 6) { rookFromX = 7; rookToX = 5; }
        else          { rookFromX = 0; rookToX = 3; }
        ChessSquare* rookFrom = _grid->getSquare(rookFromX, fromY);
        ChessSquare* rookTo   = _grid->getSquare(rookToX,   fromY);
        Bit* rook = rookFrom->bit();
        if (rook) {
            rookTo->setBit(rook); rook->setParent(rookTo);
            rook->moveTo(rookTo->getPosition()); rookFrom->setBit(nullptr);
        }
    }
    dstSq->dropBitAtPoint(bit, ImVec2(0, 0));
    srcSq->setBit(nullptr);
    if (move.flag == FLAG_PROMOTION) {
        int player = (_currentPlayer == WHITE) ? 0 : 1;
        dstSq->destroyBit();
        Bit* promoted = PieceForPlayer(player, (ChessPiece)move.promoPiece);
        promoted->setGameTag(player == 1 ? move.promoPiece + 128 : move.promoPiece);
        dstSq->setBit(promoted); promoted->setParent(dstSq);
        promoted->setPosition(dstSq->getPosition());
    }
    _enPassantSquare = -1;
    if (move.piece == Pawn) {
        int diff = (int)move.to - (int)move.from;
        if (diff == 16)       _enPassantSquare = move.from + 8;
        else if (diff == -16) _enPassantSquare = move.from - 8;
    }
    if (move.piece == King) {
        if (_currentPlayer == WHITE) { _castlingRights[0] = _castlingRights[1] = false; }
        else                         { _castlingRights[2] = _castlingRights[3] = false; }
    }
    if (move.piece == Rook) {
        if (move.from == 0)  _castlingRights[1] = false;
        if (move.from == 7)  _castlingRights[0] = false;
        if (move.from == 56) _castlingRights[3] = false;
        if (move.from == 63) _castlingRights[2] = false;
    }
    if (move.to == 0)  _castlingRights[1] = false;
    if (move.to == 7)  _castlingRights[0] = false;
    if (move.to == 56) _castlingRights[3] = false;
    if (move.to == 63) _castlingRights[2] = false;
}

bool Chess::actionForEmptyHolder(BitHolder&) { return false; }

bool Chess::canBitMoveFrom(Bit& bit, BitHolder& src)
{
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor    = bit.gameTag() & 128;
    return pieceColor == currentPlayer;
}

bool Chess::canBitMoveFromTo(Bit& bit, BitHolder& src, BitHolder& dst)
{
    ChessSquare* squareSrc = static_cast<ChessSquare*>(&src);
    ChessSquare* squareDst = static_cast<ChessSquare*>(&dst);
    int from = squareSrc->getSquareIndex();
    int to   = squareDst->getSquareIndex();
    for (auto& move : _moves)
        if (move.from == from && move.to == to) return true;
    return false;
}

void Chess::clearBoardHighlights()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->setHighlighted(false);
    });
}

void Chess::bitMovedFromTo(Bit& bit, BitHolder& src, BitHolder& dst)
{
    ChessSquare* squareSrc = static_cast<ChessSquare*>(&src);
    ChessSquare* squareDst = static_cast<ChessSquare*>(&dst);
    int from = squareSrc->getSquareIndex();
    int to   = squareDst->getSquareIndex();

    ChessMove matchedMove;
    for (auto& m : _moves) {
        if (m.from == from && m.to == to) { matchedMove = m; break; }
    }

    if (matchedMove.flag == FLAG_EN_PASSANT) {
        int capY = (_currentPlayer == WHITE) ? (to / 8) - 1 : (to / 8) + 1;
        ChessSquare* capSq = _grid->getSquare(to % 8, capY);
        if (capSq->bit()) capSq->destroyBit();
    }
    if (matchedMove.flag == FLAG_CASTLING) {
        int fromY     = from / 8;
        int rookFromX = (to % 8 == 6) ? 7 : 0;
        int rookToX   = (to % 8 == 6) ? 5 : 3;
        ChessSquare* rookFrom = _grid->getSquare(rookFromX, fromY);
        ChessSquare* rookTo   = _grid->getSquare(rookToX,   fromY);
        Bit* rook = rookFrom->bit();
        if (rook) {
            rookTo->setBit(rook); rook->setParent(rookTo);
            rook->moveTo(rookTo->getPosition()); rookFrom->setBit(nullptr);
        }
    }
    if (matchedMove.flag == FLAG_PROMOTION) {
        int player = (_currentPlayer == WHITE) ? 0 : 1;
        squareDst->destroyBit();
        Bit* promoted = PieceForPlayer(player, (ChessPiece)matchedMove.promoPiece);
        promoted->setGameTag(player == 1 ? matchedMove.promoPiece + 128 : matchedMove.promoPiece);
        squareDst->setBit(promoted); promoted->setParent(squareDst);
        promoted->setPosition(squareDst->getPosition());
    }

    _enPassantSquare = -1;
    if (matchedMove.piece == Pawn) {
        int diff = (int)to - (int)from;
        if (diff == 16)       _enPassantSquare = from + 8;
        else if (diff == -16) _enPassantSquare = from - 8;
    }
    if (matchedMove.piece == King) {
        if (_currentPlayer == WHITE) { _castlingRights[0] = _castlingRights[1] = false; }
        else                         { _castlingRights[2] = _castlingRights[3] = false; }
    }
    if (matchedMove.piece == Rook) {
        if (from == 0)  _castlingRights[1] = false;
        if (from == 7)  _castlingRights[0] = false;
        if (from == 56) _castlingRights[3] = false;
        if (from == 63) _castlingRights[2] = false;
    }
    if (to == 0)  _castlingRights[1] = false;
    if (to == 7)  _castlingRights[0] = false;
    if (to == 56) _castlingRights[3] = false;
    if (to == 63) _castlingRights[2] = false;

    _currentPlayer = (_currentPlayer == WHITE ? BLACK : WHITE);
    _moves = generateAllMoves(stateString(), _currentPlayer);
    clearBoardHighlights();
    endTurn();
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) { square->destroyBit(); });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return nullptr;
    auto sq = _grid->getSquare(x, y);
    if (!sq || !sq->bit()) return nullptr;
    return sq->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    if (_moves.empty() && isInCheck(stateString(), _currentPlayer))
        return getPlayerAt(_currentPlayer == WHITE ? 1 : 0);
    return nullptr;
}

bool Chess::checkForDraw()
{
    return _moves.empty() && !isInCheck(stateString(), _currentPlayer);
}

std::string Chess::initialStateString() { return stateString(); }

std::string Chess::stateString()
{
    std::string s; s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) { s += pieceNotation(x, y); });
    return s;
}

void Chess::setStateString(const std::string& s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char pn = s[index] - '0';
        if (pn) square->setBit(PieceForPlayer(pn - 1, Pawn));
        else    square->setBit(nullptr);
    });
}

BitboardElement Chess::generateKnightMoveBitBoard(int square)
{
    const std::pair<int,int> offsets[] = {
        {2,1},{-2,1},{2,-1},{-2,-1},{1,2},{-1,2},{1,-2},{-1,-2}
    };
    int file = square % 8, rank = square / 8;
    uint64_t data = 0;
    for (auto& o : offsets) {
        int nf = file + o.first, nr = rank + o.second;
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8)
            data |= 1ULL << (nf + nr * 8);
    }
    return BitboardElement(data);
}

BitboardElement Chess::generateKingMoveBitBoard(int square)
{
    const std::pair<int,int> offsets[] = {
        {1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}
    };
    int file = square % 8, rank = square / 8;
    uint64_t data = 0;
    for (auto& o : offsets) {
        int nf = file + o.first, nr = rank + o.second;
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8)
            data |= 1ULL << (nf + nr * 8);
    }
    return BitboardElement(data);
}

void Chess::buildBitboards(const std::string& state)
{
    for (int i = 0; i < e_numBitBoards; i++) _bitboards[i] = 0;
    for (int i = 0; i < 64; i++) {
        char ch = state[i];
        int  bi = _bitboardLookup[(unsigned char)ch];
        _bitboards[bi] |= 1ULL << i;
        if (ch != '0') {
            _bitboards[OCCUPANCY] |= 1ULL << i;
            _bitboards[std::isupper(ch) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES] |= 1ULL << i;
        }
    }
}

bool Chess::isInCheck(const std::string& state, int playerColor) const
{
    BitboardElement tmp[e_numBitBoards];
    for (int i = 0; i < e_numBitBoards; i++) tmp[i] = 0;
    int lookup[128];
    for (int i = 0; i < 128; i++) lookup[i] = EMPTY_SQUARES;
    lookup['P']=WHITE_PAWNS; lookup['N']=WHITE_KNIGHTS; lookup['B']=WHITE_BISHOPS;
    lookup['R']=WHITE_ROOKS; lookup['Q']=WHITE_QUEENS;  lookup['K']=WHITE_KING;
    lookup['p']=BLACK_PAWNS; lookup['n']=BLACK_KNIGHTS; lookup['b']=BLACK_BISHOPS;
    lookup['r']=BLACK_ROOKS; lookup['q']=BLACK_QUEENS;  lookup['k']=BLACK_KING;
    lookup['0']=EMPTY_SQUARES;

    for (int i = 0; i < 64; i++) {
        char ch = state[i];
        tmp[lookup[(unsigned char)ch]] |= 1ULL << i;
        if (ch != '0') {
            tmp[OCCUPANCY] |= 1ULL << i;
            tmp[std::isupper(ch) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES] |= 1ULL << i;
        }
    }

    int kingIdx = (playerColor == WHITE) ? WHITE_KING : BLACK_KING;
    uint64_t kingBB = tmp[kingIdx].getData();
    if (!kingBB) return false;

    int kingSq = 0;
    uint64_t kb = kingBB;
    while ((kb & 1) == 0) { kb >>= 1; kingSq++; }

    uint64_t occ   = tmp[OCCUPANCY].getData();
    int      eBase = (playerColor == WHITE) ? BLACK_PAWNS : WHITE_PAWNS;

    if (getRookAttacks(kingSq, occ)   & (tmp[eBase+3].getData() | tmp[eBase+4].getData())) return true;
    if (getBishopAttacks(kingSq, occ) & (tmp[eBase+2].getData() | tmp[eBase+4].getData())) return true;
    if (_knightBitboards[kingSq].getData() & tmp[eBase+1].getData()) return true;
    if (_kingBitboards[kingSq].getData()   & tmp[eBase+5].getData()) return true;

    uint64_t kingBit = 1ULL << kingSq;
    uint64_t enemyP  = tmp[eBase+0].getData();
    if (playerColor == WHITE) {
        uint64_t attacked = ((kingBit << 7) & ~0x8080808080808080ULL) |
                            ((kingBit << 9) & ~0x0101010101010101ULL);
        if (attacked & enemyP) return true;
    } else {
        uint64_t attacked = ((kingBit >> 7) & ~0x0101010101010101ULL) |
                            ((kingBit >> 9) & ~0x8080808080808080ULL);
        if (attacked & enemyP) return true;
    }
    return false;
}

Chess::MoveUndo Chess::applyMoveToState(char* s, const ChessMove& move, int epSquare)
{
    MoveUndo undo;
    undo.fromChar = s[move.from]; undo.toChar = s[move.to];
    undo.epSquare = epSquare;     undo.epChar = '0';

    s[move.to] = s[move.from]; s[move.from] = '0';

    if (move.flag == FLAG_EN_PASSANT && epSquare >= 0) {
        int capFile = epSquare % 8;
        int capRank = (undo.fromChar == 'P') ? epSquare / 8 - 1 : epSquare / 8 + 1;
        int capSq   = capRank * 8 + capFile;
        undo.epChar = s[capSq]; s[capSq] = '0';
    }
    if (move.flag == FLAG_PROMOTION) {
        bool isWhite = std::isupper(s[move.to]);
        char promoChar = isWhite ? 'Q' : 'q';
        switch (move.promoPiece) {
            case Knight: promoChar = isWhite ? 'N' : 'n'; break;
            case Bishop: promoChar = isWhite ? 'B' : 'b'; break;
            case Rook:   promoChar = isWhite ? 'R' : 'r'; break;
            default:     promoChar = isWhite ? 'Q' : 'q'; break;
        }
        s[move.to] = promoChar;
    }
    if (move.flag == FLAG_CASTLING) {
        int rank     = move.from / 8;
        bool kside   = (move.to % 8 == 6);
        int rookFrom = rank * 8 + (kside ? 7 : 0);
        int rookTo   = rank * 8 + (kside ? 5 : 3);
        s[rookTo] = s[rookFrom]; s[rookFrom] = '0';
    }
    return undo;
}

void Chess::unapplyMoveFromState(char* s, const ChessMove& move, const MoveUndo& undo)
{
    s[move.from] = undo.fromChar; s[move.to] = undo.toChar;
    if (move.flag == FLAG_EN_PASSANT && undo.epSquare >= 0) {
        int capFile = undo.epSquare % 8;
        int capRank = (undo.fromChar == 'P') ? undo.epSquare / 8 - 1 : undo.epSquare / 8 + 1;
        s[capRank * 8 + capFile] = undo.epChar;
    }
    if (move.flag == FLAG_CASTLING) {
        int rank     = move.from / 8;
        bool kside   = (move.to % 8 == 6);
        int rookFrom = rank * 8 + (kside ? 7 : 0);
        int rookTo   = rank * 8 + (kside ? 5 : 3);
        s[rookFrom] = s[rookTo]; s[rookTo] = '0';
    }
}

void Chess::generatePawnMoves(std::vector<ChessMove>& moves, BitboardElement pawnBoard,
                              uint64_t occupancy, uint64_t opp_occupancy,
                              int currentPlayer, int enPassantSq)
{
    int direction  = (currentPlayer == WHITE) ?  8 : -8;
    int doubleRank = (currentPlayer == WHITE) ?  1 :  6;
    int promoRank  = (currentPlayer == WHITE) ?  6 :  1;

    pawnBoard.forEachBit([&](int from) {
        int rank = from / 8;
        int to   = from + direction;
        if (to >= 0 && to < 64 && !(occupancy & (1ULL << to))) {
            if (rank == promoRank) {
                moves.emplace_back(from, to, Pawn, FLAG_PROMOTION, Queen);
                moves.emplace_back(from, to, Pawn, FLAG_PROMOTION, Rook);
                moves.emplace_back(from, to, Pawn, FLAG_PROMOTION, Bishop);
                moves.emplace_back(from, to, Pawn, FLAG_PROMOTION, Knight);
            } else {
                moves.emplace_back(from, to, Pawn, FLAG_NONE);
            }
            if (rank == doubleRank) {
                int two = from + direction * 2;
                if (two >= 0 && two < 64 && !(occupancy & (1ULL << two)))
                    moves.emplace_back(from, two, Pawn, FLAG_NONE);
            }
        }
        int file = from % 8;
        for (int side : {-1, 1}) {
            if (side == -1 && file == 0) continue;
            if (side ==  1 && file == 7) continue;
            int cap = from + direction + side;
            if (cap < 0 || cap >= 64) continue;
            bool isCapture   = (opp_occupancy & (1ULL << cap)) != 0;
            bool isEnPassant = (cap == enPassantSq);
            if (isCapture || isEnPassant) {
                MoveFlag flag = isEnPassant ? FLAG_EN_PASSANT : FLAG_NONE;
                if (rank == promoRank && !isEnPassant) {
                    moves.emplace_back(from, cap, Pawn, FLAG_PROMOTION, Queen);
                    moves.emplace_back(from, cap, Pawn, FLAG_PROMOTION, Rook);
                    moves.emplace_back(from, cap, Pawn, FLAG_PROMOTION, Bishop);
                    moves.emplace_back(from, cap, Pawn, FLAG_PROMOTION, Knight);
                } else {
                    moves.emplace_back(from, cap, Pawn, flag);
                }
            }
        }
    });
}

void Chess::generateKnightMoves(std::vector<ChessMove>& moves, BitboardElement knightBoard, uint64_t occupancy)
{
    knightBoard.forEachBit([&](int from) {
        BitboardElement canMoveTo(_knightBitboards[from].getData() & occupancy);
        canMoveTo.forEachBit([&, from](int to) { moves.emplace_back(from, to, Knight, FLAG_NONE); });
    });
}

void Chess::generateKingMoves(std::vector<ChessMove>& moves, BitboardElement kingBoard, uint64_t occupancy)
{
    kingBoard.forEachBit([&](int from) {
        BitboardElement canMoveTo(_kingBitboards[from].getData() & occupancy);
        canMoveTo.forEachBit([&, from](int to) { moves.emplace_back(from, to, King, FLAG_NONE); });
    });
}

void Chess::generateBishopMoves(std::vector<ChessMove>& moves, BitboardElement bishopBoard,
                                uint64_t occupancy, uint64_t self_occupancy)
{
    bishopBoard.forEachBit([&](int from) {
        BitboardElement canMoveTo(getBishopAttacks(from, occupancy) & ~self_occupancy);
        canMoveTo.forEachBit([&, from](int to) { moves.emplace_back(from, to, Bishop, FLAG_NONE); });
    });
}

void Chess::generateRookMoves(std::vector<ChessMove>& moves, BitboardElement rookBoard,
                              uint64_t occupancy, uint64_t self_occupancy)
{
    rookBoard.forEachBit([&](int from) {
        BitboardElement canMoveTo(getRookAttacks(from, occupancy) & ~self_occupancy);
        canMoveTo.forEachBit([&, from](int to) { moves.emplace_back(from, to, Rook, FLAG_NONE); });
    });
}

void Chess::generateQueenMoves(std::vector<ChessMove>& moves, BitboardElement queenBoard,
                               uint64_t occupancy, uint64_t self_occupancy)
{
    queenBoard.forEachBit([&](int from) {
        BitboardElement canMoveTo(getQueenAttacks(from, occupancy) & ~self_occupancy);
        canMoveTo.forEachBit([&, from](int to) { moves.emplace_back(from, to, Queen, FLAG_NONE); });
    });
}

void Chess::generateCastlingMoves(std::vector<ChessMove>& moves, const std::string& state, int currentPlayer)
{
    if (isInCheck(state, currentPlayer)) return;

    if (currentPlayer == WHITE) {
        int kingSq = 4;
        if (_castlingRights[0] && state[5] == '0' && state[6] == '0') {
            char tmp[64]; std::memcpy(tmp, state.c_str(), 64);
            tmp[5] = tmp[4]; tmp[4] = '0';
            if (!isInCheck(std::string(tmp, 64), WHITE)) {
                tmp[6] = tmp[5]; tmp[5] = '0';
                if (!isInCheck(std::string(tmp, 64), WHITE))
                    moves.emplace_back(kingSq, 6, King, FLAG_CASTLING);
            }
        }
        if (_castlingRights[1] && state[1] == '0' && state[2] == '0' && state[3] == '0') {
            char tmp[64]; std::memcpy(tmp, state.c_str(), 64);
            tmp[3] = tmp[4]; tmp[4] = '0';
            if (!isInCheck(std::string(tmp, 64), WHITE)) {
                tmp[2] = tmp[3]; tmp[3] = '0';
                if (!isInCheck(std::string(tmp, 64), WHITE))
                    moves.emplace_back(kingSq, 2, King, FLAG_CASTLING);
            }
        }
    } else {
        int kingSq = 60;
        if (_castlingRights[2] && state[61] == '0' && state[62] == '0') {
            char tmp[64]; std::memcpy(tmp, state.c_str(), 64);
            tmp[61] = tmp[60]; tmp[60] = '0';
            if (!isInCheck(std::string(tmp, 64), BLACK)) {
                tmp[62] = tmp[61]; tmp[61] = '0';
                if (!isInCheck(std::string(tmp, 64), BLACK))
                    moves.emplace_back(kingSq, 62, King, FLAG_CASTLING);
            }
        }
        if (_castlingRights[3] && state[57] == '0' && state[58] == '0' && state[59] == '0') {
            char tmp[64]; std::memcpy(tmp, state.c_str(), 64);
            tmp[59] = tmp[60]; tmp[60] = '0';
            if (!isInCheck(std::string(tmp, 64), BLACK)) {
                tmp[58] = tmp[59]; tmp[59] = '0';
                if (!isInCheck(std::string(tmp, 64), BLACK))
                    moves.emplace_back(kingSq, 58, King, FLAG_CASTLING);
            }
        }
    }
}

std::vector<ChessMove> Chess::generatePseudoLegalMoves(const std::string& state, int currentPlayer)
{
    std::vector<ChessMove> moves; moves.reserve(48);
    buildBitboards(state);
    int baseIdx    = (currentPlayer == WHITE) ? WHITE_PAWNS     : BLACK_PAWNS;
    int selfOccIdx = (currentPlayer == WHITE) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES;
    int oppOccIdx  = (currentPlayer == WHITE) ? BLACK_ALL_PIECES : WHITE_ALL_PIECES;
    generatePawnMoves  (moves, _bitboards[baseIdx+0], _bitboards[OCCUPANCY].getData(), _bitboards[oppOccIdx].getData(), currentPlayer, _enPassantSquare);
    generateKnightMoves(moves, _bitboards[baseIdx+1], ~_bitboards[selfOccIdx].getData());
    generateBishopMoves(moves, _bitboards[baseIdx+2], _bitboards[OCCUPANCY].getData(), _bitboards[selfOccIdx].getData());
    generateRookMoves  (moves, _bitboards[baseIdx+3], _bitboards[OCCUPANCY].getData(), _bitboards[selfOccIdx].getData());
    generateQueenMoves (moves, _bitboards[baseIdx+4], _bitboards[OCCUPANCY].getData(), _bitboards[selfOccIdx].getData());
    generateKingMoves  (moves, _bitboards[baseIdx+5], ~_bitboards[selfOccIdx].getData());
    generateCastlingMoves(moves, state, currentPlayer);
    return moves;
}

std::vector<ChessMove> Chess::generateAllMoves(const std::string& state, int currentPlayer)
{
    auto pseudo = generatePseudoLegalMoves(state, currentPlayer);
    std::vector<ChessMove> legal; legal.reserve(pseudo.size());
    char s[64]; std::memcpy(s, state.c_str(), 64);
    for (auto& move : pseudo) {
        auto undo = applyMoveToState(s, move, _enPassantSquare);
        if (!isInCheck(std::string(s, 64), currentPlayer)) legal.push_back(move);
        unapplyMoveFromState(s, move, undo);
    }
    return legal;
}

int Chess::evaluate(const char* state)
{
    static int values[256] = {};
    static bool inited = false;
    if (!inited) {
        inited = true;
        values['P']=100; values['p']=-100; values['N']=300; values['n']=-300;
        values['B']=350; values['b']=-350; values['R']=500; values['r']=-500;
        values['Q']=900; values['q']=-900; values['K']=2000;values['k']=-2000;
    }
    int score = 0;
    for (int i = 0; i < 64; i++) score += values[(unsigned char)state[i]];
    return score;
}

int Chess::negaMax(char* state, int depth, int alpha, int beta, int playerColor, int epSquare)
{
    _countState++;
    if (depth == 0) return evaluate(state) * playerColor;

    int savedEP = _enPassantSquare;
    _enPassantSquare = epSquare;
    auto moves = generateAllMoves(std::string(state, 64), playerColor);
    _enPassantSquare = savedEP;

    if (moves.empty()) {
        if (isInCheck(std::string(state, 64), playerColor)) return -100000 + (3 - depth) * 100;
        return 0;
    }

    int best = std::numeric_limits<int>::min() + 1;
    for (auto& move : moves) {
        int nextEP = -1;
        if (move.piece == Pawn) {
            int diff = (int)move.to - (int)move.from;
            if (diff == 16) nextEP = move.from + 8;
            else if (diff == -16) nextEP = move.from - 8;
        }
        _enPassantSquare = epSquare;
        auto undo = applyMoveToState(state, move, epSquare);
        _enPassantSquare = savedEP;
        int score = -negaMax(state, depth-1, -beta, -alpha, playerColor == WHITE ? BLACK : WHITE, nextEP);
        unapplyMoveFromState(state, move, undo);
        if (score > best)  best  = score;
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    return best;
}

void Chess::resetGame()   { stopGame(); setUpBoard(); }

void Chess::setHumanPlayer(int color)
{
    _humanPlayer  = color;
    _boardFlipped = (color == BLACK);
}

void Chess::setBoardFlipped(bool flipped)
{
    _boardFlipped = flipped;
    float sz = (float)pieceSize;
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        float px = _boardFlipped ? sz*(7-x)+sz/2.0f : sz*x+sz/2.0f;
        float py = _boardFlipped ? sz*y+sz/2.0f     : sz*(7-y)+sz/2.0f;
        square->setPosition(px, py);
        if (square->bit()) square->bit()->setPosition(square->getPosition());
    });
}

void Chess::updateAI()
{
    const int myInfinity  = std::numeric_limits<int>::max() - 1;
    const int searchDepth = 3;
    int       bestScore = -myInfinity;
    ChessMove bestMove;

    std::string baseStateStr = stateString();
    char state[65]; std::memcpy(state, baseStateStr.c_str(), 64); state[64] = '\0';
    _countState = 0;

    for (auto& move : _moves) {
        int nextEP = -1;
        if (move.piece == Pawn) {
            int diff = (int)move.to - (int)move.from;
            if (diff == 16) nextEP = move.from + 8;
            else if (diff == -16) nextEP = move.from - 8;
        }
        auto undo = applyMoveToState(state, move, _enPassantSquare);
        int score = -negaMax(state, searchDepth-1, -myInfinity, myInfinity,
                             _currentPlayer == WHITE ? BLACK : WHITE, nextEP);
        unapplyMoveFromState(state, move, undo);
        if (score > bestScore) { bestScore = score; bestMove = move; }
    }

    if (bestMove.piece != NoPiece) {
        int srcSquare = bestMove.from, dstSquare = bestMove.to;
        BitHolder& src = getHolderAt(srcSquare & 7, srcSquare / 8);
        BitHolder& dst = getHolderAt(dstSquare & 7, dstSquare / 8);
        Bit* bit = src.bit();
        if (bit) {
            dst.dropBitAtPoint(bit, ImVec2(0, 0));
            src.setBit(nullptr);
            if (bestMove.flag == FLAG_CASTLING) {
                int fromY = srcSquare/8;
                int rookFromX = (dstSquare%8==6)?7:0, rookToX = (dstSquare%8==6)?5:3;
                ChessSquare* rf = _grid->getSquare(rookFromX, fromY);
                ChessSquare* rt = _grid->getSquare(rookToX,   fromY);
                Bit* rook = rf->bit();
                if (rook) { rt->setBit(rook); rook->setParent(rt); rook->moveTo(rt->getPosition()); rf->setBit(nullptr); }
            }
            if (bestMove.flag == FLAG_EN_PASSANT) {
                int capY = (_currentPlayer==WHITE)?(dstSquare/8)-1:(dstSquare/8)+1;
                ChessSquare* capSq = _grid->getSquare(dstSquare%8, capY);
                if (capSq->bit()) capSq->destroyBit();
            }
            if (bestMove.flag == FLAG_PROMOTION) {
                int player = (_currentPlayer==WHITE)?0:1;
                ChessSquare* dstSq = _grid->getSquare(dstSquare%8, dstSquare/8);
                dstSq->destroyBit();
                Bit* promoted = PieceForPlayer(player, (ChessPiece)bestMove.promoPiece);
                promoted->setGameTag(player==1 ? bestMove.promoPiece+128 : bestMove.promoPiece);
                dstSq->setBit(promoted); promoted->setParent(dstSq); promoted->setPosition(dstSq->getPosition());
            }
            _enPassantSquare = -1;
            if (bestMove.piece == Pawn) {
                int diff = (int)dstSquare-(int)srcSquare;
                if (diff==16) _enPassantSquare = srcSquare+8;
                else if (diff==-16) _enPassantSquare = srcSquare-8;
            }
            if (bestMove.piece == King) {
                if (_currentPlayer==WHITE) { _castlingRights[0]=_castlingRights[1]=false; }
                else                       { _castlingRights[2]=_castlingRights[3]=false; }
            }
            if (bestMove.piece == Rook) {
                if (srcSquare==0)  _castlingRights[1]=false;
                if (srcSquare==7)  _castlingRights[0]=false;
                if (srcSquare==56) _castlingRights[3]=false;
                if (srcSquare==63) _castlingRights[2]=false;
            }
            _currentPlayer = (_currentPlayer==WHITE ? BLACK : WHITE);
            _moves = generateAllMoves(stateString(), _currentPlayer);
            clearBoardHighlights();
            endTurn();
        }
    }
}