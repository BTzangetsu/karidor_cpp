// main.js — Karidor lobby
// Handles: color picker, create/join forms, waiting room, socket events from the lobby.
// Redirects to /game once all players have joined.

// ── Constants ────────────────────────────────────────────────────────────────

const COLORS = [
    { hex: '#D85A30', name: 'orange' },
    { hex: '#1D9E75', name: 'teal'   },
    { hex: '#534AB7', name: 'purple' },
    { hex: '#D4537E', name: 'pink'   },
    { hex: '#378ADD', name: 'blue'   },
    { hex: '#BA7517', name: 'amber'  },
];

// ── State ────────────────────────────────────────────────────────────────────

let selectedCount = 2;      // number of players chosen by the creator (2 / 3 / 4)
let selectedColor = null;   // hex string of the color currently selected by this client
let isCreator     = false;  // true if this client created the current game

// ── Color picker ─────────────────────────────────────────────────────────────

/**
 * Renders clickable color circles into a container element.
 * The first color is pre-selected by default.
 * @param {string} containerId - id of the DOM element to fill
 */
function buildColorPicker(containerId) {
    const container = document.getElementById(containerId);
    container.innerHTML = '';

    COLORS.forEach((c, i) => {
        const btn = document.createElement('button');
        btn.className   = 'color-opt' + (i === 0 ? ' sel' : '');
        btn.style.background = c.hex;
        btn.title       = c.name;
        btn.setAttribute('aria-label', c.name);
        btn.onclick     = () => selectColor(c.hex, containerId);
        container.appendChild(btn);
    });

    // pre-select the first color when building the picker
    if (!selectedColor) selectedColor = COLORS[0].hex;
}

/**
 * Marks the clicked color as selected and deselects the others
 * within the same picker container.
 * @param {string} hex         - hex color string of the chosen color
 * @param {string} containerId - id of the picker container
 */
function selectColor(hex, containerId) {
    selectedColor = hex;
    const container = document.getElementById(containerId);
    container.querySelectorAll('.color-opt').forEach(btn => {
        btn.classList.toggle('sel', btn.style.background === hex ||
            hexToRgb(hex) === btn.style.background);
    });
}

/**
 * Small helper — browsers sometimes return rgb() instead of hex
 * when reading back style.background. This converts for comparison.
 */
function hexToRgb(hex) {
    const r = parseInt(hex.slice(1,3),16);
    const g = parseInt(hex.slice(3,5),16);
    const b = parseInt(hex.slice(5,7),16);
    return `rgb(${r}, ${g}, ${b})`;
}

// ── Player count selector ────────────────────────────────────────────────────

/**
 * Highlights the chosen player-count button and updates selectedCount.
 * Called by the onclick on each .pc-btn in index.html.
 * @param {number} n - chosen number of players (2, 3, or 4)
 */
function selectCount(n) {
    selectedCount = n;
    document.querySelectorAll('.pc-btn').forEach(btn => {
        btn.classList.toggle('sel', parseInt(btn.dataset.n) === n);
    });
}

// ── Create game ──────────────────────────────────────────────────────────────

/**
 * Reads the create-form inputs, validates them, and emits 'create_room'
 * to the server via the global socket (defined in socket.js).
 * The server responds with 'room_created'.
 */
function createGame() {
    const name = document.getElementById('create-name').value.trim();

    if (!name) {
        alert('Entre ton pseudo !');
        return;
    }
    if (!selectedColor) {
        alert('Choisis une couleur !');
        return;
    }

    isCreator = true;
    socket.emit('create_room', {
        num_players : selectedCount,
        name        : name,
        color       : selectedColor,
    });
}

// ── Join game ────────────────────────────────────────────────────────────────

/**
 * Reads the join-form inputs, validates them, and emits 'join_room'
 * to the server. The server responds with 'player_joined', 'game_start',
 * or 'error' (which we display via #join-error).
 */
function joinGame() {
    const name = document.getElementById('join-name').value.trim();
    const code = document.getElementById('join-code').value.trim();

    document.getElementById('join-error').style.display = 'none';

    if (!name) { alert('Entre ton pseudo !'); return; }
    if (!/^\d{4}$/.test(code)) {
        document.getElementById('join-error').style.display = 'block';
        return;
    }
    if (!selectedColor) { alert('Choisis une couleur !'); return; }

    isCreator = false;
    socket.emit('join_room', {
        game_code : code,
        name      : name,
        color     : selectedColor,
    });
}

// ── Waiting list renderer ────────────────────────────────────────────────────

/**
 * Fills #waiting-list with the players who have already joined
 * and placeholder rows for the remaining empty slots.
 * @param {Array} players - array of { name, color } from get_state()
 */
function updateWaitingList(players) {
    const list = document.getElementById('waiting-list');
    list.innerHTML = '';

    // show joined players
    players.forEach(p => {
        const row = document.createElement('div');
        row.className = 'waiting-player';
        row.innerHTML = `
            <div class="waiting-dot" style="background:${p.color};"></div>
            <span>${p.name}</span>
        `;
        list.appendChild(row);
    });

    // fill remaining slots with placeholders
    const remaining = selectedCount - players.length;
    for (let i = 0; i < remaining; i++) {
        const row = document.createElement('div');
        row.className = 'waiting-empty';
        row.innerHTML = `<i class="ti ti-clock" aria-hidden="true"></i> En attente…`;
        list.appendChild(row);
    }
}

// ── Socket event listeners ───────────────────────────────────────────────────

// Server → client: game room was successfully created.
// Hides the create form and shows the waiting room with the game code.
socket.on('room_created', ({ game_code }) => {
    document.getElementById('card-create').style.display  = 'none';
    document.getElementById('card-waiting').style.display = 'block';
    document.getElementById('waiting-code').textContent   = game_code;
    // show the creator alone in the waiting list immediately
    const name  = document.getElementById('create-name').value.trim();
    updateWaitingList([{ name, color: selectedColor }]);
});

// Server → client: a player joined the lobby (including the joiner themselves).
// Updates the waiting list for everyone in the room.
socket.on('player_joined', (state) => {
    // if this client just joined, hide the join form and show the waiting room
    document.getElementById('card-join').style.display    = 'none';
    document.getElementById('card-waiting').style.display = 'block';
    document.getElementById('waiting-code').textContent   = state.game_code;
    updateWaitingList(state.players);
});

// Server → client: join attempt was rejected (wrong code, game full, etc.)
socket.on('error', ({ message }) => {
    document.getElementById('join-error').style.display = 'block';
});

// Server → client: all players have joined, the game is starting.
// Saves state + creator flag to sessionStorage then redirects to /game.
socket.on('game_start', (state) => {
    sessionStorage.setItem('gameState', JSON.stringify(state));
    sessionStorage.setItem('isCreator', JSON.stringify(isCreator));
    sessionStorage.setItem('myColor',   selectedColor);
    window.location.href = '/game';
});

// ── Init ─────────────────────────────────────────────────────────────────────

// Build both color pickers and pre-select the default count button on page load.
buildColorPicker('create-color-picker');
buildColorPicker('join-color-picker');
selectCount(2);