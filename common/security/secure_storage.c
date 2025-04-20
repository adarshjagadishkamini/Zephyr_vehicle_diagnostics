#include "secure_storage.h"
#include <esp_flash_encrypt.h>
#include <nvs_flash.h>
#include <mbedtls/aes.h>

static mbedtls_aes_context aes_ctx;
static uint8_t storage_key[32];

int secure_storage_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    // Initialize encryption
    mbedtls_aes_init(&aes_ctx);
    generate_storage_key(storage_key);
    mbedtls_aes_setkey_enc(&aes_ctx, storage_key, 256);
    
    return err == ESP_OK ? 0 : -1;
}

int secure_storage_write(const char *key, const void *data, size_t len) {
    uint8_t encrypted[len];
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, len,
                          iv, data, encrypted);
                          
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return -1;
    
    err = nvs_set_blob(handle, key, encrypted, len);
    nvs_commit(handle);
    nvs_close(handle);
    
    return err == ESP_OK ? 0 : -1;
}
