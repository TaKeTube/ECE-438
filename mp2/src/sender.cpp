/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <list>
#include <queue>
#include "sender.hpp"

struct sockaddr_in si_other;
int s, slen;

FILE *fp;
state_t tcp_state;
double cw = 1.0;
int sst, dupack, seq_num = 1;

SenderBuffer send_buf = SenderBuffer();
std::queue<timestamp_t> time_stamp;
char[sizeof(packet_t)] pkt_buf;


void diep(char *s) {
    perror(s);
    exit(1);
}


void congestionCtrl(event_t event){
    switch (tcp_state)
    {
    case SLOW_START:
        if(event == TIME_OUT){

        }else if(event == NEW_ACK){

        }else if(event == DUP_ACK){

        }
        break;
    case CONGESTION_AVOIDANCE:
        if(event == TIME_OUT){

        }else if(event == NEW_ACK){

        }else if(event == DUP_ACK){
            
        }
        break;
    case FAST_RECOVERY:
        if(event == TIME_OUT){

        }else if(event == NEW_ACK){

        }else if(event == DUP_ACK){
            
        }
        break;
    default:
        break;
    }
}

void sendPkt(){

}

void fillBuf(){
    int pkt_num = cw - send_buf.size;
    /* if no need to fill the buffer */
    if(pkt_num <= 0)
        return;
    /* if need to fill the buffer */
    /* read data from file and package packets */
    for(int i = 0; i < pkt_num; ++i){
        packet_t pkt;
        int nbyte = fread(pkt.data, sizeof(char), MSS, fp);
        /* if file is not read completely */
        if(nbyte > 0){
            /* package new packet */
            pkt.type = DATA;
            pkt.seq_num = seq_num++;
            pkt.nbyte = nbyte;
            send_buf.push(pkt);
        }
    }

}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    /* open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Initialize the UDP socket */
    /* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s*/
    cw = 1;
    sst = INIT_SST;
    dupack = 0;
    tcp_state = SLOW_START;

    fillBuf();
    while(!send_buf.empty()){
        
    }




    printf("Closing the socket\n");
    close(s);
    return;
}

int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


