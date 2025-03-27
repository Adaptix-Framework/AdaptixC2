FROM golang:1.23.7-bookworm AS builder

RUN apt-get update && apt-get upgrade -y && apt-get install -y mingw-w64 make libssl-dev qt6-base-dev qt6-websockets-dev sudo libcap2-bin build-essential checkinstall zlib1g-dev libssl-dev
# client requires cmake version 3.28+
RUN wget https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5.tar.gz && tar -zxvf cmake-3.31.5.tar.gz && cd cmake-3.31.5 && bash ./bootstrap && make && make install

WORKDIR /app

COPY . .

RUN make server
RUN make extenders
RUN make client


FROM scratch AS exporter

COPY --from=builder /app/dist .
