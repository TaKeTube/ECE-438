#ifndef SENDER_HPP
#define SENDER_HPP

#include <iostream>
#include <cmath>
#include <sys/time.h>
#include <list>
#include "utility.h"

#define INIT_SST    256

enum state_t {SLOW_START, FAST_RECOVERY, CONGESTION_AVOIDANCE};
enum event_t {NEW_ACK, DUP_ACK, TIME_OUT};

typedef struct timeval timeval;

typedef struct timestamp {
    timeval tv;
    int seq_num;
} timestamp_t;

class SenderBuffer{
public:
    int sent_num;
    std::list<packet_t> data;
    std::list<packet_t>::iterator unsent_ptr;

    SenderBuffer();
    void pop();
    void push(packet_t &pkt);
    packet_t &popUnsent();
    void resetSentWnd(double cw);

    packet_t &front() { return data.front(); }
    int size() { return data.size(); }
    bool empty() { return data.empty(); }

    void print(double cw);
};

SenderBuffer::SenderBuffer(){
    sent_num = 0;
    data = std::list<packet_t>();
    unsent_ptr = data.begin();
}

void SenderBuffer::pop() {
    if(unsent_ptr == data.begin())
        unsent_ptr++;
    data.pop_front();
    if(sent_num > 0)
        sent_num--;
}

void SenderBuffer::push(packet_t &pkt) { 
    data.emplace_back(pkt); 
    if(unsent_ptr == data.end())
        unsent_ptr--; 
}

packet_t &SenderBuffer::popUnsent(){
    // if(unsent_ptr == data.end()){
    //     packet_t pkt;
    //     pkt.seq_num = -1;
    //     return pkt;
    // }
    packet_t &pkt = *unsent_ptr++;
    sent_num++;
    return pkt;
}

void SenderBuffer::resetSentWnd(double cw){
    if(sent_num > cw){
        int count = 0;
        sent_num = (int)cw;
        unsent_ptr = data.begin();
        while((count++) < sent_num)
            unsent_ptr++;
    }
}

/* For debugging */
void SenderBuffer::print(double cw){
    int count = 0;
    int cw_floor = (int)cw;
    std::cout << "Packets in SendBuffer: ";
    std::cout << "|CON| ";
    std::cout << "|SENT| ";
    for(std::list<packet_t>::iterator it = data.begin(); it != data.end(); ++it){
        count++;
        std::cout << (*it).seq_num << " ";
        if(count == sent_num)
            std::cout << "|SENT| ";
        if(count == cw_floor)
            std::cout << "|CON| ";
    }
    std::cout << std::endl;
}

void clearQ(std::queue<timestamp_t> &q);
void diep(char *s);
void sendPkt();
void resendPkt();
void fillBuf();
void setTimeOut();
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);

#endif