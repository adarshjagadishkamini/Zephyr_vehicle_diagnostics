#ifndef DNS_CLIENT_H
#define DNS_CLIENT_H

#include <zephyr/net/dns_resolve.h>

int dns_init(void);
int dns_resolve_hostname(const char *hostname, struct sockaddr *addr);

#endif /* DNS_CLIENT_H */
