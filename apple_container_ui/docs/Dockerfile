# Multi-stage build for Ping-Pong server
FROM golang:1.22-alpine as builder
WORKDIR /app
COPY main.go .
RUN go build -o ping-pong main.go

FROM alpine:latest
WORKDIR /app
COPY --from=builder /app/ping-pong /usr/local/bin/ping-pong
EXPOSE 8080 8081
ENTRYPOINT ["/usr/local/bin/ping-pong"]
