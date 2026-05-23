// game.js — Karidor in-game UI
// Handles: board rendering, player moves, wall placement,
//          socket sync, win screen, theme toggle.
// Runs after socket.js which exposes the global `socket`.

// ── Constants ────────────────────────────────────────────────────────────────

const CELL_SIZE  = 44;   // px — size of one board cell
const WALL_GAP   = 6;    // px — thickness of the gap between cells (wall slot)

// Goal sides per player index — used to draw the colored border on arrival cells
const GOAL_SIDES = ['bottom', 'top', 'right', 'left'];

// Action mode labels and colors matching game.css variables
const MODES = [
    { id: 'move',   label: '<i class="ti ti-arrow-move"   aria-hidden="true"></i> Bouger',  color: 'var(--move-color)',  shadow: 'var(--move-shadow)'  },
    { id: 'wall-h', label: '<i class="ti ti-minus"        aria-hidden="true"></i> Mur H',   color: 'var(--wallh-color)', shadow: 'var(--wallh-shadow)' },
    { id: 'wall-v', label: '<i class="ti ti-line-height"  aria-hidden="true"></i> Mur V',   color: 'var(--wallv-color)', shadow: 'var(--wallv-shadow)' },
];

// ── State ────────────────────────────────────────────────────────────────────

// Recovered from sessionStorage (set by main.js before redirect)
let gameState = JSON.parse(sessionStorage.getItem('gameState') || 'null');
let isCreator = JSON.parse(sessionStorage.getItem('isCreator') || 'false');
let myColor   = sessionStorage.getItem('myColor') || null;

// Index of this client's player in state.players — determined by color match
let myIndex = -1;

// Current UI mode: 'move' | 'wall-h' | 'wall-v'
let mode = 'move';

// ── Theme toggle ─────────────────────────────────────────────────────────────

/**
 * Toggles between dark (default) and light mode.
 * Updates the button label and icon accordingly.
 */
function toggleTheme() {
    document.body.classList.toggle('light');
    const isLight = document.body.classList.contains('light');
    document.getElementById('theme-btn').innerHTML = isLight
        ? '<i class="ti ti-moon" aria-hidden="true"></i><span id="theme-label">Dark</span>'
        : '<i class="ti ti-sun"  aria-hidden="true"></i><span id="theme-label">Light</span>';
}

// ── Action mode selector ──────────────────────────────────────────────────────

/**
 * Sets the current action mode and re-renders the action row
 * so the selected button is highlighted.
 * @param {string} m - one of 'move', 'wall-h', 'wall-v'
 */
function setMode(m) {
    mode = m;
    renderActionRow();
}

// ── Board rendering ───────────────────────────────────────────────────────────

/**
 * Full board render from the current gameState.
 * Builds a CSS grid with alternating cell columns and wall-gap columns.
 * Rows also alternate: cell rows and wall-gap rows.
 *
 * Grid layout for an N×N board:
 *   columns: cell | gap | cell | gap | … | cell   → 2N-1 columns
 *   rows:    cell | gap | cell | gap | … | cell   → 2N-1 rows
 *
 * Each gap element contains a .wpiece div that shows a wall if placed,
 * or a hover preview if the current mode allows wall placement there.
 */
