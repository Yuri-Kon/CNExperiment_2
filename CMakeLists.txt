cmake_minimum_required(VERSION 3.29)
project(GBN)

set(CMAKE_CXX_STANDARD 20)
# 创建服务器可执行文件
add_executable(GBN_Server server.cpp)
target_link_libraries(GBN_Server ws2_32)

# 创建客户端可执行文件
add_executable(GBN_Client client.cpp)
target_link_libraries(GBN_Client ws2_32)
