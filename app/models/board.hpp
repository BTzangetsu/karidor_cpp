#pragma once
#include <unordered_map>
#include <set>
#include <queue>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>

// Number of walls per player based on board size
std::unordered_map<size_t, size_t> WALLS_PER_PLAYER = {
    {9,  10},  // 2 players
    {11,  8},  // 3 players
    {13,  6}   // 4 players
};

/* Internal representation of a player.
   Stores their name, color, number of remaining walls, and position on the board.
   Positions are 1-indexed: (1,1) is the top-left corner. */
class Player {
public:
    std::string name;
    std::string color;
    size_t walls;
    std::pair<int, int> position;

    Player(const std::string& name,
           const std::string& color,
           size_t walls,
           std::pair<int, int> position = {0, 0})
        : name(name), color(color), walls(walls), position(position)
    {}

    ~Player() = default;

    /* Removes a wall from the player's stock if they have any left.
       Returns true if the wall was successfully removed, false otherwise. */
    bool remove_wall() {
        if (walls > 0) {
            --walls;
            return true;
        }
        return false;
    }

    /* Initializes the number of walls for the player at the start of the game. */
    void set_walls(size_t newWalls) {
        walls = newWalls;
    }

    /* Updates the player's position on the board (1-indexed). */
    void set_position(std::pair<int, int> newPosition) {
        position = newPosition;
    }
};


/* Internal representation of a wall.

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

   The position refers to the top-left corner of the wall. */
class Wall {
public:
    std::pair<int, int> position;
    bool vertical; // true = vertical, false = horizontal

    Wall(std::pair<int, int> position, bool vertical = true)
        : position(position), vertical(vertical)
    {}

    ~Wall() = default;
};


/* Karidor game board.

   Supported sizes:
       9  cells → 2 players, 10 walls each
       11 cells → 3 players,  8 walls each
       13 cells → 4 players,  6 walls each

   All positions are 1-indexed.
   The grid board[r][c] is true if cell (r+1, c+1) is occupied by a player
   (internal storage is 0-indexed, public API is 1-indexed).
   Rows go from top (1) to bottom (n), columns from left (1) to right (n). */
class Board {
public:
    size_t size;
    std::vector<Player*> players;
    std::vector<Wall*> walls;
    size_t player_turn;
    std::vector<std::vector<bool>> board; // 0-indexed internally

    Board(size_t size) : size(size), player_turn(0) {
        if (WALLS_PER_PLAYER.find(size) == WALLS_PER_PLAYER.end()) {
            throw std::invalid_argument("Unsupported board size. Supported sizes are 9, 11, and 13.");
        }
        // initialize the board with false (no players), 0-indexed
        board.assign(size, std::vector<bool>(size, false));
    }

    ~Board() {
        for (Player* player : players) delete player;
        players.clear();
        for (Wall* wall : walls) delete wall;
        walls.clear();
    }

    /* Adds a player and places them on their starting edge (middle of the edge).

       Initial positions (middle of each edge):
           Player 0 → top edge    : (1,    mid)
           Player 1 → bottom edge : (n,    mid)
           Player 2 → left edge   : (mid,  1  )
           Player 3 → right edge  : (mid,  n  )

       The middle cell is chosen using (size + 1) / 2
       to be exactly at the center regardless of parity. */
    void add_player(Player* player) {
        size_t num_players = players.size();
        if (num_players >= 4) {
            throw std::runtime_error("Maximum number of players (4) already added.");
        }

        int mid = (size + 1) / 2; // center of the board (1-indexed)

        std::vector<std::pair<int, int>> positions = {
            {1,                    mid},  // Player 0: top edge
            {(int)size,            mid},  // Player 1: bottom edge
            {mid,                  1  },  // Player 2: left edge
            {mid,      (int)size      },  // Player 3: right edge
        };

        auto pos = positions[num_players];
        player->set_position(pos);
        player->set_walls(WALLS_PER_PLAYER[size]);

        // board is 0-indexed internally: (r, c) 1-indexed → board[r-1][c-1]
        board[pos.first - 1][pos.second - 1] = true;
        players.push_back(player);
    }

    size_t get_size() const { return size; }

    Player* get_current_player() const { return players[player_turn]; }

