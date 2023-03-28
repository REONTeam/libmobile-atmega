#include "gbridge_prot_ma.h"

#include <string.h>

#include <mobile.h>

#include "gbridge.h"
#include "gbridge_cmd.h"
#include "gbridge_prot_ma_cmd.h"

static unsigned char data_buf[GBRIDGE_MAX_DATA_SIZE];
static struct gbridge_data data;

void gbridge_prot_ma_init(void)
{
    data.buffer = data_buf;
    data.size = 0;
}

void gbridge_prot_ma_loop(void)
{
    const struct gbridge_data *recv_data = gbridge_recv_data();
    if (!recv_data) return;

    // Nothing to do with this yet...

    gbridge_recv_data_done();
    return;
}

#define ADDRESS_MAXLEN (3 + MOBILE_HOSTLEN_IPV6)
static unsigned address_write(const struct mobile_addr *addr, unsigned char *buffer)
{
    if (!addr) {
        buffer[0] = MOBILE_ADDRTYPE_NONE;
        return 1;
    } else if (addr->type == MOBILE_ADDRTYPE_IPV4) {
        struct mobile_addr4 *addr4 = (struct mobile_addr4 *)addr;
        buffer[0] = MOBILE_ADDRTYPE_IPV4;
        buffer[1] = (addr4->port >> 8) & 0xFF;
        buffer[2] = (addr4->port >> 0) & 0xFF;
        memcpy(buffer + 3, addr4->host, MOBILE_HOSTLEN_IPV4);
        return 3 + MOBILE_HOSTLEN_IPV4;
    } else if (addr->type == MOBILE_ADDRTYPE_IPV6) {
        struct mobile_addr6 *addr6 = (struct mobile_addr6 *)addr;
        buffer[0] = MOBILE_ADDRTYPE_IPV6;
        buffer[1] = (addr6->port >> 8) & 0xFF;
        buffer[2] = (addr6->port >> 0) & 0xFF;
        memcpy(buffer + 3, addr6->host, MOBILE_HOSTLEN_IPV6);
        return 3 + MOBILE_HOSTLEN_IPV6;
    } else {
        buffer[0] = MOBILE_ADDRTYPE_NONE;
        return 1;
    }
}

static unsigned address_read(struct mobile_addr *addr, const unsigned char *buffer, unsigned size)
{
    if (size < 1) return 0;
    if (buffer[0] == MOBILE_ADDRTYPE_NONE) {
        if (addr) {
            addr->type = MOBILE_ADDRTYPE_NONE;
        }
        return 1;
    } else if (buffer[0] == MOBILE_ADDRTYPE_IPV4) {
        if (size < 3 + MOBILE_HOSTLEN_IPV4) return 0;
        if (addr) {
            addr->type = MOBILE_ADDRTYPE_IPV4;
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)addr;
            addr4->port = buffer[1] << 8 | buffer[2];
            memcpy(addr4->host, buffer + 3, MOBILE_HOSTLEN_IPV4);
        }
        return 3 + MOBILE_HOSTLEN_IPV4;
    } else if (buffer[0] == MOBILE_ADDRTYPE_IPV6) {
        if (size < 3 + MOBILE_HOSTLEN_IPV6) return 0;
        if (addr) {
            addr->type = MOBILE_ADDRTYPE_IPV6;
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)addr;
            addr6->port = buffer[1] << 8 | buffer[2];
            memcpy(addr6->host, buffer + 3, MOBILE_HOSTLEN_IPV6);
        }
        return 3 + MOBILE_HOSTLEN_IPV6;
    }
    return 0;
}

bool mobile_impl_sock_open(void *user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_OPEN;
    data.buffer[1] = conn;
    data.buffer[2] = type;
    data.buffer[3] = addrtype;
    data.buffer[4] = (bindport >> 8) & 0xFF;
    data.buffer[5] = (bindport >> 0) & 0xFF;
    data.size = 6;
    gbridge_cmd_data(data);

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size != 2) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_OPEN) goto error;
    bool res = recv_data->buffer[1] != 0;
    gbridge_recv_data_done();
    return res;

error:
    gbridge_recv_data_done();
    return false;
}

void mobile_impl_sock_close(void *user, unsigned conn)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_CLOSE;
    data.buffer[1] = conn;
    data.size = 2;
    gbridge_cmd_data(data);

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size != 1) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_CLOSE) goto error;

error:
    gbridge_recv_data_done();
}

int mobile_impl_sock_connect(void *user, unsigned conn, const struct mobile_addr *addr)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_CONNECT;
    data.buffer[1] = conn;
    unsigned addrlen = address_write(addr, data.buffer + 2);
    data.size = 2 + addrlen;
    gbridge_cmd_data(data);

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size != 2) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_CONNECT) goto error;
    int res = (char)recv_data->buffer[1];
    gbridge_recv_data_done();
    return res;

error:
    gbridge_recv_data_done();
    return -1;
}

bool mobile_impl_sock_listen(void *user, unsigned conn)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_LISTEN;
    data.buffer[1] = conn;
    data.size = 2;
    gbridge_cmd_data(data);

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size != 2) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_LISTEN) goto error;
    bool res = recv_data->buffer[1];
    gbridge_recv_data_done();
    return res;

error:
    gbridge_recv_data_done();
    return false;
}

bool mobile_impl_sock_accept(void *user, unsigned conn)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_ACCEPT;
    data.buffer[1] = conn;
    data.size = 2;
    gbridge_cmd_data(data);

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size != 2) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_ACCEPT) goto error;
    bool res = recv_data->buffer[1];
    gbridge_recv_data_done();
    return res;

error:
    gbridge_recv_data_done();
    return false;
}

int mobile_impl_sock_send(void *user, unsigned conn, const void *buffer, unsigned size, const struct mobile_addr *addr)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_SEND;
    data.buffer[1] = conn;
    unsigned addrlen = address_write(addr, data.buffer + 2);
    data.size = addrlen + 2;
    gbridge_cmd_data(data);
    if (!gbridge_connected()) goto error;

    gbridge_cmd_stream_start(size);
    gbridge_cmd_stream_data(buffer, size);
    gbridge_cmd_stream_finish();

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size != 3) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_SEND) goto error;
    int res = (int16_t)(recv_data->buffer[1] << 8 | recv_data->buffer[2]);
    gbridge_recv_data_done();
    return res;

error:
    gbridge_recv_data_done();
    return -1;
}

int mobile_impl_sock_recv(void *user, unsigned conn, void *buffer, unsigned size, struct mobile_addr *addr)
{
    (void)user;
    data.buffer[0] = GBRIDGE_PROT_MA_CMD_RECV;
    data.buffer[1] = conn;
    data.buffer[2] = size >> 8;
    data.buffer[3] = size >> 0;
    data.size = 4;
    gbridge_cmd_data(data);

    const struct gbridge_data *recv_data = gbridge_recv_data_wait();
    if (!recv_data) goto error;
    if (recv_data->size < 2) goto error;
    if (recv_data->buffer[0] != GBRIDGE_PROT_MA_CMD_RECV) goto error;
    int res = (int16_t)(recv_data->buffer[1] << 8 | recv_data->buffer[2]);

    unsigned recv_addrlen = address_read(addr, recv_data->buffer + 3,
        recv_data->size - 3);
    if (!recv_addrlen) goto error;
    if (recv_data->size != recv_addrlen + 3) goto error;
    gbridge_recv_data_done();

    if (res <= 0) return res;
    if (!gbridge_recv_stream_wait(buffer, size)) return -1;
    return res;

error:
    gbridge_recv_data_done();
    return -1;
}
