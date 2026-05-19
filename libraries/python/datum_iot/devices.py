"""Devices API — /dev/* endpoints."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any, Dict, List, Optional

if TYPE_CHECKING:
    from .client import DatumClient


class DevicesApi:
    def __init__(self, client: "DatumClient") -> None:
        self._c = client

    def list(self) -> List[Dict[str, Any]]:
        """GET /dev → list of device objects for the authenticated user."""
        return self._c.request("GET", "/dev") or []

    def get(self, device_id: str) -> Dict[str, Any]:
        """GET /dev/{id}."""
        return self._c.request("GET", f"/dev/{device_id}")

    def create(
        self,
        *,
        name: str,
        device_type: str,
        uid: Optional[str] = None,
        **extra: Any,
    ) -> Dict[str, Any]:
        """POST /dev → creates a new device, returns {id, api_key, ...}."""
        body: Dict[str, Any] = {"name": name, "type": device_type, **extra}
        if uid:
            body["uid"] = uid
        return self._c.request("POST", "/dev", body=body)

    def delete(self, device_id: str) -> None:
        """DELETE /dev/{id}."""
        self._c.request("DELETE", f"/dev/{device_id}")

    def send_command(
        self,
        device_id: str,
        action: str,
        *,
        params: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """POST /api/v1/devices/{id}/commands → queues a command on the server.

        Args:
            device_id: Target device ID.
            action: Command name, e.g. ``"set_property"`` or ``"update_firmware"``.
            params: Optional key-value parameters for the command.
        """
        body: Dict[str, Any] = {"action": action}
        if params:
            body["params"] = params
        return self._c.request("POST", f"/api/v1/devices/{device_id}/commands", body=body)

    def get_thing_description(self, device_id: str) -> Dict[str, Any]:
        """GET /dev/{id}/thing-description → W3C WoT Thing Description."""
        return self._c.request("GET", f"/dev/{device_id}/thing-description")
