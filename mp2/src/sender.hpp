#ifndef SENDER_HPP
#define SENDER_HPP

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
    int size;
    int sent_num;
    std::list<packet_t> data;
    std::list<packet_t>::iterator unsent_ptr;

    SenderBuffer();
    void push(packet_t &pkt);
    void pop();
    packet_t &popUnsent();
    // void pop_sent(int ack_num);
    packet_t &front();
    bool empty();
    void resetUnsentPtr();
};

SenderBuffer::SenderBuffer(){
    size = 0;
    sent_num = 0;
    data = std::list<packet_t>();
    packet_t pkt_holder;
    pkt_holder.type = HOLDER;
    data.push_back(pkt_holder);
    unsent = data.begin();
}

void SenderBuffer::push(packet_t &pkt){
    data.push_back(pkt);
    size++;
};

// packet_t SenderBuffer::pop(){
//     packet_t pkt = data.front();
//     data.pop_front();
//     size--;
//     sent_num--;
//     return pkt;
// }

void SenderBuffer::pop(){
    data.pop_front();
    size--;
    sent_num--;
}

packet_t &SenderBuffer::popUnsent(){
    packet_t &pkt = *unsent_ptr++;
    sent_num++;
    return pkt;
}

// void SenderBuffer::pop_sent(int ack_num){
//     while(data.front().seq_num <= ack_num){
//         data.pop_front();
//         sent_num--;
//         size--;
//     }
// }

packet_t &SenderBuffer::front(){
    return data.front();
}

bool SenderBuffer::empty(){
    return size <= 1;
}

#endif