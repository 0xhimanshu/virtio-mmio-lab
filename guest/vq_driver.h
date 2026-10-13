#ifndef VQ_DRIVER_H
#define VQ_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#ifndef PAGE
#define PAGE 4096UL
#endif

/* virtio-mmio register offsets */
#define VQ_MAGIC          0x000
#define VQ_VERSION        0x004
#define VQ_DEVICE_ID      0x008
#define VQ_VENDOR_ID      0x00c
#define VQ_DEV_FEATURES   0x010
#define VQ_DEV_FEATURES_S 0x014
#define VQ_DRV_FEATURES   0x020
#define VQ_DRV_FEATURES_S 0x024
#define VQ_QUEUE_SEL      0x030
#define VQ_QUEUE_NUM_MAX  0x034
#define VQ_QUEUE_NUM      0x038
#define VQ_QUEUE_READY    0x044
#define VQ_QUEUE_NOTIFY   0x050
#define VQ_INT_STATUS     0x060
#define VQ_INT_ACK        0x064
#define VQ_STATUS         0x070
#define VQ_Q_DESC_LO      0x080
#define VQ_Q_DESC_HI      0x084
#define VQ_Q_DRIVER_LO    0x090
#define VQ_Q_DRIVER_HI    0x094
#define VQ_Q_DEVICE_LO    0x0a0
#define VQ_Q_DEVICE_HI    0x0a4

/* status bits */
#define VQ_S_ACK          0x1
#define VQ_S_DRIVER       0x2
#define VQ_S_DRIVER_OK    0x4
#define VQ_S_FEATURES_OK  0x8
#define VQ_S_NEEDS_RESET  0x40
#define VQ_S_FAILED       0x80

/* descriptor flags */
#define VQ_DESC_F_NEXT    0x1
#define VQ_DESC_F_WRITE   0x2

struct vq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vq_used_elem { uint32_t id; uint32_t len; } __attribute__((packed));

/* A buffer mapped with its own page(s); GPA resolved independently. */
struct vbuf {
    void    *va;
    uint64_t gpa;
    size_t   len;
    size_t   size;   /* page-rounded mapping size */
};

struct vq {
    volatile uint8_t *mmio;
    size_t            mmio_len;
    int               memfd;
    uint32_t          sel;
    uint32_t          num;
    struct vbuf       desc;
    struct vbuf       avail;
    struct vbuf       used;
};

/* Allocate a page-aligned buffer, populate it, and resolve its guest physical
 * address independently via /proc/self/pagemap (no contiguity assumption for
 * len <= 4096). */
int  vbuf_alloc(struct vbuf *b, size_t len);
void vbuf_free(struct vbuf *b);

int  vq_open(struct vq *q, uint64_t base, size_t mmio_len);
void vq_close(struct vq *q);

uint32_t vq_mmio_read(struct vq *q, uint32_t off);
void     vq_mmio_write(struct vq *q, uint32_t off, uint32_t v);

void vq_status_set(struct vq *q, uint32_t bits);
void vq_reset(struct vq *q);
void vq_negotiate_all(struct vq *q);
int  vq_setup_queue(struct vq *q, uint32_t sel, uint32_t num);
void vq_notify(struct vq *q, uint32_t sel);

/* Accessors over the ring memory (host-side views). */
struct vq_desc *vq_desc_at(struct vq *q, uint32_t idx);
uint16_t       *vq_avail_idx(struct vq *q);
uint16_t       *vq_avail_ring(struct vq *q);
uint16_t       *vq_used_idx(struct vq *q);
struct vq_used_elem *vq_used_ring(struct vq *q);

#endif
