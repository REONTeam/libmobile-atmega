// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// mobile.h start
#include <stdbool.h>
#define MOBILE_MAX_CONNECTIONS 2
#define MOBILE_HOSTLEN_IPV4 4
#define MOBILE_HOSTLEN_IPV6 16
enum mobile_socktype {
    MOBILE_SOCKTYPE_TCP,
    MOBILE_SOCKTYPE_UDP,
};
enum mobile_addrtype {
    MOBILE_ADDRTYPE_NONE,
    MOBILE_ADDRTYPE_IPV4,
    MOBILE_ADDRTYPE_IPV6,
};
struct mobile_addr4 {
    enum mobile_addrtype type;
    unsigned port;
    unsigned char host[MOBILE_HOSTLEN_IPV4];
};
struct mobile_addr6 {
    enum mobile_addrtype type;
    unsigned port;
    unsigned char host[MOBILE_HOSTLEN_IPV6];
};
struct mobile_addr {
    // Make sure it's big enough to hold all types
    union {
        enum mobile_addrtype type;

        // Don't access these directly, cast instead
        struct mobile_addr4 _addr4;
        struct mobile_addr6 _addr6;
    };
};
// mobile.h end

struct socket_impl {
    int sockets[MOBILE_MAX_CONNECTIONS];
};

void socket_impl_init(struct socket_impl *state);
void socket_impl_stop(struct socket_impl *state);

bool socket_impl_open(struct socket_impl *state, unsigned conn, enum mobile_socktype socktype, enum mobile_addrtype addrtype, unsigned bindport);
void socket_impl_close(struct socket_impl *state, unsigned conn);
int socket_impl_connect(struct socket_impl *state, unsigned conn, const struct mobile_addr *addr);
bool socket_impl_listen(struct socket_impl *state, unsigned conn);
bool socket_impl_accept(struct socket_impl *state, unsigned conn);
int socket_impl_send(struct socket_impl *state, unsigned conn, const void *data, const unsigned size, const struct mobile_addr *addr);
int socket_impl_recv(struct socket_impl *state, unsigned conn, void *data, unsigned size, struct mobile_addr *addr);
