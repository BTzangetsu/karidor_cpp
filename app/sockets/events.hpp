#pragma once
#include "crow.h"
#include "rooms.hpp"
#include <mutex>
#include <unordered_map>
#include <set>

// Global mutex protecting all shared state (rooms + ws_rooms)
inline std::mutex room_mutex;

// Maps game_code → set of active WebSocket connections in that room
// This is the C++ equivalent of Flask-SocketIO's join_room() system
inline std::unordered_map<std::string, std::set<crow::websocket::connection*>> ws_rooms;

// ── Helper: broadcast a JSON event to all connections in a room ──────────────

/* Sends a named event + payload to every client currently in the room.
   The message format is { "event": "<name>", "data": <payload> }
   so that game.js can dispatch on the event name — same convention
   as the Python SocketIO version. */
inline void broadcast(const std::string& game_code,
                      const std::string& event_name,
                      crow::json::wvalue& payload) {

    crow::json::wvalue message;
    message["event"] = event_name;
    message["data"]  = std::move(payload);
    std::string msg_str = message.dump();

    std::lock_guard<std::mutex> lock(room_mutex);
    auto it = ws_rooms.find(game_code);
    if (it == ws_rooms.end()) return;

    for (crow::websocket::connection* c : it->second) {
        c->send_text(msg_str);
    }
}

// ── Helper: emit a JSON event to a single connection ────────────────────────

inline void emit_to(crow::websocket::connection& conn,
                    const std::string& event_name,
                    crow::json::wvalue payload) {
    crow::json::wvalue message;
    message["event"] = event_name;
    message["data"]  = std::move(payload);
    conn.send_text(message.dump());
}

// ── handle_create_room ───────────────────────────────────────────────────────

/* Receives { "event": "create_room", "data": { num_players, name, color } }
   Creates a new game, joins as player 0, registers the connection,
   adds it to the ws_room, and emits 'room_created' back to the creator. */
inline void handle_create_room(crow::websocket::connection& conn,
                                const crow::json::rvalue& data) {

    // s() extracts a string value directly — no quotes, no substr needed
    // i() extracts an integer value
    int         num_players = (int)data["num_players"].i();
    std::string name        = data["name"].s();
    std::string color       = data["color"].s();

    std::string game_code;
    {
        std::lock_guard<std::mutex> lock(room_mutex);

        // create_game() returns a Game* — the code is on game->code
        Game* game = Rooms::get_instance().create_game(num_players);
        game_code  = game->code;

        // add the creator as the first player (player index 0)
        Rooms::get_instance().join_game(game_code, name, color);

        // register the connection as player 0
        uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
        Rooms::get_instance().register_player(sid, game_code, 0);

        // add this connection to the ws_room for this game_code
        // equivalent of Flask-SocketIO's join_room(game_code)
        ws_rooms[game_code].insert(&conn);
    }

    // emit 'room_created' only to the creator
    crow::json::wvalue response;
    response["game_code"] = game_code;
    emit_to(conn, "room_created", std::move(response));
}

/*  Receives {game_code, name, color}.
    Joins an existing game, registers the sid.
    If the game is now started, broadcasts 'game_start' with get_state()
    to the entire room.*/
inline void handle_join_room    (crow::websocket::connection& conn, const crow::json::rvalue& data){

    //extracting data
    std::string game_code = data["game_code"].s();
    std::string name = data["name"].s();
    std::string color = data["color"].s();

    //response
    crow::json::wvalue res;

    //join_game() calls game.join() internally
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        if(!Rooms::get_instance().join_game(game_code,name,color)){
            res["message"] = "Could not join game";
            emit_to(conn,"error",std::move(res));
            return;
        }
    }

    //join_room()
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        ws_rooms[game_code].insert(&conn);
    }

    //player_index is the last index in board.players after joining
    Game* game;
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        game= Rooms::get_instance().get_game(game_code);
        int player_index = game->board->players.size() - 1;
        uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
        Rooms::get_instance().register_player(sid,game_code,player_index);
    }

    //if all players have joined, broadcast the initial state to everyone
    if(game->started){
        res = std::move(game->get_state());
        broadcast(game_code,"game_start",std::move(res));
    }
    //notify the lobby that a new player joined (useful for the waiting screen)
    else{
        res = std::move(game->get_state());
        broadcast(game_code,"player_joined",std::move(res));
    }
}

/*  Receives {r, c}.
    Retrieves the game via get_game_by_player(sid).
    Verifies it is the sender's turn.
    Calls game.play_move(r, c) and broadcasts the updated state
    to the room via 'game_update'.
    If the game is finished, broadcasts 'game_over' with the winner*/
inline void handle_play_move    (crow::websocket::connection& conn, const crow::json::rvalue& data){
    uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
    std::pair<Game*, int> result;
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        result = Rooms::get_instance().get_game_by_player(sid);
    }
    crow::json::wvalue res;

    Game* game =result.first;
    int player_index =result.second;
    
    if(!game || player_index == -1){
        res["message"] = "Player not found";
        emit_to(conn,"error",std::move(res));
        return;
    }

    //only the current player can act
    if(player_index != game->board->player_turn){
        res["message"] = "Not your turn";
        emit_to(conn,"error",std::move(res));
        return;
    }

    int r = (int)data["r"].i();
    int c = (int)data["c"].i();

    //the move must be valid
    if(!game->play_move(r,c)){
        res["message"] = "Invalid move";
        emit_to(conn,"error",std::move(res));
        return;
    }

    //broadcast the updated state to all players in the room
    if(game->finished){
        res = std::move(game->get_state());
        broadcast(game->code,"game_over",std::move(res));
    }
    else{
        res = std::move(game->get_state());
        broadcast(game->code,"game_update",std::move(res));
    }
}

