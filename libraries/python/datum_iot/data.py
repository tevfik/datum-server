"""Data API — telemetry push and query endpoints."""

from __future__ import annotations

from datetime import datetime, timezone
from typing import TYPE_CHECKING, Any, Dict, List, Optional

if TYPE_CHECKING:
    from .client import DatumClient


class DataApi:
    def __init__(self, client: "DatumClient") -> None:
        self._c = client

    def push(self, device_id: str, sample: Dict[str, Any]) -> None:
        """POST /dev/data — push one telemetry sample.

        Args:
            device_id: Target device ID.
            sample: Key-value pairs, e.g. ``{"temperature": 22.5, "humidity": 60}``.
        """
        self._c.request("POST", "/dev/data", body={"device_id": device_id, **sample})

    def push_device_data(self, device_id: str, payload: Dict[str, Any]) -> None:
        """POST /data/{device_id} — push via device API key (no user auth needed).

        Use this from firmware or edge scripts that only know the device API key.
        Set ``client.api_key`` instead of ``client.token`` for this path.
        """
        self._c.request("POST", f"/data/{device_id}", body=payload)

    def query(
        self,
        device_id: str,
        *,
        from_dt: Optional[datetime] = None,
        to_dt: Optional[datetime] = None,
        limit: Optional[int] = None,
    ) -> List[Dict[str, Any]]:
        """GET /dev/data — query historical telemetry.

        Args:
            device_id: Device to query.
            from_dt: Start of time range (UTC datetime).
            to_dt: End of time range (UTC datetime).
            limit: Maximum number of records.
        """
        params: Dict[str, Any] = {"device_id": device_id}
        if from_dt:
            params["from"] = _iso(from_dt)
        if to_dt:
            params["to"] = _iso(to_dt)
        if limit is not None:
            params["limit"] = limit
        return self._c.request("GET", "/dev/data", params=params) or []

    def get_current(self, device_id: str) -> Dict[str, Any]:
        """GET /dev/{id}/data — current device state / latest telemetry."""
        return self._c.request("GET", f"/dev/{device_id}/data") or {}


def _iso(dt: datetime) -> str:
    """Format datetime as ISO-8601 UTC string."""
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
