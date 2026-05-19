"""DatumClient — main entry point for the Datum platform REST API."""

from __future__ import annotations

from typing import Any, Dict, List, Optional
import requests

from .exceptions import DatumError
from .auth import AuthApi
from .devices import DevicesApi
from .data import DataApi


class DatumClient:
    """Thread-safe HTTP client for Datum IoT platform.

    Args:
        base_url: Root URL of your Datum server, e.g. ``https://datum.bezg.in``.
        token: Optional JWT bearer token (set automatically after :meth:`auth.login`).
        api_key: Optional application API key (alternative to user token).
        session: Optional ``requests.Session`` for custom TLS / proxy settings.
        timeout: Default request timeout in seconds (default: 10).

    Example::

        client = DatumClient("https://datum.bezg.in")
        client.auth.login(email="user@example.com", password="pass")
        print(client.devices.list())
    """

    def __init__(
        self,
        base_url: str,
        token: Optional[str] = None,
        api_key: Optional[str] = None,
        session: Optional[requests.Session] = None,
        timeout: int = 10,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.token: Optional[str] = token
        self.api_key: Optional[str] = api_key
        self.timeout = timeout
        self._session = session or requests.Session()

        self.auth = AuthApi(self)
        self.devices = DevicesApi(self)
        self.data = DataApi(self)

    # ── Internals ────────────────────────────────────────────────────────────

    def _headers(self, extra: Optional[Dict[str, str]] = None) -> Dict[str, str]:
        h = {"Content-Type": "application/json"}
        if self.token:
            h["Authorization"] = f"Bearer {self.token}"
        elif self.api_key:
            h["Authorization"] = f"Bearer {self.api_key}"
        if extra:
            h.update(extra)
        return h

    def request(
        self,
        method: str,
        path: str,
        *,
        body: Optional[Any] = None,
        params: Optional[Dict[str, Any]] = None,
    ) -> Any:
        """Perform an authenticated HTTP request and return the decoded JSON body.

        Raises:
            :class:`DatumError` on HTTP 4xx/5xx responses.
        """
        url = self.base_url + path
        resp = self._session.request(
            method,
            url,
            headers=self._headers(),
            json=body,
            params=params,
            timeout=self.timeout,
        )
        if resp.status_code >= 400:
            raise DatumError(resp.status_code, resp.text)
        if not resp.text:
            return None
        return resp.json()
