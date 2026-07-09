FROM ubuntu:24.04

RUN apt-get update && apt-get install -y g++ cmake make libssl-dev

WORKDIR /app
COPY . .

RUN cmake -S . -B build && cmake --build build

EXPOSE 8080
CMD ["./build/http_server"]
