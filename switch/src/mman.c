/*
 * mmap for Horizon, as far as it can go.
 *
 * The runtime's live recompiler (the part that runs code mods) asks mmap for memory it can write and then run.
 * Horizon gives a program no such memory through this door: it has a separate JIT service, which the port does
 * not use yet. So executable requests fail, and a code mod is reported as failing to load instead of crashing
 * the console. Plain anonymous memory is served from the heap.
 */
#include <sys/mman.h>

#include <errno.h>
#include <malloc.h>
#include <string.h>

#define PAGE_SIZE 0x1000

void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset) {
    (void)address;
    (void)offset;

    if (length == 0 || fd != -1 || !(flags & MAP_ANONYMOUS) || (protection & PROT_EXEC)) {
        errno = ENOTSUP;
        return MAP_FAILED;
    }

    const size_t size = (length + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
    void *memory = memalign(PAGE_SIZE, size);
    if (memory == NULL) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    memset(memory, 0, size);
    return memory;
}

/* Whole mappings only: the heap cannot give back part of a block. */
int munmap(void *address, size_t length) {
    (void)length;
    free(address);
    return 0;
}

int mprotect(void *address, size_t length, int protection) {
    (void)address;
    (void)length;

    if (protection & PROT_EXEC) {
        errno = ENOTSUP;
        return -1;
    }
    return 0;
}
