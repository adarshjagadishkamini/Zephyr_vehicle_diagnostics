#ifndef SECURE_STORAGE_H
#define SECURE_STORAGE_H

#include <zephyr/kernel.h>

int secure_storage_init(void);
int secure_storage_write(const char *key, const void *data, size_t len);
int secure_storage_read(const char *key, void *data, size_t len);

#endif /* SECURE_STORAGE_H */
