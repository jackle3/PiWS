#include "bytestream.h"
struct tcp_peer {
    struct {
        struct bytestream outgoing;
    } sender;
    struct {
        struct bytestream incoming;
    } receiver;
};
