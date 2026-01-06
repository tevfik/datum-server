# Time Synchronization Guide

Accurate time is critical for the "Rotating Auth" (SAS) security model. While NTP is the standard, network restrictions may block UDP port 123. This guide explains how to use the Datum Server as a time source.

## Stratum Strategy
1. **Primary**: NTP (pool.ntp.org) via UDP/123.
2. **Fallback**: Datum HTTP Time Sync via TCP/80-443.

## Datum Time Endpoint
- **URL**: `GET /system/time`
- **Response**:
  ```json
  {
    "unix": 1704528000,
    "unix_ms": 1704528000123,
    "iso8601": "2024-01-06T12:00:00.123Z",
    "timestamp": 1704528000123456789
  }
  ```

## Cristian's Algorithm (Implementation Logic)
To compensate for network latency (RTT), use Cristian's Algorithm:

```ascii
+---------+                                   +--------------+
|  ESP32  |                                   | Datum Server |
+----+----+                                   +-------+------+
     |                                                |
     | 1. Record T_start                              |
     +----------------------------------------------->|
     |      GET /system/time                          |
     |                                                | 2. Handle Request
     |                                                |
     |      200 OK {"unix_ms": T_server}              |
     |<-----------------------------------------------+
     |                                                |
     | 3. Record T_end                                |
     |                                                |
     | 4. Calculate:                                  |
     |    RTT = T_end - T_start                       |
     |    T_now = T_server + (RTT / 2)                |
     |                                                |
```

1. Record `T_start` (Device tick).
2. Request `GET /system/time`.
3. Receive `T_server` (from JSON) and record `T_end` (Device tick).
4. Calculate Round Trip Time: `RTT = T_end - T_start`.
5. Estimate Accurate Time: `T_now = T_server + (RTT / 2)`.

### Pseudo Code (C++)
```cpp
long t_start = millis();
long t_server = http.get("/system/time").json()["unix_ms"];
long t_end = millis();

long rtt = t_end - t_start;
long adjusted_time = t_server + (rtt / 2);
```

Using this method ensures your device clock is synchronized within +/- 50-100ms of the server, which is sufficient for token generation.
