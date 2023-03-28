#include "gbridge_prot_ma.h"

#include <string.h>

#include "gbridge.h"
#include "gbridge_cmd.h"
#include "gbridge_prot_ma_cmd.h"
#include "socket_impl.h"

static unsigned char data_buf[GBRIDGE_MAX_DATA_SIZE];
static struct gbridge_data data;
static struct socket_impl socket;

void gbridge_prot_ma_init(void)
{
    data.buffer = data_buf;
    data.size = 0;
    socket_impl_init(&socket);
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

static bool recv_cmd_sock_open(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size != 6) return false;

    unsigned conn = recv_data->buffer[1];
    enum mobile_socktype socktype = recv_data->buffer[2];
    enum mobile_addrtype addrtype = recv_data->buffer[3];
    unsigned bindport = recv_data->buffer[4] << 8 | recv_data->buffer[5];

    bool res = socket_impl_open(&socket, conn, socktype, addrtype,
        bindport);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_OPEN;
    data.buffer[1] = res;
    data.size = 2;
    gbridge_cmd_data(port, data);
    return true;
}

static bool recv_cmd_sock_close(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size != 2) return false;

    unsigned conn = recv_data->buffer[1];

    socket_impl_close(&socket, conn);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_CLOSE;
    data.size = 1;
    gbridge_cmd_data(port, data);
    return true;
}

static bool recv_cmd_sock_connect(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size < 2) return false;
    unsigned conn = recv_data->buffer[1];

    struct mobile_addr recv_addr;
    unsigned recv_addrlen = address_read(&recv_addr, recv_data->buffer + 2,
        recv_data->size - 2);
    if (recv_addrlen <= 1) return false;
    if (recv_data->size != 2 + recv_addrlen) return false;

    int res = socket_impl_connect(&socket, conn, &recv_addr);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_CONNECT;
    data.buffer[1] = res;
    data.size = 2;
    gbridge_cmd_data(port, data);
    return true;
}

static bool recv_cmd_sock_listen(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size != 2) return false;
    unsigned conn = recv_data->buffer[1];

    bool res = socket_impl_listen(&socket, conn);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_LISTEN;
    data.buffer[1] = res;
    data.size = 2;
    gbridge_cmd_data(port, data);
    return true;
}

static bool recv_cmd_sock_accept(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size != 2) return false;
    unsigned conn = recv_data->buffer[1];

    bool res = socket_impl_accept(&socket, conn);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_ACCEPT;
    data.buffer[1] = res;
    data.size = 2;
    gbridge_cmd_data(port, data);
    return true;
}

static bool recv_cmd_sock_send(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size < 2) return false;

    unsigned conn = recv_data->buffer[1];

    struct mobile_addr recv_addr;
    unsigned recv_addrlen = address_read(&recv_addr, recv_data->buffer + 2,
        recv_data->size - 2);
    if (!recv_addrlen) return false;

    unsigned char recv_buffer[0x200];
    int stream_res = gbridge_recv_stream(port, recv_buffer, sizeof(recv_buffer));
    if (stream_res < 0) return false;
    unsigned recv_size = stream_res;

    int res = socket_impl_send(&socket, conn, recv_buffer,
        recv_size, &recv_addr);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_SEND;
    data.buffer[1] = res >> 8;
    data.buffer[2] = res >> 0;
    data.size = 3;
    gbridge_cmd_data(port, data);
    return true;
}

static bool recv_cmd_sock_recv(const struct gbridge_data *recv_data, struct sp_port *port)
{
    if (recv_data->size != 4) return false;
    unsigned conn = recv_data->buffer[1];
    unsigned size = recv_data->buffer[2] << 8 | recv_data->buffer[3];

    unsigned char buffer[size];
    struct mobile_addr recv_addr = {0};

    int res = socket_impl_recv(&socket, conn, buffer, size, &recv_addr);

    data.buffer[0] = GBRIDGE_PROT_MA_CMD_RECV;
    data.buffer[1] = res >> 8;
    data.buffer[2] = res >> 0;
    unsigned addrlen = address_write(&recv_addr, data.buffer + 3);
    data.size = addrlen + 3;
    gbridge_cmd_data(port, data);
    if (!gbridge_connected()) return false;

    if (res <= 0) return true;
    gbridge_cmd_stream(port, buffer, res);
    return true;
}

void gbridge_prot_ma_loop(struct sp_port *port)
{
    const struct gbridge_data *recv_data = gbridge_recv_data();
    if (!recv_data) return;
    if (recv_data->size < 1) goto error;

    switch (recv_data->buffer[0]) {
    case GBRIDGE_PROT_MA_CMD_OPEN:
        recv_cmd_sock_open(recv_data, port);
        break;
    case GBRIDGE_PROT_MA_CMD_CLOSE:
        recv_cmd_sock_close(recv_data, port);
        break;
    case GBRIDGE_PROT_MA_CMD_CONNECT:
        recv_cmd_sock_connect(recv_data, port);
        break;
    case GBRIDGE_PROT_MA_CMD_LISTEN:
        recv_cmd_sock_listen(recv_data, port);
        break;
    case GBRIDGE_PROT_MA_CMD_ACCEPT:
        recv_cmd_sock_accept(recv_data, port);
        break;
    case GBRIDGE_PROT_MA_CMD_SEND:
        recv_cmd_sock_send(recv_data, port);
        break;
    case GBRIDGE_PROT_MA_CMD_RECV:
        recv_cmd_sock_recv(recv_data, port);
        break;
    }

error:
    gbridge_recv_data_done();
    return;
}
