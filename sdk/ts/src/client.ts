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
  readonly sys: SysApi;
  readonly admin: AdminApi;
  readonly rules: RulesApi;

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
    this.sys = new SysApi(this);
    this.admin = new AdminApi(this);
    this.rules = new RulesApi(this);
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
  user_id?: string;
  user?: Record<string, unknown>;
}

export interface Session {
  jti: string;
  user_id: string;
  created_at: string;
  expires_at: string;
  user_agent: string;
  ip: string;
}

class AuthApi {
  constructor(private c: DatumClient) {}

  // ── Identity ────────────────────────────────────────────────────────────
  async login(email: string, password: string): Promise<LoginResponse> {
    const res = await this.c.request<LoginResponse>("POST", "/auth/login", { email, password });
    this.c.token = res.token;
    return res;
  }
  async register(email: string, password: string, name?: string): Promise<LoginResponse> {
    const res = await this.c.request<LoginResponse>("POST", "/auth/register", {
      email,
      password,
      ...(name !== undefined ? { name } : {}),
    });
    this.c.token = res.token;
    return res;
  }
  async refresh(refreshToken: string): Promise<LoginResponse> {
    const res = await this.c.request<LoginResponse>("POST", "/auth/refresh", {
      refresh_token: refreshToken,
    });
    if (res.token) this.c.token = res.token;
    return res;
  }
  /** Best-effort server logout + local token clear. */
  async logout(): Promise<void> {
    try {
      await this.c.request("POST", "/auth/logout");
    } catch {
      // network/401 — clear locally anyway
    }
    this.c.token = undefined;
  }
  me() {
    return this.c.request<Record<string, unknown>>("GET", "/auth/me");
  }
  updateProfile(displayName: string) {
    return this.c.request<Record<string, unknown>>("PUT", "/auth/me", {
      display_name: displayName,
    });
  }

  // ── Password ────────────────────────────────────────────────────────────
  changePassword(oldPassword: string, newPassword: string) {
    return this.c.request<void>("PUT", "/auth/password", {
      old_password: oldPassword,
      new_password: newPassword,
    });
  }
  forgotPassword(email: string) {
    return this.c.request<void>("POST", "/auth/forgot-password", { email });
  }
  resetPassword(token: string, newPassword: string) {
    return this.c.request<void>("POST", "/auth/reset-password", {
      token,
      new_password: newPassword,
    });
  }

  // ── Sessions ────────────────────────────────────────────────────────────
  async sessions(): Promise<Session[]> {
    const res = await this.c.request<{ sessions?: Session[] }>("GET", "/auth/sessions");
    return res.sessions ?? [];
  }
  revokeSession(jti: string) {
    return this.c.request<void>("DELETE", `/auth/sessions/${encodeURIComponent(jti)}`);
  }

  // ── API keys ────────────────────────────────────────────────────────────
  async keys(): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<{ keys?: Array<Record<string, unknown>> }>(
      "GET",
      "/auth/keys",
    );
    return res.keys ?? [];
  }
  createKey(name: string) {
    return this.c.request<Record<string, unknown>>("POST", "/auth/keys", { name });
  }
  deleteKey(id: string) {
    return this.c.request<void>("DELETE", `/auth/keys/${encodeURIComponent(id)}`);
  }

  // ── Push tokens ─────────────────────────────────────────────────────────
  async pushTokens(): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<{ tokens?: Array<Record<string, unknown>> }>(
      "GET",
      "/auth/push-tokens",
    );
    return res.tokens ?? [];
  }
  registerPushToken(platform: string, token: string) {
    return this.c.request<Record<string, unknown>>("POST", "/auth/push-token", {
      platform,
      token,
    });
  }
  deletePushToken(id: string) {
    return this.c.request<void>("DELETE", `/auth/push-token/${encodeURIComponent(id)}`);
  }

  // ── Account lifecycle ───────────────────────────────────────────────────
  async deleteAccount(): Promise<void> {
    try {
      await this.c.request("DELETE", "/auth/user");
    } finally {
      this.c.token = undefined;
    }
  }

  // ── OAuth ───────────────────────────────────────────────────────────────
  async oauthProviders(): Promise<string[]> {
    for (const path of ["/auth/providers", "/auth/oauth/providers"]) {
      try {
        const res = await this.c.request<unknown>("GET", path);
        if (Array.isArray(res)) return res.map(String);
        if (res && typeof res === "object" && Array.isArray((res as { providers?: unknown }).providers)) {
          return ((res as { providers: unknown[] }).providers).map(String);
        }
      } catch {
        continue;
      }
    }
    return [];
  }
}

