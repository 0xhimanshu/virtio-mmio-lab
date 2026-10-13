#!/usr/bin/env bash
# Pack a gzip newc cpio initramfs containing a static busybox, the guest
# harness, and init.sh.
set -euo pipefail

WORK="${WORK:-$PWD/.lab}"
OUT="$WORK/out"
ROOT="$WORK/initramfs"
BB_VER="${BB_VER:-1.35.0}"
BB_URL="https://busybox.net/downloads/binaries/${BB_VER}-x86_64-linux-musl/busybox"

mkdir -p "$OUT"
rm -rf "$ROOT"; mkdir -p "$ROOT"/{proc,sys,dev,tmp,bin}

bb="$WORK/busybox"
if [ ! -x "$bb" ]; then
  curl -fL -o "$bb" "$BB_URL"
  chmod 0755 "$bb"
fi
install -m 0755 "$bb" "$ROOT/bin/busybox"
for app in sh mount umount mknod poweroff reboot sync cat sleep; do
  ln -sf busybox "$ROOT/bin/$app"
done

ROOT="$ROOT" "$PWD/guest/build.sh" "$ROOT"
install -m 0755 "$PWD/guest/init.sh" "$ROOT/init"

( cd "$ROOT" && find . -print0 | cpio --null -ov --format=newc 2>/dev/null | gzip -9 ) > "$OUT/initramfs.cpio.gz"
echo "initramfs -> $OUT/initramfs.cpio.gz"
