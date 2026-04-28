import { DatumError } from "./errors.js";

export interface DatumClientOptions {
  baseUrl: string;
  token?: string;
  apiKey?: string;
  fetch?: typeof globalThis.fetch;
}

export class DatumClient {
  baseUrl: string;
  token?: string;
  apiKey?: string;
  private _fetch: typeof globalThis.fetch;

  readonly auth: AuthApi;
  readonly devices: DevicesApi;
  readonly data: DataApi;
  readonly buckets: BucketsApi;
  readonly notify: NotifyApi;
  readonly realtime: RealtimeClient;

  constructor(opts: DatumClientOptions) {
    this.baseUrl = opts.baseUrl.replace(/\/+$/, "");
    this.token = opts.token;
    this.apiKey = opts.apiKey;
    this._fetch = opts.fetch ?? globalThis.fetch.bind(globalThis);

    this.auth = new AuthApi(this);
    this.devices = new DevicesApi(this);
    this.data = new DataApi(this);
    this.buckets = new BucketsApi(this);
    this.notify = new NotifyApi(this);
    this.realtime = new RealtimeClient(this);
  }

  headers(extra: Record<string, string> = {}): Record<string, string> {
    const h: Record<string, string> = { ...extra };
    if (this.token) h["Authorization"] = `Bearer ${this.token}`;
    else if (this.apiKey) h["Authorization"] = `Bearer ${this.apiKey}`;
    return h;
  }

  async request<T = unknown>(
    method: string,
    path: string,
    body?: unknown,
    query?: Record<string, string | number | undefined>,
  ): Promise<T> {
    const url = new URL(this.baseUrl + path);
    if (query) {
      for (const [k, v] of Object.entries(query)) {
        if (v !== undefined) url.searchParams.set(k, String(v));
      }
    }
    const init: RequestInit = {
      method,
      headers: this.headers(body !== undefined ? { "Content-Type": "application/json" } : {}),
    };
    if (body !== undefined) init.body = JSON.stringify(body);
    const res = await this._fetch(url, init);
    if (!res.ok) throw new DatumError(res.status, await res.text());
    if (res.status === 204) return undefined as T;
    const text = await res.text();
    return text ? (JSON.parse(text) as T) : (undefined as T);
  }

  fetchRaw(path: string, init?: RequestInit) {
    return this._fetch(this.baseUrl + path, {
      ...init,
      headers: { ...this.headers(), ...(init?.headers as Record<string, string> | undefined) },
    });
  }
}

// ---------------- modules ----------------

export interface LoginResponse {
  token: string;
  refresh_token?: string;
  user?: Record<string, unknown>;
}

class AuthApi {
  constructor(private c: DatumClient) {}
  async login(email: string, password: string): Promise<LoginResponse> {
    const res = await this.c.request<LoginResponse>("POST", "/auth/login", { email, password });
    this.c.token = res.token;
    return res;
  }
  async logout() {
    await this.c.request("POST", "/auth/logout");
    this.c.token = undefined;
  }
  me() {
    return this.c.request<Record<string, unknown>>("GET", "/auth/me");
  }
}

class DevicesApi {
  constructor(private c: DatumClient) {}
  list() {
    return this.c.request<Array<Record<string, unknown>>>("GET", "/dev");
  }
  get(id: string) {
    return this.c.request<Record<string, unknown>>("GET", `/dev/${encodeURIComponent(id)}`);
  }
  create(body: Record<string, unknown>) {
    return this.c.request<Record<string, unknown>>("POST", "/dev", body);
  }
  delete(id: string) {
    return this.c.request<void>("DELETE", `/dev/${encodeURIComponent(id)}`);
  }
  sendCommand(deviceId: string, action: string, params?: Record<string, unknown>) {
    return this.c.request<Record<string, unknown>>(
      "POST",
      `/api/v1/devices/${encodeURIComponent(deviceId)}/commands`,
      { action, params },
    );
  }
}

