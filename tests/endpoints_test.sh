#!/usr/bin/env bash
# =============================================================================
# endpoints_test.sh — exhaustive endpoint reachability + auth-policy test
# =============================================================================
# Exercises every documented HTTP route on a running datum-server and verifies:
#   - public endpoints respond 200/expected
#   - protected endpoints respond 401 without auth
#   - protected endpoints respond 200/expected with auth
#
# Usage:
#   SERVER_URL=https://datum.bezg.in ./tests/endpoints_test.sh
#   SERVER_URL=http://localhost:8000 ./tests/endpoints_test.sh
#
# A test user is registered (or logged into) at runtime; deletion is best-effort
# at the end. Set TEST_EMAIL / TEST_PASSWORD to reuse an existing account.
# =============================================================================
set -uo pipefail

SERVER_URL="${SERVER_URL:-http://localhost:8000}"
TEST_EMAIL="${TEST_EMAIL:-endpoint-test+$(date +%s)@example.com}"
TEST_PASSWORD="${TEST_PASSWORD:-endpoint-test-password-2026}"
JQ="$(command -v jq || true)"

# ── Output helpers ───────────────────────────────────────────────────────────
G='\033[0;32m'; R='\033[0;31m'; Y='\033[1;33m'; B='\033[0;34m'; N='\033[0m'
PASS=0; FAIL=0; SKIP=0
declare -a FAILED

ok()    { printf "${G}✓${N} %-7s %-40s %s\n" "$1" "$2" "$3"; PASS=$((PASS+1)); }
bad()   { printf "${R}✗${N} %-7s %-40s %s\n" "$1" "$2" "$3"; FAIL=$((FAIL+1)); FAILED+=("$1 $2 — $3"); }
skip()  { printf "${Y}-${N} %-7s %-40s %s\n" "$1" "$2" "$3"; SKIP=$((SKIP+1)); }
section() { printf "\n${B}━━ %s ━━${N}\n" "$1"; }

# ── HTTP helpers ─────────────────────────────────────────────────────────────
# req METHOD PATH [EXPECTED_STATUS] [DATA] [AUTH_TOKEN]
req() {
    local method="$1" path="$2" expected="${3:-200}" data="${4:-}" token="${5:-}"
    local args=(-s -o /tmp/etest_body -w '%{http_code}' -X "$method" "$SERVER_URL$path")
    [[ -n "$data" ]] && args+=(-H 'Content-Type: application/json' --data "$data")
    [[ -n "$token" ]] && args+=(-H "Authorization: Bearer $token")
    local code
    code=$(curl "${args[@]}" 2>/dev/null || echo "ERR")
    if [[ "$code" == "$expected" ]]; then
        ok "$method" "$path" "→ $code"
    else
        local body=""
        [[ -s /tmp/etest_body ]] && body="$(head -c 120 /tmp/etest_body)"
        bad "$method" "$path" "expected $expected, got $code  $body"
    fi
}

# ── 1. Public health & info ──────────────────────────────────────────────────
section "Public: health, system info, docs"
req GET /                       200
req GET /health                 200
req GET /healthz                200
req GET /live                   200
req GET /ready                  200
req GET /sys/info               200
req GET /sys/time               200
req GET /sys/ip                 200
req GET /sys/status             200
req GET /sys/metrics            200

# /docs is now auth-gated → expect 401 anonymously
req GET /docs                   401
req GET /docs/openapi.yaml      401

# ── 2. Auth: register + login ────────────────────────────────────────────────
section "Auth: register, login, refresh, oauth"

