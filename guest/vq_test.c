#define _GNU_SOURCE
#include "vq_driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static const uint64_t BASES[] = {
    0xc0001000ULL, 0xc0002000ULL, 0xc0003000ULL, 0xc0004000ULL, 0xc0005000ULL,
};
#define NBASES (sizeof(BASES) / sizeof(BASES[0]))

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    uint64_t want = 0;
    int cases = (argc > 2) ? atoi(argv[2]) : 64;
    if (argc > 1) want = strtoull(argv[1], NULL, 0);

    /* Inventory: find the target device by id (default rng=4) or by base. */
    uint64_t base = 0; uint32_t devid = 0;
    for (unsigned i = 0; i < NBASES; i++) {
        struct vq q;
        if (vq_open(&q, BASES[i], 0x1000) != 0) continue;
        uint32_t magic = vq_mmio_read(&q, VQ_MAGIC);
        uint32_t ver   = vq_mmio_read(&q, VQ_VERSION);
        uint32_t id    = vq_mmio_read(&q, VQ_DEVICE_ID);
        uint32_t qmax  = vq_mmio_read(&q, VQ_QUEUE_NUM_MAX);
        printf("INV base=%#llx magic=%#x ver=%u devid=%u qmax=%u\n",
               (unsigned long long)BASES[i], magic, ver, id, qmax);
        if ((want && BASES[i] == want) || (!want && id == 4)) { base = BASES[i]; devid = id; }
        vq_close(&q);
    }
    if (!base) { base = BASES[NBASES - 1]; }

    struct vq q;
    if (vq_open(&q, base, 0x1000) != 0) { printf("LAB_RESULT error=open\n"); return 1; }
    devid = vq_mmio_read(&q, VQ_DEVICE_ID);

    vq_reset(&q);
    vq_negotiate_all(&q);

    if (vq_setup_queue(&q, 0, 8) != 0) {
        printf("LAB_RESULT error=queue\n"); return 1;
    }
    printf("RING base=%#llx devid=%u desc_gpa=%#llx avail_gpa=%#llx used_gpa=%#llx\n",
           (unsigned long long)base, devid,
           (unsigned long long)q.desc.gpa,
           (unsigned long long)q.avail.gpa,
           (unsigned long long)q.used.gpa);

    int processed = 0, rejected = 0, timedout = 0;

    static const uint32_t lens[] = { 0, 1, 16, 64, 4096, 0x10000u };
    static const uint16_t flags[] = { VQ_DESC_F_WRITE, 0, VQ_DESC_F_WRITE | VQ_DESC_F_NEXT, 0xffff };
    static const uint64_t bad_addr[] = { 0, 1, 0x1000, 0xffffffffffffffffULL };

    for (int c = 0; c < cases; c++) {
        struct vbuf data;
        if (vbuf_alloc(&data, PAGE) != 0) { rejected++; continue; }

        struct vq_desc *d0 = vq_desc_at(&q, 0);
        struct vq_desc *d1 = vq_desc_at(&q, 1);
        uint32_t L = lens[c % (int)(sizeof lens / sizeof lens[0])];
        uint16_t fl = flags[c % (int)(sizeof flags / sizeof flags[0])];
        uint64_t addr = data.gpa;
        if ((c % 7) == 3) addr = bad_addr[c % 4];

        d0->addr = addr; d0->len = L; d0->flags = fl; d0->next = 1;
        d1->addr = data.gpa; d1->len = (c % 2) ? 16 : 0;
        d1->flags = VQ_DESC_F_WRITE; d1->next = 0;

        uint16_t *ai = vq_avail_idx(&q);
        uint16_t *ar = vq_avail_ring(&q);
        uint16_t *ui = vq_used_idx(&q);

        uint16_t before = *ui;
        ar[*ai % q.num] = 0;
        __sync_synchronize();
        *ai = (uint16_t)(*ai + 1);
        __sync_synchronize();

        vq_notify(&q, 0);

        long deadline = now_ms() + 250;
        int done = 0;
        while (now_ms() < deadline) {
            if (*ui != before) { done = 1; break; }
            usleep(200);
        }
        if (done) processed++;
        else { timedout++; rejected++; }

        printf("CASE id=%d base_off=%d addr=%#llx len=%u flags=%#x used_delta=%d %s\n",
               c, (int)(addr - data.gpa), (unsigned long long)addr, L, fl,
               (int)(*ui - before), done ? "processed" : "rejected");
        vbuf_free(&data);
    }

    printf("LAB_RESULT device=%u cases=%d processed=%d rejected=%d timed_out=%d\n",
           devid, cases, processed, rejected, timedout);
    vq_close(&q);
    return 0;
}
