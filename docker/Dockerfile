# Build Stage
FROM golang:1.23-alpine AS builder

WORKDIR /app

# Copy go mod files
COPY go.mod ./
COPY go.sum* ./
RUN go mod download && go mod tidy

# Copy source code
COPY . .

# Build both server and datumctl binaries
RUN CGO_ENABLED=0 GOOS=linux go build -o server ./cmd/server
RUN CGO_ENABLED=0 GOOS=linux go build -o datumctl ./cmd/datumctl

# Run Stage
FROM alpine:latest

WORKDIR /root/

# Install ca-certificates and wget for healthchecks
RUN apk --no-cache add ca-certificates wget

# Copy binaries from builder
COPY --from=builder /app/server .
COPY --from=builder /app/datumctl /usr/local/bin/datumctl

EXPOSE 8000

CMD ["./server"]
