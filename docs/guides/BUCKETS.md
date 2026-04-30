# Object Storage (Buckets)

> Phase 3 deliverable. The bucket subsystem provides a Supabase/Firebase-style
> object store on top of either the local filesystem (default) or any S3-
> compatible service.

## Configuration

```yaml
buckets:
  backend: "localfs"        # localfs | s3
  base_dir: "./data/buckets"  # localfs only
  endpoint: ""               # s3 only (e.g. https://s3.us-east-1.amazonaws.com)
  region: ""                 # s3 only
  access_key: ""             # s3 only
  secret_key: ""             # s3 only
  bucket_prefix: ""          # s3 only — prepended to every bucket name
  max_object_mb: 50
```

Environment overrides: `BUCKETS_BACKEND`, `BUCKETS_BASE_DIR`, `BUCKETS_ENDPOINT`, …

The S3 backend is currently a stub returning `ErrNotImplemented` — it
validates configuration but does not perform any I/O. A future revision will
plug in `aws-sdk-go-v2` or `minio-go`.

## REST API

All routes are mounted at `/storage`.

| Method | Path                                | Auth                          | Notes                              |
|--------|-------------------------------------|-------------------------------|------------------------------------|
| GET    | `/storage`                          | user                          | List buckets                       |
| POST   | `/storage/{bucket}`                 | user                          | Create / ensure bucket             |
| DELETE | `/storage/{bucket}`                 | user                          | Delete empty bucket                |
| GET    | `/storage/{bucket}`                 | user                          | List objects (`?prefix=&limit=`)   |
| PUT    | `/storage/{bucket}/{path...}`       | user **or** presigned         | Upload — body is the object bytes  |
| GET    | `/storage/{bucket}/{path...}`       | user **or** presigned         | Download                           |
| HEAD   | `/storage/{bucket}/{path...}`       | user **or** presigned         | Stat                               |
| DELETE | `/storage/{bucket}/{path...}`       | user                          | Delete object                      |
| POST   | `/storage/{bucket}/presign`         | user                          | Issue presigned URL                |

The `Content-Type` of `PUT` requests is preserved on the object and replayed
on `GET`. `ETag`, `Content-Length`, and `Last-Modified` headers are emitted on
download.

### Presigned URLs (LocalFS)

LocalFS uses HMAC-SHA256 over `method\nbucket\npath\nexpires`, keyed with the
active JWT secret. The signature is appended as `?expires=…&sig=…&method=…`.
Verification is performed inline by the bucket router: when a request carries
a valid signature, normal user authentication is skipped.

```bash
$ datumctl bucket presign images cat.jpg --method GET --expires 600
{
  "url": "http://localhost:8000/storage/images/cat.jpg?expires=1730000000&sig=...",
  "expires_secs": 600
}
```

## CLI

```bash
datumctl bucket list                     # list buckets
datumctl bucket list images              # list objects in bucket "images"
datumctl bucket create images
datumctl bucket put images cats/1.jpg ./local.jpg --content-type image/jpeg
datumctl bucket get images cats/1.jpg ./out.jpg
datumctl bucket delete images cats/1.jpg
datumctl bucket delete images            # only succeeds when bucket is empty
datumctl bucket presign images cats/1.jpg --method GET --expires 900
```

## MQTT events

When the embedded MQTT broker is running, bucket lifecycle events are
published with retain=false:

| Topic                              | Payload                  |
|------------------------------------|--------------------------|
| `bucket/{name}/created`            | empty                    |
| `bucket/{name}/deleted`            | empty                    |
| `bucket/{name}/object/put`         | object path (utf-8)      |
| `bucket/{name}/object/deleted`     | object path (utf-8)      |

## Storage layout (LocalFS)

```
<base_dir>/
  <bucket>/
    <object-path>
  .meta/
    <bucket>/
      <object-path>.json   ← sidecar metadata (etag, owner_id, content_type, …)
```

Path traversal (`..`) is rejected before any filesystem operation.
