#pragma once

#define GBRIDGE_HANDSHAKE {0x99, 0x66, 'G', 'B'}

#define GBRIDGE_TIMEOUT_US 1000000
#define GBRIDGE_TIMEOUT_MS (GBRIDGE_TIMEOUT_US / 1000)

#define GBRIDGE_MAX_DATA_SIZE 0x80

// Flag set when replying to a message
#define GBRIDGE_CMD_REPLY_F 0x80

enum gbridge_cmd {
    GBRIDGE_CMD_NONE = 0,

    // to PC
    GBRIDGE_CMD_PING = 0x01,
    GBRIDGE_CMD_DEBUG_LINE = 0x02,
    GBRIDGE_CMD_DEBUG_CHAR = 0x03,
    GBRIDGE_CMD_DATA = 0x0A,
    GBRIDGE_CMD_DATA_FAIL = 0x0B,  // Checksum failure, retry
    GBRIDGE_CMD_STREAM = 0x0C,
    GBRIDGE_CMD_STREAM_FAIL = 0x0D,  // Checksum failure, retry

    // from PC
    GBRIDGE_CMD_PROG_STOP = 0x41,
    GBRIDGE_CMD_PROG_START = 0x42,
    GBRIDGE_CMD_DATA_PC = 0x4A,
    GBRIDGE_CMD_DATA_FAIL_PC = 0x4B,  // Checksum failure, retry
    GBRIDGE_CMD_STREAM_PC = 0x4C,
    GBRIDGE_CMD_STREAM_FAIL_PC = 0x4D,  // Checksum failure, retry
    GBRIDGE_CMD_RESET = 0x4F,
};
