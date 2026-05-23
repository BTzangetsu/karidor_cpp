from collections import deque


class Player:
    """Internal representation of a player.
    Stores their name, color, number of remaining walls, and position on the board.
    Positions are 1-indexed: (1,1) is the top-left corner."""

    def __init__(self, name: str, color: str, walls: int = 0):
        self.name = name
        self.color = color
        self.walls = walls
        self.position = (0, 0)  # will be initialized by add_player()

    def remove_wall(self) -> bool:
        """Removes a wall from the player's stock if they have any left.
        Returns True if the wall was successfully removed, False otherwise."""
        if self.walls > 0:
            self.walls -= 1
            return True
        return False

    def set_walls(self, walls: int) -> None:
        """Initializes the number of walls for the player at the start of the game."""
        self.walls = walls

    def set_position(self, position: tuple) -> None:
        """Updates the player's position on the board (1-indexed)."""
        self.position = position


class Wall:
    """Internal representation of a wall.
    
    Placement convention (1-indexed):
    - VERTICAL wall   Wall(r, c): placed in the groove to the left of column c.
                                  Blocks passage between col (c-1) and col c,
                                  across rows r AND r+1.
                                  Valid if 1 ≤ r ≤ n-1 and 1 ≤ c ≤ n+1.
                                  c=1 = left edge, c=n+1 = right edge.

    - HORIZONTAL wall Wall(r, c): placed in the groove above row r.
                                  Blocks passage between row (r-1) and row r,
                                  across columns c AND c+1.
                                  Valid if 1 ≤ r ≤ n+1 and 1 ≤ c ≤ n-1.
                                  r=1 = top edge, r=n+1 = bottom edge.
    
    The position refers to the top-left corner of the wall."""

    def __init__(self, position: tuple = (0, 0), vertical: bool = True):
        self.position = position   # (row, col) 1-indexed, top-left corner
        self.vertical = vertical   # True = vertical, False = horizontal


