#!/usr/bin/env bash
# Build a minimal pinned Linux guest kernel with virtio-mmio transport enabled
# and the drivers for the devices under test disabled (they are driven from
# userspace). See configs/kernel.fragment for the toggles.
set -euo pipefail

KERNEL_VERSION="${KERNEL_VERSION:-6.18.49}"
MAJOR="${KERNEL_VERSION%%.*}"
LAB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FRAG="$LAB_ROOT/configs/kernel.fragment"
WORK="${WORK:-$LAB_ROOT/.lab}"
SRC="$WORK/linux-$KERNEL_VERSION"
OUT="$WORK/out"
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$WORK" "$OUT"

if [ ! -d "$SRC" ]; then
  tarball="$WORK/linux-$KERNEL_VERSION.tar.xz"
  [ -f "$tarball" ] || curl -fL -o "$tarball" \
    "https://cdn.kernel.org/pub/linux/kernel/v${MAJOR}.x/linux-$KERNEL_VERSION.tar.xz"
  tar -C "$WORK" -xf "$tarball"
fi

cd "$SRC"
make tinyconfig
./scripts/kconfig/merge_config.sh -m .config "$FRAG" || cat "$FRAG" >> .config
make olddefconfig
make -j"$JOBS" bzImage

install -m 0644 arch/x86/boot/bzImage "$OUT/bzImage"
echo "kernel -> $OUT/bzImage ($KERNEL_VERSION)"
