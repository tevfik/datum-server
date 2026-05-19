"""DatumError — base exception returned by all API methods on HTTP 4xx/5xx."""


class DatumError(Exception):
    """Raised when the server returns an HTTP error."""

    def __init__(self, status: int, body: str) -> None:
        self.status = status
        self.body = body
        super().__init__(f"HTTP {status}: {body}")
