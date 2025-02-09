# Этап сборки
FROM golang:1.22-bullseye AS builder

# Установка необходимых зависимостей для сборки
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    musl-dev \
    libcap-dev \
    sqlite3 \
    && rm -rf /var/lib/apt/lists/*

# Установка рабочей директории
WORKDIR /app

# Копирование исходного кода
COPY AdaptixServer/ AdaptixServer/
COPY Extenders/ Extenders/

# Сборка сервера и расширений
RUN cd AdaptixServer && \
    go mod download && \
    CGO_ENABLED=1 GOOS=linux go build -ldflags="-s -w" -o adaptixserver && \
    cd ../Extenders/agent_beacon && \
    go mod download && \
    CGO_ENABLED=1 GOOS=linux go build -buildmode=plugin -ldflags="-s -w" -o agent_beacon.so && \
    cd ../listener_beacon_http && \
    go mod download && \
    CGO_ENABLED=1 GOOS=linux go build -buildmode=plugin -ldflags="-s -w" -o listener_beacon_http.so

# Финальный этап
FROM debian:bookworm-slim

# Установка необходимых зависимостей для работы
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    libcap2-bin \
    sqlite3 \
    openssl \
    && rm -rf /var/lib/apt/lists/* && \
    useradd -m -d /app adaptix

WORKDIR /app

# Создание необходимых директорий
RUN mkdir -p /app/extenders/agent_beacon /app/extenders/listener_beacon_http

# Копирование файлов из этапа сборки
COPY --from=builder /app/AdaptixServer/adaptixserver /app/
COPY --from=builder /app/AdaptixServer/404page.html /app/
COPY --from=builder /app/Extenders/extender.txt /app/extenders/
COPY --from=builder /app/Extenders/agent_beacon/agent_beacon.so /app/extenders/agent_beacon/
COPY --from=builder /app/Extenders/agent_beacon/_ui_agent.json /app/extenders/agent_beacon/
COPY --from=builder /app/Extenders/agent_beacon/_cmd_agent.json /app/extenders/agent_beacon/
COPY --from=builder /app/Extenders/listener_beacon_http/listener_beacon_http.so /app/extenders/listener_beacon_http/
COPY --from=builder /app/Extenders/listener_beacon_http/_ui_listener.json /app/extenders/listener_beacon_http/

# Установка прав и возможностей
RUN chown -R adaptix:adaptix /app && \
    chmod +x /app/adaptixserver && \
    setcap 'cap_net_bind_service=+ep' /app/adaptixserver

# Установка переменных окружения
ENV TEAMSERVER_PORT=4321 \
    TEAMSERVER_ENDPOINT="/endpoint" \
    TEAMSERVER_PASSWORD="pass" \
    TEAMSERVER_CERT="server.rsa.crt" \
    TEAMSERVER_KEY="server.rsa.key" \
    TEAMSERVER_EXTENDER="extenders/extender.txt"

# Переключение на пользователя без прав root
USER adaptix

# Добавление скрипта запуска
RUN echo '#!/bin/sh' > /app/docker-entrypoint.sh && \
    echo 'set -e' >> /app/docker-entrypoint.sh && \
    echo '' >> /app/docker-entrypoint.sh && \
    echo '# Функция для проверки готовности сертификатов' >> /app/docker-entrypoint.sh && \
    echo 'wait_for_certs() {' >> /app/docker-entrypoint.sh && \
    echo '    echo "Ожидание генерации сертификатов..."' >> /app/docker-entrypoint.sh && \
    echo '    while [ ! -f "$TEAMSERVER_CERT" ] || [ ! -f "$TEAMSERVER_KEY" ] || [ ! -s "$TEAMSERVER_CERT" ] || [ ! -s "$TEAMSERVER_KEY" ]; do' >> /app/docker-entrypoint.sh && \
    echo '        sleep 1' >> /app/docker-entrypoint.sh && \
    echo '    done' >> /app/docker-entrypoint.sh && \
    echo '    echo "Сертификаты успешно сгенерированы"' >> /app/docker-entrypoint.sh && \
    echo '}' >> /app/docker-entrypoint.sh && \
    echo '' >> /app/docker-entrypoint.sh && \
    echo '# Генерация SSL сертификатов, если они отсутствуют' >> /app/docker-entrypoint.sh && \
    echo 'if [ ! -f "$TEAMSERVER_CERT" ] || [ ! -f "$TEAMSERVER_KEY" ]; then' >> /app/docker-entrypoint.sh && \
    echo '    echo "Генерация SSL сертификатов..."' >> /app/docker-entrypoint.sh && \
    echo '    openssl req -x509 -newkey rsa:4096 -keyout "$TEAMSERVER_KEY" -out "$TEAMSERVER_CERT" -days 365 -nodes -subj "/CN=localhost" &' >> /app/docker-entrypoint.sh && \
    echo '    wait_for_certs' >> /app/docker-entrypoint.sh && \
    echo 'fi' >> /app/docker-entrypoint.sh && \
    echo '' >> /app/docker-entrypoint.sh && \
    echo '# Проверка сертификатов' >> /app/docker-entrypoint.sh && \
    echo 'echo "Проверка сертификатов..."' >> /app/docker-entrypoint.sh && \
    echo 'openssl x509 -in "$TEAMSERVER_CERT" -text -noout || { echo "Ошибка в сертификате"; exit 1; }' >> /app/docker-entrypoint.sh && \
    echo 'openssl rsa -in "$TEAMSERVER_KEY" -check || { echo "Ошибка в приватном ключе"; exit 1; }' >> /app/docker-entrypoint.sh && \
    echo '' >> /app/docker-entrypoint.sh && \
    echo '# Генерация profile.json из переменных окружения' >> /app/docker-entrypoint.sh && \
    echo 'cat > /app/profile.json << EOF' >> /app/docker-entrypoint.sh && \
    echo '{' >> /app/docker-entrypoint.sh && \
    echo '  "Teamserver": {' >> /app/docker-entrypoint.sh && \
    echo '    "port": '$TEAMSERVER_PORT',' >> /app/docker-entrypoint.sh && \
    echo '    "endpoint": "'$TEAMSERVER_ENDPOINT'",' >> /app/docker-entrypoint.sh && \
    echo '    "password": "'$TEAMSERVER_PASSWORD'",' >> /app/docker-entrypoint.sh && \
    echo '    "cert": "'$TEAMSERVER_CERT'",' >> /app/docker-entrypoint.sh && \
    echo '    "key": "'$TEAMSERVER_KEY'",' >> /app/docker-entrypoint.sh && \
    echo '    "extender": "'$TEAMSERVER_EXTENDER'"' >> /app/docker-entrypoint.sh && \
    echo '  },' >> /app/docker-entrypoint.sh && \
    echo '  "ServerResponse": {' >> /app/docker-entrypoint.sh && \
    echo '    "status": 404,' >> /app/docker-entrypoint.sh && \
    echo '    "headers": {' >> /app/docker-entrypoint.sh && \
    echo '      "Content-Type": "text/html; charset=UTF-8",' >> /app/docker-entrypoint.sh && \
    echo '      "Server": "AdaptixC2",' >> /app/docker-entrypoint.sh && \
    echo '      "Adaptix Version": "v0.2-DEV"' >> /app/docker-entrypoint.sh && \
    echo '    },' >> /app/docker-entrypoint.sh && \
    echo '    "page": "404page.html"' >> /app/docker-entrypoint.sh && \
    echo '  }' >> /app/docker-entrypoint.sh && \
    echo '}' >> /app/docker-entrypoint.sh && \
    echo 'EOF' >> /app/docker-entrypoint.sh && \
    echo '' >> /app/docker-entrypoint.sh && \
    echo 'echo "Конфигурация и сертификаты готовы"' >> /app/docker-entrypoint.sh && \
    echo '' >> /app/docker-entrypoint.sh && \
    echo '# Запуск сервера' >> /app/docker-entrypoint.sh && \
    echo 'exec /app/adaptixserver -profile /app/profile.json' >> /app/docker-entrypoint.sh && \
    chmod +x /app/docker-entrypoint.sh

# Открытие необходимых портов
EXPOSE 443 80 4321

ENTRYPOINT ["/app/docker-entrypoint.sh"]
