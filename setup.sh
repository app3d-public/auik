#!/usr/bin/env bash
set -euo pipefail

git submodule update --init
cd modules/awin
git submodule update --init -- modules/agrb
cd modules/agrb
git submodule update --init -- modules/3rd-party/vma
cd - >/dev/null
echo "Submodules initialized."