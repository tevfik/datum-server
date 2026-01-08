#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$SCRIPT_DIR/run_cppcheck.sh" || { echo "cppcheck failed"; exit 1; }
"$SCRIPT_DIR/run_flawfinder.sh"

echo "Quality checks completed."
