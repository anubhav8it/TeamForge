# TeamForge web server for Render (Docker). Build context: the project folder (Phase-2/).
# Serves the WebAssembly app from web/static/app and the sign-in/data API (web/server.py).
FROM python:3.12-slim
WORKDIR /app
COPY web/requirements.txt web/requirements.txt
RUN pip install --no-cache-dir -r web/requirements.txt
COPY . .
RUN python web/tools/precompress.py
# Render sets PORT; 10000 is its default.
ENV PORT=10000
EXPOSE 10000
CMD ["python", "web/server.py"]
