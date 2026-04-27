package ratelimit

import (
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

// entry tracks request counts for a single client.
type entry struct {
	count    int
	windowAt time.Time
}

// Limiter is an in-memory token-bucket style rate limiter.
type Limiter struct {
	mu      sync.Mutex
	clients map[string]*entry
	limit   int
	window  time.Duration
	lastGC  time.Time
	gcEvery time.Duration
}

// New creates a rate limiter allowing `limit` requests per `window`.
func New(limit int, window time.Duration) *Limiter {
	return &Limiter{
		clients: make(map[string]*entry),
		limit:   limit,
		window:  window,
		lastGC:  time.Now(),
		gcEvery: 5 * time.Minute,
	}
}

// Allow reports whether the client identified by key is within limits.
func (l *Limiter) Allow(key string) bool {
	l.mu.Lock()
	defer l.mu.Unlock()

	now := time.Now()

	// Lazy GC: sweep expired entries occasionally.
	if now.Sub(l.lastGC) > l.gcEvery {
		for k, e := range l.clients {
			if now.Sub(e.windowAt) > l.window {
				delete(l.clients, k)
			}
		}
		l.lastGC = now
	}

	e, ok := l.clients[key]
	if !ok || now.Sub(e.windowAt) > l.window {
		l.clients[key] = &entry{count: 1, windowAt: now}
		return true
	}

	e.count++
	return e.count <= l.limit
}

// Middleware returns a Gin middleware that rate-limits by client IP.
func Middleware(limit int, window time.Duration) gin.HandlerFunc {
	lim := New(limit, window)
	return func(c *gin.Context) {
		key := c.ClientIP()
		if !lim.Allow(key) {
			c.AbortWithStatusJSON(http.StatusTooManyRequests, gin.H{
				"error": "rate limit exceeded",
			})
			return
		}
		c.Next()
	}
}
