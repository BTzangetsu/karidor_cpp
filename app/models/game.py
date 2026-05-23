from app.models.board import Board, Player, Wall
import random


class Game:
    def __init__(self, num_players: int = 2):
        self.num_players = num_players        # players expected in the game (2, 3 or 4)
        self.board = Board(size=self._get_board_size())
        self.started = False                  # True when all players have joined
        self.finished = False
        self.winner = None                    # index of the winning player in self.board.players
        self.game_code = self._generate_code()

    def _get_board_size(self) -> int:
        if self.num_players == 2:
            return 9
        elif self.num_players == 3:
            return 11
        elif self.num_players == 4:
            return 13
        else:
            raise ValueError("Unsupported number of players")

    def _generate_code(self) -> str:
        # return a random 4-digit code for the game
        return f"{random.randint(1000, 9999)}"

    def join(self, name: str, color: str) -> bool:
        # if the game is already started, we can't join
        if self.started:
            return False

        # create a new Player(name, color) and add it to self.board.players
        player = Player(name, color)

        try:
            self.board.add_player(player)
        except Exception as e:
            print(f"Error adding player: {e}")
            return False

        # if all expected players have joined, mark the game as started
        if len(self.board.players) == self.num_players:
            self.started = True

        return True

    def play_move(self, r: int, c: int) -> bool:
        # check that the game is ongoing before allowing a move
        if not self.started or self.finished:
            return False

        # r and c are passed as separate arguments, not as a tuple
        result = self.board.make_move(r, c)

        if result:
            # after each valid move, check if someone has won
            winner_index = self.board.check_winner()
            if winner_index is not None:
                self.winner = winner_index
                self.finished = True

        return result

    def play_wall(self, r: int, c: int, vertical: bool) -> bool:
        # check that started=True and finished=False
        # create a Wall object and call board.place_wall(wall)
        if self.started and not self.finished:
            wall = Wall((r, c), vertical)
            return self.board.add_wall(wall)
        return False

    def get_state(self) -> dict:
        # return a dict representing the current state of the game, including
        # players' positions, walls, and any other relevant information for the frontend to render the game
        return {
            "game_code": self.game_code,
            "started": self.started,
            "finished": self.finished,
            "winner": self.winner,
            "current_turn": self.board.playerturn,  # index of the player whose turn it is
            "players": [
                {
                    "name": player.name,
                    "color": player.color,
                    "position": player.position,
                    "walls_left": player.walls   # needed by the frontend to display remaining walls
                } for player in self.board.players
            ],
            "walls": [
                {
                    "position": wall.position,
                    "vertical": wall.vertical
                } for wall in self.board.walls
            ],
        }