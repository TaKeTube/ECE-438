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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <algorithm>

#include <list>
#include <queue>
#include "sender.hpp"

struct sockaddr_in si_other;
int s, slen;
FILE *fp;
struct sockaddr_storage their_addr;
socklen_t addr_len;

state_t tcp_state;
double cw = 1.0, sst;
int dupack, seq_num = 1;
long long int nbyteToTransfer;

SenderBuffer send_buf = SenderBuffer();

std::queue<timestamp_t> time_stamps;
timeval rtt_tv = {0, 1000*RTO};

void clearQ(std::queue<timestamp_t> &q)
{
    std::queue<timestamp_t> empty;
    std::swap(q, empty);
}

void diep(char *s) {
    perror(s);
    exit(1);
}

void sendPkt(){
    /*
     * if the file is already completely read, 
     * sent_num may never reach cw 
     * so we cannot simply use cw
     */
    int pkt_num = std::min((int)cw, send_buf.size()) - send_buf.sent_num;
    for(int i = 0; i < pkt_num; ++i){
        packet_t pkt = send_buf.popUnsent();
        /* set time stamp for this packet */
        timestamp_t stamp;
        stamp.seq_num = pkt.seq_num;
        gettimeofday(&stamp.tv, NULL);
        time_stamps.push(stamp);
        /* send packet */
        sendto(s, (char*)&pkt, sizeof(packet_t), 0, (sockaddr*)&si_other, slen);
        printf("Pkt # %d sent.\n", pkt.seq_num);
        setTimeOut();
    }
}

void resendPkt(){
    packet_t pkt = send_buf.front();
    /* set time stamp for this packet */
    timestamp_t stamp;
    stamp.seq_num = pkt.seq_num;
    gettimeofday(&stamp.tv, NULL);
    time_stamps.push(stamp);
    /* send packet */
    sendto(s, (char*)&pkt, sizeof(packet_t), 0, (sockaddr*)&si_other, slen);
    printf("Pkt # %d resent.\n", pkt.seq_num);
    setTimeOut();
}

void fillBuf(){
    int pkt_num = (int)cw - send_buf.size();
    /* if no need to fill the buffer */
    if(pkt_num <= 0)
        return;
    /* if need to fill the buffer */
    /* read data from file and package packets */
    /*  
     * If we use send_buf.size instead of pkt_num in each step, 
     * if the file is already completely read, 
     * size may never reach cw 
    */
    for(int i = 0; i < pkt_num; ++i){
        if(nbyteToTransfer <= 0)
            return;
        packet_t pkt;
        int nbyteExpected = (nbyteToTransfer >= MSS) ? MSS : ((int)nbyteToTransfer);
        int nbyte = fread(pkt.data, sizeof(char), nbyteExpected, fp);
        /* At the end of file, pkt may not be fully filled */
        /* Receiver need to read packet according to nbyte */
        if(nbyte > 0){
            /* package new packet */
            pkt.type = DATA;
            pkt.seq_num = seq_num++;
            pkt.nbyte = nbyte;
            send_buf.push(pkt);
            nbyteToTransfer -= nbyte;
        }
    }
}

