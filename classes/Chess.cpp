#include "Chess.h"
#include <limits>
#include <cmath>
#include <string>
#include <cctype>

using namespace std;

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    if (!bit) return '0';
    return bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag() - 128];
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    std::string spritePath = std::string(playerNumber == 0 ? "w_" : "b_") + pieces[piece - 1];
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    bit->setGameTag(playerNumber == 1 ? piece + 128 : piece);
    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;
    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    startGame();
    
    std::vector<BitMove> initialMoves = generateAllMoves(0);  
    size_t moveCount = initialMoves.size();  
}

void Chess::FENtoBoard(const std::string& fen)
{
    auto charToPiece = [](char c) -> ChessPiece {
        switch (tolower(c)) {
            case 'p': return Pawn;
            case 'n': return Knight;
            case 'b': return Bishop;
            case 'r': return Rook;
            case 'q': return Queen;
            case 'k': return King;
            default:  return NoPiece;
        }
    };

    int col = 0;
    int rank = 7;

    for (char c : fen) {
        if (c == ' ') break;

        if (c == '/') {
            col = 0;
            rank--;
        } else if (isdigit(c)) {
            col += c - '0';
        } else {
            ChessPiece piece = charToPiece(c);
            if (piece != NoPiece) {
                int playerNumber = isupper(c) ? 0 : 1;
                Bit* bit = PieceForPlayer(playerNumber, piece);
                ChessSquare* square = _grid->getSquare(col, rank);
                bit->setPosition(square->getPosition());
                square->setBit(bit);
                col++;
            }
        }
    }
}

int Chess::squareToIndex(int x, int y) const
{
    return y * 8 + x;
}

void Chess::indexToSquare(int index, int& x, int& y) const
{
    x = index % 8;
    y = index / 8;
}

bool Chess::isValidSquare(int x, int y) const
{
    return x >= 0 && x < 8 && y >= 0 && y < 8;
}

bool Chess::isOccupied(int x, int y) const
{
    if (!isValidSquare(x, y)) return false;
    BitHolder* holder = _grid->getSquare(x, y);
    return holder && holder->bit() != nullptr;
}

bool Chess::isEnemyPiece(int x, int y, int playerNumber) const
{
    if (!isValidSquare(x, y)) return false;
    Bit* bit = _grid->getSquare(x, y)->bit();
    if (!bit) return false;
    int pieceOwner = (bit->gameTag() & 128) ? 1 : 0;
    return pieceOwner != playerNumber;
}

bool Chess::isFriendlyPiece(int x, int y, int playerNumber) const
{
    if (!isValidSquare(x, y)) return false;
    Bit* bit = _grid->getSquare(x, y)->bit();
    if (!bit) return false;
    int pieceOwner = (bit->gameTag() & 128) ? 1 : 0;
    return pieceOwner == playerNumber;
}

int Chess::getPieceType(Bit* bit) const
{
    if (!bit) return 0;
    return bit->gameTag() & 0x7F;
}

int Chess::getPieceTypeAt(int x, int y) const
{
    if (!isValidSquare(x, y)) return 0;
    Bit* bit = _grid->getSquare(x, y)->bit();
    return getPieceType(bit);
}

uint64_t Chess::getOccupancyBitboard() const
{
    uint64_t occupancy = 0ULL;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (isOccupied(x, y)) {
                occupancy |= (1ULL << squareToIndex(x, y));
            }
        }
    }
    return occupancy;
}

uint64_t Chess::getFriendlyBitboard(int playerNumber) const
{
    uint64_t friendly = 0ULL;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (isFriendlyPiece(x, y, playerNumber)) {
                friendly |= (1ULL << squareToIndex(x, y));
            }
        }
    }
    return friendly;
}

uint64_t Chess::getEnemyBitboard(int playerNumber) const
{
    uint64_t enemy = 0ULL;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (isEnemyPiece(x, y, playerNumber)) {
                enemy |= (1ULL << squareToIndex(x, y));
            }
        }
    }
    return enemy;
}

