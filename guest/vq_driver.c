#define _GNU_SOURCE
#include "vq_driver.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

static uint64_t resolve_gpa(void *p)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) return 0;
    uint64_t v = (uint64_t)(uintptr_t)p;
    uint64_t entry = 0;
    off_t off = (off_t)((v / PAGE) * sizeof(entry));
    ssize_t r = pread(fd, &entry, sizeof(entry), off);
    close(fd);
    if (r != (ssize_t)sizeof(entry)) return 0;
    if (!(entry & (1ULL << 63))) return 0;              /* not present */
    uint64_t pfn = entry & ((1ULL << 55) - 1);
    if (!pfn) return 0;                                  /* PFN hidden */
    return (pfn << 12) | (v & (PAGE - 1));
}

int vbuf_alloc(struct vbuf *b, size_t len)
{
    if (len == 0) len = 1;
    if (len > PAGE) return -1;               /* one page per buffer by design */
    size_t sz = PAGE;
    void *va = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (va == MAP_FAILED) return -1;
    memset(va, 0, sz);
    *(volatile uint8_t *)va = 0;             /* fault it in */
    b->va = va; b->len = len; b->size = sz;
    b->gpa = resolve_gpa(va);
    if (!b->gpa) { munmap(va, sz); b->va = NULL; return -2; }
    return 0;
}

void vbuf_free(struct vbuf *b)
{
    if (b->va && b->va != MAP_FAILED) munmap(b->va, b->size);
    b->va = NULL; b->gpa = 0;
}

uint32_t vq_mmio_read(struct vq *q, uint32_t off)
{
    return *(volatile uint32_t *)(q->mmio + off);
}

void vq_mmio_write(struct vq *q, uint32_t off, uint32_t v)
{
    *(volatile uint32_t *)(q->mmio + off) = v;
    __sync_synchronize();
}

int vq_open(struct vq *q, uint64_t base, size_t mmio_len)
{
    memset(q, 0, sizeof(*q));
    q->mmio_len = mmio_len;
    q->memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (q->memfd < 0) return -1;
    q->mmio = mmap(NULL, mmio_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                   q->memfd, (off_t)base);
    if (q->mmio == MAP_FAILED) { close(q->memfd); return -2; }
    return 0;
}

void vq_close(struct vq *q)
{
    if (q->mmio && q->mmio != MAP_FAILED) munmap((void *)q->mmio, q->mmio_len);
    if (q->memfd >= 0) close(q->memfd);
    vbuf_free(&q->desc); vbuf_free(&q->avail); vbuf_free(&q->used);
}

void vq_status_set(struct vq *q, uint32_t bits) { vq_mmio_write(q, VQ_STATUS, bits); }

void vq_reset(struct vq *q) { vq_mmio_write(q, VQ_STATUS, 0); }

void vq_negotiate_all(struct vq *q)
{
    vq_mmio_write(q, VQ_STATUS, VQ_S_ACK);
    vq_mmio_write(q, VQ_STATUS, VQ_S_ACK | VQ_S_DRIVER);

    vq_mmio_write(q, VQ_DEV_FEATURES_S, 0);
    uint32_t lo = vq_mmio_read(q, VQ_DEV_FEATURES);
    vq_mmio_write(q, VQ_DEV_FEATURES_S, 1);
    uint32_t hi = vq_mmio_read(q, VQ_DEV_FEATURES);

    vq_mmio_write(q, VQ_DRV_FEATURES_S, 0);
    vq_mmio_write(q, VQ_DRV_FEATURES, lo);
    vq_mmio_write(q, VQ_DRV_FEATURES_S, 1);
    vq_mmio_write(q, VQ_DRV_FEATURES, hi);

    vq_mmio_write(q, VQ_STATUS, VQ_S_ACK | VQ_S_DRIVER | VQ_S_FEATURES_OK);
    if (!(vq_mmio_read(q, VQ_STATUS) & VQ_S_FEATURES_OK))
        fprintf(stderr, "vq: FEATURES_OK not accepted\n");
}

int vq_setup_queue(struct vq *q, uint32_t sel, uint32_t num)
{
    if (vbuf_alloc(&q->desc, PAGE) || vbuf_alloc(&q->avail, PAGE) ||
        vbuf_alloc(&q->used, PAGE))
        return -1;

    uint64_t desc_gpa = q->desc.gpa, avail_gpa = q->avail.gpa, used_gpa = q->used.gpa;

    q->sel = sel; q->num = num;
    vq_mmio_write(q, VQ_QUEUE_SEL, sel);
    uint32_t max = vq_mmio_read(q, VQ_QUEUE_NUM_MAX);
    if (num > max) q->num = max;
    vq_mmio_write(q, VQ_QUEUE_NUM, q->num);

    vq_mmio_write(q, VQ_Q_DESC_LO,   (uint32_t)(desc_gpa & 0xffffffff));
    vq_mmio_write(q, VQ_Q_DESC_HI,   (uint32_t)(desc_gpa >> 32));
    vq_mmio_write(q, VQ_Q_DRIVER_LO, (uint32_t)(avail_gpa & 0xffffffff));
    vq_mmio_write(q, VQ_Q_DRIVER_HI, (uint32_t)(avail_gpa >> 32));
    vq_mmio_write(q, VQ_Q_DEVICE_LO, (uint32_t)(used_gpa & 0xffffffff));
    vq_mmio_write(q, VQ_Q_DEVICE_HI, (uint32_t)(used_gpa >> 32));
    vq_mmio_write(q, VQ_QUEUE_READY, 1);

    vq_mmio_write(q, VQ_STATUS,
                  VQ_S_ACK | VQ_S_DRIVER | VQ_S_FEATURES_OK | VQ_S_DRIVER_OK);
    return 0;
}

void vq_notify(struct vq *q, uint32_t sel) { vq_mmio_write(q, VQ_QUEUE_NOTIFY, sel); }

struct vq_desc *vq_desc_at(struct vq *q, uint32_t idx) { return ((struct vq_desc *)q->desc.va) + idx; }
uint16_t *vq_avail_idx(struct vq *q)  { return ((uint16_t *)q->avail.va) + 1; }
uint16_t *vq_avail_ring(struct vq *q) { return ((uint16_t *)q->avail.va) + 2; }
uint16_t *vq_used_idx(struct vq *q)   { return ((uint16_t *)q->used.va) + 1; }
struct vq_used_elem *vq_used_ring(struct vq *q) { return (struct vq_used_elem *)(((uint8_t *)q->used.va) + 4); }
