#ifndef UTILITY_H
#define UTILITY_H

#define MSS         512
#define RTT         20
#define RTO         (2*RTT)

enum pkt_type {ACK, DATA, SYN, FIN, HOLDER};

typedef struct packet {
    pkt_type    type;
    int         seq_num;
    int         nbyte;
    char        data[MSS];
} packet_t;

#endif