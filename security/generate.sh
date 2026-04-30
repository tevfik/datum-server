#!/usr/bin/env bash
set -e

# This script generates the SBOM (Software Bill of Materials) and runs govulncheck.
# It requires the Go toolchain to be installed.

# Navigate to the backend directory
cd "$(dirname "$0")/.."

echo "==> Ensuring required security tools are installed..."
if ! command -v cyclonedx-gomod &> /dev/null; then
    echo "Installing cyclonedx-gomod..."
    go install github.com/CycloneDX/cyclonedx-gomod/cmd/cyclonedx-gomod@latest
fi

if ! command -v govulncheck &> /dev/null; then
    echo "Installing govulncheck..."
    go install golang.org/x/vuln/cmd/govulncheck@latest
fi

# Ensure bin is in PATH
export PATH="$PATH:$(go env GOPATH)/bin"

echo "==> Generating SBOM (Software Bill of Materials) in CycloneDX format..."
cyclonedx-gomod mod -licenses -json -output security/sbom.cdx.json .
echo "SBOM saved to security/sbom.cdx.json"

echo "==> Running Vulnerability Scan (govulncheck)..."
# govulncheck may exit with non-zero if vulnerabilities are found, so we don't let it crash the script
set +e
govulncheck ./... > security/govulncheck.txt 2>&1
SCAN_RESULT=$?
set -e
echo "Vulnerability scan results saved to security/govulncheck.txt"

if [ $SCAN_RESULT -ne 0 ]; then
    echo "⚠️  govulncheck found potential vulnerabilities. Please review security/govulncheck.txt"
else
    echo "✅ No known vulnerabilities found."
fi

echo "==> Security reports generation complete."
