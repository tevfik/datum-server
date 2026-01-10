# Implementation Plan - Remove Server-Side IP Enrichment

The user requested the removal of logic that automatically injects the `public_ip` field into telemetry data based on the client's connection IP. This ensures `public_ip` is treated strictly as device-reported data.

## Proposed Changes

### [MODIFY] [internal/processing/telemetry.go](file:///home/tevfik/backingup/WORKSPACE/git_bezgin/datum-server/internal/processing/telemetry.go)
-   Update `Process` function signature to remove `clientIP` argument.
-   Remove the logic that checks and adds `public_ip` to the data map.

### [MODIFY] [internal/mqtt/broker.go](file:///home/tevfik/backingup/WORKSPACE/git_bezgin/datum-server/internal/mqtt/broker.go)
-   Remove the "Enrichment" block in `OnPublish` hook (lines ~281-295).
-   Update `h.processor.Process(...)` call to remove the IP argument.

### [MODIFY] [cmd/server/handlers_data.go](file:///home/tevfik/backingup/WORKSPACE/git_bezgin/datum-server/cmd/server/handlers_data.go)
-   Update `postDataHandler` and `pushDataViaGetHandler` to call `telemetryProcessor.Process` without passing `c.ClientIP()`.

### [MODIFY] [internal/processing/telemetry_test.go](file:///home/tevfik/backingup/WORKSPACE/git_bezgin/datum-server/internal/processing/telemetry_test.go)
-   Update test calls to `Process` to match the new signature.

## Verification
-   Run `make build-server` to ensure no compilation errors.
-   Run `go test ./internal/processing/...` to verify telemetry processing.
