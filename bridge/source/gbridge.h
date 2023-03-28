#pragma once

#include <stdbool.h>
struct sp_port;

struct gbridge_data {
    unsigned char size;
    unsigned char *buffer;
};

void gbridge_init(void);
bool gbridge_handshake(struct sp_port *port);
void gbridge_loop(struct sp_port *port);
bool gbridge_connected(void);
const struct gbridge_data *gbridge_recv_data(void);
void gbridge_recv_data_done(void);
int gbridge_recv_stream(struct sp_port *port, void *buffer, unsigned max_size);
void gbridge_cmd_data(struct sp_port *port, struct gbridge_data data);
void gbridge_cmd_stream(struct sp_port *port, void *buffer, unsigned size);