// ── /sys/* ──────────────────────────────────────────────────────────────────

class SysApi {
  constructor(private c: DatumClient) {}
  health() {
    return this.c.request<Record<string, unknown>>("GET", "/health");
  }
  info() {
    return this.c.request<Record<string, unknown>>("GET", "/sys/info");
  }
  time() {
    return this.c.request<Record<string, unknown>>("GET", "/sys/time");
  }
  async ip(): Promise<string> {
    const res = await this.c.request<unknown>("GET", "/sys/ip");
    if (res && typeof res === "object" && typeof (res as { ip?: unknown }).ip === "string") {
      return (res as { ip: string }).ip;
    }
    return String(res ?? "");
  }
  status() {
    return this.c.request<Record<string, unknown>>("GET", "/sys/status");
  }
  metrics() {
    return this.c.request<Record<string, unknown>>("GET", "/sys/metrics");
  }
}

// ── /admin/* ───────────────────────────────────────────────────────────────

class AdminApi {
  readonly sys: AdminSysApi;
  readonly users: AdminUsersApi;
  readonly devices: AdminDevicesApi;
  readonly database: AdminDatabaseApi;
  readonly mqtt: AdminMqttApi;
  constructor(c: DatumClient) {
    this.sys = new AdminSysApi(c);
    this.users = new AdminUsersApi(c);
    this.devices = new AdminDevicesApi(c);
    this.database = new AdminDatabaseApi(c);
    this.mqtt = new AdminMqttApi(c);
  }
}

class AdminSysApi {
  constructor(private c: DatumClient) {}
  config() {
    return this.c.request<Record<string, unknown>>("GET", "/admin/sys/config");
  }
  setRegistration(allowRegister: boolean) {
    return this.c.request<void>("PUT", "/admin/sys/registration", {
      allow_register: allowRegister,
    });
  }
  setRetention(days: number, checkIntervalHours?: number) {
    return this.c.request<void>("PUT", "/admin/sys/retention", {
      days,
      ...(checkIntervalHours !== undefined ? { check_interval_hours: checkIntervalHours } : {}),
    });
  }
  setRateLimit(maxRequests: number, windowSeconds: number) {
    return this.c.request<void>("PUT", "/admin/sys/rate-limit", {
      max_requests: maxRequests,
      window_seconds: windowSeconds,
    });
  }
  setAlerts(emailEnabled: boolean, diskThreshold: number, memoryThreshold: number) {
    return this.c.request<void>("PUT", "/admin/sys/alerts", {
      email_enabled: emailEnabled,
      disk_threshold: diskThreshold,
      memory_threshold: memoryThreshold,
    });
  }
  async logs(opts: { level?: string; search?: string } = {}): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<{ logs?: Array<Record<string, unknown>> }>(
      "GET",
      "/admin/sys/logs",
      undefined,
      opts,
    );
    return res.logs ?? [];
  }
  clearLogs() {
    return this.c.request<void>("DELETE", "/admin/sys/logs");
  }
}

class AdminUsersApi {
  constructor(private c: DatumClient) {}
  async list(): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<unknown>("GET", "/admin/users");
    if (Array.isArray(res)) return res as Array<Record<string, unknown>>;
    if (res && typeof res === "object" && Array.isArray((res as { users?: unknown }).users)) {
      return (res as { users: Array<Record<string, unknown>> }).users;
    }
    return [];
  }
  get(userId: string) {
    return this.c.request<Record<string, unknown>>("GET", `/admin/users/${encodeURIComponent(userId)}`);
  }
  setStatus(userId: string, status: "active" | "suspended") {
    return this.c.request<void>("PUT", `/admin/users/${encodeURIComponent(userId)}`, { status });
  }
  delete(userId: string) {
    return this.c.request<void>("DELETE", `/admin/users/${encodeURIComponent(userId)}`);
  }
  resetPassword(username: string) {
    return this.c.request<Record<string, unknown>>(
      "POST",
      `/admin/users/${encodeURIComponent(username)}/reset-password`,
    );
  }
}

