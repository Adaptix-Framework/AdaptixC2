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
# Stage: build-client (Qt client)
# ============================================
FROM ubuntu:22.04 AS build-client

ENV DEBIAN_FRONTEND=noninteractive
ENV QT_VERSION=6.9.2

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    python3-pip \
    libgl1-mesa-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb-xkb1 \
    libxcb1-dev \
    libfontconfig1-dev \
    libfreetype6-dev \
    libssl-dev \
    libdbus-1-dev \
    libegl1-mesa-dev \
    libfuse2 \
    wget \
    file \
    fuse \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Qt via aqtinstall
RUN python3 -m pip install --upgrade pip setuptools wheel && \
    pip3 install --no-cache-dir --default-timeout=100 aqtinstall && \
    aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 \
    -m qtwebsockets qtnetworkauth \
    -O /opt/qt

ENV PATH="/usr/local/bin:/opt/qt/${QT_VERSION}/gcc_64/bin:$PATH"

# Install linuxdeployqt and appimagetool
RUN wget -q https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage \
    -O /usr/local/bin/linuxdeployqt && \
    chmod +x /usr/local/bin/linuxdeployqt && \
    wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage \
    -O /usr/local/bin/appimagetool && \
    chmod +x /usr/local/bin/appimagetool

# Install CMake 3.31.6
RUN wget -qO- "https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz" | \
    tar --strip-components=1 -xz -C /usr/local

RUN cd /opt/qt/${QT_VERSION}/gcc_64/plugins/sqldrivers && \
    rm -f libqsqlmimer.so* libqsqlmysql.so* libqsqlpsql.so* libqsqlodbc.so* 2>/dev/null || true && \
    rm -f /opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6Sql/Qt6QMYSQLDriverPlugin*.cmake 2>/dev/null || true && \
    rm -f /opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6Sql/Qt6QPSQLDriverPlugin*.cmake 2>/dev/null || true && \
    rm -f /opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6Sql/Qt6QODBCDriverPlugin*.cmake 2>/dev/null || true && \
    rm -f /opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6Sql/Qt6QMimerSQLDriverPlugin*.cmake 2>/dev/null || true && \
    sed -i 's/QMYSQLDriverPlugin//g; s/QPSQLDriverPlugin//g; s/QODBCDriverPlugin//g; s/QMimerSQLDriverPlugin//g' \
        /opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6Sql/Qt6SqlPlugins.cmake 2>/dev/null || true && \
    ls -la /opt/qt/${QT_VERSION}/gcc_64/plugins/sqldrivers/

ENV CMAKE_PREFIX_PATH="/opt/qt/${QT_VERSION}/gcc_64"
ENV LD_LIBRARY_PATH="/opt/qt/${QT_VERSION}/gcc_64/lib"
ENV QT_DIR="/opt/qt/${QT_VERSION}/gcc_64"

WORKDIR /src
COPY ./AdaptixClient /src

# Build client
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/qt/${QT_VERSION}/gcc_64 && \
    cmake --build build -j$(nproc)

# Prepare AppDir structure
RUN mkdir -p /client-dist/AdaptixClient.AppDir/usr/bin && \
    mkdir -p /client-dist/AdaptixClient.AppDir/usr/share/applications && \
    mkdir -p /client-dist/AdaptixClient.AppDir/usr/share/icons/hicolor/256x256/apps && \
    cp build/AdaptixClient /client-dist/AdaptixClient.AppDir/usr/bin/ && \
    cp Resources/Logo.png /client-dist/AdaptixClient.AppDir/usr/share/icons/hicolor/256x256/apps/AdaptixClient.png && \
    echo '[Desktop Entry]\nType=Application\nName=Adaptix Client\nExec=AdaptixClient\nIcon=AdaptixClient\nCategories=Network;Security;\nTerminal=false' > /client-dist/AdaptixClient.AppDir/usr/share/applications/AdaptixClient.desktop && \
    ln -sf usr/bin/AdaptixClient /client-dist/AdaptixClient.AppDir/AppRun && \
    ln -sf usr/share/icons/hicolor/256x256/apps/AdaptixClient.png /client-dist/AdaptixClient.AppDir/AdaptixClient.png && \
    ln -sf usr/share/applications/AdaptixClient.desktop /client-dist/AdaptixClient.AppDir/AdaptixClient.desktop

# Deploy Qt libraries and create AppImage
RUN cd /client-dist && \
    /usr/local/bin/linuxdeployqt --appimage-extract && \
    ./squashfs-root/AppRun AdaptixClient.AppDir/usr/share/applications/AdaptixClient.desktop \
        -bundle-non-qt-libs \
        -no-translations && \
    rm -rf squashfs-root && \
    /usr/local/bin/appimagetool --appimage-extract && \
    ARCH=x86_64 ./squashfs-root/AppRun AdaptixClient.AppDir AdaptixClient-x86_64.AppImage && \
    rm -rf squashfs-root



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
    mkdir -p /usr/lib/go-win7/pkg/include && \
    cd /usr/lib/go-win7/src/runtime && \
    for f in *.h; do ln -sf /usr/lib/go-win7/src/runtime/$f /usr/lib/go-win7/pkg/include/$f; done && \
    echo "[+] go-win7 library installed successfully"

WORKDIR /app

COPY ./AdaptixServer/server-dist/adaptixserver /app/adaptixserver
COPY ./AdaptixServer/server-dist/profile.yaml /app/profile.yaml
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
CMD ["/app/adaptixserver", "-profile", "/app/profile.yaml"]
