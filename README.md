# Karidor

A real-time multiplayer board game for 2, 3, or 4 players.  
Move your pawn across the board while blocking your opponents with walls.  
First to reach the opposite side wins.

> This is the **C++ version** — backend built with [Crow](https://crowcpp.org) and native WebSockets.  
> See the `main` branch for the Python/Flask version.

---

## Deploy with Docker (recommended)

**Requirements:** Docker + Docker Compose installed on your server.

The build uses a **multi-stage Dockerfile**: the first stage compiles a fully static binary using GCC and CMake (~500MB, discarded after build), the second stage copies only the binary into a minimal `scratch` image (~15MB final size).

```bash
# 1. Clone the repository 
git clone https://github.com/BTzangetsu/karidor_cpp.git
cd karidor

# 2. Build and start
docker compose up -d

# 3. Open your browser
http://localhost:5000
```

To stop:
```bash
docker compose down
```

To rebuild after a code change:
```bash
docker compose up -d --build

# Clean up the builder cache left behind after the build
docker image prune -f
```

Check disk usage:
```bash
docker system df
```

---

## Run locally without Docker

**Requirements:** GCC 12+, CMake 3.16+, OpenSSL, Make

```bash
# 1. Configure and compile
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 2. Start the server
./build/karidor

# 3. Open your browser
http://localhost:5000
```

---

## Configuration

No environment variables needed — the C++ version has no external runtime dependencies.  
To change the port, edit `app/main.cpp`:

```cpp
app.port(5000).multithreaded().run();
```

---

## Game rules

- **2 players** → 9×9 board, 10 walls each
- **3 players** → 11×11 board, 8 walls each
- **4 players** → 13×13 board, 6 walls each

On each turn a player can either **move** their pawn one square  
or **place a wall** (if they have any left).  
A wall spans two cells and cannot completely block any player's path.  
The first player to reach the opposite side of the board wins.

---

## Project structure

```
karidor/
├── app/
│   ├── main.cpp           # Entry point — Crow app, HTTP routes, WebSocket dispatcher
│   ├── crow.h             # Crow single-header
│   ├── asio.hpp           # Asio single-header (async I/O)
│   ├── crow/              # Crow internal headers
│   ├── asio/              # Asio internal headers
│   ├── models/
│   │   ├── board.hpp      # Board, Player, Wall — pure game logic + BFS
│   │   └── game.hpp       # Game session manager + get_state() JSON serialization
│   └── sockets/
│       ├── events.hpp     # WebSocket event handlers (create_room, play_move, …)
│       └── rooms.hpp      # RoomManager — singleton registry of all active games
├── static/
│   ├── css/game.css       # All styles (dark mode default, light toggle)
│   └── js/
│       ├── socket.js      # Native WebSocket client — mimics Socket.io API
│       ├── main.js        # Lobby logic (create / join)
│       └── game.js        # Board rendering + in-game interactions
├── templates/
│   ├── index.html         # Lobby page
│   └── game.html          # Game page
├── CMakeLists.txt
├── Dockerfile             # Multi-stage: builder (GCC+CMake) → scratch (binary only)
└── docker-compose.yml
```
