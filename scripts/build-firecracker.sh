#!/usr/bin/env bash
# Build the pinned VMM (Firecracker) from source.
set -euo pipefail

FIRECRACKER_COMMIT="${FIRECRACKER_COMMIT:-6c5d9f3e04170d0fbb24d94676c07571113bcf77}"
RUST_TOOLCHAIN="${RUST_TOOLCHAIN:-1.97.0}"
FIRECRACKER_REPO="${FIRECRACKER_REPO:-https://github.com/firecracker-microvm/firecracker.git}"
WORK="${WORK:-$PWD/.lab}"
SRC="$WORK/firecracker"
OUT="$WORK/out"
export CARGO_TARGET_DIR="$WORK/fc-target"
mkdir -p "$WORK" "$OUT"

if [ ! -d "$SRC/.git" ]; then
  rm -rf "$SRC"
  git clone "$FIRECRACKER_REPO" "$SRC"
fi
git -C "$SRC" fetch --all --tags
git -C "$SRC" checkout "$FIRECRACKER_COMMIT"

if ! command -v rustup >/dev/null 2>&1; then
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain "$RUST_TOOLCHAIN"
  # shellcheck disable=SC1091
  . "$HOME/.cargo/env"
fi
rustup toolchain install "$RUST_TOOLCHAIN" >/dev/null 2>&1 || true
rustup override set "$RUST_TOOLCHAIN" --path "$SRC"

cargo_bin="$(command -v cargo)"
( cd "$SRC" && "$cargo_bin" build --release )

BIN=""
for c in "$CARGO_TARGET_DIR/release/firecracker" "$SRC/build/cargo_target/release/firecracker"; do
  [ -x "$c" ] && BIN="$c" && break
done
[ -n "$BIN" ] || { echo "firecracker binary not found"; exit 1; }
install -m 0755 "$BIN" "$OUT/firecracker"
echo "firecracker -> $OUT/firecracker"
"$OUT/firecracker" --version || true
