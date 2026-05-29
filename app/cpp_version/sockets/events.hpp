#include "rooms.hpp"
#include <mutex>

inline std::mutex room_mutex; // Mutex to protect access to the games map

/*Receives {num_players, name, color}.
        Creates a new game, joins it as player 0, adds the sid to the SocketIO
        room, registers the sid, and emits 'room_created' to the creator.*/

        void handle_create_room(const crow::json::wvalue& data, crow::response& res, uintptr_t sid) {
            int num_players = data["num_players"].dump().empty() ? 0 : std::stoi(data["num_players"].dump());
            std::string name = data["name"].dump().empty() ? "" : data["name"].dump().substr(1, data["name"].dump().size() - 2); // Remove quotes
            std::string color = data["color"].dump().empty() ? "" : data["color"].dump().substr(1, data["color"].dump().size() - 2); // Remove quotes

            std::string code;
            // Create a new game and add the player
            {
                //create_game() returns a Game object, not a code
                std::lock_guard<std::mutex> lock(room_mutex);
                Game* game = Rooms::get_instance().create_game(num_players);
                code = game->code;

                //add the creator as the first player
                Rooms::get_instance().join_game(code, name, color);

                // Join the SocketIO room
                crow::WebSocket::join(game->code);
                Rooms::get_instance().register_player(sid,game->code,0);
            };

            // Emit 'room_created' to the creator
            crow::json::wvalue response_data;
            response_data["game_code"] = code;
            crow::socketio::emit("room_created", response_data);

            res.code = 200;
        }