# Bootstrap: if the system is uninitialized we can create the admin via
# /sys/setup. Useful for pristine local instances; on production this returns
# 409 and we fall through to the regular login/register flow.
INIT_BODY=$(curl -s "$SERVER_URL/sys/status" || echo '{}')
if [[ "$INIT_BODY" == *'"initialized":false'* ]]; then
    SETUP_PAYLOAD="{\"platform_name\":\"endpoints-test\",\"admin_email\":\"$TEST_EMAIL\",\"admin_password\":\"$TEST_PASSWORD\",\"allow_register\":true}"
    SETUP_CODE=$(curl -s -o /tmp/etest_setup -w '%{http_code}' -X POST \
        -H 'Content-Type: application/json' --data "$SETUP_PAYLOAD" \
        "$SERVER_URL/sys/setup" 2>/dev/null || echo ERR)
    if [[ "$SETUP_CODE" =~ ^(200|201)$ ]]; then
        ok POST /sys/setup "→ $SETUP_CODE (admin bootstrapped)"
    else
        skip POST /sys/setup "→ $SETUP_CODE"
    fi
fi

# Try login first (idempotent reruns); if 401, register fresh.
LOGIN_BODY="{\"email\":\"$TEST_EMAIL\",\"password\":\"$TEST_PASSWORD\"}"
LOGIN_CODE=$(curl -s -o /tmp/etest_login -w '%{http_code}' -X POST \
    -H 'Content-Type: application/json' --data "$LOGIN_BODY" \
    "$SERVER_URL/auth/login" 2>/dev/null || echo ERR)

if [[ "$LOGIN_CODE" != "200" ]]; then
    REG_BODY="$LOGIN_BODY"
    REG_CODE=$(curl -s -o /tmp/etest_reg -w '%{http_code}' -X POST \
        -H 'Content-Type: application/json' --data "$REG_BODY" \
        "$SERVER_URL/auth/register" 2>/dev/null || echo ERR)
    if [[ "$REG_CODE" =~ ^(200|201)$ ]]; then
        ok POST /auth/register "→ $REG_CODE"
        cp /tmp/etest_reg /tmp/etest_login
        LOGIN_CODE=200
    elif [[ "$REG_CODE" == "403" ]]; then
        skip POST /auth/register "public registration disabled (admin policy)"
    else
        bad POST /auth/register "expected 200/201/403, got $REG_CODE"
    fi
fi

if [[ "$LOGIN_CODE" == "200" ]]; then
    ok POST /auth/login "→ $LOGIN_CODE"
    if [[ -n "$JQ" ]]; then
        TOKEN=$(jq -r '.token // empty' < /tmp/etest_login)
        REFRESH=$(jq -r '.refresh_token // empty' < /tmp/etest_login)
        USER_ID=$(jq -r '.user_id // empty' < /tmp/etest_login)
    else
        TOKEN=$(grep -oE '"token":"[^"]+"' /tmp/etest_login | sed 's/.*:"\(.*\)"/\1/')
        REFRESH=""
        USER_ID=""
    fi
else
    skip POST /auth/login "got $LOGIN_CODE — set TEST_EMAIL/TEST_PASSWORD to a valid account or enable public registration"
    TOKEN=""; REFRESH=""
fi

req GET /auth/providers          200            # public: configured providers list
req GET /auth/oauth/providers    200            # alias via OAuthRedirect
req GET /auth/oauth/google       400            # provider name OK, not configured
req GET /auth/me                 401            # no auth → 401
[[ -n "$TOKEN" ]] && req GET /auth/me  200 "" "$TOKEN" || skip GET /auth/me "no token"

# ── 3. Protected user endpoints ──────────────────────────────────────────────
section "Protected: requires JWT"

if [[ -z "$TOKEN" ]]; then
    skip "*"  "/auth/*, /dev/*, /db/*, /admin/*"  "no token — skipping authenticated suite"