    /* Checks if the path from from_pos to to_pos is blocked by a wall.

       For a VERTICAL move (same column, row changes):
           Looks for a HORIZONTAL wall whose groove lies exactly between the two rows.
           A horizontal wall Wall(r, c) blocks passage between row (r-1) and row r
           across columns c AND c+1.

       For a HORIZONTAL move (same row, column changes):
           Looks for a VERTICAL wall. A vertical wall Wall(r, c) blocks passage
           between col (c-1) and col c across rows r AND r+1. */
    bool _is_blocked_by_wall(const std::pair<int, int>& from_pos,
                              const std::pair<int, int>& to_pos) {
        int from_r = from_pos.first;
        int from_c = from_pos.second;
        int to_r   = to_pos.first;
        int to_c   = to_pos.second;

        for (const Wall* wall : walls) {
            int wr = wall->position.first;
            int wc = wall->position.second;

            if (!wall->vertical) {
                // Horizontal wall: blocks vertical movement (same column)
                if (from_c != to_c) continue;

                // player's column must be wc or wc+1 (the two columns the wall covers)
                if (from_c < wc || from_c > wc + 1) continue;

                // the groove must be exactly between from_r and to_r
                int top_row = (from_r <= to_r) ? from_r : to_r;
                if (wr == top_row + 1) return true;
            } else {
                // Vertical wall: blocks horizontal movement (same row)
                if (from_r != to_r) continue;

                // player's row must be wr or wr+1 (the two rows the wall covers)
                if (from_r < wr || from_r > wr + 1) continue;

                // the groove must be exactly between from_c and to_c
                int left_col = (from_c <= to_c) ? from_c : to_c;
                if (wc == left_col + 1) return true;
            }
        }
        return false;
    }

    /* Moves the current player to cell (r, c).

       Valid if:
           1. Destination is within board boundaries (1..n)
           2. Destination is free (no other player on it)
           3. Movement is strictly orthogonal and exactly one cell away
           4. No wall blocks the path

       If the move is valid, frees the old cell, occupies the new one,
       updates the player's position, and passes the turn to the next player.
       Returns true if the move is valid and executed, false otherwise. */
    bool make_move(int r, int c) {
        // 1. Boundary check (1-indexed)
        if (!(1 <= r && r <= (int)size && 1 <= c && c <= (int)size)) {
            return false;
        }

        // 2. Destination occupied? (convert to 0-indexed for board access)
        if (board[r - 1][c - 1]) {
            return false;
        }

        int curr_r = get_current_player()->position.first;
        int curr_c = get_current_player()->position.second;

        int dr = std::abs(curr_r - r);
        int dc = std::abs(curr_c - c);

        // 3. Strict orthogonal move: exactly one step in one direction
        // Bug fix: the parentheses were wrong in the original — ! must wrap the whole expression
        if (!((dr == 1 && dc == 0) || (dr == 0 && dc == 1))) {
            return false;
        }

        // 4. Wall blocking check
        if (_is_blocked_by_wall({curr_r, curr_c}, {r, c})) {
            return false;
        }

        // Valid move: free old cell, occupy new one (0-indexed access)
        board[curr_r - 1][curr_c - 1] = false;
        board[r - 1][c - 1]           = true;
        players[player_turn]->set_position({r, c});

        // Pass turn to the next player
        player_turn = (player_turn + 1) % players.size();
        return true;
    }

    /* Returns the winning condition for player i.

       Each player must reach the edge OPPOSITE to their starting side:
           Player 0 (starts at top)    → must reach row n
           Player 1 (starts at bottom) → must reach row 1
           Player 2 (starts at left)   → must reach column n
           Player 3 (starts at right)  → must reach column 1 */
    struct GoalTarget {
        std::string axis;
        int target;
        GoalTarget(std::string a, int t) : axis(a), target(t) {}
    };

    GoalTarget get_goal_rows_cols(int player_index) const {
        const std::vector<GoalTarget> targets = {
            {"row", (int)size},  // Player 0: reach bottom row
            {"row", 1         },  // Player 1: reach top row
            {"col", (int)size},  // Player 2: reach right column
            {"col", 1         },  // Player 3: reach left column
        };
        if (player_index < 0 || player_index >= 4) {
            throw std::out_of_range("Player index must be between 0 and 3.");
        }
        return targets[player_index];
    }

