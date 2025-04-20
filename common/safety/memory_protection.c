#include "memory_protection.h"
#include <zephyr/arch/cpu.h>
#include <zephyr/sys/crc.h>

#define MAX_PROTECTED_REGIONS 8
#define MAX_MONITORED_TASKS 10
#define PAGE_SIZE 4096

struct protected_region {
    void *start_addr;
    size_t size;
    uint32_t flags;
    uint32_t crc;
};

static struct protected_region regions[MAX_PROTECTED_REGIONS];
static uint8_t num_regions = 0;

void memory_protection_init(void) {
    // Initialize MPU
    arch_mpu_init();
    
    // Set up default memory regions
    protect_memory_region(CONFIG_FLASH_BASE_ADDRESS, 
                         CONFIG_FLASH_SIZE * 1024,
                         PROT_READ | PROT_EXECUTE);
    
    protect_memory_region(CONFIG_SRAM_BASE_ADDRESS,
                         CONFIG_SRAM_SIZE * 1024,
                         PROT_READ | PROT_WRITE);
}

int protect_memory_region(void *addr, size_t size, uint32_t flags) {
    if (num_regions >= MAX_PROTECTED_REGIONS) {
        return -ENOMEM;
    }

    // Configure MPU region
    arch_mpu_region_init(num_regions, (uintptr_t)addr, size, flags);
    
    // Store region info
    regions[num_regions].start_addr = addr;
    regions[num_regions].size = size;
    regions[num_regions].flags = flags;
    regions[num_regions].crc = crc32_ieee(addr, size);
    
    num_regions++;
    return 0;
}

// Add stack guard feature
void setup_stack_guard(void) {
    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
        uint8_t *stack_bottom = task_stacks[i];
        // Add guard pages at stack boundaries
        arch_mpu_region_init(num_regions++, 
                           (uintptr_t)stack_bottom,
                           PAGE_SIZE,
                           MPU_REGION_NO_ACCESS);
    }
}

// Add memory access monitoring
void monitor_memory_access(void *addr, size_t size, uint32_t access_type) {
    struct mem_access_record {
        void *addr;
        uint32_t access_type;
        uint64_t timestamp;
    } record;

    record.addr = addr;
    record.access_type = access_type;
    record.timestamp = k_uptime_get();

    log_memory_access(&record);
}

bool verify_memory_region(void *addr, size_t size, uint32_t expected_crc)
{
    uint32_t current_crc = crc32_ieee(addr, size);
    return current_crc == expected_crc;
}

void handle_memory_violation(void *addr)
{
    // Log violation
    log_safety_event("Memory violation", (uint32_t)addr);
    
    // Enter safe state
    enter_safe_state();
    
    // Attempt recovery
    if (verify_memory_integrity()) {
        restore_safe_configuration();
    } else {
        trigger_system_reset();
    }
}

void memory_violation_handler(void) {
    void *fault_address = (void *)SCB->MMFAR;
    
    // Log violation details
    LOG_ERR("Memory Protection Fault at address: %p", fault_address);
    
    // Check if address is in any protected region
    for (int i = 0; i < num_regions; i++) {
        if (fault_address >= regions[i].start_addr && 
            fault_address < (regions[i].start_addr + regions[i].size)) {
            LOG_ERR("Violation in protected region %d", i);
            break;
        }
    }
    
    // Handle violation
    handle_memory_violation(fault_address);
    
    // Reset if unrecoverable
    if (is_critical_memory_region(fault_address)) {
        sys_reboot(SYS_REBOOT_COLD);
    }
}

bool verify_memory_integrity(void) {
    for (int i = 0; i < num_regions; i++) {
        if (!verify_memory_region(regions[i].start_addr, 
                                regions[i].size,
                                regions[i].crc)) {
            return false;
        }
    }
    return true;
}
