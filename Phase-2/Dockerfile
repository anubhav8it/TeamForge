# TeamForge web server image, for Azure Container Apps (see web/AZURE-DEPLOY.md); it also runs
# on Render or any Docker host. Build context: the project folder (Phase-2/). Only web/ and
# data/ go in (.dockerignore): the server, the built WebAssembly app and the sample data.
FROM python:3.12-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PORT=8000 \
    TEAMFORGE_SQLITE=/tmp/teamforge-local.db
WORKDIR /app
COPY web/requirements.txt web/requirements.txt
RUN pip install --no-cache-dir -r web/requirements.txt
COPY web web
COPY data data
# gzip copies of the app files, so the server does not compress 38 MB on every cold start.
RUN python web/tools/precompress.py \
    && useradd --system --uid 10001 --no-create-home teamforge
USER teamforge
EXPOSE 8000
# Data lives in PostgreSQL (DATABASE_URL); without it, a throwaway SQLite file in /tmp.
CMD ["python", "web/server.py"]
