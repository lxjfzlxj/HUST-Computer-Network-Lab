#ifndef TCP_RDT_SENDER_H
#define TCP_RDT_SENDER_H

#include "RdtSender.h"
#include <queue>

struct TCPRdtSender: public RdtSender {
private:
	static const int SEQNUM_WIDTH = 3; // 序号位数
	static const int WIN_LEN = 4; // 窗口大小
	Packet packets[1 << SEQNUM_WIDTH]; // 等待 ACK 的数据包队列
	int base; // 窗口的基序号
	int expectSequenceNumberSend; // 下一个发送序号
	int repetitiveAckCount; // 重复ACK计数


public:

	bool getWaitingState();
	bool send(const Message &message);						//发送应用层下来的Message，由NetworkServiceSimulator调用,如果发送方成功地将Message发送到网络层，返回true;如果因为发送方处于等待正确确认状态而拒绝发送Message，则返回false
	void receive(const Packet &ackPkt);						//接受确认Ack，将被NetworkServiceSimulator调用	
	void timeoutHandler(int seqNum);					//Timeout handler，将被NetworkServiceSimulator调用
	void printWindow(const char *description); // 打印当前窗口状态

public:
	TCPRdtSender();
	virtual ~TCPRdtSender();
};

#endif