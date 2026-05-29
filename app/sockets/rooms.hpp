#include "../models/game.hpp"

class Rooms{

    private:
    static Rooms* rooms;

     Rooms() {} // Private constructor to prevent instantiation

    public:


    static Rooms& get_instance() {
        if(!rooms){
            rooms = new Rooms();
        }
        return *rooms;
    }

    ~Rooms(){
        for(auto game : games){
            delete game.second;
        }
    }

    //maps game_code -> Game object
    std::unordered_map<std::string, Game*> games;

    //maps socket_id -> (game_code, player_index)  
    std::unordered_map<uintptr_t,std::pair<std::string,int>> sid_to_game;

    /*Creates a new Game instance with the given number of players,
    stores it in games under its game_code, and returns the Game object*/
    Game* create_game(int number_player){
        Game* game = new Game(number_player);
        games[game->code] = game;
        return game;
    }

    /*Returns the Game object associated with game_code,
    or null ptr if no game with that code exists*/
    Game* get_game(std::string game_code){
        auto it = games.find(game_code);
        if (it != games.end()) {
            return it->second;
        }
        return nullptr;
    }

    /*Retrieves the game via get_game(), then calls game.join(name, color).
    Returns False if the game does not exist or if join() fails, True otherwise.*/
    bool join_game(std::string game_code, std::string name, std::string color){
        Game* game = get_game(game_code);
        if(!game){
            return false;
        }
        
        return game->join(name, color);
    }

    /*Stores the mapping sid -> (game_code, player_index) in sid_to_game.
    Must be called right after a successful join_game().*/
    void register_player(uintptr_t sid, std::string game_code, int player_index){
        sid_to_game[sid] = {game_code, player_index};
    }

    /*Looks up sid in sid_to_game to find the associated game_code and player_index.
    Retrieves the Game via get_game() and returns (Game, player_index).
    Returns {null,-1} if the sid is not registered.*/
    std::pair<Game*,int> get_game_by_player(uintptr_t sid){
        auto result = sid_to_game.find(sid);
        
        if(result != sid_to_game.end()){
            Game* game = get_game(result->second.first);
            return {game,result->second.second};
        }

        return {nullptr,-1};

    }

    /*Removes sid from sid_to_game.
    Called when a player disconnects*/
    void unregister_player(uintptr_t sid){
        sid_to_game.erase(sid);
    }

    /*Deletes the Game from games.
    Should also clean up any sid_to_game entries pointing to that game
    to avoid dangling references.*/
    void remove_game(std::string game_code){
        auto it = games.find(game_code);
        if(it != games.end()){
            delete it->second; // free the Game object
            games.erase(it);   // remove from games map

            // Clean up sid_to_game entries pointing to this game
            for(auto it = sid_to_game.begin(); it != sid_to_game.end(); ){
                if(it->second.first == game_code){
                    it = sid_to_game.erase(it); // erase returns the next iterator
                } else {
                    ++it;
                }
            }
        }
    }
};

inline Rooms* Rooms::rooms = nullptr;