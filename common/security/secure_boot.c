#include <esp_secure_boot.h>
#include <esp_flash_encrypt.h>

int secure_boot_init(void) {
    if (!esp_secure_boot_enabled()) {
        return -1;
    }
    
    if (!esp_flash_encryption_enabled()) {
        return -2;
    }

    return 0;
}
