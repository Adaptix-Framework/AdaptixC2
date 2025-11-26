# 1. Base image and system dependencies
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    ca-certificates openssl curl wget git mingw-w64 gcc g++ make \
    && rm -rf /var/lib/apt/lists/*

# 2. Install Go (Auto-detect architecture: amd64 or arm64)
RUN ARCH=$(dpkg --print-architecture) && \
    if [ "$ARCH" = "amd64" ]; then GO_ARCH="amd64"; \
    elif [ "$ARCH" = "arm64" ]; then GO_ARCH="arm64"; \
    else echo "Unsupported architecture $ARCH" >&2; exit 1; fi && \
    wget "https://go.dev/dl/go1.24.4.linux-${GO_ARCH}.tar.gz" -O "/tmp/go.tar.gz" && \
    rm -rf /usr/local/go /usr/local/bin/go && \
    tar -C /usr/local -xzf /tmp/go.tar.gz && \
    ln -s /usr/local/go/bin/go /usr/local/bin/go && \
    rm /tmp/go.tar.gz

# 3. Install go-win7 library (for cross-compilation)
RUN git clone https://github.com/Adaptix-Framework/go-win7 /tmp/go-win7 && \
    mv /tmp/go-win7 /usr/lib/go-win7

# 4. Configure Go environment
ENV PATH="/usr/local/go/bin:${PATH}" \
    GOPATH="/root/go" \
    GOROOT="/usr/local/go" \
    GOROOT_WIN7="/usr/lib/go-win7" \
    GO111MODULE=on

WORKDIR /app

# 5. Build Adaptix Server
COPY . .

WORKDIR /app/AdaptixServer
RUN go mod tidy && \
    go build -o adaptixserver . && \
    chmod +x adaptixserver

# 6. Create required directories
RUN mkdir -p /app/AdaptixServer/data /app/AdaptixServer/logs /app/AdaptixServer/certs

# 7. Download dependencies for Extenders
RUN cd /app/Extenders/gopher_agent/src_gopher && go mod tidy

# 8. Build Plugins (Listeners & Agents)
RUN cd /app/Extenders/beacon_listener_http/ && make && \
    cd /app/Extenders/beacon_listener_smb/ && make && \
    cd /app/Extenders/beacon_listener_tcp/ && make && \
    cd /app/Extenders/beacon_agent/ && make && \
    cd /app/Extenders/gopher_listener_tcp/ && make && \
    cd /app/Extenders/gopher_agent/ && make

# 9. Install Plugins (Move compiled artifacts to Server directory)
RUN mkdir -p /app/AdaptixServer/extenders/beacon_listener_http && \
    mv /app/Extenders/beacon_listener_http/dist/* /app/AdaptixServer/extenders/beacon_listener_http/ && \
    mkdir -p /app/AdaptixServer/extenders/beacon_listener_smb && \
    mv /app/Extenders/beacon_listener_smb/dist/* /app/AdaptixServer/extenders/beacon_listener_smb/ && \
    mkdir -p /app/AdaptixServer/extenders/beacon_listener_tcp && \
    mv /app/Extenders/beacon_listener_tcp/dist/* /app/AdaptixServer/extenders/beacon_listener_tcp/ && \
    mkdir -p /app/AdaptixServer/extenders/beacon_agent && \
    mv /app/Extenders/beacon_agent/dist/* /app/AdaptixServer/extenders/beacon_agent/ && \
    mkdir -p /app/AdaptixServer/extenders/gopher_listener_tcp && \
    mv /app/Extenders/gopher_listener_tcp/dist/* /app/AdaptixServer/extenders/gopher_listener_tcp/ && \
    mkdir -p /app/AdaptixServer/extenders/gopher_agent && \
    mv /app/Extenders/gopher_agent/dist/* /app/AdaptixServer/extenders/gopher_agent/ && \
    # Cleanup source dist folders
    rm -rf /app/Extenders/*/dist

# 10. Create Entrypoint Script (Handles SSL Certificate generation)
WORKDIR /app/AdaptixServer
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
CERT_SOURCE="/app/AdaptixServer/certs"\n\
CRT_NAME="server.rsa.crt"\n\
KEY_NAME="server.rsa.key"\n\
\n\
echo "[*] Starting Adaptix C2 Server..."\n\
\n\
# Check if certificates exist in the volume, otherwise generate them\n\
if [[ -f "$CERT_SOURCE/$CRT_NAME" && -f "$CERT_SOURCE/$KEY_NAME" ]]; then\n\
    echo "[+] Found existing certificates in volume"\n\
else\n\
    echo "[!] Generating new self-signed certificates ($CRT_NAME)..."\n\
    openssl req -x509 -nodes -newkey rsa:2048 \\\n\
        -keyout "$CERT_SOURCE/$KEY_NAME" \\\n\
        -out "$CERT_SOURCE/$CRT_NAME" \\\n\
        -days 3650 \\\n\
        -subj "/C=US/ST=Docker/L=Adaptix/O=C2/CN=localhost"\n\
fi\n\
\n\
# Copy certificates to current directory for the binary to find them\n\
cp -f "$CERT_SOURCE/$CRT_NAME" .\n\
cp -f "$CERT_SOURCE/$KEY_NAME" .\n\
\n\
echo "[+] Launching Server..."\n\
exec "$@"\n\
' > /app/Entrypoint.sh && chmod +x /app/Entrypoint.sh

# 11. Expose ports and set startup command
EXPOSE 4321 80 443 8080 8443 8000 8888 50050-50055 9000-9002 7000-7010

ENTRYPOINT ["/app/Entrypoint.sh"]
CMD ["./adaptixserver", "-profile", "profile.json", "-debug"]
