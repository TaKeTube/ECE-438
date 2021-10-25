#ifndef UTILITY_H
#define UTILITY_H

#define MSS         512

enum pkt_type {ACK, DATA, SYN, FIN, HOLDER};

typedef struct packet {
    pkt_type    type;
    int         seq_num;
    int         nbyte;
    char        data[MSS];
} packet_t;

#endif