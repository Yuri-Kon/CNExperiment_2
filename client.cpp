// GBN 客户端代码（步骤 2 - 改进 GBN 协议以支持双向数据传输）
// 此代码实现了一个基于 Go-Back-N (GBN) 协议的客户端，用于通过 UDP 进行可靠的数据传输。
// 客户端从服务器接收数据包，发送 ACK，并模拟丢包以进行测试。
// 改进后的代码支持双向数据传输，即客户端既可以接收数据，也可以发送数据。

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <winsock2.h>
#include <cstdlib>
#include <iomanip>  // 用于格式化时间输出

#pragma comment(lib,"ws2_32.lib")

#define SERVER_PORT 12345  // 服务器端口号
#define CLIENT_PORT 12346  // 客户端端口号
#define SERVER_IP "127.0.0.1"  // 服务器 IP 地址（本地环回地址）
#define BUFFER_SIZE 1024  // 缓冲区大小
#define PACKET_LOSS_PROBABILITY 0.2  // 数据包丢失概率（20%）
#define WINDOW_SIZE 4  // 窗口大小

SOCKET sockClient;  // 客户端套接字
SOCKADDR_IN serverAddr, clientAddr;
int base = 0;  // 窗口的基序号
int nextSeqNum = 0;  // 下一个要发送的数据包序号
bool ackReceived[WINDOW_SIZE] = { false };  // 存储每个数据包是否已收到 ACK
bool timerRunning = false;  // 计时器是否在运行
std::chrono::steady_clock::time_point startTime;  // 计时器开始时间

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

// 启动计时器
void startTimer() {
    timerRunning = true;
    startTime = std::chrono::steady_clock::now();
}

// 停止计时器
void stopTimer() {
    timerRunning = false;
}

// 检查是否发生超时
bool isTimeout() {
    if (timerRunning) {
        auto currentTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        return duration > 2000;  // 超时时间设置为 2000 毫秒
    }
    return false;
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

// 发送数据包函数
void sendData(const char* data, int dataSize) {
    char buffer[BUFFER_SIZE];
    while (base < dataSize) {
        // 在窗口范围内发送数据包
        while (nextSeqNum < base + WINDOW_SIZE && nextSeqNum < dataSize) {
            if (!isPacketLost()) {  // 如果数据包未丢失
                snprintf(buffer, BUFFER_SIZE, "%d:%s", nextSeqNum, data + nextSeqNum);  // 格式化数据包，包含序号和数据
                sendto(sockClient, buffer, strlen(buffer), 0, (SOCKADDR*)&serverAddr, sizeof(serverAddr));  // 发送数据包
                std::cout << getCurrentTime() << " - Packet sent: " << nextSeqNum << std::endl;
            } else {
                // 模拟数据包丢失并记录丢失信息
                std::cout << getCurrentTime() << " - Packet lost: " << nextSeqNum << std::endl;
            }
            if (base == nextSeqNum) {
                startTimer();  // 如果是窗口中的第一个数据包，启动计时器
            }
            nextSeqNum++;
        }
        if (isTimeout()) {  // 如果超时，重传窗口中的所有数据包
            std::cout << getCurrentTime() << " - Timeout occurred, resending packets..." << std::endl;
            for (int i = base; i < nextSeqNum; ++i) {
                snprintf(buffer, BUFFER_SIZE, "%d:%s", i, data + i);
                sendto(sockClient, buffer, strlen(buffer), 0, (SOCKADDR*)&serverAddr, sizeof(serverAddr));  // 重新发送数据包
                std::cout << getCurrentTime() << " - Packet retransmitted: " << i << std::endl;
            }
            startTimer();  // 重传后重新启动计时器
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 每次循环暂停 500 毫秒
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

    // 设置客户端地址结构
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(CLIENT_PORT);

    // 绑定客户端套接字
    bind(sockClient, (SOCKADDR*)&clientAddr, sizeof(clientAddr));

    // 向服务器发送初始连接消息
    const char* initMessage = "Client connected";
    sendto(sockClient, initMessage, strlen(initMessage), 0, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    std::cout << getCurrentTime() << " - Connected to server." << std::endl;

    // 创建接收数据的线程
    std::thread receiveThread(receiveData);

    // 发送测试数据
    const char* data = "This is a test message for GBN protocol.";
    int dataSize = strlen(data);
    sendData(data, dataSize);

    receiveThread.join();  // 等待接收线程结束

    // 关闭套接字并清理 Winsock
    closesocket(sockClient);
    WSACleanup();
    return 0;
}
