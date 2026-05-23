from flask_socketio import SocketIO, emit, join_room, leave_room
from flask import request
from app.sockets.rooms import RoomManager
from app.models.game import Game

room_manager = RoomManager()


def register_events(socketio: SocketIO):

    @socketio.on('create_room')
    def handle_create_room(data):
        """Receives {num_players, name, color}.
        Creates a new game, joins it as player 0, adds the sid to the SocketIO
        room, registers the sid, and emits 'room_created' to the creator."""

        # create_game() returns a Game object, not a code
        game = room_manager.create_game(data['num_players'])
        game_code = game.game_code

        # add the creator as the first player
        room_manager.join_game(game_code, data['name'], data['color'])

        # join the SocketIO room so this client receives broadcasts
        join_room(game_code)

        # player 0 because the creator is always the first to join
        room_manager.register_player(request.sid, game_code, 0)

        emit('room_created', {'game_code': game_code})

    @socketio.on('join_room')
    def handle_join_room(data):
        """Receives {game_code, name, color}.
        Joins an existing game, registers the sid.
        If the game is now started, broadcasts 'game_start' with get_state()
        to the entire room."""

        game_code = data['game_code']
        name = data['name']
        color = data['color']

        # join_game() calls game.join() internally
        if not room_manager.join_game(game_code, name, color):
            emit('error', {'message': 'Could not join game'})
            return

        # add to SocketIO room before any broadcast
        join_room(game_code)

        # player_index is the last index in board.players after joining
        game = room_manager.get_game(game_code)
        player_index = len(game.board.players) - 1
        room_manager.register_player(request.sid, game_code, player_index)

        # if all players have joined, broadcast the initial state to everyone
        if game.started:
            emit('game_start', game.get_state(), to=game_code)
        else:
            # notify the lobby that a new player joined (useful for the waiting screen)
            emit('player_joined', game.get_state(), to=game_code)

    @socketio.on('play_move')
    def handle_play_move(data):
        """Receives {r, c}.
        Retrieves the game via get_game_by_player(sid).
        Verifies it is the sender's turn.
        Calls game.play_move(r, c) and broadcasts the updated state
        to the room via 'game_update'.
        If the game is finished, broadcasts 'game_over' with the winner."""

        result = room_manager.get_game_by_player(request.sid)
        if not result:
            emit('error', {'message': 'Player not found'})
            return

        game, player_index = result

        # only the current player can act
        if game.board.playerturn != player_index:
            emit('error', {'message': 'Not your turn'})
            return

        if not game.play_move(data['r'], data['c']):
            emit('error', {'message': 'Invalid move'})
            return

        # broadcast the updated state to all players in the room
        if game.finished:
            emit('game_over', game.get_state(), to=game.game_code)
        else:
            emit('game_update', game.get_state(), to=game.game_code)

    @socketio.on('play_wall')
    def handle_play_wall(data):
        """Receives {r, c, vertical}.
        Same flow as play_move but calls game.play_wall(r, c, vertical).
        Broadcasts 'game_update' on success."""

        result = room_manager.get_game_by_player(request.sid)
        if not result:
            emit('error', {'message': 'Player not found'})
            return

        game, player_index = result

        # only the current player can act
        if game.board.playerturn != player_index:
            emit('error', {'message': 'Not your turn'})
            return

        if not game.play_wall(data['r'], data['c'], data['vertical']):
            emit('error', {'message': 'Invalid wall placement'})
            return

        emit('game_update', game.get_state(), to=game.game_code)

    @socketio.on('restart_game')
    def handle_restart_game(data):
        """Receives {game_code}.
        Only allowed for the player at index 0 (the creator).
        Removes the old game, creates a new one with the same num_players
        and the same game_code, broadcasts 'game_restart' to the room."""

        result = room_manager.get_game_by_player(request.sid)
        if not result:
            emit('error', {'message': 'Player not found'})
            return

        game, player_index = result

        # only the creator (player 0) can restart
        if player_index != 0:
            emit('error', {'message': 'Only the creator can restart'})
            return

        game_code = game.game_code
        num_players = game.num_players

        # remove the old game and all its sid mappings
        room_manager.remove_game(game_code)

        # create a fresh game and force its code to stay the same
        new_game = Game(num_players)
        new_game.game_code = game_code
        room_manager.games[game_code] = new_game

        # notify all clients still in the SocketIO room to reset their UI
        emit('game_restart', {'game_code': game_code}, to=game_code)

    @socketio.on('disconnect')
    def handle_disconnect():
        """No data.
        Unregisters the sid from room_manager.
        If the game was ongoing, broadcasts 'player_disconnected' to the room."""

        result = room_manager.get_game_by_player(request.sid)

        if result:
            game, player_index = result
            game_code = game.game_code

            room_manager.unregister_player(request.sid)

            # notify remaining players only if the game was in progress
            if game.started and not game.finished:
                emit('player_disconnected', {
                    'player_index': player_index,
                    'state': game.get_state()
                }, to=game_code)
        else:
            room_manager.unregister_player(request.sid)