#include "gbridge.h"

#include <stdint.h>
#include <stdio.h>
#include <libserialport.h>

#include "gbridge_cmd.h"

static const unsigned char handshake[] = GBRIDGE_HANDSHAKE;
static unsigned char handshake_progress;

static bool connected;
static enum gbridge_cmd waiting_cmd;

static unsigned char data_buf[GBRIDGE_MAX_DATA_SIZE];
struct gbridge_data data;
static bool data_ready;

void gbridge_init(void)
{
    connected = false;
    handshake_progress = 0;
    waiting_cmd = GBRIDGE_CMD_NONE;
    data_ready = false;
    data = (struct gbridge_data){.buffer = data_buf};
}

bool gbridge_handshake(struct sp_port *port)
{
    if (connected) return true;

    sp_blocking_write(port, handshake, sizeof(handshake), 0);
    unsigned char c;
    while (sp_blocking_read(port, &c, 1, GBRIDGE_TIMEOUT_MS) == 1) {
        if (c != handshake[handshake_progress++]) {
            handshake_progress = c == handshake[0];
        }
        if (handshake_progress == sizeof(handshake)) {
            handshake_progress = 0;
            connected = true;
            return true;
        }
    }
    return false;
}

static bool recv_data(struct sp_port *port, void *buf, size_t count)
{
    if (sp_blocking_read(port, buf, count, GBRIDGE_TIMEOUT_MS) !=
            (enum sp_return)count) {
        fprintf(stderr, "recv_data: timed out\n");
        gbridge_init();
        return false;
    }
    return true;
}

static void send_ack(struct sp_port *port, enum gbridge_cmd cmd)
{
    sp_blocking_write(port, &(char []){cmd | GBRIDGE_CMD_REPLY_F}, 1, 0);
}

static uint16_t checksum_data(struct gbridge_data data)
{
    uint16_t checksum = 0;
    for (unsigned char i = 0; i < data.size; i++) checksum += data.buffer[i];
    return checksum;
}

static void recv_cmd_debug_line(struct sp_port *port)
{
    unsigned char length;
    if (!recv_data(port, &length, 1)) return;

    unsigned char string[length];
    if (!recv_data(port, string, length)) return;

    send_ack(port, GBRIDGE_CMD_DEBUG_LINE);

    fwrite(string, length, 1, stderr);
    fputc('\n', stderr);
}

static void recv_cmd_data(struct sp_port *port)
{
    if (data_ready) {
        fprintf(stderr, "recv_cmd_data: double receive\n");
        gbridge_init();
        return;
    }

    unsigned char c[2];

    if (!recv_data(port, &data.size, 1)) return;
    if (!recv_data(port, data.buffer, data.size)) return;
    if (!recv_data(port, &c, 2)) return;
    uint16_t checksum = c[0] << 8 | c[1];
    if (checksum != checksum_data(data)) {
        // TODO: Implement retrying?
        fprintf(stderr, "recv_cmd_data: invalid checksum\n");
        gbridge_init();
        return;
    }
    data_ready = true;
    send_ack(port, GBRIDGE_CMD_DATA);
}

void gbridge_loop(struct sp_port *port)
{
    if (!connected) return;

    unsigned char cmd;
    enum sp_return rc = sp_blocking_read(port, &cmd, 1, 100);
    if (rc == 0) return;
    if (rc < 0) {
        connected = false;
        return;
    }

    if (waiting_cmd != GBRIDGE_CMD_NONE) {
        if (cmd == (waiting_cmd | GBRIDGE_CMD_REPLY_F)) {
            waiting_cmd = GBRIDGE_CMD_NONE;
        }
        return;
    }

    switch (cmd) {
    case GBRIDGE_CMD_DEBUG_LINE:
        recv_cmd_debug_line(port);
        break;
    case GBRIDGE_CMD_DATA:
        recv_cmd_data(port);
        break;
    default:
        break;
    }
}

bool gbridge_connected(void)
{
    return connected;
}

const struct gbridge_data *gbridge_recv_data(void)
{
    if (!data_ready) return NULL;
    return &data;
}

void gbridge_recv_data_done(void)
{
    data_ready = false;
}

int gbridge_recv_stream(struct sp_port *port, void *buffer, unsigned max_size)
{
    if (!connected) return -1;
    unsigned char c[2];

    if (!recv_data(port, &c, 1)) return -1;
    if (c[0] != GBRIDGE_CMD_STREAM) {
        fprintf(stderr, "recv_stream: unexpected byte\n");
        gbridge_init();
        return -1;
    }

    if (!recv_data(port, &c, 2)) return -1;
    unsigned size = c[0] << 8 | c[1];
    if (size > max_size) return -1;
    if (!recv_data(port, buffer, size)) return -1;
    if (!recv_data(port, &c, 2)) return -1;
    uint16_t checksum = c[0] << 8 | c[1];

    struct gbridge_data data = {.buffer = buffer, .size = size};
    if (checksum != checksum_data(data)) {
        // TODO: Implement retrying?
        fprintf(stderr, "recv_stream: invalid checksum\n");
        gbridge_init();
        return -1;
    }
    send_ack(port, GBRIDGE_CMD_STREAM);
    return size;
}

static void wait_cmd(struct sp_port *port, unsigned char cmd)
{
    if (!connected) return;
    while (waiting_cmd != GBRIDGE_CMD_NONE) gbridge_loop(port);
    waiting_cmd = cmd;
    while (waiting_cmd == cmd) gbridge_loop(port);
}

void gbridge_cmd_data(struct sp_port *port, struct gbridge_data data)
{
    if (!connected) return;
    if (data.size > GBRIDGE_MAX_DATA_SIZE) return;

    uint16_t checksum = checksum_data(data);

    sp_blocking_write(port, &(char []){GBRIDGE_CMD_DATA_PC, data.size}, 2, 0);
    sp_blocking_write(port, data.buffer, data.size, 0);
    sp_blocking_write(port, &(char []){checksum >> 8, checksum >> 0}, 2, 0);
    wait_cmd(port, GBRIDGE_CMD_DATA_PC);
}

void gbridge_cmd_stream(struct sp_port *port, void *buffer, unsigned size)
{
    if (!connected) return;

    uint16_t checksum = checksum_data((struct gbridge_data){.buffer=buffer,.size=size});

    sp_blocking_write(port, &(char []){GBRIDGE_CMD_STREAM_PC, size >> 8, size >> 0}, 3, 0);
    sp_blocking_write(port, buffer, size, 0);
    sp_blocking_write(port, &(char []){checksum >> 8, checksum >> 0}, 2, 0);
    wait_cmd(port, GBRIDGE_CMD_STREAM_PC);
}
