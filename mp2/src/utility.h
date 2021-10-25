#ifndef UTILITY_H
#define UTILITY_H

#define MSS         512

enum pkt_t {ACK, DATA, SYN, FIN, HOLDER};

typedef struct packet {
    pkt_t   type;
    int     seq_num;
    char    data[MSS];
} packet_t;

#endif