#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
cd "$ROOT_DIR"

cppcheck --enable=warning,performance,portability,unusedFunction \
  --inline-suppr --suppress=missingIncludeSystem --error-exitcode=1 \
  -I core/include -I dsp/include -I da/include -I dl/include -I cv/include -I bf/include -I ml/include -I hal/include -I nlp/include \
  core dsp da dl cv bf ml hal nlp
