#include "dns_client.h"
#include <zephyr/net/dns_resolve.h>

#define DNS_RESOLVE_TIMEOUT_MS 5000

static struct dns_resolve_context dns_ctx;

int dns_init(void) {
    return dns_resolve_init(&dns_ctx);
}

int dns_resolve_hostname(const char *hostname, struct sockaddr *addr) {
    struct dns_addrinfo info;
    int ret;

    ret = dns_resolve_name(&dns_ctx, hostname, DNS_QUERY_TYPE_A,
                          K_MSEC(DNS_RESOLVE_TIMEOUT_MS), &info);
    if (ret == 0) {
        memcpy(addr, &info.ai_addr, sizeof(struct sockaddr));
    }
    return ret;
}
