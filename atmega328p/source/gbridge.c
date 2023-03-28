#include "gbridge.h"

#include <string.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "gbridge_cmd.h"
#include "serial.h"
#include "timer.h"

static bool connected;

static const unsigned char handshake[] PROGMEM = GBRIDGE_HANDSHAKE;
static unsigned char handshake_progress;

static enum gbridge_cmd processing_cmd;
static unsigned char processing_cmd_state;
static uint32_t processing_cmd_time;

static enum gbridge_cmd waiting_cmd;
static uint32_t waiting_cmd_time;

static unsigned char data_buf[GBRIDGE_MAX_DATA_SIZE];
static struct gbridge_data data;
static unsigned char data_len;
static unsigned char data_cur;
static bool data_ready;

static struct gbridge_data stream_recv;
static unsigned stream_max_size;
static unsigned stream_cur;
static unsigned stream_checksum;

void gbridge_init(void)
{
    connected = false;
    handshake_progress = 0;
    waiting_cmd = GBRIDGE_CMD_NONE;
    processing_cmd = GBRIDGE_CMD_NONE;
    data_ready = false;
    data = (struct gbridge_data){.buffer = data_buf};
    stream_max_size = 0;
}

static bool do_handshake(void)
{
    if (!serial_available()) return false;

    unsigned char c = serial_getchar();
    if (pgm_read_byte(handshake + handshake_progress) != c) {
        handshake_progress = c == pgm_read_byte(handshake + 0);
        return false;
    }
    if (++handshake_progress != sizeof(handshake)) return false;

    handshake_progress = 0;

    // Reply handshake
    for (unsigned char i = 0; i < sizeof(handshake); i++) {
        serial_putchar(pgm_read_byte(handshake + i));
    }
    return true;
}

static void checksum_add(uint16_t *checksum, const unsigned char *data, unsigned char size)
{
    for (unsigned char i = 0; i < size; i++) *checksum += data[i];
}

static uint16_t checksum_data(struct gbridge_data data)
{
    uint16_t checksum = 0;
    for (unsigned char i = 0; i < data.size; i++) checksum += data.buffer[i];
    return checksum;
}

static char recv_cmd_reset(void)
{
    gbridge_init();
    serial_putchar(GBRIDGE_CMD_RESET | GBRIDGE_CMD_REPLY_F);
    return 1;
}

static char recv_cmd_data_pc(void)
{
    if (data_ready) return -1;

    uint16_t checksum;

    switch (processing_cmd_state) {
    case 0:
        if (!serial_available()) break;
        data_len = serial_getchar();
        if (data_len > GBRIDGE_MAX_DATA_SIZE) return -1;

        data_cur = 0;
        processing_cmd_state = 1;
        // fallthrough
    case 1:
        while (serial_available()) {
            data.buffer[data_cur++] = serial_getchar();
            if (data_cur >= data_len) {
                data.size = data_len;
                processing_cmd_state = 2;
                break;
            }
        }
        if (processing_cmd_state == 1) break;
        // fallthrough
    case 2:
        if (serial_available() < 2) break;

        checksum = serial_getchar() << 8;
        checksum |= serial_getchar() << 0;

        if (checksum != checksum_data(data)) {
            // TODO: Implement retrying?
            return -1;
        }
        serial_putchar(GBRIDGE_CMD_DATA_PC | GBRIDGE_CMD_REPLY_F);
        data_ready = true;
        return 1;
    }
    return 0;
}

static char recv_cmd_stream_pc(void)
{
    if (!stream_max_size) return -1;

    uint16_t checksum;

    switch (processing_cmd_state) {
    case 0:
        if (serial_available() < 2) break;
        stream_recv.size = serial_getchar() << 8;
        stream_recv.size |= serial_getchar() << 0;
        if (stream_recv.size > stream_max_size) return -1;

        stream_cur = 0;
        processing_cmd_state = 1;
        // fallthrough
    case 1:
        while (serial_available()) {
            stream_recv.buffer[stream_cur++] = serial_getchar();
            if (stream_cur >= stream_recv.size) {
                processing_cmd_state = 2;
                break;
            }
        }
        if (processing_cmd_state == 1) break;
        // fallthrough
    case 2:
        if (serial_available() < 2) break;

        checksum = serial_getchar() << 8;
        checksum |= serial_getchar() << 0;

        if (checksum != checksum_data(stream_recv)) {
            // TODO: Implement retrying?
            return -1;
        }
        serial_putchar(GBRIDGE_CMD_STREAM_PC | GBRIDGE_CMD_REPLY_F);
        stream_max_size = 0;
        return 1;
    }
    return 0;
}

