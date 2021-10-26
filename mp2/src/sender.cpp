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
double cw = 1.0, sst;
int dupack, seq_num = 1;

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
    int pkt_num = min(floor(cw), send_buf.size()) - sent_num;
    for(int i = 0; i < pkt_num; ++i){
        packet_t pkt = send_buf.popUnsent();
        /* set time stamp for this packet */
        timestamp_t stamp;
        stamp.seq_num = pkt.seq_num;
        gettimeofday(&stamp.tv, NULL);
        time_stamps.push(stamp);
        /* send packet */
        sendto(s, (char*)&pkt, sizeof(packet), 0, si_other.sin_addr, slen);
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
    sendto(s, (char*)&pkt, sizeof(packet), 0, si_other.sin_addr, slen);
    setTimeOut();
}

void fillBuf(){
    int pkt_num = floor(cw) - send_buf.size();
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
        packet_t pkt;
        int nbyte = fread(pkt.data, sizeof(char), MSS, fp);
        /* At the end of file, pkt may not be fully filled */
        /* Receiver need to read packet according to nbyte */
        if(nbyte > 0){
            /* package new packet */
            pkt.type = DATA;
            pkt.seq_num = seq_num++;
            pkt.nbyte = nbyte;
            send_buf.push(pkt);
        }
    }
}

void setTimeOut(){
    timeval curr_tv, diff_tv;
    if(!time_stamps.empty()){
        timeval &prev_tv = time_stamps.front().tv;
        gettimeofday(&curr_tv, NULL);
        timeradd(prev_tv, rtt_tv, diff_tv);
        timersub(diff_tv, curr_tv, diff_tv);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &diff_tv, sizeof(timeval));
    }else{
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rtt_tv, sizeof(timeval));
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
    packet_t received_pkt;
    event_t event;
    cw = 1.0;
    sst = INIT_SST;
    dupack = 0;
    tcp_state = SLOW_START;
    received_pkt.seq_num = -1;
    received_pkt.type = ACK;

    fillBuf();
    sendPkt();
    while(!send_buf.empty()){
        if(received_pkt.seq_num > send_buf.front().seq_num){
            event = NEW_ACK;
        }else{
            if(recvfrom(s, (char*)&received_pkt, sizeof(packet), 0, NULL, NULL) == -1)
                /* TIME OUT */
                event = TIME_OUT;
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
                if(cw >= sst)
                    tcp_state = CONGESTION_AVOIDANCE;
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
            }else if(event == NEW_ACK){
                /* update state */
                cw = cw + 1 / floor(cw);
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
            }else if(event == NEW_ACK){
                /* update state */
                cw = sst;
                dupack = 0;
                tcp_state = CONGESTION_AVOIDANCE;
                send_buf.pop();
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
        
        fillBuf();
        sendPkt();
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
