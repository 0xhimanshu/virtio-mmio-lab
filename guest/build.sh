#!/usr/bin/env bash
# Build the guest harness and install it into an initramfs root.
set -euo pipefail
ROOT="${1:?usage: build.sh <initramfs-root>}"
DIR="$(cd "$(dirname "$0")" && pwd)"
make -C "$DIR" clean all
install -m 0755 "$DIR/vq_test" "$ROOT/vq_test"
