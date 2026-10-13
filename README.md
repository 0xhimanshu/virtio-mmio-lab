# virtio-mmio device-model lab

A small, self-contained lab for exercising a **virtio-mmio device model** (as
implemented by a VMM such as Firecracker) with guest-controlled virtqueue
descriptors, and for producing a **per-case processed/rejected signal** instead
of a binary "the box died" oracle.

It is deliberately **target-agnostic**: it boots a minimal Linux guest on a VMM,
then drives a chosen virtio-mmio device **from userspace** (`/dev/mem` MMIO),
resolving the GPA of every ring and data buffer **independently** (one page per
buffer, per-page `pagemap`) so there is no hidden physical-contiguity
assumption. Console I/O uses the serial port, so the control transport is kept
**separate from the device under test**.

## What it is not

- It is not an exploit. It is a validation/oracle harness.
- It contains no target-, vendor-, or engagement-specific data.

## Layout

```
lab-ci/
  Dockerfile                     # build/dev image (kernel + firecracker deps)
  .github/workflows/lab.yml      # CI: build + run the lab on a KVM runner
  scripts/
    build-firecracker.sh         # build the pinned VMM
    build-kernel.sh              # build the pinned guest kernel + fragment
    make-initramfs.sh            # pack initramfs with the guest harness
    run-lab.sh                   # launch VMM, capture serial, parse LAB_RESULT
  configs/
    kernel.fragment              # minimal config (transport on, drivers off)
    firecracker.json             # 5 virtio-mmio devices (blk,blk,net,vsock,rng)
  guest/
    init.sh                      # PID1: mount fs, run harness, poweroff
    vq_driver.[ch]               # portable virtio-mmio userspace driver
    vq_test.c                    # case driver + per-case accounting
    Makefile
```

## Requirements

- A **Linux x86_64 host with `/dev/kvm`** (Firecracker runs the guest as L1, so
  plain KVM suffices; nested virt is *not* required).
  - macOS/Apple Silicon cannot run this (no KVM).
  - GitHub-hosted Linux runners expose `/dev/kvm`; the CI workflow checks for it.
- Docker (optional), for the dev image.

## Run

```
scripts/build-firecracker.sh
scripts/build-kernel.sh
scripts/make-initramfs.sh
sudo scripts/run-lab.sh            # needs /dev/kvm + tap/vsock perms
```

`run-lab.sh` prints the serial log and a final line:

```
LAB_RESULT device=<id> cases=<n> processed=<p> rejected=<n-p> timed_out=<t>
```

`processed` increments only when the used ring advanced for a case (the device
consumed the descriptor); `rejected` counts cases the device did not complete.
That is the per-case signal — **not** stream closure.

## Pins

Edit at the top of each script (defaults shown):

| var | default |
|---|---|
| `FIRECRACKER_COMMIT` | `6c5d9f3e04170d0fbb24d94676c07571113bcf77` |
| `RUST_TOOLCHAIN` | `1.97.0` |
| `KERNEL_VERSION` | `6.18.49` |

## Notes

- The kernel fragment keeps the virtio-mmio **transport** enabled but disables the
  **drivers** for the devices under test (they are driven from userspace). Re-enable
  a driver only when you specifically want to test the normal driver path.
- MMIO base and device order must match the VMM's configuration; confirm from the
  guest's `dmesg` / `/proc/iomem` and pass the base to `vq_test` (default `0xc0005000`).
- Keep destructive/fault-class experiments off shared hardware; run them only in a
  disposable host.
