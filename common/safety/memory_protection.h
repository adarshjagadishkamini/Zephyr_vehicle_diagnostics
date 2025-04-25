#ifndef MEMORY_PROTECTION_H
#define MEMORY_PROTECTION_H

#include <zephyr/kernel.h>
#include <zephyr/arch/arm/aarch32/mpu/arm_mpu.h>

// Memory regions
#define REGION_FLASH          0
#define REGION_RAM           1
#define REGION_CRITICAL      2
#define REGION_PERIPHERAL    3
#define REGION_STACK_GUARD   4
#define REGION_SECURE        5

// Protection flags
#define PROT_READ           (1 << 0)
#define PROT_WRITE          (1 << 1)
#define PROT_EXECUTE        (1 << 2)
#define PROT_PRIVILEGED     (1 << 3)
#define PROT_SECURE         (1 << 4)
#define PROT_NO_CACHE       (1 << 5)

// Memory access types for monitoring
#define ACCESS_READ         0x01
#define ACCESS_WRITE        0x02
#define ACCESS_EXECUTE      0x04
#define ACCESS_PRIVILEGED   0x08

// Error codes
#define MEM_ERR_INVALID_REGION    -1
#define MEM_ERR_NO_MEMORY         -2
#define MEM_ERR_INVALID_PERM      -3
#define MEM_ERR_OVERLAP           -4
#define MEM_ERR_NOT_ALIGNED       -5

// Memory protection configuration
struct mpu_config {
    uint32_t base_addr;
    uint32_t size;
    uint32_t attributes;
    bool enabled;
};

// Memory access monitoring record
struct mem_access_record {
    void *addr;
    uint32_t access_type;
    uint64_t timestamp;
    uint32_t task_id;
    uint32_t caller_addr;
};

// Function prototypes
void memory_protection_init(void);
int protect_memory_region(void *addr, size_t size, uint32_t flags);
bool verify_memory_region(void *addr, size_t size, uint32_t expected_crc);
void handle_memory_violation(void *addr);
int setup_stack_protection(uint8_t *stack_ptr, size_t stack_size);
int configure_secure_region(void *addr, size_t size);
void monitor_memory_access(void *addr, size_t size, uint32_t access_type);
bool verify_memory_bounds(void *addr, size_t size);
int register_memory_callback(void (*callback)(struct mem_access_record *));
void memory_violation_handler(void);
const char *get_memory_error_string(int error_code);

// ASIL-B specific protections
int setup_redundant_memory(void *primary, void *secondary, size_t size);
bool verify_redundant_memory(void *primary, void *secondary, size_t size);
int protect_critical_data(void *addr, size_t size);
void periodic_memory_check(void);

#endif /* MEMORY_PROTECTION_H */