    /* BFS from the player's current position.
       Returns true if the player can still reach their winning edge,
       taking all currently placed walls into account.
       Other players' positions are ignored (they can move). */
    bool player_can_reach_goal(int player_index) {
        std::pair<int, int> start = players[player_index]->position;
        GoalTarget goal           = get_goal_rows_cols(player_index);

        std::set<std::pair<int, int>>   visited;
        std::queue<std::pair<int, int>> bfs_queue;

        bfs_queue.push(start);
        visited.insert(start);

        while (!bfs_queue.empty()) {
            // front() reads, pop() removes — unlike Python's queue.popleft()
            std::pair<int, int> current = bfs_queue.front();
            bfs_queue.pop();

            int r = current.first;
            int c = current.second;

            // Winning condition reached?
            if (goal.axis == "row" && r == goal.target) return true;
            if (goal.axis == "col" && c == goal.target) return true;

            // Explore 4 orthogonal neighbors
            std::pair<int, int> neighbors[4] = {
                {r - 1, c},
                {r + 1, c},
                {r, c - 1},
                {r, c + 1},
            };

            for (const auto& neighbor : neighbors) {
                int nr = neighbor.first;
                int nc = neighbor.second;

                if (!(1 <= nr && nr <= (int)size && 1 <= nc && nc <= (int)size)) continue;
                if (visited.count(neighbor) > 0) continue;
                if (_is_blocked_by_wall(current, neighbor)) continue;

                visited.insert(neighbor);
                bfs_queue.push(neighbor);
            }
        }
        return false;
    }

    /* Tries to place a wall on the board.

       Validations in order:
           1. Current player still has walls to place
           2. Wall is within board boundaries (depending on orientation)
           3. No identical wall already exists at this location
           4. Wall does not partially overlap an existing wall of the same orientation
           5. Wall does not completely block any player's path to their goal (BFS)

       If all validations pass, the wall is placed, the player loses one wall,
       and the turn passes to the next player.
       Returns true if placed, false otherwise. */
    bool add_wall(Wall* wall) {
        int wr = wall->position.first;
        int wc = wall->position.second;

        // 1. Current player must have walls remaining
        // Bug fix: original had !players[...]->walls > 0 which is always false
        if (players[player_turn]->walls == 0) {
            return false;
        }

        // 2. Boundary check based on orientation
        if (wall->vertical) {
            // Vertical: covers rows wr and wr+1 → wr in [1, n-1], wc in [1, n+1]
            if (!(1 <= wr && wr <= (int)size - 1 && 1 <= wc && wc <= (int)size + 1)) {
                return false;
            }
        } else {
            // Horizontal: covers cols wc and wc+1 → wr in [1, n+1], wc in [1, n-1]
            if (!(1 <= wr && wr <= (int)size + 1 && 1 <= wc && wc <= (int)size - 1)) {
                return false;
            }
        }

        // 3. Exact conflict: identical wall already placed here
        for (Wall* in_wall : walls) {
            if (in_wall->position == wall->position && in_wall->vertical == wall->vertical) {
                return false;
            }
        }

        // 4. Partial overlap: same orientation, adjacent position shares one of the two cells
        for (Wall* in_wall : walls) {
            if (in_wall->vertical == wall->vertical) {
                if (wall->vertical) {
                    // Both walls are vertical: conflict if same column and rows are adjacent
                    if (in_wall->position.second == wc &&
                        std::abs(in_wall->position.first - wr) == 1) {
                        return false;
                    }
                } else {
                    // Both walls are horizontal: conflict if same row and columns are adjacent
                    if (in_wall->position.first == wr &&
                        std::abs(in_wall->position.second - wc) == 1) {
                        return false;
                    }
                }
            }
        }

        // 5. BFS — wall must not trap any player
        // Temporarily push the wall, test all players, pop it back if invalid
        walls.push_back(wall);
        for (int i = 0; i < (int)players.size(); i++) {
            if (!player_can_reach_goal(i)) {
                walls.pop_back(); // remove temporary wall — do NOT delete, caller owns it
                return false;
            }
        }

        // All validations passed: wall stays, decrement stock, advance turn
        players[player_turn]->remove_wall();
        player_turn = (player_turn + 1) % players.size();
        return true;
    }

    /* Win detection.
       Checks if any player has reached their winning edge.
       Returns the index of the winning player, or -1 if the game continues. */
    int check_winner() {
        for (int i = 0; i < (int)players.size(); i++) {
            int r = players[i]->position.first;
            int c = players[i]->position.second;
            GoalTarget goal = get_goal_rows_cols(i);

            if (goal.axis == "row" && goal.target == r) return i;
            if (goal.axis == "col" && goal.target == c) return i;
        }
        return -1;
    }
};