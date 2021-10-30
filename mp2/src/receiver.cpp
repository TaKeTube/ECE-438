/* 
 * File:   sender_main.c
 * Author: Zimu Guan
 *
 * Created on 2021.10.23
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <list>
#include "utility.h"

#define RECV_BUF_SIZE    128

struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

    /* Initialize the UDP socket */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    FILE* fp = fopen(destinationFile,"wb");
    int seq_num = 0;
    packet_t empty_pkt;
    empty_pkt.type = HOLDER;
    
    /* initialize receive buffer */
    std::list<packet> recv_buf = std::list<packet>();
    for(int i = 0; i < RECV_BUF_SIZE; ++i)
        recv_buf.emplace_back(empty_pkt);
    std::list<packet>::iterator recv_buf_begin = recv_buf.begin();

    packet_t pkt;
    /* Now receive data and send acknowledgements */
    recvfrom(s, (char*)&pkt, sizeof(packet), 0, NULL, NULL);
    while(pkt.type != FIN){
        if(pkt.type != DATA)
            continue;
        /* If receive previous packet, ignore it */
        if(pkt.seq_num < seq_num)
            continue;
        /* If receive the correct packet */
        else if(pkt.seq_num == seq_num){
            (*recv_buf_begin) = pkt;
            while((*recv_buf_begin).type == DATA){
                /* write buffered packets */
                fwrite(pkt.data, sizeof(char), pkt.nbyte, fp);
                (*recv_buf_begin) = empty_pkt;
                recv_buf_begin++;
                /* skip the end node of stl list */
                if(recv_buf_begin == recv_buf.end())
                    recv_buf_begin++;
                seq_num++;
            }
            /* Send ack */
            packet_t ack;
            ack.type = ACK;
            ack.seq_num = seq_num - 1;
            sendto(s, (char*)&pkt, sizeof(packet_t), 0, si_other.sin_addr, slen);
        /* If receive ahead packet */
        }else{
            int offset = pkt.seq_num - seq_num;
            bool full_flag = false;

            /* Find the position to buffer */
            std::list<packet>::iterator buf_pkt_ptr = recv_buf_begin;
            while(offset > 0){
                buf_pkt_ptr++;
                /* skip the end node of stl list */
                if(buf_pkt_ptr == recv_buf.end())
                    buf_pkt_ptr++;
                if(buf_pkt_ptr == recv_buf_begin){
                    full_flag = true;
                    break
                }
                offset--;
            }
            /* if buffer is full, drop the packet */
            if(full_flag)
                continue;

            /* check whether the buf_pkt_ptr points to the correst place */
            // if((*buf_pkt_ptr).type == DATA && (*buf_pkt_ptr).seq_num != pkt.seq_num){
            //     printf("something wrong with the receive buffer...");
            // }
            
            /* send ack */
            packet_t ack;
            ack.type = ACK;
            ack.seq_num = seq_num - 1;
            sendto(s, (char*)&pkt, sizeof(packet_t), 0, si_other.sin_addr, slen);

            /* buffer the packet if needed */
            if((*buf_pkt_ptr).type == HOLDER)
                (*buf_pkt_ptr) = pkt;
        }

        packet_t pkt;
        recvfrom(s, (char*)&pkt, sizeof(packet), 0, NULL, NULL);
    }

    close(s);
    printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {
    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

