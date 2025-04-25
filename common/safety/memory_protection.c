#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/sys/crc.h>
#include "memory_protection.h"
#include "error_handler.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(memory_protection, CONFIG_SAFETY_LOG_LEVEL);

#define MAX_PROTECTED_REGIONS 16
#define MAX_MONITORED_TASKS 10
#define MAX_CALLBACKS 8
#define PAGE_SIZE 4096
#define STACK_GUARD_SIZE PAGE_SIZE
#define MEMORY_CHECK_INTERVAL_MS 1000

struct protected_region {
    void *start_addr;
    size_t size;
    uint32_t flags;
    uint32_t crc;
    bool is_redundant;
    void *redundant_addr;
};

static struct protected_region regions[MAX_PROTECTED_REGIONS];
static uint8_t num_regions = 0;

static void (*mem_callbacks[MAX_CALLBACKS])(struct mem_access_record *);
static uint8_t num_callbacks = 0;

K_THREAD_STACK_DEFINE(memory_check_stack, 2048);
static struct k_thread memory_check_thread;

static K_MUTEX_DEFINE(region_mutex);

static void memory_check_task(void *p1, void *p2, void *p3) {
    while (1) {
        periodic_memory_check();
        k_sleep(K_MSEC(MEMORY_CHECK_INTERVAL_MS));
    }
}

