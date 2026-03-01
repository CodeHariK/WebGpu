# Simple Dockerfile using locally built binary
FROM alpine:latest

WORKDIR /app

# Copy the locally built Linux/ARM64 binary
COPY hello /usr/local/bin/hello

# Expose the application port
EXPOSE 8080

# Run the application
ENTRYPOINT ["/usr/local/bin/hello"]