function renderBoard() {
    if (!gameState) return;

    const N     = gameState.board_size;
    const board = document.getElementById('board');
    const cols  = N + (N - 1);  // cell cols + gap cols

    // Set CSS grid template
    board.style.gridTemplateColumns = Array.from({ length: cols }, (_, i) =>
        i % 2 === 0 ? `${CELL_SIZE}px` : `${WALL_GAP}px`
    ).join(' ');

    // Build a fast lookup for placed walls
    const hWallSet = new Set();
    const vWallSet = new Set();
    gameState.walls.forEach(w => {
        const key = `${w.position[0]},${w.position[1]}`;
        if (w.vertical) vWallSet.add(key);
        else            hWallSet.add(key);
    });

    // Build a lookup for player positions: "r,c" → player object
    const playerAt = {};
    gameState.players.forEach(p => {
        playerAt[`${p.position[0]},${p.position[1]}`] = p;
    });

    let html = '';

    for (let r = 1; r <= N; r++) {
        // ── Cell row ──────────────────────────────────────────────────────────
        for (let c = 1; c <= N; c++) {

            // Goal border: cells on the arrival edge of a player get a colored border
            const goalBorder = getGoalBorder(r, c, N, gameState.players);

            // Is this cell reachable by the current player this turn?
            const canMove = isMyTurn() && mode === 'move' && isMoveTarget(r, c);

            // Player pawn on this cell?
            const p = playerAt[`${r},${c}`];
            const pawnHtml = p
                ? `<div class="player" style="
                        width:${Math.round(CELL_SIZE * 0.58)}px;
                        height:${Math.round(CELL_SIZE * 0.58)}px;
                        background:${p.color};
                        box-shadow:0 3px 0 ${darken(p.color)};">
                        ${p.name[0].toUpperCase()}
                   </div>`
                : '';

            html += `<div class="cell${canMove ? ' can-move' : ''}"
                          style="width:${CELL_SIZE}px;height:${CELL_SIZE}px;${goalBorder}"
                          onclick="handleCellClick(${r},${c})">
                       ${pawnHtml}
                     </div>`;

            // ── Vertical wall gap (between columns) ───────────────────────────
            if (c < N) {
                // A vertical wall at position (r, c+1) blocks passage between col c and col c+1
                // on row r — we check both (r, c+1) and (r-1, c+1) because one wall covers 2 rows
                const placed = vWallSet.has(`${r},${c+1}`) || vWallSet.has(`${r-1},${c+1}`);
                const canWall = isMyTurn() && mode === 'wall-v' && hasWallsLeft();

                html += `<div class="gap-v${canWall ? ' can-place' : ''}"
                               style="width:${WALL_GAP}px;height:${CELL_SIZE}px;"
                               onclick="handleVWallClick(${r},${c+1})">
                           <div class="wpiece ${placed ? 'placed' : (canWall ? 'preview' : '')}"
                                style="width:${WALL_GAP - 1}px;height:${CELL_SIZE}px;"></div>
                         </div>`;
            }
        }

        // ── Horizontal gap row (between rows) ─────────────────────────────────
        if (r < N) {
            for (let c = 1; c <= N; c++) {
                // A horizontal wall at (r+1, c) blocks passage between row r and row r+1
                // on column c — covers both (r+1, c) and (r+1, c-1)
                const placed = hWallSet.has(`${r+1},${c}`) || hWallSet.has(`${r+1},${c-1}`);
                const canWall = isMyTurn() && mode === 'wall-h' && hasWallsLeft();

                html += `<div class="gap-h${canWall ? ' can-place' : ''}"
                               style="width:${CELL_SIZE}px;height:${WALL_GAP}px;"
                               onclick="handleHWallClick(${r+1},${c})">
                           <div class="wpiece ${placed ? 'placed' : (canWall ? 'preview' : '')}"
                                style="height:${WALL_GAP - 1}px;width:${CELL_SIZE}px;"></div>
                         </div>`;

                // Corner intersection square between two gaps
                if (c < N) {
                    html += `<div class="gap-corner"
                                  style="width:${WALL_GAP}px;height:${WALL_GAP}px;"></div>`;
                }
            }
        }
    }

    board.innerHTML = html;
}

// ── Goal border helper ────────────────────────────────────────────────────────

/**
 * Returns an inline CSS border string for a cell that sits on a player's goal edge.
 * Each player must reach the opposite side from where they started:
 *   player 0 (top)   → goal = bottom row  → border-bottom
 *   player 1 (bottom)→ goal = top row     → border-top
 *   player 2 (left)  → goal = right col   → border-right
 *   player 3 (right) → goal = left col    → border-left
 *
 * @param {number} r        - row (1-indexed)
 * @param {number} c        - col (1-indexed)
 * @param {number} N        - board size
 * @param {Array}  players  - array of player objects from gameState
 * @returns {string} inline style string, e.g. "border-bottom:3px solid #D85A30;"
 */
function getGoalBorder(r, c, N, players) {
    const sides = ['bottom', 'top', 'right', 'left'];
    const conditions = [
        () => r === N,   // player 0 goal: last row
        () => r === 1,   // player 1 goal: first row
        () => c === N,   // player 2 goal: last col
        () => c === 1,   // player 3 goal: first col
    ];

    for (let i = 0; i < players.length; i++) {
        if (conditions[i] && conditions[i]()) {
            return `border-${sides[i]}: 3px solid ${players[i].color};`;
        }
    }
    return '';
}

// ── Players bar rendering ─────────────────────────────────────────────────────

/**
 * Renders one card per player into #players-bar.
 * The active player's card is highlighted with their color and lifts up.
 */
