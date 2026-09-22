FROM ubuntu:24.04 AS builder

RUN apt update && apt install -y \
    build-essential \
    libmysqlclient-dev \
    libjwt-dev \
    libsodium-dev \
    libjansson-dev \
    pkg-config

WORKDIR /app
COPY . .

RUN make clean && make all

FROM ubuntu:24.04

RUN apt update && apt install -y \
    libmysqlclient-dev \
    libjwt0 \
    libsodium23 \
    libjansson4

WORKDIR /app
COPY --from=builder /app/ctf-server .

EXPOSE 8080

CMD ["./ctf-server"]