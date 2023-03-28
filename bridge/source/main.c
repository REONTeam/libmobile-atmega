#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <libserialport.h>

#include "gbridge.h"
#include "gbridge_prot_ma.h"

const char *program_name;

void program_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", program_name);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void serial_error(enum sp_return code, const char *fmt, ...)
{
    if (code == SP_OK) return;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", program_name);
    vfprintf(stderr, fmt, ap);
    if (code == SP_ERR_FAIL) {
        char *err = sp_last_error_message();
        fprintf(stderr, ": %s\n", err);
        sp_free_error_message(err);
    } else {
        fprintf(stderr, "\n");
    }
    va_end(ap);
}

struct sp_port *serial_guess_port(void)
{
    struct sp_port *res;
    struct sp_port **ports;

    if (sp_list_ports(&ports) != SP_OK || ports[0] == NULL) {
        program_error("No serial devices detected");
        return NULL;
    }
    if (ports[1] != NULL) {
        fprintf(stderr, "Multiple serial ports available, please specify a port:\n");
        for (struct sp_port **port = ports; *port != NULL; port++) {
            // To get proper port info, check the port_info.c example
            fprintf(stderr, "- %s\n", sp_get_port_name(*port));
        }
        sp_free_port_list(ports);
        return NULL;
    }
    if (sp_copy_port(ports[0], &res) != SP_OK) {
        program_error("sp_copy_port failed");
        sp_free_port_list(ports);
        return NULL;
    }

    sp_free_port_list(ports);
    return res;
}

int serial_open(struct sp_port *port)
{
    // Open a port with default settings
    enum sp_return rc;
    if ((rc = sp_open(port, SP_MODE_READ_WRITE)) != SP_OK) {
        serial_error(rc, "sp_open failed");
        return -1;
    }
    if (sp_set_baudrate(port, 500000) != SP_OK) return -1;
    if (sp_set_bits(port, 8) != SP_OK) return -1;
    if (sp_set_parity(port, SP_PARITY_NONE) != SP_OK) return -1;
    if (sp_set_stopbits(port, 1) != SP_OK) return -1;
    if (sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE) != SP_OK) return -1;
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    program_name = argv[0];

    struct sp_port *port;

    if (argc > 1) {
        if (sp_get_port_by_name(argv[1], &port) != SP_OK) {
            program_error("Can't get serial port: '%s'", argv[1]);
            return 1;
        }
    } else {
        port = serial_guess_port();
        if (!port) return 1;
    }

    printf("Selected port: %s\n", sp_get_port_name(port));
    if (serial_open(port) != 0) {
        program_error("serial_open failed");
        return 1;
    }

    gbridge_init();
    gbridge_prot_ma_init();
    while (!gbridge_handshake(port));
    printf("Connected!\n");

    while (gbridge_connected()) {
        gbridge_loop(port);
        gbridge_prot_ma_loop(port);
    }

    sp_close(port);
    sp_free_port(port);
}
