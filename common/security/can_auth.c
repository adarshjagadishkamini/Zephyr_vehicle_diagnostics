#include "can_auth.h"
#include <mbedtls/cmac.h>
#include <mbedtls/aes.h>

#define MAC_LENGTH 8
static const uint8_t auth_key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};

int authenticate_can_message(struct can_frame *frame) {
    mbedtls_cipher_context_t ctx;
    unsigned char mac[16];
    
    // Calculate CMAC over CAN ID and data
    mbedtls_cipher_init(&ctx);
    mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB));
    mbedtls_cipher_cmac_starts(&ctx, auth_key, 128);
    
    // Include CAN ID in MAC calculation
    mbedtls_cipher_cmac_update(&ctx, (uint8_t*)&frame->id, sizeof(frame->id));
    mbedtls_cipher_cmac_update(&ctx, frame->data, frame->dlc);
    mbedtls_cipher_cmac_finish(&ctx, mac);
    
    // Append truncated MAC to CAN frame
    memcpy(&frame->data[frame->dlc], mac, MAC_LENGTH);
    frame->dlc += MAC_LENGTH;
    
    mbedtls_cipher_free(&ctx);
    return 0;
}

int verify_can_message(struct can_frame *frame) {
    mbedtls_cipher_context_t ctx;
    unsigned char mac[16];
    unsigned char received_mac[MAC_LENGTH];
    
    // Extract received MAC
    frame->dlc -= MAC_LENGTH;
    memcpy(received_mac, &frame->data[frame->dlc], MAC_LENGTH);
    
    // Calculate expected MAC
    mbedtls_cipher_init(&ctx);
    mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB));
    mbedtls_cipher_cmac_starts(&ctx, auth_key, 128);
    mbedtls_cipher_cmac_update(&ctx, (uint8_t*)&frame->id, sizeof(frame->id));
    mbedtls_cipher_cmac_update(&ctx, frame->data, frame->dlc);
    mbedtls_cipher_cmac_finish(&ctx, mac);
    
    mbedtls_cipher_free(&ctx);
    
    // Compare MACs
    return memcmp(mac, received_mac, MAC_LENGTH) == 0 ? 0 : -1;
}