void setTimeOut(){
    timeval curr_tv, diff_tv;
    if(!time_stamps.empty()){
        timeval &prev_tv = time_stamps.front().tv;
        gettimeofday(&curr_tv, NULL);
        timeradd(&prev_tv, &rtt_tv, &diff_tv);
        timersub(&diff_tv, &curr_tv, &diff_tv);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &diff_tv, sizeof(timeval));
    }else{
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rtt_tv, sizeof(timeval));
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    /* open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.\n");
        exit(1);
    }

    /* Initialize the UDP socket */
    /* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep((char*)"socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s*/
    /* 3 ways handshakes */
    packet_t syn;
    packet_t pkt_buf;
    syn.type = SYN;
    while(1){
        sendto(s, (char*)&syn, sizeof(packet_t), 0, (sockaddr*)&si_other, slen);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rtt_tv, sizeof(timeval));
        if(recvfrom(s, (char*)&pkt_buf, sizeof(packet_t), 0, (sockaddr *)&their_addr, &addr_len) == -1){
            printf("Time Out, resend SYN.\n");
            continue;
        }
        if(pkt_buf.type == SYNACK){
            printf("SYNACK received.\n");
            break;
        }
        printf("Unknown Pkt, resend SYN.\n");
    }
    packet_t ack;
    ack.type = ACK;
    sendto(s, (char*)&ack, sizeof(packet_t), 0, (sockaddr*)&si_other, slen);
    printf("ACK sent.\n");

    /* initialize */
    packet_t received_pkt;
    event_t event;
    cw = 1.0;
    sst = INIT_SST;
    dupack = 0;
    tcp_state = SLOW_START;
    received_pkt.seq_num = -1;
    received_pkt.type = ACK;
    nbyteToTransfer = bytesToTransfer;

    fillBuf();
    sendPkt();
    while(!send_buf.empty()){
        if(received_pkt.seq_num > send_buf.front().seq_num){
            event = NEW_ACK;
        }else{
            if(recvfrom(s, (char*)&received_pkt, sizeof(packet_t), 0, (sockaddr *)&their_addr, &addr_len) == -1){
                /* TIME OUT */
                event = TIME_OUT;
                printf("|| Time Out ||\n");
            }
            else if(received_pkt.seq_num == send_buf.front().seq_num)
                /* NEW ACK */
                event = NEW_ACK;
            else if(received_pkt.seq_num == send_buf.front().seq_num - 1)
                /* DUP ACK */
                event = DUP_ACK;
            else
                /* PREV ACK / AHEAD ACK */
                /*  Ignore  /  Update   */
                continue;
        }

        if(received_pkt.type != ACK)
            continue;

        printf("Ack # %d received.\n", received_pkt.seq_num);

        switch (tcp_state)
        {
        case SLOW_START:
            if(event == TIME_OUT){
                /* clear timestamp */
                clearQ(time_stamps);
                /* resend packet */
                resendPkt();
                /* update state */
                sst = cw / 2.0;
                cw = 1.0;
                dupack = 0;
                send_buf.resetSentWnd(cw);
                tcp_state = SLOW_START;
            }else if(event == NEW_ACK){
                /* update state */
                cw++;
                dupack = 0;
                send_buf.pop();
                if(cw >= sst){
                    tcp_state = CONGESTION_AVOIDANCE;
                    printf("|| CONGESTION_AVOIDANCE ||\n");
                }
            }else if(event == DUP_ACK){
                dupack++;
                tcp_state = CONGESTION_AVOIDANCE;
                if(dupack==3){
                    /* resend packet */
                    resendPkt();
                    /* update state */
                    sst = cw / 2.0;
                    cw = sst + 3;
                    dupack = 0;
                    send_buf.resetSentWnd(cw);
                    tcp_state = FAST_RECOVERY;
                    printf("|| FAST_RECOVERY ||\n");
                }
            }
            break;
        case CONGESTION_AVOIDANCE:
            if(event == TIME_OUT){
                /* clear timestamp */
                clearQ(time_stamps);
                /* resend packet */
                resendPkt();
                /* update state */
                sst = cw / 2.0;
                cw = 1.0;
                dupack = 0;
                send_buf.resetSentWnd(cw);
                tcp_state = SLOW_START;
                printf("|| SLOW_START ||\n");
            }else if(event == NEW_ACK){
                /* update state */
                cw = cw + 1 / (int)cw;
                dupack = 0;
                tcp_state = CONGESTION_AVOIDANCE;
                send_buf.pop();
            }else if(event == DUP_ACK){
                dupack++;
                tcp_state = CONGESTION_AVOIDANCE;
                if(dupack==3){
                    /* resend packet */
                    resendPkt();
                    /* update state */
                    sst = cw / 2.0;
                    cw = sst + 3;
                    dupack = 0;
                    send_buf.resetSentWnd(cw);
                    tcp_state = FAST_RECOVERY;
                    printf("|| FAST_RECOVERY ||\n");
                }
            }
            break;
        case FAST_RECOVERY:
            if(event == TIME_OUT){
                /* clear timestamp */
                clearQ(time_stamps);
                /* resend packet */
                resendPkt();
                /* update state */
                sst = cw / 2.0;
                cw = 1.0;
                dupack = 0;
                tcp_state = SLOW_START;
                printf("|| SLOW_START ||\n");
            }else if(event == NEW_ACK){
                /* update state */
                cw = sst;
                dupack = 0;
                /* TODO need to think for a while, cw would shrink here */
                send_buf.resetSentWnd(cw);
                tcp_state = CONGESTION_AVOIDANCE;
                send_buf.pop();
                printf("|| CONGESTION_AVOIDANCE ||\n");
            }else if(event == DUP_ACK){
                /* update state */
                cw++;
                dupack++;
                tcp_state = FAST_RECOVERY;
            }
            break;
        default:
            break;
        }
        
        while(!time_stamps.empty() && time_stamps.front().seq_num <= received_pkt.seq_num)
            time_stamps.pop();
        
        /* deal with the case that cw is less than 1 */
        if(cw < 1)
            cw = 1;

        fillBuf();
        sendPkt();
    }

    packet_t fin;
    fin.type = FIN;
    while(1){
        sendto(s, (char*)&fin, sizeof(packet_t), 0, (sockaddr*)&si_other, slen);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rtt_tv, sizeof(timeval));
        if(recvfrom(s, (char*)&pkt_buf, sizeof(packet_t), 0, (sockaddr *)&their_addr, &addr_len) == -1){
            printf("Time Out, resend FIN.\n");
            continue;
        }
        if(pkt_buf.type == FINACK)
            break;
        printf("Unknown Pkt, resend FIN.\n");
    }
    ack.type = ACK;
    sendto(s, (char*)&ack, sizeof(packet_t), 0, (sockaddr*)&si_other, slen);

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
