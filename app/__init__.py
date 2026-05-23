from flask import Flask
from flask_socketio import SocketIO

# SocketIO instance created here so it can be imported by events.py
socketio = SocketIO()


def create_app() -> Flask:
    """Application factory. Creates and configures the Flask app,
    initializes SocketIO, registers routes and socket events.
    Returns the configured Flask app instance."""

    app = Flask(__name__, static_folder='../static', template_folder='../templates')
    app.config['SECRET_KEY'] = 'karidor-secret-key'

    # attach SocketIO to the app — cors_allowed_origins='*' for local dev
    socketio.init_app(app, cors_allowed_origins='*')

    # register HTTP routes
    from app.routes import routes
    app.register_blueprint(routes)

    # register all SocketIO event handlers
    from app.sockets.events import register_events
    register_events(socketio)

    return app