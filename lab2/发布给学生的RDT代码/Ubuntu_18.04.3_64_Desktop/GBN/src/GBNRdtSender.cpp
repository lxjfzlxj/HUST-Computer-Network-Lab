#include "GBNRdtSender.h"
#include "Global.h"

GBNRdtSender::GBNRdtSender(): expectSequenceNumberSend(0), base(0) {}

GBNRdtSender::~GBNRdtSender() {}

bool GBNRdtSender::getWaitingState() {
    return (expectSequenceNumberSend - base + (1 << SEQNUM_WIDTH)) % (1 << SEQNUM_WIDTH) >= WIN_LEN;
}

bool GBNRdtSender::send(const Message &message) {
    if (getWaitingState()) { // 发送方处于等待确认状态
        return false;
    }
    Packet pkt;
    pkt.acknum = -1;
    pkt.seqnum = expectSequenceNumberSend;
    pkt.checksum = 0;
    memcpy(pkt.payload, message.data, sizeof(message.data));
    pkt.checksum = pUtils->calculateCheckSum(pkt);
    packets[expectSequenceNumberSend] = pkt; // 缓存发送的包
    pUtils->printPacket("发送方发送报文", pkt);
    pns->sendToNetworkLayer(RECEIVER, pkt);
    if (base == expectSequenceNumberSend) {
        pns->startTimer(SENDER, Configuration::TIME_OUT, pkt.seqnum); // 启动发送方定时器
    }
    expectSequenceNumberSend = (expectSequenceNumberSend + 1) % (1 << SEQNUM_WIDTH);
    printWindow();

    return true;
} 

void GBNRdtSender::receive(const Packet &ackPkt) {
    int checkSum = pUtils->calculateCheckSum(ackPkt); // 检查校验和是否正确
    if (checkSum == ackPkt.checksum && (expectSequenceNumberSend >= base && base <= ackPkt.acknum && ackPkt.acknum < expectSequenceNumberSend || expectSequenceNumberSend < base && (ackPkt.acknum >= base || ackPkt.acknum < expectSequenceNumberSend))) {
        int lastBase = base;
        base = (ackPkt.acknum + 1) % (1 << SEQNUM_WIDTH);
        pUtils->printPacket("发送方正确收到确认", ackPkt);
        printWindow();
        if (base == expectSequenceNumberSend) {
            pns->stopTimer(SENDER, lastBase); // 关闭定时器
        } else {
            pns->stopTimer(SENDER, lastBase);
            pns->startTimer(SENDER, Configuration::TIME_OUT, base); // 重新启动发送方定时器
        }
    }
}

void GBNRdtSender::timeoutHandler(int seqNum) {
    pUtils->printPacket("发送方定时器时间到，重发窗口内所有未确认的报文", packets[seqNum]);
    pns->stopTimer(SENDER, seqNum);
    pns->startTimer(SENDER, Configuration::TIME_OUT, base); // 重新启动发送方定时器
    if (base <= expectSequenceNumberSend) {
        for (int i = base; i < expectSequenceNumberSend; i++) {
            pns->sendToNetworkLayer(RECEIVER, packets[i]);
        }
    } else {
        for (int i = base; i < (1 << SEQNUM_WIDTH); i++) {
            pns->sendToNetworkLayer(RECEIVER, packets[i]);
        }
        for (int i = 0; i < expectSequenceNumberSend; i++) {
            pns->sendToNetworkLayer(RECEIVER, packets[i]);
        }
    }
}

void GBNRdtSender::printWindow() {
    printf("滑动窗口：");
    if (base <= expectSequenceNumberSend) {
        for (int i = base; i < expectSequenceNumberSend; i++) {
            printf("%d, ", packets[i].seqnum);
        }
    } else {
        for (int i = base; i < (1 << SEQNUM_WIDTH); i++) {
            printf("%d, ", packets[i].seqnum);
        }
        for (int i = 0; i < expectSequenceNumberSend; i++) {
            printf("%d, ", packets[i].seqnum);
        }
    }
    puts("");
}