class DataApi {
  constructor(private c: DatumClient) {}
  push(deviceId: string, sample: Record<string, unknown>) {
    return this.c.request("POST", "/dev/data", { device_id: deviceId, ...sample });
  }
  query(deviceId: string, opts: { from?: Date; to?: Date; limit?: number } = {}) {
    return this.c.request<Array<Record<string, unknown>>>("GET", "/dev/data", undefined, {
      device_id: deviceId,
      from: opts.from?.toISOString(),
      to: opts.to?.toISOString(),
      limit: opts.limit,
    });
  }
}

class BucketsApi {
  constructor(private c: DatumClient) {}
  async list(): Promise<string[]> {
    const res = await this.c.request<{ buckets: string[] }>("GET", "/storage");
    return res.buckets ?? [];
  }
  create(bucket: string) {
    return this.c.request("POST", `/storage/${encodeURIComponent(bucket)}`);
  }
  async objects(bucket: string, opts: { prefix?: string; limit?: number } = {}) {
    const res = await this.c.request<{ objects: Array<Record<string, unknown>> }>(
      "GET",
      `/storage/${encodeURIComponent(bucket)}`,
      undefined,
      opts,
    );
    return res.objects ?? [];
  }
  async put(bucket: string, path: string, body: BodyInit, contentType = "application/octet-stream") {
    const res = await this.c.fetchRaw(
      `/storage/${encodeURIComponent(bucket)}/${path.split("/").map(encodeURIComponent).join("/")}`,
      { method: "PUT", body, headers: { "Content-Type": contentType } },
    );
    if (!res.ok) throw new DatumError(res.status, await res.text());
    return res.json() as Promise<Record<string, unknown>>;
  }
  presign(bucket: string, path: string, opts: { method?: string; expires_secs?: number } = {}) {
    return this.c.request<{ url: string; expires_secs: number }>(
      "POST",
      `/storage/${encodeURIComponent(bucket)}/presign`,
      { path, method: opts.method ?? "GET", expires_secs: opts.expires_secs ?? 900 },
    );
  }
  delete(bucket: string, path: string) {
    return this.c.request<void>(
      "DELETE",
      `/storage/${encodeURIComponent(bucket)}/${path.split("/").map(encodeURIComponent).join("/")}`,
    );
  }
}

class NotifyApi {
  constructor(private c: DatumClient) {}
  publish(topic: string, msg: { title?: string; body: string; priority?: number }) {
    return this.c.request("POST", `/notify/${encodeURIComponent(topic)}`, msg);
  }
}

/**
 * Subscription returned by RealtimeClient.subscribe(). Iterating with
 * `for await` is the canonical consumption pattern.
 */
export interface RealtimeSubscription extends AsyncIterable<unknown> {
  close(): void;
}

class RealtimeClient {
  constructor(private c: DatumClient) {}

  /** Subscribe to a topic over Server-Sent Events. */
  subscribe(topic: string): RealtimeSubscription {
    const ctrl = new AbortController();
    const c = this.c;

    async function* iterate(): AsyncGenerator<unknown> {
      const res = await c.fetchRaw(`/notify/${encodeURIComponent(topic)}/sse`, {
        signal: ctrl.signal,
        headers: { Accept: "text/event-stream" },
      });
      if (!res.ok || !res.body) {
        throw new DatumError(res.status, await res.text());
      }
      const reader = res.body.getReader();
      const decoder = new TextDecoder();
      let buf = "";
      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) return;
          buf += decoder.decode(value, { stream: true });
          for (;;) {
            const idx = buf.indexOf("\n");
            if (idx === -1) break;
            const line = buf.slice(0, idx).trim();
            buf = buf.slice(idx + 1);
            if (line.startsWith("data:")) {
              const body = line.slice(5).trim();
              if (!body) continue;
              try {
                yield JSON.parse(body);
              } catch {
                yield body;
              }
            }
          }
        }
      } finally {
        try {
          reader.releaseLock();
        } catch {
          /* noop */
        }
      }
    }

    return {
      [Symbol.asyncIterator]: iterate,
      close: () => ctrl.abort(),
    };
  }
}
