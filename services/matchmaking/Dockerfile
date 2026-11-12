# Vector War matchmaking server. Standard library only; builds for amd64 and
# arm64 (the free Oracle Ampere A1 shape).
FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /app
COPY matchmaking/ ./matchmaking/

# Unprivileged, and the image needs no writable filesystem.
USER 65534:65534
EXPOSE 7100/udp
ENTRYPOINT ["python", "-m", "matchmaking"]
CMD ["--host", "0.0.0.0", "--port", "7100"]
