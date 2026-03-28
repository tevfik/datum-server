package auth

import (
	"net/http"
	"os"
	"strconv"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

// RateLimiter implements a sharded token bucket rate limiter.
// Sharding reduces lock contention under high concurrency.
const shardCount = 32

type RateLimiter struct {
	shards [shardCount]*rateLimiterShard
	rate   int
	window time.Duration
}

type rateLimiterShard struct {
	visitors map[string]*visitor
	mu       sync.RWMutex
}

type visitor struct {
	limiter  *tokenBucket
	lastSeen time.Time
}

type tokenBucket struct {
	tokens    int
	maxTokens int
	refillAt  time.Time
	mu        sync.Mutex
}

func newTokenBucket(maxTokens int, window time.Duration) *tokenBucket {
	return &tokenBucket{
		tokens:    maxTokens,
		maxTokens: maxTokens,
		refillAt:  time.Now().Add(window),
	}
}

func (tb *tokenBucket) allow() bool {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	now := time.Now()
	if now.After(tb.refillAt) {
		tb.tokens = tb.maxTokens
		tb.refillAt = now.Add(time.Minute)
	}

	if tb.tokens > 0 {
		tb.tokens--
		return true
	}

	return false
}

// NewRateLimiter creates a new rate limiter with configuration from environment
func NewRateLimiter() *RateLimiter {
	rate := 100                // default: 100 requests
	window := 60 * time.Second // default: per minute

	if envRate := os.Getenv("RATE_LIMIT_REQUESTS"); envRate != "" {
		if r, err := strconv.Atoi(envRate); err == nil {
			rate = r
		}
	}

	if envWindow := os.Getenv("RATE_LIMIT_WINDOW_SECONDS"); envWindow != "" {
		if w, err := strconv.Atoi(envWindow); err == nil {
			window = time.Duration(w) * time.Second
		}
	}

	rl := &RateLimiter{
		rate:   rate,
		window: window,
	}
	for i := 0; i < shardCount; i++ {
		rl.shards[i] = &rateLimiterShard{
			visitors: make(map[string]*visitor),
		}
	}

	// Cleanup old visitors every 5 minutes
	go rl.cleanupLoop()

	return rl
}

// getShard returns the shard for a given IP using FNV-like hash.
func (rl *RateLimiter) getShard(ip string) *rateLimiterShard {
	var hash uint32
	for i := 0; i < len(ip); i++ {
		hash = hash*31 + uint32(ip[i])
	}
	return rl.shards[hash%shardCount]
}

func (rl *RateLimiter) getVisitor(ip string) *visitor {
	shard := rl.getShard(ip)
	shard.mu.Lock()
	defer shard.mu.Unlock()

	v, exists := shard.visitors[ip]
	if !exists {
		v = &visitor{
			limiter:  newTokenBucket(rl.rate, rl.window),
			lastSeen: time.Now(),
		}
		shard.visitors[ip] = v
	}

	v.lastSeen = time.Now()
	return v
}

func (rl *RateLimiter) cleanupLoop() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for range ticker.C {
		rl.cleanup()
	}
}

func (rl *RateLimiter) cleanup() {
	threshold := time.Now().Add(-10 * time.Minute)
	for i := 0; i < shardCount; i++ {
		shard := rl.shards[i]
		shard.mu.Lock()
		for ip, v := range shard.visitors {
			if v.lastSeen.Before(threshold) {
				delete(shard.visitors, ip)
			}
		}
		shard.mu.Unlock()
	}
}

// RateLimitMiddleware returns a Gin middleware for rate limiting
func RateLimitMiddleware() gin.HandlerFunc {
	limiter := NewRateLimiter()

	return func(c *gin.Context) {
		ip := c.ClientIP()
		visitor := limiter.getVisitor(ip)

		if !visitor.limiter.allow() {
			c.Header("X-RateLimit-Limit", strconv.Itoa(limiter.rate))
			c.Header("X-RateLimit-Remaining", "0")
			c.Header("Retry-After", "60")
			c.JSON(http.StatusTooManyRequests, gin.H{
				"error": "Rate limit exceeded. Please try again later.",
			})
			c.Abort()
			return
		}

		c.Header("X-RateLimit-Limit", strconv.Itoa(limiter.rate))
		c.Next()
	}
}
