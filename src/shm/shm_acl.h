// shm_acl.h
#ifndef SHM_ACL_H
#define SHM_ACL_H

#include <stdint.h>
#include <stddef.h>
#include "shm_base.h"

#define ACL_FLAG_READ  (1 << 0)
#define ACL_FLAG_WRITE (1 << 1)

// Individual hash table bucket layout (strictly matches Python's struct map)
typedef struct {
    _Atomic uint64_t visitor_id;
    _Atomic uint64_t owner_id;
    _Atomic uint32_t flags;
    uint32_t padding;
} shm_acl_entry_t;

// Metadata header physically located at the very start of the SHM region
typedef struct {
    uint32_t capacity;
    uint32_t count;
} shm_acl_hdr_t;

// Runtime tracking structure inside the C-server heap
typedef struct {
    shm_base_t *base;            // Internal reference to the allocated shm_base
    shm_acl_hdr_t *hdr;          // Points directly to the start of base->ptr
    shm_acl_entry_t *table;      // Points to base->ptr + sizeof(shm_acl_hdr_t)
} shm_acl_t;

/**
 * Creates a new ACL table segment. Called ONCE by the C-server master process on startup.
 */
shm_acl_t* shm_acl_create(const char *name, uint32_t capacity);

/**
 * Destroys the tracking context and completely removes the SHM file on shutdown.
 */
void shm_acl_destroy(shm_acl_t *acl);

/**
 * Validates permission flags for visitor/owner pair. Lock-free O(1) linear probing.
 * This is the ONLY hot-path function the C-server loop actually executes.
 * @return Allowed flags bitmask, or 0 if access is denied/not found.
 */
uint32_t shm_acl_check_permission(shm_acl_t *acl, uint64_t visitor_id, uint64_t owner_id);

#endif // SHM_ACL_H