uint64_t Chess::getBitboardForPieceType(int pieceType, int playerNumber) const
{
    uint64_t bb = 0ULL;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (getPieceTypeAt(x, y) == pieceType && isFriendlyPiece(x, y, playerNumber)) {
                bb |= (1ULL << squareToIndex(x, y));
            }
        }
    }
    return bb;
}

std::vector<BitMove> Chess::generatePawnMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    uint64_t pawns = getBitboardForPieceType(Pawn, playerNumber);
    
    int direction = (playerNumber == 0) ? 1 : -1;
    int startRank = (playerNumber == 0) ? 1 : 6;
    
    for (int i = 0; i < 64; i++) {
        if (!(pawns & (1ULL << i))) continue;
        
        int x, y;
        indexToSquare(i, x, y);
        
        int newY = y + direction;
        if (isValidSquare(x, newY) && !isOccupied(x, newY)) {
            moves.emplace_back(i, squareToIndex(x, newY), Pawn);
            
            if (y == startRank) {
                int doubleY = y + 2 * direction;
                if (!isOccupied(x, doubleY)) {
                    moves.emplace_back(i, squareToIndex(x, doubleY), Pawn);
                }
            }
        }
        
        int captureX[] = {x - 1, x + 1};
        for (int cx : captureX) {
            if (isValidSquare(cx, newY) && isEnemyPiece(cx, newY, playerNumber)) {
                moves.emplace_back(i, squareToIndex(cx, newY), Pawn);
            }
        }
    }
    
    return moves;
}

std::vector<BitMove> Chess::generateKnightMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    uint64_t knights = getBitboardForPieceType(Knight, playerNumber);
    uint64_t friendly = getFriendlyBitboard(playerNumber);
    
    int knightOffsets[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    
    for (int i = 0; i < 64; i++) {
        if (!(knights & (1ULL << i))) continue;
        
        int x, y;
        indexToSquare(i, x, y);
        
        for (const auto& offset : knightOffsets) {
            int newX = x + offset[0];
            int newY = y + offset[1];
            
            if (isValidSquare(newX, newY)) {
                int targetSquare = squareToIndex(newX, newY);
                if (!(friendly & (1ULL << targetSquare))) {
                    moves.emplace_back(i, targetSquare, Knight);
                }
            }
        }
    }
    
    return moves;
}

std::vector<BitMove> Chess::generateBishopMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    uint64_t bishops = getBitboardForPieceType(Bishop, playerNumber);
    uint64_t friendly = getFriendlyBitboard(playerNumber);
    
    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    
    for (int i = 0; i < 64; i++) {
        if (!(bishops & (1ULL << i))) continue;
        
        int x, y;
        indexToSquare(i, x, y);
        
        for (const auto& dir : directions) {
            int newX = x + dir[0];
            int newY = y + dir[1];
            
            while (isValidSquare(newX, newY)) {
                int targetSquare = squareToIndex(newX, newY);
                
                if (friendly & (1ULL << targetSquare)) break;
                
                moves.emplace_back(i, targetSquare, Bishop);
                
                if (isOccupied(newX, newY)) break;
                
                newX += dir[0];
                newY += dir[1];
            }
        }
    }
    
    return moves;
}

std::vector<BitMove> Chess::generateRookMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    uint64_t rooks = getBitboardForPieceType(Rook, playerNumber);
    uint64_t friendly = getFriendlyBitboard(playerNumber);
    
    int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    
    for (int i = 0; i < 64; i++) {
        if (!(rooks & (1ULL << i))) continue;
        
        int x, y;
        indexToSquare(i, x, y);
        
        for (const auto& dir : directions) {
            int newX = x + dir[0];
            int newY = y + dir[1];
            
            while (isValidSquare(newX, newY)) {
                int targetSquare = squareToIndex(newX, newY);
                
                if (friendly & (1ULL << targetSquare)) break;
                
                moves.emplace_back(i, targetSquare, Rook);
                
                if (isOccupied(newX, newY)) break;
                
                newX += dir[0];
                newY += dir[1];
            }
        }
    }
    
    return moves;
}

