#include "shm_acl.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define ACL_EMPTY_KEY     ((uint64_t)0)
#define ACL_TOMBSTONE_KEY ((uint64_t)-1)

// Standard FNV-1a hash implementation for a composite 64-bit pair key
static inline uint32_t shm_acl_hash(uint64_t v_id, uint64_t o_id) {
    uint64_t combined = v_id ^ (o_id << 32) ^ (o_id >> 32);
    uint32_t hash = 2166136261U;
    const uint8_t *ptr = (const uint8_t*)&combined;
    for (size_t i = 0; i < sizeof(combined); i++) {
        hash ^= ptr[i];
        hash *= 16777619U;
    }
    return hash;
}

// Rounds up any unsigned 32-bit integer to the next power of 2
static uint32_t next_pow2(uint32_t n) {
    if (n < 16) return 16;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

shm_acl_t* shm_acl_create(const char *name, uint32_t capacity) {
    if (!name || capacity == 0) return NULL;

    shm_acl_t *acl = malloc(sizeof(shm_acl_t));
    if (!acl) return NULL;

    uint32_t real_capacity = next_pow2(capacity);
    size_t required_size = sizeof(shm_acl_hdr_t) + (sizeof(shm_acl_entry_t) * real_capacity);

    // Initialize the base shm layout (creates the file and runs mmap)
    acl->base = shm_base_create(name, required_size);
    if (!acl->base) {
        free(acl);
        return NULL;
    }

    acl->hdr = (shm_acl_hdr_t*)acl->base->ptr;
    acl->table = (shm_acl_entry_t*)((char*)acl->base->ptr + sizeof(shm_acl_hdr_t));

    // Clear entire map space and initialize metadata
    memset(acl->base->ptr, 0, required_size);
    acl->hdr->capacity = real_capacity;
    acl->hdr->count = 0;

    return acl;
}

void shm_acl_destroy(shm_acl_t *acl) {
    if (!acl) return;

    if (acl->base) {
        shm_base_destroy(acl->base);
    }
    free(acl);
}

uint32_t shm_acl_check_permission(shm_acl_t *acl, uint64_t visitor_id, uint64_t owner_id) {
    if (!acl || visitor_id == ACL_EMPTY_KEY) return 0;

    uint32_t cap = acl->hdr->capacity;
    uint32_t mask = cap - 1;
    uint32_t hash = shm_acl_hash(visitor_id, owner_id);
    uint32_t idx = hash & mask;

    for (uint32_t i = 0; i < cap; i++) {
        uint32_t curr_idx = (idx + i) & mask;
        shm_acl_entry_t *entry = &acl->table[curr_idx];

        // Fetch primary key with acquire barrier to sync with Python's release stores
        uint64_t curr_v = atomic_load_explicit(&entry->visitor_id, memory_order_acquire);

        if (curr_v == ACL_EMPTY_KEY) {
            return 0; // Terminating empty node found, rule definitely does not exist
        }

        if (curr_v == visitor_id) {
            uint64_t curr_o = atomic_load_explicit(&entry->owner_id, memory_order_relaxed);
            if (curr_o == owner_id) {
                return atomic_load_explicit(&entry->flags, memory_order_relaxed);
            }
        }
        // If ACL_TOMBSTONE_KEY (-1) or collision key is found, the loop continues linearly
    }

    return 0;
}
