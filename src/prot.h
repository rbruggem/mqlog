#ifndef HEADER_H_
#define HEADER_H_

#include <stdint.h>
#include <stddef.h>

// * Header *
//
// | 1 byte |
// | 2 bytes ....... |
// | 4 bytes ......................... |
//
// |--------|--------|--------|--------|
// | Flags           |Version |Padding |
// |--------|--------|--------|--------|
// | Frame size (incl header)          |
// |-----------------------------------|
// | CRC32                             |
// |-----------------------------------|
// End of header
// |--------|--------|--------|--------|
// | Payload                           |
// | ...                               |
// |--------|--------|--------|--------|

struct header {
    uint16_t flags;
    uint8_t  version;
    uint8_t  pad; // not used
    uint32_t size;
    uint32_t crc32;
};

#define HEADER_FLAGS_EMPTY 0x00
#define HEADER_FLAGS_READY 0xbeef  // marks that the header and payload
                                   // is ready to be consumed

#define HEADER_VERSION     0x0

#define HEADER_PAD         0x0


struct frame {
    const struct header* hdr;
    const unsigned char* buffer;
};


void header_init(struct header*);

size_t frame_payload_size(const struct frame*);

#endif