class Board:
    """WALLI game board.
    
    Supported sizes:
        9  cells → 2 players, 10 walls each
        11 cells → 3 players,  8 walls each
        13 cells → 4 players,  6 walls each
    
    All positions are 1-indexed.
    The grid board[r][c] is True if cell (r, c) is occupied by a player.
    Rows go from top (1) to bottom (n), columns from left (1) to right (n).
    """

    # Number of walls per player based on board size
    WALLS_PER_PLAYER = {9: 10, 11: 8, 13: 6}

    def __init__(self, size: int):
        if size not in self.WALLS_PER_PLAYER:
            raise ValueError(f"Invalid size: {size}. Accepted values: 9, 11, 13.")
        self.size = size
        # board[r][c]: True if cell (r, c) is occupied — indices 1..size, 0 ignored
        self.board = [[False] * (size + 1) for _ in range(size + 1)]
        self.players: list[Player] = []
        self.walls: list[Wall] = []
        self.playerturn = 0   # index in self.players of the player whose turn it is

    # -------------------------------------------------------------------------
    # Player Management
    # -------------------------------------------------------------------------

    def add_player(self, player: Player) -> None:
        """Adds a player and places them on their starting edge (middle of the edge).
        
        Initial positions (middle of each edge):
            Player 0 → top edge    : (1,      mid)
            Player 1 → bottom edge : (n,      mid)
            Player 2 → left edge   : (mid,    1  )
            Player 3 → right edge  : (mid,    n  )
        
        The middle cell is chosen using (size + 1) // 2
        to be exactly at the center regardless of parity."""

        n = len(self.players)  # index of the player currently being added
        if n >= 4:
            raise Exception("Too many players (maximum 4).")

        mid = (self.size + 1) // 2  # center of the board (1-indexed)

        positions = [
            (1,          mid),        # player 0: top edge
            (self.size,  mid),        # player 1: bottom edge
            (mid,        1),          # player 2: left edge
            (mid,        self.size),  # player 3: right edge
        ]

        pos = positions[n]
        player.set_walls(self.WALLS_PER_PLAYER[self.size])
        player.set_position(pos)
        self.board[pos[0]][pos[1]] = True
        self.players.append(player)

    def get_size(self) -> int:
        """Returns the size of the board."""
        return self.size

    def get_current_player(self) -> Player:
        """Returns the player whose turn it is."""
        return self.players[self.playerturn]

    # -------------------------------------------------------------------------
    # Wall Verification — helper for make_move()
    # -------------------------------------------------------------------------

    def _is_blocked_by_wall(self, from_pos: tuple, to_pos: tuple) -> bool:
        """Checks if the path from from_pos to to_pos is blocked by a wall.
        
        For a VERTICAL move (same column, row changes):
            We look for a HORIZONTAL wall whose groove lies exactly
            between the two rows. A horizontal wall Wall(r, c) blocks the passage
            between row (r-1) and row r across columns c and c+1.
            → blocked if wall.row == to_row  (when moving down)
                   or wall.row == from_row (when moving up, to_row = from_row - 1)
            AND the player's column is either c or c+1.
        
        For a HORIZONTAL move (same row, column changes):
            We look for a VERTICAL wall. A vertical wall Wall(r, c) blocks the passage
            between col (c-1) and col c across rows r and r+1.
            → blocked if wall.col == to_col  (when moving right)
                   or wall.col == from_col (when moving left, to_col = from_col - 1)
            AND the player's row is either r or r+1."""

        from_r, from_c = from_pos
        to_r,   to_c   = to_pos

        for wall in self.walls:
            wr, wc = wall.position

            if not wall.vertical:
                # Horizontal wall: blocks vertical movement (same column)
                if from_c != to_c:
                    continue  # horizontal movement, not affected

                # The groove is above row wr.
                # It blocks between row (wr-1) and row wr,
                # across columns wc and wc+1.
                if from_c not in (wc, wc + 1):
                    continue  # player is not within the covered columns

                # Check if the groove is exactly between from_r and to_r
                top_row = min(from_r, to_r)
                if wr == top_row + 1:
                    return True

            else:
                # Vertical wall: blocks horizontal movement (same row)
                if from_r != to_r:
                    continue  # vertical movement, not affected

                # The groove is to the left of column wc.
                # It blocks between col (wc-1) and col wc,
                # across rows wr and wr+1.
                if from_r not in (wr, wr + 1):
                    continue  # player is not within the covered rows

                # Check if the groove is exactly between from_c and to_c
                left_col = min(from_c, to_c)
                if wc == left_col + 1:
                    return True

        return False

    # -------------------------------------------------------------------------
    # Movement
    # -------------------------------------------------------------------------

    def make_move(self, r: int, c: int) -> bool:
        """Moves the current player to cell (r, c).
        
        Valid if:
            1. Destination is within board boundaries (1..n)
            2. Destination is free (no other player on it)
            3. Movement is strictly orthogonal (no diagonals)
               and exactly one cell away
            4. No wall blocks the path
        
        If the move is valid, frees the old cell, occupies the new one,
        updates the player's position, and passes the turn to the next player.
        Returns True if the move is valid and executed, False otherwise."""

        # 1. Boundary check
        if not (1 <= r <= self.size and 1 <= c <= self.size):
            return False

        # 2. Is destination cell occupied?
        if self.board[r][c]:
            return False

        current_pos = self.players[self.playerturn].position
        curr_r, curr_c = current_pos

        # 3. Strictly orthogonal movement of exactly one cell
        dr = abs(r - curr_r)
        dc = abs(c - curr_c)
        if not ((dr == 1 and dc == 0) or (dr == 0 and dc == 1)):
            return False

        # 4. Path blocked by a wall?
        if self._is_blocked_by_wall(current_pos, (r, c)):
            return False

        # Valid move: free old cell, occupy new one
        self.board[curr_r][curr_c] = False
        self.board[r][c] = True
        self.players[self.playerturn].set_position((r, c))

        # Pass turn to the next player
        self.playerturn = (self.playerturn + 1) % len(self.players)
        return True

    # -------------------------------------------------------------------------
    # BFS — checks that a player can still reach their winning edge
    # -------------------------------------------------------------------------

    def _goal_rows_cols(self, player_index: int) -> dict:
        """Returns the winning condition for player i.
        
        Each player must reach the edge OPPOSITE to their starting side:
            Player 0 (starts at top)    → must reach row n
            Player 1 (starts at bottom) → must reach row 1
            Player 2 (starts at left)   → must reach column n
            Player 3 (starts at right)  → must reach column 1
        
        Returns a dict with keys 'axis' ('row' or 'col')
        and 'target' (the target row/column value)."""

        targets = [
            {"axis": "row", "target": self.size},  # player 0: bottom edge
            {"axis": "row", "target": 1},          # player 1: top edge
            {"axis": "col", "target": self.size},  # player 2: right edge
            {"axis": "col", "target": 1},          # player 3: left edge
        ]
        return targets[player_index]

    def _player_can_reach_goal(self, player_index: int) -> bool:
        """BFS from the current position of player player_index.
        Returns True if the player can still reach their winning edge,
        taking into account all walls currently placed.
        
        Other players are ignored (their cells are considered free)
        since they can move — we only test blocking by walls."""

        start = self.players[player_index].position
        goal = self._goal_rows_cols(player_index)
        visited = set()
        queue = deque([start])
        visited.add(start)

        while queue:
            r, c = queue.popleft()

            # Winning condition reached?
            if goal["axis"] == "row" and r == goal["target"]:
                return True
            if goal["axis"] == "col" and c == goal["target"]:
                return True

            # Explore 4 orthogonal neighbors
            for nr, nc in [(r - 1, c), (r + 1, c), (r, c - 1), (r, c + 1)]:
                if not (1 <= nr <= self.size and 1 <= nc <= self.size):
                    continue
                if (nr, nc) in visited:
                    continue
                # Only check walls, not players
                if self._is_blocked_by_wall((r, c), (nr, nc)):
                    continue
                visited.add((nr, nc))
                queue.append((nr, nc))

        return False  # no path found

    # -------------------------------------------------------------------------
    # Wall Placement
    # -------------------------------------------------------------------------

    def add_wall(self, wall: Wall) -> bool:
        """Tries to place a wall on the board.
        
        Validations performed in order:
            1. Current player still has walls to place
            2. Wall is within board boundaries (depending on orientation)
            3. No identical wall already exists at this location
            4. Wall does not overlap an existing wall (same groove partially)
            5. Wall does not completely block any player's path (BFS)
        
        If all is valid, the wall is placed and the player loses a wall from their stock.
        Passes the turn to the next player.
        Returns True if the wall was placed, False otherwise."""

        wr, wc = wall.position

        # 1. Does the current player have remaining walls?
        if self.players[self.playerturn].walls <= 0:
            return False

        # 2. Boundary check based on orientation
        if wall.vertical:
            # Vertical wall: covers rows wr and wr+1 → wr from 1 to n-1
            # The groove runs along column wc to its left → wc from 1 to n+1
            # wc=1 = left edge of the board, wc=n+1 = right edge
            if not (1 <= wr <= self.size - 1 and 1 <= wc <= self.size + 1):
                return False
        else:
            # Horizontal wall: covers columns wc and wc+1 → wc from 1 to n-1
            # The groove runs along row wr above it → wr from 1 to n+1
            # wr=1 = top edge of the board, wr=n+1 = bottom edge
            if not (1 <= wr <= self.size + 1 and 1 <= wc <= self.size - 1):
                return False

        # 3. Exact conflict: an identical wall (same position, same orientation) already exists
        for w in self.walls:
            if w.position == wall.position and w.vertical == wall.vertical:
                return False

        # 4. Partial overlap: a wall of the same orientation occupies the adjacent cell
        #    A wall covers two consecutive cells, so we check if the next cell
        #    (wr+1, wc) for vertical or (wr, wc+1) for horizontal is already taken.
        for w in self.walls:
            if w.vertical == wall.vertical:
                if wall.vertical:
                    # The two rows covered are wr and wr+1.
                    # Conflict if an existing wall covers wr+1 (position (wr+1, wc))
                    # or if that wall would cover wr from (wr-1, wc)
                    if w.position[1] == wc and abs(w.position[0] - wr) == 1:
                        return False
                else:
                    # The two columns covered are wc and wc+1.
                    if w.position[0] == wr and abs(w.position[1] - wc) == 1:
                        return False

        # 5. BFS — the wall must not trap any player
        #    Temporarily place the wall to test paths
        self.walls.append(wall)
        for i in range(len(self.players)):
            if not self._player_can_reach_goal(i):
                self.walls.pop()  # remove the temporary wall
                return False

        # Everything is valid: wall remains, decrement stock of current player
        self.players[self.playerturn].remove_wall()
        self.playerturn = (self.playerturn + 1) % len(self.players)
        return True

    # -------------------------------------------------------------------------
    # Win Detection
    # -------------------------------------------------------------------------

    def check_winner(self) -> int | None:
        """Checks if a player has reached their winning edge.
        Returns the index of the winning player, or None if the game continues."""

        for i, player in enumerate(self.players):
            goal = self._goal_rows_cols(i)
            r, c = player.position
            if goal["axis"] == "row" and r == goal["target"]:
                return i
            if goal["axis"] == "col" and c == goal["target"]:
                return i
        return None