#!/usr/bin/env python3
"""Validate openapi.yaml for duplicate path keys and YAML syntax errors."""
import re
import sys
import yaml

OPENAPI_PATH = "cmd/server/openapi.yaml"

with open(OPENAPI_PATH) as f:
    lines = f.readlines()

seen = {}
dups = []
for i, line in enumerate(lines, 1):
    m = re.match(r"^  (/[^\s:]+):", line)
    if m:
        k = m.group(1)
        if k in seen:
            dups.append(f"  {k}: lines {seen[k]} and {i}")
        else:
            seen[k] = i

if dups:
    print("❌ Duplicate paths found in openapi.yaml:")
    for d in dups:
        print(d)
    sys.exit(1)

try:
    yaml.safe_load(open(OPENAPI_PATH))
except yaml.YAMLError as e:
    print(f"❌ YAML parse error: {e}")
    sys.exit(1)
