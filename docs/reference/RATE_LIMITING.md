# Rate Limiting Configuration Guide

## Problem: High Failure Rate in Load Tests

When running load tests, you may encounter **429 Too Many Requests** errors. This is caused by the rate limiting middleware protecting the API from abuse.

## Current Configuration

**Default Settings** (in [ratelimit.go](../backend/go/internal/auth/ratelimit.go)):
```go
rate := 100                // 100 requests per minute
window := 60 * time.Second // per minute window
```

**Rate Limiting Strategy:**
- Token bucket algorithm
- Per IP address (not per device)
- Automatic cleanup of inactive clients
- Minute-based token refill

## Why This Causes Test Failures

### Load Test Scenario
```
Concurrent Users: 100
Test Duration: 30 seconds
Requests per User: ~2-3
Total Requests: ~250
Source IP: Single (localhost)
```

### Result
```
Rate Limit: 100 requests/minute from same IP
Actual Requests: 250 requests in 30 seconds
Blocked: 150+ requests (60% failure rate)
```

## Solution: Adjust Rate Limits

### Option 1: Environment Variables (Recommended)

**For Development:**
```bash
# .env or docker-compose.yml
RATE_LIMIT_REQUESTS=1000
RATE_LIMIT_WINDOW_SECONDS=60
```

**For Production:**
```bash
# Adjust based on expected load
RATE_LIMIT_REQUESTS=5000    # 5000 requests per minute
RATE_LIMIT_WINDOW_SECONDS=60
```

**Apply Changes:**
```bash
# Update docker-compose.yml
docker compose down
docker compose up -d

# Verify new limits
curl -I http://localhost:8007/health
# Look for: X-RateLimit-Limit: 5000
```

### Option 2: Per-Device Rate Limiting

Modify `backend/go/internal/auth/ratelimit.go`:

```go
// Instead of IP-based limiting, use device_id or api_key
func (rl *RateLimiter) getVisitor(identifier string) *visitor {
    rl.mu.Lock()
    defer rl.mu.Unlock()

    v, exists := rl.visitors[identifier]
    if !exists {
        v = &visitor{
            limiter:  newTokenBucket(rl.rate, rl.window),
            lastSeen: time.Now(),
        }
        rl.visitors[identifier] = v
    }
    
    v.lastSeen = time.Now()
    return v
}

// Middleware extracts device ID instead of IP
func RateLimitMiddleware() gin.HandlerFunc {
    limiter := NewRateLimiter()

    return func(c *gin.Context) {
        // Try to get device ID from various sources
        identifier := getDeviceIdentifier(c)
        
        // Fallback to IP for unauthenticated requests
        if identifier == "" {
            identifier = c.ClientIP()
        }

        visitor := limiter.getVisitor(identifier)
        
        if !visitor.limiter.allow() {
            c.JSON(http.StatusTooManyRequests, gin.H{
                "error": "Rate limit exceeded",
                "retry_after": visitor.limiter.refillAt.Sub(time.Now()).Seconds(),
            })
            c.Abort()
            return
        }

        c.Next()
    }
}

func getDeviceIdentifier(c *gin.Context) string {
    // 1. Try device_id from URL path
    if deviceID := c.Param("device_id"); deviceID != "" {
        return "device:" + deviceID
    }
    
    // 2. Try API key from Authorization header
    if apiKey := c.GetHeader("Authorization"); apiKey != "" {
        return "key:" + apiKey
    }
    
    // 3. Try user from context (after auth middleware)
    if userID, exists := c.Get("user_id"); exists {
        return "user:" + userID.(string)
    }
    
    return ""
}
```

**Benefits:**
- ✅ Each device has its own rate limit
- ✅ Multiple devices from same network work fine
- ✅ More granular control
- ✅ Better for IoT scenarios

### Option 3: Tiered Rate Limiting

Create different limits based on authentication level:

```go
type RateLimitTier struct {
    Name            string
    RequestsPerMin  int
    BurstSize       int
}

var (
    PublicTier = RateLimitTier{
        Name:           "public",
        RequestsPerMin: 100,
        BurstSize:      20,
    }
    
    AuthenticatedTier = RateLimitTier{
        Name:           "authenticated",
        RequestsPerMin: 1000,
        BurstSize:      100,
    }
    
    AdminTier = RateLimitTier{
        Name:           "admin",
        RequestsPerMin: 10000,
        BurstSize:      1000,
    }
)

func getRateLimitTier(c *gin.Context) RateLimitTier {
    // Check if admin
    if role, exists := c.Get("role"); exists && role == "admin" {
        return AdminTier
    }
    
    // Check if authenticated
    if _, exists := c.Get("user_id"); exists {
        return AuthenticatedTier
    }
    
    // Default to public
    return PublicTier
}
```

## Recommended Settings by Scale

### Development Environment
```bash
RATE_LIMIT_REQUESTS=10000   # Effectively unlimited
RATE_LIMIT_WINDOW_SECONDS=60
```

### Small Deployment (< 100 devices)
```bash
RATE_LIMIT_REQUESTS=500     # Per IP or device
RATE_LIMIT_WINDOW_SECONDS=60
```