function renderPlayersBar() {
    if (!gameState) return;

    document.getElementById('players-bar').innerHTML = gameState.players.map((p, i) => {
        const isCurrent = i === gameState.current_turn;
        const cardStyle = isCurrent
            ? `border-color:${p.color};background:${p.color}22;`
            : '';
        return `<div class="pcard${isCurrent ? ' active' : ''}" style="${cardStyle}">
            <div class="pcard-top">
                <div class="pdot" style="background:${p.color};"></div>
                <div class="pname">${p.name}</div>
            </div>
            <div class="pwalls">
                <i class="ti ti-wall" style="font-size:13px;vertical-align:-1px;" aria-hidden="true"></i>
                ${p.walls_left} mur${p.walls_left !== 1 ? 's' : ''}
            </div>
            ${isCurrent
                ? `<div class="pturn" style="background:${p.color};color:#fff;">son tour !</div>`
                : ''}
        </div>`;
    }).join('');
}

// ── Action row rendering ──────────────────────────────────────────────────────

/**
 * Renders the three action buttons (move, wall-h, wall-v).
 * The selected button is highlighted.
 * Buttons are disabled (greyed out) when it is not this client's turn.
 */
function renderActionRow() {
    const myTurn = isMyTurn();
    const hints = {
        'move'  : 'clique une case adjacente',
        'wall-h': 'clique une tranchée horizontale',
        'wall-v': 'clique une tranchée verticale',
    };

    document.getElementById('action-row').innerHTML =
        MODES.map(m => {
            const sel = mode === m.id;
            const selStyle = sel
                ? `background:${m.color};box-shadow:0 4px 0 ${m.shadow};border-color:transparent;color:#fff;`
                : '';
            const disabled = !myTurn ? 'opacity:0.45;pointer-events:none;' : '';
            return `<button class="abtn${sel ? ' sel' : ''}"
                            style="${selStyle}${disabled}"
                            onclick="setMode('${m.id}')">
                      ${m.label}
                    </button>`;
        }).join('')
        + `<span class="hint">${myTurn ? hints[mode] : 'en attente…'}</span>`;
}

// ── Click handlers ────────────────────────────────────────────────────────────

/**
 * Called when a board cell is clicked.
 * If in 'move' mode and it is this client's turn, emits 'play_move'.
 * @param {number} r - row (1-indexed)
 * @param {number} c - col (1-indexed)
 */
function handleCellClick(r, c) {
    if (!isMyTurn() || mode !== 'move') return;
    socket.emit('play_move', { r, c });
}

/**
 * Called when a vertical wall gap is clicked.
 * Emits 'play_wall' with vertical=true.
 * @param {number} r - row of the wall's top cell (1-indexed)
 * @param {number} c - column index of the wall trench
 */
function handleVWallClick(r, c) {
    if (!isMyTurn() || mode !== 'wall-v' || !hasWallsLeft()) return;
    socket.emit('play_wall', { r, c, vertical: true });
}

/**
 * Called when a horizontal wall gap is clicked.
 * Emits 'play_wall' with vertical=false.
 * @param {number} r - row index of the wall trench
 * @param {number} c - column of the wall's left cell (1-indexed)
 */
function handleHWallClick(r, c) {
    if (!isMyTurn() || mode !== 'wall-h' || !hasWallsLeft()) return;
    socket.emit('play_wall', { r, c, vertical: false });
}

// ── Turn / wall helpers ───────────────────────────────────────────────────────

/**
 * Returns true if it is currently this client's turn to play.
 * Compares gameState.current_turn with myIndex (determined by color match).
 */
function isMyTurn() {
    return gameState && myIndex !== -1 && gameState.current_turn === myIndex;
}

/**
 * Returns true if the current player still has walls to place.
 */
function hasWallsLeft() {
    if (!gameState || myIndex === -1) return false;
    return gameState.players[myIndex].walls_left > 0;
}

/**
 * Returns true if (r, c) is a valid move target for the current player.
 * A valid move is exactly one step orthogonally from the player's position.
 * Wall collision is validated server-side — we just highlight reachable cells.
 * @param {number} r - row (1-indexed)
 * @param {number} c - col (1-indexed)
 */
function isMoveTarget(r, c) {
    if (!gameState || myIndex === -1) return false;
    const pos = gameState.players[myIndex].position;
    const dr  = Math.abs(r - pos[0]);
    const dc  = Math.abs(c - pos[1]);
    return (dr === 1 && dc === 0) || (dr === 0 && dc === 1);
}

// ── Win screen ────────────────────────────────────────────────────────────────

