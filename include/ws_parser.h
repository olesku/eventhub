#ifndef WS_PARSER
#define WS_PARSER

#include <stddef.h>
#include <stdint.h>

typedef enum {
    WS_FRAME_TEXT   = 0x1,
    WS_FRAME_BINARY = 0x2,
    WS_FRAME_CLOSE  = 0x8,
    WS_FRAME_PING   = 0x9,
    WS_FRAME_PONG   = 0xA,
}
ws_frame_type_t;

typedef struct {
    int(*on_data_begin)     (void*, ws_frame_type_t);
    int(*on_data_payload)   (void*, const char*, size_t);
    int(*on_data_end)       (void*);
    int(*on_control_begin)  (void*, ws_frame_type_t);
    int(*on_control_payload)(void*, const char*, size_t);
    int(*on_control_end)    (void*);
}
ws_parser_callbacks_t;

typedef struct {
    uint64_t bytes_remaining;
    uint8_t mask[4];
    uint8_t fragment  : 1;
    uint8_t fin       : 1;
    uint8_t control   : 1;
    uint8_t mask_flag : 1;
    uint8_t mask_pos  : 2;
    uint8_t state     : 5;
}
ws_parser_t;

#define WS_PARSER_ERROR_CODES(XX) \
    XX(WS_OK,                    0) \
    XX(WS_RESERVED_BITS_SET,    -1) \
    XX(WS_INVALID_OPCODE,       -2) \
    XX(WS_INVALID_CONTINUATION, -3) \
    XX(WS_CONTROL_TOO_LONG,     -4) \
    XX(WS_NON_CANONICAL_LENGTH, -5) \
    XX(WS_FRAGMENTED_CONTROL,   -6) \

enum {
    #define XX(name, code) name = code,
    WS_PARSER_ERROR_CODES(XX)
    #undef XX
};

void
ws_parser_init(ws_parser_t* parser);

int
ws_parser_execute(
    ws_parser_t* parser,
    const ws_parser_callbacks_t* callbacks,
    void* data,
    char* buff /* mutates! */,
    size_t len);

const char*
ws_parser_error(int rc);

#endif
