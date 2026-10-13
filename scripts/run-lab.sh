#!/usr/bin/env bash
# Launch the VMM with the lab config, capture the serial console, and parse the
# per-case LAB_RESULT line. Requires /dev/kvm and tap/vsock access.
set -euo pipefail

WORK="${WORK:-$PWD/.lab}"
OUT="$WORK/out"
QEMU_FC="${FC:-$OUT/firecracker}"
LOG="$OUT/lab-serial.log"
CASES="${CASES:-64}"
MMIO_BASE="${MMIO_BASE:-0xc0005000}"   # default: rng device (no control traffic)
TIMEOUT="${TIMEOUT:-120}"

# Prepare the JSON config with absolute paths and per-run boot args.
CFG="$OUT/firecracker.json"
sed -e "s#@KERNEL@#$OUT/bzImage#g" \
    -e "s#@INITRD@#$OUT/initramfs.cpio.gz#g" \
    -e "s#@EXTRA@#vq_base=$MMIO_BASE vq_cases=$CASES#g" \
    "$PWD/configs/firecracker.json" > "$CFG"

# Optional host tap for the net device (skip if not needed).
if ip link show tap0 >/dev/null 2>&1; then :; else
  ip tuntap add dev tap0 mode tap 2>/dev/null || true
  ip addr add 169.254.0.1/24 dev tap0 2>/dev/null || true
  ip link set tap0 up 2>/dev/null || true
fi

: > "$LOG"
timeout "$TIMEOUT" "$QEMU_FC" --no-api --config-file "$CFG" 2>&1 | tee "$LOG" || true

echo "----"
grep -E "LAB_RESULT|KVM|panic|Oops|BUG" "$LOG" || true
grep -q LAB_RESULT "$LOG" && echo "lab: OK" || { echo "lab: NO RESULT"; exit 1; }
