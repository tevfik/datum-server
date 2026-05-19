"""Auth API — /auth/* endpoints."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any, Dict, Optional

if TYPE_CHECKING:
    from .client import DatumClient


class AuthApi:
    def __init__(self, client: "DatumClient") -> None:
        self._c = client

    # ── Identity ──────────────────────────────────────────────────────────

    def login(self, *, email: str, password: str) -> Dict[str, Any]:
        """POST /auth/login → stores token and returns full response dict."""
        res = self._c.request("POST", "/auth/login", body={"email": email, "password": password})
        self._c.token = res.get("token")
        return res

    def register(self, *, email: str, password: str, name: Optional[str] = None) -> Dict[str, Any]:
        """POST /auth/register."""
        body: Dict[str, Any] = {"email": email, "password": password}
        if name:
            body["name"] = name
        res = self._c.request("POST", "/auth/register", body=body)
        self._c.token = res.get("token")
        return res

    def refresh(self, refresh_token: str) -> Dict[str, Any]:
        """POST /auth/refresh → updates stored token."""
        res = self._c.request("POST", "/auth/refresh", body={"refresh_token": refresh_token})
        if res.get("token"):
            self._c.token = res["token"]
        return res

    def logout(self) -> None:
        """POST /auth/logout — clears local token regardless of server error."""
        try:
            self._c.request("POST", "/auth/logout")
        except Exception:
            pass
        self._c.token = None

    def me(self) -> Dict[str, Any]:
        """GET /auth/me → current user profile."""
        return self._c.request("GET", "/auth/me")

    def change_password(self, *, old_password: str, new_password: str) -> None:
        """POST /auth/password/change."""
        self._c.request(
            "POST",
            "/auth/password/change",
            body={"old_password": old_password, "new_password": new_password},
        )

    def reset_password_request(self, email: str) -> None:
        """POST /auth/password/reset — sends a reset email."""
        self._c.request("POST", "/auth/password/reset", body={"email": email})

    # ── API Keys ─────────────────────────────────────────────────────────

    def list_api_keys(self) -> list:
        """GET /auth/keys → list of application keys."""
        return self._c.request("GET", "/auth/keys") or []

    def create_api_key(self, name: str) -> Dict[str, Any]:
        """POST /auth/keys → {key, id, name} — key is shown ONCE."""
        return self._c.request("POST", "/auth/keys", body={"name": name})

    def revoke_api_key(self, key_id: str) -> None:
        """DELETE /auth/keys/{id}."""
        self._c.request("DELETE", f"/auth/keys/{key_id}")
