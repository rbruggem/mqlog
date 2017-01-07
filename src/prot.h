#ifndef MQLOG_PROT_H_
#define MQLOG_PROT_H_

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
    volatile uint16_t flags;
    uint8_t  version;
    uint8_t  pad; // not used
    uint32_t size;
    uint32_t crc32;
};

#define HEADER_FLAGS_EMPTY 0x0000
 // Marks that the header and payload is ready to be consumed.
#define HEADER_FLAGS_READY 0xbeef
// End Of Segment (EOS)
// An EOS frame is required when a producer claims a write offset
// but fails to write because the segment does not have enough
// contiguous space available to hold the new frame with its payload.
// The write offset has already been claimed and needs to be used.
#define HEADER_FLAGS_EOS 0xaaaa


#define HEADER_VERSION     0x0

#define HEADER_PAD         0x0


struct frame {
    const struct header* hdr;
    const unsigned char* buffer;
};


void header_init(struct header*);

size_t frame_payload_size(const struct frame*);

int prot_is_header(void*);

#endif
