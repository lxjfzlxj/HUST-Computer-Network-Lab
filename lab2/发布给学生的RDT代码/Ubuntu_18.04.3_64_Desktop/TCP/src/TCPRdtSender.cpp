#include "TCPRdtSender.h"
#include "Global.h"

TCPRdtSender::TCPRdtSender(): expectSequenceNumberSend(0), base(0), repetitiveAckCount(0) {}

TCPRdtSender::~TCPRdtSender() {}

bool TCPRdtSender::getWaitingState() {
    return (expectSequenceNumberSend - base + (1 << SEQNUM_WIDTH)) % (1 << SEQNUM_WIDTH) >= WIN_LEN;
}

bool TCPRdtSender::send(const Message &message) {
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

void TCPRdtSender::receive(const Packet &ackPkt) {
    int checkSum = pUtils->calculateCheckSum(ackPkt); // 检查校验和是否正确
    if (checkSum == ackPkt.checksum) {
        if ((expectSequenceNumberSend >= base && base <= ackPkt.acknum && ackPkt.acknum < expectSequenceNumberSend || expectSequenceNumberSend < base && (ackPkt.acknum >= base || ackPkt.acknum < expectSequenceNumberSend))) {
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
            repetitiveAckCount = 1; // 重置重复ACK计数
        }
        else if (expectSequenceNumberSend == (base - 1 + (1 << SEQNUM_WIDTH)) % (1 << SEQNUM_WIDTH)) {
            repetitiveAckCount++;
            pUtils->printPacket("发送方收到重复ACK", ackPkt);
            if (repetitiveAckCount == 3 && base != expectSequenceNumberSend) {
                pUtils->printPacket("发送方收到三个重复ACK，重发窗口内第一个未确认的报文", packets[base]);
                pns->sendToNetworkLayer(RECEIVER, packets[base]);
                pns->stopTimer(SENDER, base);
                pns->startTimer(SENDER, Configuration::TIME_OUT, base); // 重新启动发送方定时器
                repetitiveAckCount = 0; // 重置重复ACK计数
            }
        }
    }
}

void TCPRdtSender::timeoutHandler(int seqNum) {
    pUtils->printPacket("发送方定时器时间到，重发窗口内第一个未确认的报文", packets[seqNum]);
    pns->stopTimer(SENDER, seqNum);
    pns->startTimer(SENDER, Configuration::TIME_OUT, base); // 重新启动发送方定时器
    pns->sendToNetworkLayer(RECEIVER, packets[seqNum]);
}

void TCPRdtSender::printWindow() {
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