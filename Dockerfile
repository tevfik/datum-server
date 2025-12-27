# Build Stage
FROM golang:1.24-alpine AS builder

WORKDIR /app

# Copy go mod files
COPY go.mod go.sum ./
RUN go mod download

# Copy source code
COPY . .

# Build binary
RUN CGO_ENABLED=0 GOOS=linux go build -o server ./cmd/server

# Run Stage
FROM alpine:latest

WORKDIR /root/

# Install wget for healthchecks
RUN apk --no-cache add wget

# Copy binary from builder
COPY --from=builder /app/server .

EXPOSE 8000

CMD ["./server"]
