#include "SRRdtSender.h"
#include "Global.h"

SRRdtSender::SRRdtSender(): expectSequenceNumberSend(0), base(0) {
    memset(isACK, false, sizeof(isACK));
}

SRRdtSender::~SRRdtSender() {}

bool SRRdtSender::getWaitingState() {
    return (expectSequenceNumberSend - base + (1 << SEQNUM_WIDTH)) % (1 << SEQNUM_WIDTH) >= WIN_LEN;
}

bool SRRdtSender::send(const Message &message) {
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
    pns->startTimer(SENDER, Configuration::TIME_OUT, pkt.seqnum); // 启动发送方定时器
    expectSequenceNumberSend = (expectSequenceNumberSend + 1) % (1 << SEQNUM_WIDTH);
    printWindow();
    return true;
}

void SRRdtSender::receive(const Packet &ackPkt) {
    int checkSum = pUtils->calculateCheckSum(ackPkt); // 检查校验和是否正确
    if (checkSum == ackPkt.checksum && (expectSequenceNumberSend >= base && base <= ackPkt.acknum && ackPkt.acknum < expectSequenceNumberSend || expectSequenceNumberSend < base && (ackPkt.acknum >= base || ackPkt.acknum < expectSequenceNumberSend))) {
        isACK[ackPkt.acknum] = true;
        pUtils->printPacket("发送方正确收到确认", ackPkt);
        pns->stopTimer(SENDER, ackPkt.acknum);
        if (ackPkt.acknum == base) {
            while (isACK[base]) {
                isACK[base] = false;
                base = (base + 1) % (1 << SEQNUM_WIDTH);
            }
        }
        printWindow();
    }
}

void SRRdtSender::timeoutHandler(int seqNum) {
    pUtils->printPacket("发送方定时器时间到，重发该报文", packets[seqNum]);
    pns->stopTimer(SENDER, seqNum);
    pns->startTimer(SENDER, Configuration::TIME_OUT, seqNum); // 重新启动发送方定时器
    pns->sendToNetworkLayer(RECEIVER, packets[seqNum]);
}

void SRRdtSender::printWindow() {
    printf("发送方滑动窗口：");
    int rBound = (base + WIN_LEN) % (1 << SEQNUM_WIDTH);
    if (base <= rBound) {
        for (int i = base; i < rBound; i++) {
            printf("%d, ", i);
        }
    } else {
        for (int i = base; i < (1 << SEQNUM_WIDTH); i++) {
            printf("%d, ", i);
        }
        for (int i = 0; i < rBound; i++) {
            printf("%d, ", i);
        }
    }
    puts("");
}