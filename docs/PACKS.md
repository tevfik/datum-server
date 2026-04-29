# Knowledge Packs — Specification

> **Status:** v1, implemented.
> **Owner of registry/runtime:** [gleann](https://github.com/tevfik/gleann)
> **Owner of gateway/auth:** [datum-server](https://github.com/tevfik/datum-server)
> **Owner of pack content:** the application repo (e.g. `ekiyo/packs/crops-tr`).

## What is a Knowledge Pack?

A self-contained, versioned bundle of **curated reference data** that gleann
loads at startup and serves over HTTP. Examples:

- `crops-tr` — Türkiye'de yaygın sebzeler için ekim/dikim pencereleri (ekiyo).
- `pests-tr` — bitki hastalıkları/zararlıları (planned, ekiyo).
- `bees-tr` — beekeeping calendar (hypothetical future app).

Packs are **content**, not code. They live in the app repo that owns them and
are mounted into gleann's container as a read-only volume.

## On-disk layout

```
ekiyo/packs/
  crops-tr/
    pack.yaml          # manifest (required)
    crops.yaml         # content file(s), declared in manifest
```

### `pack.yaml` schema

```yaml
id: crops-tr               # globally unique; matches directory name
version: 1.0.0             # semver; bump when content changes
schema_version: 1          # bump on incompatible item-schema changes
locale: tr                 # BCP-47 language tag
title: Türkiye Ekim Takvimi
description: |
  Bölgesel ekim/dikim/hasat pencereleri…
license: CC-BY-4.0         # free-form; informational
tier: free                 # free | pro (future plan-gating)
content_files:             # 1+ YAML/JSON files in the same directory
  - crops.yaml
search:
  fields: [common_name_tr, common_name_en, category]
  semantic: false          # gleann will not (yet) embed this pack
app_hints:                 # client apps consult these for auto-load logic
  - app_id: ekiyo
    required: true
    auto_load: true
```

### Content files

Each content file is YAML (or JSON) and must decode to either:

1. A **top-level list** of items (each item is a map), **or**
2. A **map containing exactly one list field** (e.g. `{ items: [...] }`).

Items SHOULD have an `id`, `slug`, or `key` string field — gleann uses it to
support `GET /api/packs/:id/items/:slug`.

Path traversal in `content_files` is rejected at load time.

## HTTP API

### Gleann (origin)

Mounted at `/api/packs/*`. No auth.

| Method | Path                                | Description                |
|--------|-------------------------------------|----------------------------|
| GET    | `/api/packs?app=<id>`               | List manifests (filterable)|
| GET    | `/api/packs/{id}`                   | Single manifest            |
| GET    | `/api/packs/{id}/data`              | Manifest + items           |
| GET    | `/api/packs/{id}/items/{slug}`      | Single item                |
| GET    | `/api/packs/{id}/search?q=&n=`      | Substring search           |
| POST   | `/api/packs/reload`                 | Re-scan packs directory    |

Strong `ETag` headers and `If-None-Match` short-circuiting (`304`) are
honoured on the manifest and data endpoints.

### Datum-server (gateway)

Mounted at `/packs/*`. Requires user auth (`UserAuthMiddleware`).

| Method | Path                            | Notes                              |
|--------|---------------------------------|------------------------------------|
| GET    | `/packs?app=<id>`               | proxied to `/api/packs?app=<id>`   |
| GET    | `/packs/:id`                    | proxied; ETag round-trip preserved |
| GET    | `/packs/:id/data`               | proxied                            |
| GET    | `/packs/:id/items/:slug`        | proxied                            |
| GET    | `/packs/:id/search?q=&n=`       | proxied                            |

The gateway maintains a 5-minute in-memory cache keyed on full upstream URL
to absorb stampedes; cached entries expose ETag back to clients.

### Configuration

| Variable           | Default                  | Used by         | Purpose                       |
|--------------------|--------------------------|-----------------|-------------------------------|
| `GLEANN_PACKS_DIR` | `<IndexDir>/packs`       | gleann          | Where to scan for packs       |
| `GLEANN_URL`       | `http://localhost:8080`  | datum-server    | Upstream gleann URL for proxy |

## Deployment

Production (and dev) wiring lives in `ekiyo/backend/docker-compose.yml`. The
ekiyo `packs/` directory is bind-mounted into gleann read-only:

```yaml
gleann:
  volumes:
    - ../packs:/var/lib/gleann/packs:ro
  environment:
    GLEANN_PACKS_DIR: /var/lib/gleann/packs

datum-server:
  environment:
    GLEANN_URL: http://gleann:8080
```

Updating a pack is therefore: edit YAML in `ekiyo/packs/<id>/`, commit,
`docker compose restart gleann` (or call `POST /api/packs/reload` directly).

## Client integration (ekiyo)

`lib/core/pack_loader.dart` provides offline-first access:

1. First call: hits `/packs/<id>/data`, populates a local cache collection.
2. Subsequent calls: serve cached items immediately, refresh in the
   background.
3. On 304 the cache is retained.
4. On network failure the cache is returned (offline mode).

`CropCatalog` in `lib/features/calendar/crop_catalog.dart` is the first
consumer; it filters items by climate zone client-side.

## Versioning & rollouts

- `version` is informational (semver).
- `schema_version` should be bumped on incompatible changes; clients can
  refuse to use packs they don't understand.
- A pack is "tied" to a deployment via the docker volume mount. There is
  intentionally no remote-pack-registry yet — the trust model is "ship
  packs with your backend".

## Future work

- **Semantic search**: when `search.semantic: true`, gleann should
  pre-compute embeddings and surface a `/search?q=…&semantic=true` mode.
- **RAG retrieval**: `/ai/ask` should auto-include relevant pack items in
  the LLM context based on the user's app and topic.
- **Premium tiers**: gate packs by user plan in the gateway.
- **Signed manifests**: SHA256 + signature for tamper detection in shared
  deployments.
