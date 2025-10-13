#include "Global.h"
#include "SRRdtReceiver.h"

SRRdtReceiver::SRRdtReceiver(): base(0) {
    memset(isReceived, false, sizeof(isReceived));
}

SRRdtReceiver::~SRRdtReceiver() {}

void SRRdtReceiver::receive(const Packet &packet) {
    int checkSum = pUtils->calculateCheckSum(packet); // 检查校验和是否正确
    if(checkSum == packet.checksum) {
        pUtils->printPacket("接收方正确收到发送方的报文", packet);
        int rBound = (base + WIN_LEN) % (1 << SEQNUM_WIDTH);
        Packet ackPkt;
        ackPkt.acknum = packet.seqnum; // 确认序号等于收到的报文序号
        ackPkt.seqnum = -1; // 忽略该字段
        ackPkt.checksum = 0;
        for (int i = 0; i < Configuration::PAYLOAD_SIZE; i++) {
            ackPkt.payload[i] = '.';
        }
        ackPkt.checksum = pUtils->calculateCheckSum(ackPkt);
        pns->sendToNetworkLayer(SENDER, ackPkt); // 调用模拟网络环境的sendToNetworkLayer，通过网络层发送确认报文到对方
        pUtils->printPacket("接收方发送确认报文", ackPkt);
        if (base < rBound && base <= packet.seqnum && packet.seqnum < rBound || base > rBound && (base <= packet.seqnum || packet.seqnum < rBound)) {
            if (!isReceived[packet.seqnum]) { // 如果该包之前没有收到过
                packets[packet.seqnum] = packet; // 缓存收到的包
                isReceived[packet.seqnum] = true;
                while (isReceived[base]) { // 如果窗口基序号的包已收到，则递交给应用层并滑动窗口
                    Message msg;
                    memcpy(msg.data, packets[base].payload, sizeof(packets[base].payload));
                    pns->delivertoAppLayer(RECEIVER, msg);
                    isReceived[base] = false;
                    base = (base + 1) % (1 << SEQNUM_WIDTH);
                }
            }
        }
        printWindow();
    } else {
        pUtils->printPacket("接收方没有正确收到发送方的报文,数据校验错误", packet);
    }
}

void SRRdtReceiver::printWindow() {
    printf("接收方滑动窗口（仅显示已接收到报文）：");
    int rBound = (base + WIN_LEN) % (1 << SEQNUM_WIDTH);
    if (base <= rBound) {
        for (int i = base; i < rBound; i++) {
            if(isReceived[i]) printf("%d, ", packets[i].seqnum);
        }
    } else {
        for (int i = base; i < (1 << SEQNUM_WIDTH); i++) {
            if(isReceived[i]) printf("%d, ", packets[i].seqnum);
        }
        for (int i = 0; i < rBound; i++) {
            if(isReceived[i]) printf("%d, ", packets[i].seqnum);
        }
    }
    puts("");
}