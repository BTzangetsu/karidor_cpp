// socket.js — Karidor SocketIO client
// Must be loaded BEFORE main.js and game.js in every HTML page.
// Exposes a single global `socket` used by all other JS files.

// Connect to the Flask-SocketIO server.
// The URL is omitted so it defaults to the current host — works both
// in local dev (localhost:5000) and in production behind a reverse proxy.
const socket = io();

// Log connection events in development — harmless in production.
socket.on('connect',    () => console.log('Karidor socket connected:', socket.id));
socket.on('disconnect', () => console.log('Karidor socket disconnected'));
socket.on('connect_error', (err) => console.error('Socket connection error:', err.message));
// When the socket connects on the game page, re-register this client
// because the sid changed after the page redirect from the lobby.
socket.on('connect', () => {
    const gameState = JSON.parse(sessionStorage.getItem('gameState') || 'null');
    const myColor   = sessionStorage.getItem('myColor');

    // only emit on the game page (both values must be present)
    if (gameState && myColor) {
        socket.emit('rejoin_game', {
            game_code : gameState.game_code,
            color     : myColor,
        });
    }
});