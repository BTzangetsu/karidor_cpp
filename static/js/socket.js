// socket.js — Karidor WebSocket client (native WebSocket, no Socket.io)
// Must be loaded BEFORE main.js and game.js in every HTML page.
// Exposes a global `socket` object that mimics the Socket.io API
// used by main.js and game.js so those files don't need to change.
//
// The server sends messages in the format: { "event": "...", "data": { ... } }
// The client sends messages in the format: { "event": "...", "data": { ... } }
// This matches the convention set in events.hpp on the C++ side.

// ── Internal state ────────────────────────────────────────────────────────────

// Registry of event listeners: { eventName: [callback, ...] }
const _listeners = {};

// Queue of messages sent before the connection was open
// They are flushed once the connection is established
const _queue = [];

// The native WebSocket connection
const _ws = new WebSocket(`ws://${window.location.host}/ws`);

// ── Native WebSocket event handlers ──────────────────────────────────────────

_ws.onopen = () => {
    console.log('Karidor WebSocket connected');

    // Flush any messages that were emitted before the connection opened
    _queue.forEach(msg => _ws.send(msg));
    _queue.length = 0;

    // Notify listeners registered on 'connect'
    _dispatch('connect', {});
};

_ws.onclose = () => {
    console.log('Karidor WebSocket disconnected');
    _dispatch('disconnect', {});
};

_ws.onerror = (err) => {
    console.error('Karidor WebSocket error:', err);
    _dispatch('connect_error', { message: 'WebSocket error' });
};

_ws.onmessage = (event) => {
    let parsed;
    try {
        parsed = JSON.parse(event.data);
    } catch (e) {
        console.error('Karidor: received invalid JSON:', event.data);
        return;
    }

    const eventName = parsed.event;
    const data      = parsed.data ?? {};

    if (!eventName) {
        console.warn('Karidor: received message without event field:', parsed);
        return;
    }

    // Dispatch to all registered listeners for this event name
    _dispatch(eventName, data);
};

// ── Internal helpers ──────────────────────────────────────────────────────────

// Calls all listeners registered for a given event name
function _dispatch(eventName, data) {
    const callbacks = _listeners[eventName] || [];
    callbacks.forEach(cb => cb(data));
}

// ── Public API — mimics Socket.io so main.js and game.js are unchanged ────────

const socket = {

    /* Register a listener for a server-sent event.
       Equivalent of: socket.on('game_update', (data) => { ... })  */
    on(eventName, callback) {
        if (!_listeners[eventName]) _listeners[eventName] = [];
        _listeners[eventName].push(callback);
    },

    /* Remove all listeners for a given event, or a specific one.
       Equivalent of: socket.off('game_update') */
    off(eventName, callback) {
        if (!_listeners[eventName]) return;
        if (callback) {
            _listeners[eventName] = _listeners[eventName].filter(cb => cb !== callback);
        } else {
            delete _listeners[eventName];
        }
    },

    /* Send an event to the server with an optional payload.
       Equivalent of: socket.emit('play_move', { r: 3, c: 4 })
       Serializes to: { "event": "play_move", "data": { "r": 3, "c": 4 } } */
    emit(eventName, data = {}) {
        const message = JSON.stringify({ event: eventName, data });

        if (_ws.readyState === WebSocket.OPEN) {
            _ws.send(message);
        } else {
            // Connection not yet open — queue and send once connected
            _queue.push(message);
        }
    },

    /* Returns true if the WebSocket is currently connected. */
    get connected() {
        return _ws.readyState === WebSocket.OPEN;
    },
};