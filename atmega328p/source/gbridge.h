#pragma once

#include <stdbool.h>

struct gbridge_data {
    unsigned char size;
    unsigned char *buffer;
};

void gbridge_init(void);
void gbridge_loop(void);
bool gbridge_connected(void);
const struct gbridge_data *gbridge_recv_data(void);
const struct gbridge_data *gbridge_recv_data_wait(void);
void gbridge_recv_data_done(void);
void gbridge_recv_stream(void *buffer, unsigned max_size);
bool gbridge_recv_stream_done(void);
bool gbridge_recv_stream_wait(void *buffer, unsigned max_size);
void gbridge_cmd_debug_line(const char *line);
void gbridge_cmd_data(struct gbridge_data data);
void gbridge_cmd_stream_start(unsigned length);
void gbridge_cmd_stream_data(const void *data, unsigned length);
void gbridge_cmd_stream_finish(void);
