# ── Build stage ───────────────────────────────────────────────
# Use a slim Python 3.12 image to keep the final image light
FROM python:3.12-slim

# Set working directory inside the container
WORKDIR /app

# Copy dependency list first — Docker caches this layer separately
# so a code change doesn't trigger a full pip reinstall
COPY requirements.txt .

# Install Python dependencies
# --no-cache-dir keeps the image smaller
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the project into the container
COPY . .

# Expose the port Flask-SocketIO listens on
EXPOSE 5000

# Start the app via run.py using the eventlet worker
# Eventlet is required for WebSocket support with Flask-SocketIO
CMD ["python", "run.py"]