/**
 * Shows the win overlay with the winner's name.
 * Creator sees Rejouer + Nouvelle partie buttons.
 * Others see a "waiting for creator" message.
 * @param {object} state - the final game state from the server
 */
function showWinScreen(state) {
    const winner = state.players[state.winner];
    document.getElementById('win-title').textContent = `${winner.name} gagne !`;
    document.getElementById('win-sub').textContent   = 'Bien joué à tous 🎉';

    // color the Rejouer button with the winner's color
    const replayBtn = document.getElementById('btn-replay');
    replayBtn.style.background = winner.color;
    replayBtn.style.boxShadow  = `0 4px 0 ${darken(winner.color)}`;

    if (isCreator) {
        document.getElementById('btn-replay').style.display   = 'block';
        document.getElementById('btn-newgame').style.display  = 'block';
        document.getElementById('win-waiting').style.display  = 'none';
    } else {
        document.getElementById('btn-replay').style.display   = 'none';
        document.getElementById('btn-newgame').style.display  = 'none';
        document.getElementById('win-waiting').style.display  = 'block';
    }

    document.getElementById('win-overlay').style.display = 'flex';
}

/**
 * Creator clicked Rejouer — emits 'restart_game' to the server.
 * The server will reset the game state and broadcast 'game_restart'.
 */
function requestReplay() {
    socket.emit('restart_game', { game_code: gameState.game_code });
}

/**
 * Creator clicked Nouvelle partie — goes back to the lobby.
 */
function requestNewGame() {
    sessionStorage.clear();
    window.location.href = '/';
}

// ── Color utility ─────────────────────────────────────────────────────────────

/**
 * Returns a darkened version of a hex color for drop shadows.
 * Subtracts ~40 from each RGB channel (clamped to 0).
 * @param {string} hex - e.g. '#D85A30'
 * @returns {string} darkened hex string
 */
function darken(hex) {
    if (!hex || hex[0] !== '#') return '#000';
    const amt = 55;
    const r   = Math.max(0, parseInt(hex.slice(1,3),16) - amt);
    const g   = Math.max(0, parseInt(hex.slice(3,5),16) - amt);
    const b   = Math.max(0, parseInt(hex.slice(5,7),16) - amt);
    return `#${r.toString(16).padStart(2,'0')}${g.toString(16).padStart(2,'0')}${b.toString(16).padStart(2,'0')}`;
}

// ── Full render pass ──────────────────────────────────────────────────────────

/**
 * Re-renders all UI components from the current gameState.
 * Called after every state update received from the server.
 */
function render() {
    renderBoard();
    renderPlayersBar();
    renderActionRow();
}

// ── Socket event listeners ────────────────────────────────────────────────────

// Server → client: a move or wall was played, state has changed.
// Re-renders the entire board with the new state.
socket.on('game_update', (state) => {
    gameState = state;
    render();
});

// Server → client: someone won. Shows the win screen.
socket.on('game_over', (state) => {
    gameState = state;
    render();
    showWinScreen(state);
});

// Server → client: creator restarted the game.
// Non-creators are redirected to the lobby to re-join with the same code.
socket.on('game_restart', ({ game_code }) => {
    if (!isCreator) {
        sessionStorage.clear();
        window.location.href = '/';
    } else {
        // creator stays on the game page — server will emit game_start again
        document.getElementById('win-overlay').style.display = 'none';
    }
});

socket.on('game_state', (state) => {
    gameState = state;
    myIndex   = state.players.findIndex(p => p.color === myColor);
    render();
});

// Server → client: a player disconnected mid-game.
// Shows a brief notice but keeps the game running (server handles missing turns).
socket.on('player_disconnected', ({ player_index, state }) => {
    gameState = state;
    const name = state.players[player_index]?.name ?? 'Un joueur';
    console.warn(`${name} s'est déconnecté.`);
    render();
});

// Server → client: an invalid action was attempted by this client.
// Logged to console — no visible error to avoid disrupting the UX.
socket.on('error', ({ message }) => {
    console.warn('Karidor error:', message);
});

// ── Init ──────────────────────────────────────────────────────────────────────

// Determine this client's player index by matching their chosen color
// against the players array in the initial game state.
if (gameState) {
    myIndex = gameState.players.findIndex(p => p.color === myColor);
    // board_size must be in the state — add it in game.py get_state() if missing
    render();
} else {
    // No state in sessionStorage — redirect back to lobby
    window.location.href = '/';
}