else
    req GET    /auth/sessions         200 ""    "$TOKEN"
    req GET    /auth/push-tokens      200 ""    "$TOKEN"

    # Devices
    req GET    /dev                   200 ""    "$TOKEN"
    DEV_BODY='{"name":"endpoint-test-device","type":"sensor"}'
    DEV_CODE=$(curl -s -o /tmp/etest_dev -w '%{http_code}' -X POST \
        -H "Authorization: Bearer $TOKEN" \
        -H 'Content-Type: application/json' --data "$DEV_BODY" \
        "$SERVER_URL/dev" 2>/dev/null || echo ERR)
    if [[ "$DEV_CODE" =~ ^(200|201)$ ]]; then
        ok POST /dev "→ $DEV_CODE"
        if [[ -n "$JQ" ]]; then
            DEV_ID=$(jq -r '.id // .device_id // empty' < /tmp/etest_dev)
        else
            DEV_ID=$(grep -oE '"(id|device_id)":"[^"]+"' /tmp/etest_dev | head -1 | sed 's/.*:"\(.*\)"/\1/')
        fi
    else
        bad POST /dev "expected 200/201, got $DEV_CODE"
        DEV_ID=""
    fi

    if [[ -n "$DEV_ID" ]]; then
        req GET    /dev/$DEV_ID        200 ""    "$TOKEN"
        # Fresh device has no telemetry → 404 is the expected "no data" response
        req GET    /dev/$DEV_ID/data   404 ""    "$TOKEN"
        req GET    "/dev/$DEV_ID/data/history?last=1h" 404 "" "$TOKEN"
        req DELETE /dev/$DEV_ID        200 ""    "$TOKEN"
    else
        skip "*" "/dev/:id/*" "no device id"
    fi

    # Generic Document Store
    req GET    /db/notes              200 ""    "$TOKEN"
    DOC_CODE=$(curl -s -o /tmp/etest_doc -w '%{http_code}' -X POST \
        -H "Authorization: Bearer $TOKEN" \
        -H 'Content-Type: application/json' --data '{"title":"hi","body":"test"}' \
        "$SERVER_URL/db/notes" 2>/dev/null || echo ERR)
    if [[ "$DOC_CODE" == "201" ]]; then
        ok POST /db/notes "→ $DOC_CODE"
        if [[ -n "$JQ" ]]; then DOC_ID=$(jq -r .id < /tmp/etest_doc); else DOC_ID=""; fi
        if [[ -n "$DOC_ID" ]]; then
            req GET    /db/notes/$DOC_ID 200 ""                                  "$TOKEN"
            req PUT    /db/notes/$DOC_ID 200 '{"title":"hi","body":"updated"}'    "$TOKEN"
            req DELETE /db/notes/$DOC_ID 200 ""                                  "$TOKEN"
        fi
    else
        bad POST /db/notes "expected 201, got $DOC_CODE"
    fi

    # Docs page now accessible WITH token
    req GET "/docs?token=$TOKEN" 200
    req GET "/docs/openapi.yaml?token=$TOKEN" 200
fi

# ── 4. Public device data ────────────────────────────────────────────────────
section "Public: /pub/:device_id"
# /pub/:device_id is a per-device public endpoint; using a non-existent ID
# should yield 404 (not 500/401) — a 404 here means the route exists.
req GET /pub/__noexist__         404

# ── 5. Negative tests (auth must reject) ─────────────────────────────────────
section "Negative: auth rejection"
req GET /dev                       401
req GET /db/notes                  401
req GET /admin/users               401
req GET /admin/sys/config          401

# ── 6. Cleanup (best-effort) ─────────────────────────────────────────────────
section "Cleanup"
if [[ -n "$TOKEN" ]]; then
    DEL_CODE=$(curl -s -o /dev/null -w '%{http_code}' -X DELETE \
        -H "Authorization: Bearer $TOKEN" "$SERVER_URL/auth/user" 2>/dev/null || echo ERR)
    [[ "$DEL_CODE" =~ ^2 ]] && ok DELETE /auth/user "→ $DEL_CODE" || skip DELETE /auth/user "→ $DEL_CODE"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
printf "Total: ${G}%d passed${N}, ${R}%d failed${N}, ${Y}%d skipped${N}\n" "$PASS" "$FAIL" "$SKIP"
if (( FAIL > 0 )); then
    echo
    echo "Failures:"
    for f in "${FAILED[@]}"; do echo "  • $f"; done
    exit 1
fi
exit 0
