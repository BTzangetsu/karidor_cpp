#pragma once
#include "board.hpp"
#include "crow.h"
#include <string>
#include <random>
#include <chrono>
#include <stdexcept>

class Game {
public:
    size_t num_players;
    Board* board;
    bool started;
    bool finished;
    int winner;       // index of the winning player, -1 if none
    std::string code;

    Game(size_t nb_players) : num_players(nb_players) {
        board    = new Board(get_board_size());
        started  = false;
        finished = false;
        winner   = -1;
        code     = generate_code();
    }

    ~Game() {
        delete board;
    }

    /* Maps num_players to the correct board size. */
    int get_board_size() const {
        switch (num_players) {
            case 2: return 9;
            case 3: return 11;
            case 4: return 13;
            default: throw std::invalid_argument("Unsupported number of players (must be 2, 3, or 4).");
        }
    }

    /* Returns a random 4-digit string between "1000" and "9999".
       Uses a seeded Mersenne Twister so every run gives a different code. */
    std::string generate_code() {
        std::mt19937 rng(
            static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
        );
        std::uniform_int_distribution<int> dist(1000, 9999);
        return std::to_string(dist(rng));
    }

    /* Called when a player enters the lobby code and joins.
       - Rejects if the game has already started.
       - Creates a Player object and calls board->add_player().
       - Once board->players.size() == num_players, sets started = true.
       - Returns true on success, false on failure. */
    bool join(const std::string& name, const std::string& color) {
        if (started) return false;

        Player* player = new Player(name, color, 0);

        try {
            board->add_player(player);
        } catch (const std::exception& e) {
            // add_player threw (board full or invalid state) — free the player
            delete player;
            return false;
        }

        if (board->players.size() == num_players) {
            started = true;
        }

        return true;
    }

    /* Called when the current player moves their ball.
       - Guards: must be started and not finished.
       - Calls board->make_move(r, c).
       - If valid, checks for a winner.
       - Returns true if the move was accepted, false otherwise. */
    bool play_move(int r, int c) {
        if (!started || finished) return false;

        if (board->make_move(r, c)) {
            int winner_index = board->check_winner();
            if (winner_index != -1) {
                finished = true;
                winner   = winner_index;
            }
            return true;
        }

        return false;
    }

    /* Called when the current player places a wall.
       - Guards: must be started and not finished.
       - Builds a Wall and calls board->add_wall().
       - If the wall is rejected, deletes it and returns false.
       - If accepted, board->add_wall() takes ownership via walls.push_back().
       - Returns true if the wall was placed, false otherwise. */
    bool play_wall(int r, int c, bool vertical) {
        if (!started || finished) return false;

        Wall* wall = new Wall({r, c}, vertical);

        if (board->add_wall(wall)) {
            // board owns the wall now (stored in board->walls, deleted in ~Board)
            return true;
        }

        // add_wall rejected it — we still own it, so delete here
        delete wall;
        return false;
    }

    void reset() {
        //first we keep the information about the players and the number of players, then we delete the old board and create a new one with the same size, then we re-add the players to the new board and reset the game state
        std::vector<std::pair<std::string, std::string>> player_info; // vector of (name, color) pairs
        for(Player* player : board->players){
            player_info.push_back({player->name, player->color});
        }

        
        started = false;
        finished = false;
        winner = -1;

        size_t num_players = board->players.size();
        delete board;
        board = new Board(get_board_size());
        for(size_t i = 0; i < num_players; i++){
            join(player_info[i].first, player_info[i].second);
        }
    }

    /* Returns the full game state as a crow::json::wvalue.
       This is broadcast to all clients via WebSocket after every valid action.

       crow::json::wvalue is move-only (not copyable) — always use std::move()
       when assigning nested wvalue objects.

       Structure:
         {
           game_code    : string,
           board_size   : int,
           started      : bool,
           finished     : bool,
           winner       : int  (-1 if none),
           current_turn : int,
           players      : [ { name, color, walls_left, position: [r, c] }, ... ],
           walls        : [ { vertical, position: [r, c] }, ... ]
         } */
    crow::json::wvalue get_state() {
        crow::json::wvalue state;

        state["game_code"]    = code;
        state["board_size"]   = (int)board->size;
        state["started"]      = started;
        state["finished"]     = finished;
        state["winner"]       = winner;
        state["current_turn"] = (int)board->player_turn;

        // players array
        crow::json::wvalue players_json(crow::json::type::List);
        for (size_t i = 0; i < board->players.size(); i++) {
            crow::json::wvalue p;
            p["name"]       = board->players[i]->name;
            p["color"]      = board->players[i]->color;
            p["walls_left"] = (int)board->players[i]->walls;

            crow::json::wvalue pos(crow::json::type::List);
            pos[0] = board->players[i]->position.first;
            pos[1] = board->players[i]->position.second;
            p["position"] = std::move(pos);

            players_json[i] = std::move(p);
        }
        state["players"] = std::move(players_json);

        // walls array
        crow::json::wvalue walls_json(crow::json::type::List);
        for (size_t i = 0; i < board->walls.size(); i++) {
            crow::json::wvalue w;
            w["vertical"] = board->walls[i]->vertical;

            crow::json::wvalue pos(crow::json::type::List);
            pos[0] = board->walls[i]->position.first;
            pos[1] = board->walls[i]->position.second;
            w["position"] = std::move(pos);

            walls_json[i] = std::move(w);
        }
        state["walls"] = std::move(walls_json);

        return state;
    }
};