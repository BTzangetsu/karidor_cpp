from app.models.game import Game

class RoomManager:

    def __init__(self):
        self.games: dict[str, Game] = {}
        # maps game_code -> Game object
        
        self.sid_to_game: dict[str, tuple[str, int]] = {}
        # maps socket_id -> (game_code, player_index)

    def create_game(self, num_players: int) -> Game:
        """Creates a new Game instance with the given number of players,
        stores it in self.games under its game_code, and returns the Game object."""
        game = Game(num_players)
        self.games[game.game_code] = game
        return game
    
    def get_game(self, game_code: str) -> Game | None:
        """Returns the Game object associated with game_code,
        or None if no game with that code exists."""
        return self.games.get(game_code)

    def join_game(self, game_code: str, name: str, color: str) -> bool:
        """Retrieves the game via get_game(), then calls game.join(name, color).
        Returns False if the game does not exist or if join() fails, True otherwise."""
        game = self.get_game(game_code)
        if not game:
            return False
        return game.join(name, color)

    def register_player(self, sid: str, game_code: str, player_index: int) -> None:
        """Stores the mapping sid -> (game_code, player_index) in sid_to_game.
        Must be called right after a successful join_game()."""
        self.sid_to_game[sid] = (game_code, player_index)

    def get_game_by_player(self, sid: str) -> tuple[Game, int] | None:
        """Looks up sid in sid_to_game to find the associated game_code and player_index.
        Retrieves the Game via get_game() and returns (Game, player_index).
        Returns None if the sid is not registered."""
        if sid in self.sid_to_game:
            game_code, player_index = self.sid_to_game[sid]
            game = self.get_game(game_code)
            if game:
                return game, player_index
        return None

    def unregister_player(self, sid: str) -> None:
        """Removes sid from sid_to_game.
        Called when a player disconnects (SocketIO disconnect event)."""
        self.sid_to_game.pop(sid, None)

    def remove_game(self, game_code: str) -> None:
        """Deletes the Game from self.games.
        Should also clean up any sid_to_game entries pointing to that game
        to avoid dangling references."""
        if game_code in self.games:
            del self.games[game_code]
            # Remove all players associated with this game
            for sid, (code, _) in list(self.sid_to_game.items()):
                if code == game_code:
                    del self.sid_to_game[sid]