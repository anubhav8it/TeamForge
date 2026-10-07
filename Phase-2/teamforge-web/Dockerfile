# Build and run TeamForge in a container (for hosts that deploy Docker images).
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
      g++ cmake make curl unzip ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN sh third_party/get-deps.sh \
 && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j \
 && cd /src && ./build/teamforge_core_tests && ./build/teamforge_api_tests

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /src/build/teamforge /app/teamforge
COPY static /app/static
COPY data /app/data
RUN mkdir -p /data
# Mount a persistent volume at /data so the database survives redeploys.
ENV DATABASE=/data/teamforge.db PORT=8080
EXPOSE 8080
# First start: load sample data. If ADMIN_EMAIL and ADMIN_PASSWORD are set, create that admin.
CMD sh -c '[ -f "$DATABASE" ] || ./teamforge seed; \
  if [ -n "$ADMIN_EMAIL" ] && [ -n "$ADMIN_PASSWORD" ]; then ./teamforge create-admin "$ADMIN_EMAIL" --password "$ADMIN_PASSWORD"; fi; \
  exec ./teamforge serve --host 0.0.0.0'