inline void handle_play_wall    (crow::websocket::connection& conn, const crow::json::rvalue& data){
    uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
    
    std::pair<Game*, int> result;
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        result = Rooms::get_instance().get_game_by_player(sid);
    }
    crow::json::wvalue res;

    Game* game =result.first;
    int player_index =result.second;
    
    if(!game || player_index == -1){
        res["message"] = "Player not found";
        emit_to(conn,"error",std::move(res));
        return;
    }

    //only the current player can act
    if(player_index != game->board->player_turn){
        res["message"] = "Not your turn";
        emit_to(conn,"error",std::move(res));
        return;
    }

    int r1 = (int)data["r1"].i();
    int c1 = (int)data["c1"].i();
    bool vertical = data["vertical"].b();

    //the move must be valid
    if(!game->play_wall(r1,c1,vertical)){
        res["message"] = "Invalid wall placement";
        emit_to(conn,"error",std::move(res));
        return;
    }

    //broadcast the updated state to all players in the room
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        res = std::move(game->get_state());
        broadcast(game->code,"game_update",std::move(res));
    }
}


/*  Receives {game_code}.
    Only allowed for the player at index 0 (the creator).
    Removes the old game, creates a new one with the same num_players
    and the same game_code, broadcasts 'game_restart' to the room.*/
inline void handle_restart_game (crow::websocket::connection& conn, const crow::json::rvalue& data){
    uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
    std::pair<Game*, int> result;
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        result = Rooms::get_instance().get_game_by_player(sid);
    }
    crow::json::wvalue res;

    Game* game =result.first;
    int player_index =result.second;

    //check if the player is found
    if(!game || player_index == -1){
        res["message"] = "Player not found";
        emit_to(conn,"error",std::move(res));
        return;
    }
    
    if(player_index != 0){
        res["message"] = "Only the game creator can restart the game";
        emit_to(conn,"error",std::move(res));
        return;
    }

    std::string game_code = game->code;
    int num_players = game->board->players.size();

    {
        std::lock_guard<std::mutex> lock(room_mutex);
        game->reset();
        // Note: we don't need to update sid_to_game because the player indices and game_code remain the same
        // The connections in ws_rooms also remain the same since the room code doesn't change
    }

    //notify everyone in the room that the game has been restarted
    res["game_code"] = game_code;
    broadcast(game_code,"game_restart",std::move(res));
        
}

/*Receives {game_code, color}.
        Called by game.js on connect to re-register the new sid
        after the page redirect from the lobby.
        Finds the player by color match, re-registers the sid,
        re-joins the SocketIO room.*/
inline void handle_rejoin_game(crow::websocket::connection& conn, const crow::json::rvalue& data){
    std::string game_code = data["game_code"].s();
    std::string color = data["color"].s();

    Game*game;
    {
    std::lock_guard<std::mutex> lock(room_mutex);
    game = Rooms::get_instance().get_game(game_code);
    }
    crow::json::wvalue res; 

    if(!game){
        res["message"] = "Game not found";
        emit_to(conn,"error",std::move(res));
        return;
    }

    //find the player index by matching color
    int player_index = -1;
    for(size_t i=0; i<game->board->players.size(); i++){
        if(game->board->players[i]->color == color){
            player_index = i;
            break;
        }
    }

    if(player_index == -1){
        res["message"] = "Player not found";
        emit_to(conn,"error",std::move(res));
        return;
    }

    //re-join the SocketIO room and re-register the new sid
    {
        std::lock_guard<std::mutex> lock(room_mutex);
        uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
        Rooms::get_instance().register_player(sid, game_code, player_index);
        ws_rooms[game_code].insert(&conn);
    }

    //send the current state so game.js can render immediately
    res = std::move(game->get_state());
    emit_to(conn,"game_update",std::move(res));

}

/*  No data.
    Unregisters the sid from room_manager.
    If the game was ongoing, broadcasts 'player_disconnected' to the room.*/
inline void handle_disconnect   (crow::websocket::connection& conn) {
    uintptr_t sid = reinterpret_cast<uintptr_t>(&conn);
    std::pair<Game*, int> result;

    {
        std::lock_guard<std::mutex> lock(room_mutex);
        result = Rooms::get_instance().get_game_by_player(sid);
    }

    if(!result.first || result.second == -1){
        std::lock_guard<std::mutex> lock(room_mutex);
        Rooms::get_instance().unregister_player(sid);
        //we remove the player from any room they were in, but since we don't know which one it is, we can't broadcast a message to that room
        for(auto& pair : ws_rooms){
            pair.second.erase(&conn);
        }
        return;
    }
    else{
        Game* game = result.first;
        std::string game_code = game->code;

        {
            std::lock_guard<std::mutex> lock(room_mutex);
            Rooms::get_instance().unregister_player(sid);
            // Remove the connection from any room it was in
            for(auto& pair : ws_rooms){
                pair.second.erase(&conn);
            }
        }

        if(game->started && !game->finished){
            crow::json::wvalue res;
            res["player_index"] = result.second;
            res["state"] = std::move(game->get_state());
            broadcast(game_code,"player_disconnected",std::move(res));
        }
    }
}