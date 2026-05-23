from app import create_app, socketio

app = create_app()

if __name__ == '__main__':
    # debug=True enables auto-reload on code changes
    # use_reloader=False avoids SocketIO conflicts with the reloader in debug mode
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, use_reloader=False)