class AdminDevicesApi {
  constructor(private c: DatumClient) {}
  async list(): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<unknown>("GET", "/admin/dev");
    if (Array.isArray(res)) return res as Array<Record<string, unknown>>;
    if (res && typeof res === "object" && Array.isArray((res as { devices?: unknown }).devices)) {
      return (res as { devices: Array<Record<string, unknown>> }).devices;
    }
    return [];
  }
  get(deviceId: string) {
    return this.c.request<Record<string, unknown>>("GET", `/admin/dev/${encodeURIComponent(deviceId)}`);
  }
  provision(spec: Record<string, unknown>) {
    return this.c.request<Record<string, unknown>>("POST", "/admin/dev", spec);
  }
  update(deviceId: string, patch: Record<string, unknown>) {
    return this.c.request<void>("PUT", `/admin/dev/${encodeURIComponent(deviceId)}`, patch);
  }
  delete(deviceId: string) {
    return this.c.request<void>("DELETE", `/admin/dev/${encodeURIComponent(deviceId)}`);
  }
  rotateKey(deviceId: string) {
    return this.c.request<Record<string, unknown>>(
      "POST",
      `/admin/dev/${encodeURIComponent(deviceId)}/rotate-key`,
    );
  }
  revokeKey(deviceId: string) {
    return this.c.request<void>("POST", `/admin/dev/${encodeURIComponent(deviceId)}/revoke-key`);
  }
}

class AdminDatabaseApi {
  constructor(private c: DatumClient) {}
  stats() {
    return this.c.request<Record<string, unknown>>("GET", "/admin/database/stats");
  }
  export() {
    return this.c.request<Record<string, unknown>>("POST", "/admin/database/export");
  }
  cleanup() {
    return this.c.request<Record<string, unknown>>("POST", "/admin/database/cleanup");
  }
  /** DESTROYS the entire database. */
  reset() {
    return this.c.request<void>("DELETE", "/admin/database/reset", { confirm: "RESET" });
  }
}

class AdminMqttApi {
  constructor(private c: DatumClient) {}
  stats() {
    return this.c.request<Record<string, unknown>>("GET", "/admin/mqtt/stats");
  }
  async clients(): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<{ clients?: Array<Record<string, unknown>> }>(
      "GET",
      "/admin/mqtt/clients",
    );
    return res.clients ?? [];
  }
  publish(opts: { topic: string; payload: string; qos?: number; retain?: boolean }) {
    return this.c.request<void>("POST", "/admin/mqtt/publish", opts);
  }
}

// ── /admin/rules/* ─────────────────────────────────────────────────────────

class RulesApi {
  constructor(private c: DatumClient) {}
  async list(): Promise<Array<Record<string, unknown>>> {
    const res = await this.c.request<unknown>("GET", "/admin/rules");
    if (Array.isArray(res)) return res as Array<Record<string, unknown>>;
    if (res && typeof res === "object" && Array.isArray((res as { rules?: unknown }).rules)) {
      return (res as { rules: Array<Record<string, unknown>> }).rules;
    }
    return [];
  }
  get(id: string) {
    return this.c.request<Record<string, unknown>>("GET", `/admin/rules/${encodeURIComponent(id)}`);
  }
  create(spec: Record<string, unknown>) {
    return this.c.request<Record<string, unknown>>("POST", "/admin/rules", spec);
  }
  delete(id: string) {
    return this.c.request<void>("DELETE", `/admin/rules/${encodeURIComponent(id)}`);
  }
  enable(id: string) {
    return this.c.request<void>("POST", `/admin/rules/${encodeURIComponent(id)}/enable`);
  }
  disable(id: string) {
    return this.c.request<void>("POST", `/admin/rules/${encodeURIComponent(id)}/disable`);
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
