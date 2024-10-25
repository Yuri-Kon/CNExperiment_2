// GBN 客户端代码
// 此代码实现了一个基于 Go-Back-N (GBN) 协议的客户端，用于通过 UDP 进行可靠的数据传输。
// 客户端从服务器接收数据包，发送 ACK，并模拟丢包以进行测试。

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <winsock2.h>
#include <cstdlib>
#include <iomanip>  // 用于格式化时间输出

#pragma comment(lib,"ws2_32.lib")

#define SERVER_PORT 12345  // 服务器端口号
#define SERVER_IP "127.0.0.1"  // 服务器 IP 地址（本地环回地址）
#define BUFFER_SIZE 1024  // 缓冲区大小
#define PACKET_LOSS_PROBABILITY 0.2  // 数据包丢失概率（20%）

SOCKET sockClient;  // 客户端套接字
SOCKADDR_IN serverAddr;  // 服务器地址结构

// 获取当前时间的字符串表示
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &currentTime);
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// 判断数据包是否丢失（模拟）
bool isPacketLost() {
    // 生成一个随机数，并与丢包概率比较来决定是否丢失数据包
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < PACKET_LOSS_PROBABILITY;
}

// 发送 ACK 确认信息
void sendAck(int ackNum) {
    char buffer[BUFFER_SIZE];
    // 将 ACK 序号格式化为字符串并存储到缓冲区中
    snprintf(buffer, BUFFER_SIZE, "%d", ackNum);
    // 向服务器发送 ACK
    sendto(sockClient, buffer, strlen(buffer), 0, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    std::cout << getCurrentTime() << " - ACK sent for packet: " << ackNum << std::endl;
}

// 接收服务器发送的数据包
void receiveData() {
    char buffer[BUFFER_SIZE];  // 缓冲区用于存储接收到的数据
    int expectedSeqNum = 0;  // 期望接收的数据包序号
    while (true) {
        // 从服务器接收数据包
        int recvLen = recvfrom(sockClient, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (recvLen > 0) {
            int seqNum;  // 数据包的序号
            char message[BUFFER_SIZE];  // 数据包的内容
            // 解析接收到的数据包，提取序号和消息内容
            sscanf(buffer, "%d:%s", &seqNum, message);

            // 检查数据包是否丢失（模拟）
            if (!isPacketLost()) {
                // 如果接收到的数据包序号与期望的序号一致，说明是正确的数据包
                if (seqNum == expectedSeqNum) {
                    std::cout << getCurrentTime() << " - Packet received: " << seqNum << " - " << message << std::endl;
                    // 发送 ACK 确认接收到的数据包
                    sendAck(seqNum);
                    // 更新期望的下一个数据包序号
                    expectedSeqNum++;
                } else {
                    // 如果数据包乱序，忽略它但记录事件
                    std::cout << getCurrentTime() << " - Out of order packet received: " << seqNum << " (Expected: " << expectedSeqNum << ")" << std::endl;
                }
            } else {
                // 模拟数据包丢失并记录丢失信息
                std::cout << getCurrentTime() << " - Packet lost (simulated): " << seqNum << std::endl;
            }
        }
    }
}

int main() {
    // 设置随机数种子用于数据包丢失模拟
    srand(static_cast<unsigned int>(time(0)));
    WSADATA wsaData;
    // 初始化 Winsock 库
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 创建用于 UDP 通信的套接字
    sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    // 设置服务器地址结构
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_port = htons(SERVER_PORT);

    // 向服务器发送初始连接消息
    const char* initMessage = "Client connected";
    sendto(sockClient, initMessage, strlen(initMessage), 0, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    std::cout << getCurrentTime() << " - Connected to server." << std::endl;

    // 开始接收来自服务器的数据包
    receiveData();

    // 关闭套接字并清理 Winsock
    closesocket(sockClient);
    WSACleanup();
    return 0;
}
