#!/bin/sh
# PID 1 in the lab guest. Control I/O is the serial console only; the device
# under test is driven from userspace by /vq_test.
export PATH=/bin

mount -t proc proc /proc 2>/dev/null
mount -t sysfs sys /sys 2>/dev/null
mount -t devtmpfs dev /dev 2>/dev/null
[ -e /dev/mem ] || mknod /dev/mem c 1 1 2>/dev/null

echo "LAB: guest up $(uname -r)"
cat /proc/cmdline | tr ' ' '\n' | grep '^vq_base=' | head -n1

VQ_BASE="$(cat /proc/cmdline | tr ' ' '\n' | sed -n 's/^vq_base=//p')"
CASES="$(cat /proc/cmdline | tr ' ' '\n' | sed -n 's/^vq_cases=//p')"
[ -n "$CASES" ] || CASES=64

/vq_test "$VQ_BASE" "$CASES"
echo "LAB: harness exit=$?"

sync
poweroff -f 2>/dev/null || reboot -f
while :; do sleep 3600; done
