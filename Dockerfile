FROM ubuntu:24.04 AS builder

RUN apt update && apt install -y \
    build-essential \
    libmysqlclient-dev

WORKDIR /app
COPY . .

RUN make clean && make all

FROM ubuntu:24.04

RUN apt update && apt install -y \
    libmysqlclient-dev

WORKDIR /app
COPY --from=builder /app/ctf-server .

EXPOSE 8080

CMD ["./ctf-server"]