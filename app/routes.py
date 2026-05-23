from flask import Blueprint, render_template

routes = Blueprint('routes', __name__)


@routes.route('/')
def index():
    """Serves the main page — lobby where players can create or join a game."""
    return render_template('index.html')


@routes.route('/game')
def game():
    """Serves the game page — the actual board UI.
    The frontend connects via SocketIO once this page loads."""
    return render_template('game.html')