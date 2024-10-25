// GBN 服务器代码（步骤 2 - 添加数据包丢失模拟和超时处理）
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <winsock2.h>
#include <cstdlib>
#include <ctime>
#include <iomanip>  // 用于格式化时间输出

#pragma comment(lib,"ws2_32.lib")

#define SERVER_PORT 12345  // 服务器端口号
#define BUFFER_SIZE 1024  // 缓冲区大小
#define WINDOW_SIZE 4  // 滑动窗口大小
#define TIMEOUT_DURATION 2000  // 超时时间（毫秒）
#define PACKET_LOSS_PROBABILITY 0.2  // 数据包丢失概率（20%）

SOCKET sockServer;
SOCKADDR_IN serverAddr, clientAddr;
int clientAddrSize = sizeof(clientAddr);

bool ackReceived[WINDOW_SIZE] = { false };  // 存储每个数据包是否已收到 ACK
int base = 0;  // 当前窗口的基序号
int nextSeqNum = 0;  // 下一个要发送的数据包序号
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
        return duration > TIMEOUT_DURATION;
    }
    return false;
}

// 判断数据包是否丢失（模拟）
bool isPacketLost() {
    // 生成一个随机数，并与丢包概率比较来决定是否丢失数据包
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < PACKET_LOSS_PROBABILITY;
}

// 接收 ACK 的线程函数
void receiveAck() {
    char buffer[BUFFER_SIZE];
    while (true) {
        // 接收来自客户端的 ACK
        int recvLen = recvfrom(sockServer, buffer, BUFFER_SIZE, 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
        if (recvLen > 0) {
            int ackNum = atoi(buffer);  // 将接收到的 ACK 转换为整数
            if (ackNum >= base && ackNum < base + WINDOW_SIZE) {
                ackReceived[ackNum % WINDOW_SIZE] = true;  // 标记该序号的 ACK 已收到
                std::cout << getCurrentTime() << " - ACK received for packet: " << ackNum << std::endl;
                // 更新窗口的基序号，滑动窗口
                while (ackReceived[base % WINDOW_SIZE]) {
                    ackReceived[base % WINDOW_SIZE] = false;
                    base++;
                    if (base == nextSeqNum) {
                        stopTimer();  // 如果窗口内没有未确认的数据包，则停止计时器
                    } else {
                        startTimer();  // 否则重新启动计时器
                    }
                }
            }
        }
    }
}

// 重传窗口中的所有数据包
void resendPackets(const char* data, int dataSize) {
    char buffer[BUFFER_SIZE];
    for (int i = base; i < nextSeqNum; ++i) {
        // 格式化数据包，包含序号和数据
        snprintf(buffer, BUFFER_SIZE, "%d:%s", i, data + i);
        sendto(sockServer, buffer, strlen(buffer), 0, (SOCKADDR*)&clientAddr, clientAddrSize);  // 发送数据包
        std::cout << getCurrentTime() << " - Packet retransmitted: " << i << std::endl;
    }
    startTimer();  // 重传后重新启动计时器
}

// 发送数据包函数
void sendData(const char* data, int dataSize) {
    char buffer[BUFFER_SIZE];
    while (base < dataSize) {
        // 在窗口范围内发送数据包
        while (nextSeqNum < base + WINDOW_SIZE && nextSeqNum < dataSize) {
            if (!isPacketLost()) {  // 如果数据包未丢失
                snprintf(buffer, BUFFER_SIZE, "%d:%s", nextSeqNum, data + nextSeqNum);  // 格式化数据包，包含序号和数据
                sendto(sockServer, buffer, strlen(buffer), 0, (SOCKADDR*)&clientAddr, clientAddrSize);  // 发送数据包
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
            resendPackets(data, dataSize);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 每次循环暂停 500 毫秒
    }
}

int main() {
    srand(static_cast<unsigned int>(time(0)));  // 设置随机种子用于数据包丢失模拟
    WSADATA wsaData;
    // 初始化 Winsock 库
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 创建用于 UDP 通信的套接字
    sockServer = socket(AF_INET, SOCK_DGRAM, 0);
    // 设置服务器地址结构
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // 绑定套接字到指定端口
    bind(sockServer, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    std::cout << getCurrentTime() << " - Waiting for client connection..." << std::endl;
    char buffer[BUFFER_SIZE];
    // 接收客户端的初始消息
    recvfrom(sockServer, buffer, BUFFER_SIZE, 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
    std::cout << getCurrentTime() << " - Client connected." << std::endl;

    // 启动接收 ACK 的线程
    std::thread ackThread(receiveAck);

    const char* data = "This is a test message for GBN protocol.";  // 要发送的数据
    int dataSize = strlen(data);
    sendData(data, dataSize);  // 开始发送数据

    ackThread.join();  // 等待 ACK 接收线程结束
    // 关闭套接字并清理 Winsock
    closesocket(sockServer);
    WSACleanup();
    return 0;
}
