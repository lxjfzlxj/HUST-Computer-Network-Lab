#ifndef SR_RDT_RECEIVER_H
#define SR_RDT_RECEIVER_H

#include "RdtReceiver.h"
class SRRdtReceiver: public RdtReceiver {
private:
    static const int SEQNUM_WIDTH = 3; // 序号位数
    static const int WIN_LEN = 4; // 窗口大小
    Packet packets[1 << SEQNUM_WIDTH]; // 接收报文缓存区
    bool isReceived[1 << SEQNUM_WIDTH]; // 报文是否已接收
    int base; // 窗口的基序号

public:
    SRRdtReceiver();
    virtual ~SRRdtReceiver();

    void receive(const Packet &packet); // 接收报文，将被NetworkService调用
};

#endif