void memory_protection_init(void) {
    // Initialize MPU
    arch_mpu_init();
    
    k_mutex_lock(&region_mutex, K_FOREVER);
    
    // Set up default memory regions
    protect_memory_region((void *)CONFIG_FLASH_BASE_ADDRESS, 
                         CONFIG_FLASH_SIZE * 1024,
                         PROT_READ | PROT_EXECUTE);
    
    protect_memory_region((void *)CONFIG_SRAM_BASE_ADDRESS,
                         CONFIG_SRAM_SIZE * 1024,
                         PROT_READ | PROT_WRITE);
                         
    // Set up peripheral region with no-cache attribute
    protect_memory_region((void *)CONFIG_PERIPHERAL_BASE_ADDRESS,
                         CONFIG_PERIPHERAL_SIZE,
                         PROT_READ | PROT_WRITE | PROT_NO_CACHE);
    
    k_mutex_unlock(&region_mutex);
    
    // Start memory check task
    k_thread_create(&memory_check_thread, memory_check_stack,
                   K_THREAD_STACK_SIZEOF(memory_check_stack),
                   memory_check_task,
                   NULL, NULL, NULL,
                   K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
}

static bool is_aligned(void *addr, size_t alignment) {
    return (((uintptr_t)addr) & (alignment - 1)) == 0;
}

static bool regions_overlap(void *addr1, size_t size1, void *addr2, size_t size2) {
    uintptr_t start1 = (uintptr_t)addr1;
    uintptr_t end1 = start1 + size1;
    uintptr_t start2 = (uintptr_t)addr2;
    uintptr_t end2 = start2 + size2;
    
    return (start1 < end2) && (start2 < end1);
}

int protect_memory_region(void *addr, size_t size, uint32_t flags) {
    if (!is_aligned(addr, PAGE_SIZE)) {
        return MEM_ERR_NOT_ALIGNED;
    }
    
    k_mutex_lock(&region_mutex, K_FOREVER);
    
    if (num_regions >= MAX_PROTECTED_REGIONS) {
        k_mutex_unlock(&region_mutex);
        return MEM_ERR_NO_MEMORY;
    }
    
    // Check for overlapping regions
    for (int i = 0; i < num_regions; i++) {
        if (regions_overlap(addr, size, regions[i].start_addr, regions[i].size)) {
            k_mutex_unlock(&region_mutex);
            return MEM_ERR_OVERLAP;
        }
    }
    
    // Configure MPU region
    uint32_t mpu_flags = 0;
    if (flags & PROT_READ) mpu_flags |= ARM_MPU_REGION_READ;
    if (flags & PROT_WRITE) mpu_flags |= ARM_MPU_REGION_WRITE;
    if (flags & PROT_EXECUTE) mpu_flags |= ARM_MPU_REGION_EXEC;
    if (flags & PROT_PRIVILEGED) mpu_flags |= ARM_MPU_REGION_PRIV;
    if (flags & PROT_NO_CACHE) mpu_flags |= ARM_MPU_REGION_NO_CACHE;
    
    arm_mpu_region_attr_t attr = ARM_MPU_REGION(num_regions, (uintptr_t)addr, size, mpu_flags);
    arm_mpu_set_region(num_regions, &attr);
    
    // Store region info
    regions[num_regions].start_addr = addr;
    regions[num_regions].size = size;
    regions[num_regions].flags = flags;
    regions[num_regions].crc = crc32_ieee(addr, size);
    regions[num_regions].is_redundant = false;
    
    num_regions++;
    k_mutex_unlock(&region_mutex);
    
    return 0;
}

int setup_stack_protection(uint8_t *stack_ptr, size_t stack_size) {
    // Add guard pages at stack boundaries
    int ret = protect_memory_region(stack_ptr, STACK_GUARD_SIZE, 0); // No access
    if (ret != 0) return ret;
    
    return protect_memory_region(stack_ptr + stack_size - STACK_GUARD_SIZE, 
                               STACK_GUARD_SIZE, 0); // No access
}

int setup_redundant_memory(void *primary, void *secondary, size_t size) {
    k_mutex_lock(&region_mutex, K_FOREVER);
    
    if (num_regions >= MAX_PROTECTED_REGIONS - 1) { // Need space for both regions
        k_mutex_unlock(&region_mutex);
        return MEM_ERR_NO_MEMORY;
    }
    
    // Set up primary region
    int ret = protect_memory_region(primary, size, PROT_READ | PROT_WRITE);
    if (ret != 0) {
        k_mutex_unlock(&region_mutex);
        return ret;
    }
    
    // Set up secondary region
    ret = protect_memory_region(secondary, size, PROT_READ | PROT_WRITE);
    if (ret != 0) {
        k_mutex_unlock(&region_mutex);
        return ret;
    }
    
    // Mark as redundant pair
    regions[num_regions-2].is_redundant = true;
    regions[num_regions-2].redundant_addr = secondary;
    regions[num_regions-1].is_redundant = true;
    regions[num_regions-1].redundant_addr = primary;
    
    k_mutex_unlock(&region_mutex);
    return 0;
}

bool verify_redundant_memory(void *primary, void *secondary, size_t size) {
    return memcmp(primary, secondary, size) == 0;
}

static void notify_memory_access(struct mem_access_record *record) {
    for (int i = 0; i < num_callbacks; i++) {
        if (mem_callbacks[i]) {
            mem_callbacks[i](record);
        }
    }
}

void monitor_memory_access(void *addr, size_t size, uint32_t access_type) {
    struct mem_access_record record = {
        .addr = addr,
        .access_type = access_type,
        .timestamp = k_uptime_get(),
        .task_id = k_current_get(),
        .caller_addr = (uint32_t)__builtin_return_address(0)
    };
    
    notify_memory_access(&record);
}

void periodic_memory_check(void) {
    k_mutex_lock(&region_mutex, K_FOREVER);
    
    for (int i = 0; i < num_regions; i++) {
        if (!verify_memory_region(regions[i].start_addr, 
                                regions[i].size,
                                regions[i].crc)) {
            LOG_ERR("Memory corruption detected in region %d", i);
            handle_memory_violation(regions[i].start_addr);
        }
        
        if (regions[i].is_redundant) {
            if (!verify_redundant_memory(regions[i].start_addr,
                                       regions[i].redundant_addr,
                                       regions[i].size)) {
                LOG_ERR("Redundant memory mismatch in region %d", i);
                handle_memory_violation(regions[i].start_addr);
            }
        }
    }
    
    k_mutex_unlock(&region_mutex);
}

void memory_violation_handler(void) {
    void *fault_address = (void *)SCB->MMFAR;
    
    LOG_ERR("Memory Protection Fault at address: %p", fault_address);
    
    k_mutex_lock(&region_mutex, K_FOREVER);
    
    // Check if address is in any protected region
    for (int i = 0; i < num_regions; i++) {
        if (fault_address >= regions[i].start_addr && 
            fault_address < (void *)((uintptr_t)regions[i].start_addr + regions[i].size)) {
            LOG_ERR("Violation in protected region %d", i);
            break;
        }
    }
    
    k_mutex_unlock(&region_mutex);
    
    // Handle violation
    handle_memory_violation(fault_address);
    
    // Reset if unrecoverable
    if (is_critical_memory_region(fault_address)) {
        sys_reboot(SYS_REBOOT_COLD);
    }
}

const char *get_memory_error_string(int error_code) {
    switch (error_code) {
        case 0:
            return "Success";
        case MEM_ERR_INVALID_REGION:
            return "Invalid memory region";
        case MEM_ERR_NO_MEMORY:
            return "No memory available for region";
        case MEM_ERR_INVALID_PERM:
            return "Invalid permissions";
        case MEM_ERR_OVERLAP:
            return "Region overlap";
        case MEM_ERR_NOT_ALIGNED:
            return "Address not aligned";
        default:
            return "Unknown error";
    }
}
