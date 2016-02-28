#include "prot.h"

void header_init(struct header* hdr) {
    // TODO: address endianess
    hdr->flags = HEADER_FLAGS_EMPTY;
    hdr->version = HEADER_VERSION;
    hdr->pad = HEADER_PAD;
}

size_t frame_payload_size(const struct frame* fr) {
    return fr->hdr->size - sizeof(struct header);
}
