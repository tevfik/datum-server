# Build Stage
FROM golang:1.23-alpine AS builder

WORKDIR /app

# Copy go mod files
COPY go.mod go.sum ./
RUN go mod download && go mod verify

# Copy source code
COPY . .

# Build arguments
ARG VERSION=1.0.0
ARG BUILD_DATE
ARG DEFAULT_SERVER_URL=http://localhost:8000

# Build both server and datumctl binaries
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags "-X main.Version=${VERSION} -X main.BuildDate=${BUILD_DATE}" -o server ./cmd/server
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags "-X main.Version=${VERSION} -X main.DefaultServerURL=${DEFAULT_SERVER_URL}" -o datumctl ./cmd/datumctl

# Run Stage
FROM alpine:latest

WORKDIR /root/

# Install ca-certificates and wget for healthchecks
RUN apk --no-cache add ca-certificates wget

# Copy binaries from builder
COPY --from=builder /app/server .
COPY --from=builder /app/datumctl /usr/local/bin/datumctl

# Create data directory for database files
RUN mkdir -p /root/data

EXPOSE 8000

CMD ["./server"]
