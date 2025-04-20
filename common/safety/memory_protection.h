#ifndef MEMORY_PROTECTION_H
#define MEMORY_PROTECTION_H

#include <zephyr/kernel.h>

// Memory regions
#define REGION_FLASH      0
#define REGION_RAM       1
#define REGION_CRITICAL  2

// Protection flags
#define PROT_READ     (1 << 0)
#define PROT_WRITE    (1 << 1)
#define PROT_EXECUTE  (1 << 2)

void memory_protection_init(void);
int protect_memory_region(void *addr, size_t size, uint32_t flags);
bool verify_memory_region(void *addr, size_t size, uint32_t expected_crc);
void handle_memory_violation(void *addr);

#endif /* MEMORY_PROTECTION_H */
