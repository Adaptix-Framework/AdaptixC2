FROM golang:1.25-bookworm AS base

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        mingw-w64 \
        g++-mingw-w64 \
        gcc \
        g++ \
        make \
        build-essential \
        libssl-dev \
        zlib1g-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# ============================================
# Stage: build-server (server only)
# ============================================
FROM base AS build-server

COPY . .

RUN make server

# ============================================
# Stage: build-extenders (extenders only)
# ============================================
FROM base AS build-extenders

COPY . .

RUN make extenders

# ============================================
# Stage: build-server-ext (server + extenders)
# ============================================
FROM base AS build-server-ext

COPY . .

RUN make server && make extenders

# ============================================
# Stage: runtime (runtime for server execution)
# ============================================
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    mingw-w64 \
    g++-mingw-w64 \
    gcc \
    g++ \
    make \
    openssl \
    wget \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://go.dev/dl/go1.25.4.linux-amd64.tar.gz -O /tmp/go1.25.4.linux-amd64.tar.gz && \
    rm -rf /usr/local/go /usr/local/bin/go && \
    tar -C /usr/local -xzf /tmp/go1.25.4.linux-amd64.tar.gz && \
    ln -s /usr/local/go/bin/go /usr/local/bin/go && \
    rm /tmp/go1.25.4.linux-amd64.tar.gz && \
    echo "[+] Go 1.25.4 installed successfully"

RUN git clone https://github.com/Adaptix-Framework/go-win7 /tmp/go-win7 && \
    mv /tmp/go-win7 /usr/lib/ && \
    echo "[+] go-win7 library installed successfully"

WORKDIR /app

COPY ./AdaptixServer/server-dist/adaptixserver /app/adaptixserver
COPY ./AdaptixServer/server-dist/profile.json /app/profile.json
COPY ./AdaptixServer/server-dist/404page.html /app/404page.html
COPY ./AdaptixServer/server-dist/ssl_gen.sh /app/ssl_gen.sh
COPY ./AdaptixServer/server-dist/extenders /app/extenders

RUN mkdir -p /app/data && \
    echo '#!/bin/bash\n\
set -e\n\
echo "[*] Starting Adaptix C2 Server..."\n\
if [ ! -f /app/server.rsa.crt ] || [ ! -f /app/server.rsa.key ]; then\n\
    echo "[*] Generating self-signed certificates..."\n\
    cd /app && openssl req -x509 -nodes -newkey rsa:2048 -keyout server.rsa.key -out server.rsa.crt -days 3650 -subj "/C=US/ST=State/L=City/O=AdaptixC2/CN=localhost"\n\
    echo "[+] Certificates generated successfully"\n\
fi\n\
echo "[+] Launching Adaptix Server..."\n\
exec "$@"' > /app/entrypoint.sh && \
    chmod +x /app/entrypoint.sh

EXPOSE 4321 80 443 8080 8443 8000 8888 50050-50055 9000-9002 7000-7010

ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["/app/adaptixserver", "-profile", "/app/profile.json"]