void gbridge_loop(void)
{
    // TODO: Decouple receiving from sending

    // Make sure we've passed the handshake first
    if (!connected) {
        if (!do_handshake()) return;
        connected = true;
    }

    // Handle timeout
    if (processing_cmd != GBRIDGE_CMD_NONE &&
            timer_get() - processing_cmd_time > GBRIDGE_TIMEOUT_US) {
        gbridge_init();
        return;
    }
    if (waiting_cmd != GBRIDGE_CMD_NONE &&
            timer_get() - waiting_cmd_time > GBRIDGE_TIMEOUT_US) {
        gbridge_init();
        return;
    }

    // Wait until we receive a command
    if (processing_cmd == GBRIDGE_CMD_NONE) {
        if (!serial_available()) return;
        enum gbridge_cmd cmd = serial_getchar();
        if (cmd == GBRIDGE_CMD_NONE) return;

        // If wait_cmd() has been called, process that
        if (waiting_cmd != GBRIDGE_CMD_NONE) {
            if (cmd == (waiting_cmd | GBRIDGE_CMD_REPLY_F)) {
                waiting_cmd = GBRIDGE_CMD_NONE;
            }
            return;
        }

        processing_cmd = cmd;
        processing_cmd_state = 0;
        processing_cmd_time = timer_get();
    }

    char rc;
    switch (processing_cmd) {
    case GBRIDGE_CMD_RESET:
        rc = recv_cmd_reset();
        break;
    case GBRIDGE_CMD_DATA_PC:
        rc = recv_cmd_data_pc();
        break;
    case GBRIDGE_CMD_STREAM_PC:
        rc = recv_cmd_stream_pc();
        break;
    default:
        rc = 1;
        break;
    }
    if (rc == 1) processing_cmd = GBRIDGE_CMD_NONE;
    if (rc == -1) gbridge_init();
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

const struct gbridge_data *gbridge_recv_data_wait(void)
{
    const struct gbridge_data *data = NULL;
    while (connected) {
        if ((data = gbridge_recv_data())) break;
        gbridge_loop();
    }
    return data;
}

void gbridge_recv_data_done(void)
{
    data_ready = false;
}

// Initialize stream receive buffer when it's expected
void gbridge_recv_stream(void *buffer, unsigned max_size)
{
    stream_recv.buffer = buffer;
    stream_recv.size = 0;
    stream_max_size = max_size;
    stream_cur = 0;
    stream_checksum = 0;
}

// Check if the stream has been fully received
bool gbridge_recv_stream_done(void)
{
    return !stream_max_size;
}

// Receive a stream packet into a buffer with a given size
bool gbridge_recv_stream_wait(void *buffer, unsigned max_size)
{
    gbridge_recv_stream(buffer, max_size);
    while (connected) {
        if (gbridge_recv_stream_done()) return true;
        gbridge_loop();
    }
    return false;
}

// Wait until the bridge confirms that a message has been received
// This function blocks until the relay confirms this, or a timeout occurs
static void wait_cmd(unsigned char cmd)
{
    if (!connected) return;

    // If a previous command is being waited on, wait
    while (waiting_cmd != GBRIDGE_CMD_NONE) gbridge_loop();

    // Wait for this command
    waiting_cmd = cmd;
    waiting_cmd_time = timer_get();
    while (waiting_cmd == cmd) gbridge_loop();
}

// Send a debug message
void gbridge_cmd_debug_line(const char *line)
{
    if (!connected) return;

    size_t length = strlen(line);
    if (length > 0xff) return;

    serial_putchar(GBRIDGE_CMD_DEBUG_LINE);
    serial_putchar(length);
    for (unsigned char i = 0; i < length; i++) serial_putchar(line[i]);
    wait_cmd(GBRIDGE_CMD_DEBUG_LINE);
}

// Send a data packet
void gbridge_cmd_data(struct gbridge_data data)
{
    if (!connected) return;
    if (data.size > GBRIDGE_MAX_DATA_SIZE) return;

    uint16_t checksum = 0;

    serial_putchar(GBRIDGE_CMD_DATA);
    serial_putchar(data.size);
    for (unsigned char i = 0; i < data.size; i++) {
        serial_putchar(data.buffer[i]);
    }
    checksum_add(&checksum, data.buffer, data.size);
    serial_putchar(checksum >> 8);
    serial_putchar(checksum >> 0);
    wait_cmd(GBRIDGE_CMD_DATA);
}

// Start a stream packet, allowing sending a big chunked message
void gbridge_cmd_stream_start(unsigned length)
{
    if (!connected) return;

    serial_putchar(GBRIDGE_CMD_STREAM);
    serial_putchar(length >> 8);
    serial_putchar(length >> 0);
    stream_checksum = 0;
}

// Send data within the stream packet
void gbridge_cmd_stream_data(const void *data, unsigned length)
{
    if (!connected) return;

    checksum_add(&stream_checksum, data, length);
    for (const char *c = data; length--; c++) serial_putchar(*c);
}

// Close off the stream packet, and wait for the bridge to confirm the
//   reception of the packet.
void gbridge_cmd_stream_finish(void)
{
    if (!connected) return;

    serial_putchar(stream_checksum >> 8);
    serial_putchar(stream_checksum >> 0);
    wait_cmd(GBRIDGE_CMD_STREAM);
}