### Medium Deployment (100-1000 devices)
```bash
RATE_LIMIT_REQUESTS=2000
RATE_LIMIT_WINDOW_SECONDS=60
```

### Large Deployment (1000+ devices)
```bash
RATE_LIMIT_REQUESTS=10000
RATE_LIMIT_WINDOW_SECONDS=60

# Or use Redis-based distributed rate limiting
RATE_LIMIT_BACKEND=redis
REDIS_URL=redis://localhost:6379
```

## Testing Rate Limits

### Check Current Limits
```bash
# Make request and check headers
curl -I http://localhost:8007/health

# Response headers:
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 99
X-RateLimit-Reset: 1640000000
```

### Test Rate Limit Hit
```bash
# Rapid requests to trigger limit
for i in {1..150}; do
  curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8007/health
done

# First 100 should return 200
# Next 50 should return 429
```

### Load Test with Adjusted Limits
```bash
# After increasing rate limits
docker compose down
docker compose up -d

# Wait for services
sleep 10

# Run load test again
locust -f tests/enhanced_load_test.py \
  --host=http://localhost:8007 \
  --users 100 \
  --spawn-rate 10 \
  --run-time 30s \
  --headless

# Expected: <5% failure rate
```

## Monitoring Rate Limits

### Add Prometheus Metrics

```go
var (
    rateLimitHits = prometheus.NewCounterVec(
        prometheus.CounterOpts{
            Name: "rate_limit_hits_total",
            Help: "Total rate limit hits",
        },
        []string{"identifier"},
    )
    
    rateLimitRejects = prometheus.NewCounterVec(
        prometheus.CounterOpts{
            Name: "rate_limit_rejects_total",
            Help: "Total rate limit rejections",
        },
        []string{"identifier"},
    )
)
```

### Dashboard Alerts

```yaml
# Grafana alert
- name: high_rate_limit_rejects
  condition: rate(rate_limit_rejects_total[5m]) > 100
  severity: warning
  message: "High rate limit rejection rate detected"
```

## Advanced: Distributed Rate Limiting with Redis

For multi-instance deployments:

```go
import "github.com/go-redis/redis/v8"

type RedisRateLimiter struct {
    client *redis.Client
    rate   int
    window time.Duration
}

func (r *RedisRateLimiter) Allow(identifier string) bool {
    key := "ratelimit:" + identifier
    
    // Use Redis INCR with TTL
    count, err := r.client.Incr(ctx, key).Result()
    if err != nil {
        return false
    }
    
    if count == 1 {
        r.client.Expire(ctx, key, r.window)
    }
    
    return count <= int64(r.rate)
}
```

**Benefits:**
- ✅ Works across multiple API instances
- ✅ Centralized rate limiting
- ✅ More accurate limits
- ✅ Better for horizontal scaling

## Troubleshooting

### Issue: Still getting 429 errors after increasing limits

**Check:**
1. Did you restart the services?
   ```bash
   docker compose restart
   ```

2. Are environment variables loaded?
   ```bash
   docker compose exec api env | grep RATE_LIMIT
   ```

3. Check application logs:
   ```bash
   docker compose logs api | grep -i rate
   ```

### Issue: Rate limits too permissive (security concern)

**Solution:**
1. Implement tiered limits (see Option 3)
2. Add IP-based global limit + device-based limit
3. Enable Redis-based distributed limiting
4. Add alerting for suspicious patterns

### Issue: Different devices sharing same IP

**Solution:**
Use per-device rate limiting (see Option 2) instead of per-IP

## Best Practices

1. ✅ **Use device-based limiting** for IoT scenarios
2. ✅ **Set different limits** for different auth levels
3. ✅ **Monitor rate limit hits** with metrics
4. ✅ **Return clear error messages** with retry-after
5. ✅ **Document limits** in API documentation
6. ✅ **Use Redis** for distributed systems
7. ✅ **Test under load** before production
8. ✅ **Have fallback limits** if Redis fails

## Quick Fix for Load Testing

**Immediate solution for testing:**

```bash
# Edit docker-compose.yml
services:
  api:
    environment:
      - RATE_LIMIT_REQUESTS=10000
      - RATE_LIMIT_WINDOW_SECONDS=60

# Restart
docker compose restart api

# Test again
locust -f tests/enhanced_load_test.py --headless \
  --users 100 --spawn-rate 10 --run-time 30s
```

**Expected result:**
- Success rate: >95%
- Average response time: <50ms
- No 429 errors

## Summary

| Configuration | Use Case | Limit | Devices Supported |
|---------------|----------|-------|-------------------|
| Default | Demo/Testing | 100/min | ~10 |
| Development | Local dev | 10000/min | Unlimited |
| Small | < 100 devices | 500/min | 100 |
| Medium | 100-1000 devices | 2000/min | 1000 |
| Large | 1000+ devices | 10000/min | 5000+ |
| Enterprise | High scale | Redis-based | 100,000+ |

**Recommended for production:** Per-device limiting with tiered rates and Redis backend.

## References

- [Rate Limiting Implementation](../backend/go/internal/auth/ratelimit.go)
- [Load Test Analysis](./LOAD_TEST_ANALYSIS.md)
- [API Documentation](./API.md)
- [Recommendations](./RECOMMENDATIONS.md)
