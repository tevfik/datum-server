"""datum-iot — Python client for the Datum IoT Platform.

Usage::

    from datum_iot import DatumClient

    client = DatumClient("https://datum.bezg.in")
    client.auth.login(email="user@example.com", password="secret")

    devices = client.devices.list()
    client.data.push(devices[0]["id"], {"temperature": 22.5})
"""

from .client import DatumClient
from .exceptions import DatumError

__all__ = ["DatumClient", "DatumError"]
__version__ = "1.3.0"