std::vector<BitMove> Chess::generateQueenMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    uint64_t queens = getBitboardForPieceType(Queen, playerNumber);
    uint64_t friendly = getFriendlyBitboard(playerNumber);
    
    int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    
    for (int i = 0; i < 64; i++) {
        if (!(queens & (1ULL << i))) continue;
        
        int x, y;
        indexToSquare(i, x, y);
        
        for (const auto& dir : directions) {
            int newX = x + dir[0];
            int newY = y + dir[1];
            
            while (isValidSquare(newX, newY)) {
                int targetSquare = squareToIndex(newX, newY);
                
                if (friendly & (1ULL << targetSquare)) break;
                
                moves.emplace_back(i, targetSquare, Queen);
                
                if (isOccupied(newX, newY)) break;
                
                newX += dir[0];
                newY += dir[1];
            }
        }
    }
    
    return moves;
}

std::vector<BitMove> Chess::generateKingMoves(int playerNumber)
{
    std::vector<BitMove> moves;
    uint64_t kings = getBitboardForPieceType(King, playerNumber);
    uint64_t friendly = getFriendlyBitboard(playerNumber);
    
    int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    
    for (int i = 0; i < 64; i++) {
        if (!(kings & (1ULL << i))) continue;
        
        int x, y;
        indexToSquare(i, x, y);
        
        for (const auto& dir : directions) {
            int newX = x + dir[0];
            int newY = y + dir[1];
            
            if (isValidSquare(newX, newY)) {
                int targetSquare = squareToIndex(newX, newY);
                
                if (!(friendly & (1ULL << targetSquare))) {
                    moves.emplace_back(i, targetSquare, King);
                }
            }
        }
    }
    
    return moves;
}

std::vector<BitMove> Chess::generateAllMoves(int playerNumber)
{
    std::vector<BitMove> allMoves;
    
    auto pawnMoves = generatePawnMoves(playerNumber);
    auto knightMoves = generateKnightMoves(playerNumber);
    auto bishopMoves = generateBishopMoves(playerNumber);
    auto rookMoves = generateRookMoves(playerNumber);
    auto queenMoves = generateQueenMoves(playerNumber);
    auto kingMoves = generateKingMoves(playerNumber);
    
    allMoves.insert(allMoves.end(), pawnMoves.begin(), pawnMoves.end());
    allMoves.insert(allMoves.end(), knightMoves.begin(), knightMoves.end());
    allMoves.insert(allMoves.end(), bishopMoves.begin(), bishopMoves.end());
    allMoves.insert(allMoves.end(), rookMoves.begin(), rookMoves.end());
    allMoves.insert(allMoves.end(), queenMoves.begin(), queenMoves.end());
    allMoves.insert(allMoves.end(), kingMoves.begin(), kingMoves.end());
    
    return allMoves;
}

bool Chess::isLegalMove(const BitMove& move, int playerNumber)
{
    std::vector<BitMove> legalMoves = generateAllMoves(playerNumber);
    
    for (const auto& legal : legalMoves) {
        if (legal == move) {
            return true;
        }
    }
    
    return false;
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayer = getCurrentPlayer()->playerNumber();
    int pieceColor = (bit.gameTag() & 128) ? 1 : 0;
    return pieceColor == currentPlayer;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    int srcX = 0, srcY = 0;
    int dstX = 0, dstY = 0;
    
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (_grid->getSquare(x, y) == &src) {
                srcX = x;
                srcY = y;
            }
            if (_grid->getSquare(x, y) == &dst) {
                dstX = x;
                dstY = y;
            }
        }
    }
    
    int currentPlayer = getCurrentPlayer()->playerNumber();
    int fromSquare = squareToIndex(srcX, srcY);
    int toSquare = squareToIndex(dstX, dstY);
    int pieceType = getPieceType(&bit);
    
    BitMove move(fromSquare, toSquare, pieceType);
    return isLegalMove(move, currentPlayer);
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return nullptr;
    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) return nullptr;
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        s += pieceNotation(x, y);
    });
    return s;
}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}

Grid* Chess::getGrid()
{
    return _grid;
}