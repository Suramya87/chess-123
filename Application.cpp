#include "Application.h"
#include "imgui/imgui.h"
#include "classes/TicTacToe.h"
#include "classes/Checkers.h"
#include "classes/Othello.h"
#include "classes/Chess.h"

namespace ClassGame {

    Game*  game        = nullptr;
    Chess* chessGame   = nullptr;
    bool   gameOver    = false;
    int    gameWinner  = -1;

    void GameStartUp()
    {
        game      = nullptr;
        chessGame = nullptr;
    }

    void RenderGame()
    {
        ImGui::DockSpaceOverViewport();

        ImGui::Begin("Settings");

        if (!game) {
            ImGui::Text("Select a game:");
            ImGui::Spacing();

            if (ImGui::Button("Start Tic-Tac-Toe")) {
                chessGame = nullptr;
                game = new TicTacToe();
                game->setUpBoard();
                gameOver = false; gameWinner = -1;
            }
            if (ImGui::Button("Start Checkers")) {
                chessGame = nullptr;
                game = new Checkers();
                game->setUpBoard();
                gameOver = false; gameWinner = -1;
            }
            if (ImGui::Button("Start Othello")) {
                chessGame = nullptr;
                game = new Othello();
                game->setUpBoard();
                gameOver = false; gameWinner = -1;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Chess:");
            ImGui::Spacing();

            if (ImGui::Button("Play as White")) {
                chessGame = new Chess();
                chessGame->setHumanPlayer(WHITE);
                chessGame->setUpBoard();
                game = chessGame;
                gameOver = false; gameWinner = -1;
            }
            if (ImGui::Button("Play as Black")) {
                chessGame = new Chess();
                chessGame->setHumanPlayer(BLACK);
                chessGame->setUpBoard();
                chessGame->setBoardFlipped(true);
                game = chessGame;
                gameOver = false; gameWinner = -1;
            }

        } else {
            if (gameOver) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Game Over!");
                if (gameWinner >= 0)
                    ImGui::Text("Winner: Player %d", gameWinner + 1);
                else
                    ImGui::Text("Draw!");
                ImGui::Spacing();
            }

            if (ImGui::Button("Reset Game")) {
                if (chessGame) chessGame->resetGame();
                else { game->stopGame(); game->setUpBoard(); }
                gameOver = false; gameWinner = -1;
            }

            if (chessGame) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Chess controls:");
                ImGui::Spacing();

                bool flipped = chessGame->isBoardFlipped();
                if (ImGui::Checkbox("Flip board", &flipped))
                    chessGame->setBoardFlipped(flipped);

                ImGui::Spacing();
                if (ImGui::Button("Switch sides & reset")) {
                    int newHuman = chessGame->isBoardFlipped() ? WHITE : BLACK;
                    chessGame->setHumanPlayer(newHuman);
                    chessGame->resetGame();
                    chessGame->setBoardFlipped(newHuman == BLACK);
                    gameOver = false; gameWinner = -1;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Back to menu")) {
                game->stopGame();
                delete game;
                game = nullptr; chessGame = nullptr; gameOver = false;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Turn: %d", game->getCurrentTurnNo());
            ImGui::Text("Current player: %d", game->getCurrentPlayer()->playerNumber() + 1);

            std::string stateString = game->stateString();
            int stride = game->_gameOptions.rowX;
            int height = game->_gameOptions.rowY;
            ImGui::Spacing();
            ImGui::Text("Board state:");
            for (int y = 0; y < height; y++)
                ImGui::Text("%s", stateString.substr(y * stride, stride).c_str());
        }

        ImGui::End();

        ImGui::Begin("GameWindow");
        if (game) {
            if (!gameOver && game->gameHasAI() &&
                (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
            {
                game->updateAI();
            }
            game->drawFrame();
        }
        ImGui::End();
    }

    void EndOfTurn()
    {
        Player* winner = game->checkForWinner();
        if (winner) { gameOver = true; gameWinner = winner->playerNumber(); }
        if (game->checkForDraw()) { gameOver = true; gameWinner = -1; }
    }

}