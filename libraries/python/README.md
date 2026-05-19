# datum-iot — Python Client Library

Python client for the [Datum IoT Platform](https://github.com/tevfik/datum-server).

## Install

```bash
pip install datum-iot          # once published to PyPI
# or directly from source:
pip install ./libraries/python
```

## Quick Start

```python
from datum_iot import DatumClient

# User-token authentication
client = DatumClient("https://datum.bezg.in")
client.auth.login(email="user@example.com", password="secret")

# List devices
devices = client.devices.list()

# Push telemetry
client.data.push(devices[0]["id"], {"temperature": 22.5, "humidity": 60})

# Query history
from datetime import datetime, timedelta
history = client.data.query(
    devices[0]["id"],
    from_dt=datetime.utcnow() - timedelta(hours=1),
    limit=100,
)

# Send a command (e.g. toggle a relay)
client.devices.send_command(
    devices[0]["id"],
    "set_property",
    params={"key": "relay_0", "value": True},
)

# Device-only auth (firmware/edge scripts)
device_client = DatumClient("https://datum.bezg.in", api_key="<device_api_key>")
device_client.data.push_device_data("<device_id>", {"temp": 21.0})
```

## API Reference

| Module | Methods |
|--------|---------|
| `client.auth` | `login`, `register`, `logout`, `refresh`, `me`, `change_password`, `reset_password_request`, `list_api_keys`, `create_api_key`, `revoke_api_key` |
| `client.devices` | `list`, `get`, `create`, `delete`, `send_command`, `get_thing_description` |
| `client.data` | `push`, `push_device_data`, `query`, `get_current` |

## Error Handling

All API errors raise `datum_iot.DatumError(status, body)`:

```python
from datum_iot import DatumError

try:
    client.devices.get("unknown-id")
except DatumError as e:
    print(f"Error {e.status}: {e.body}")
```

## TLS Verification

By default `requests` verifies TLS certificates. To use a custom CA or
disable verification (not recommended for production):

```python
import requests
session = requests.Session()
session.verify = "/path/to/custom-ca.crt"  # or False to disable
client = DatumClient("https://datum.bezg.in", session=session)
```
