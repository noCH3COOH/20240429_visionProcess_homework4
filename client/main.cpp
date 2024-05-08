// ==================== includes ====================

#include <vector>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <opencv2/opencv.hpp>

// ==================== define ====================

#define IP_SERVER "127.0.0.1"
#define PORT_TCP 3000
#define PORT_UDP 3001

// ==================== global variables ====================

bool flag_tcp = false;
bool flag_udp = false;

int client_fd;
sockaddr_in server_addr;

// ==================== functions declaration ====================

bool socket_connect_tcp_client(void);
bool socket_connect_udp_client(void);

size_t socket_recv_client(void* buffer, size_t length);

bool socket_disconnect_client(void);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;

    if(!socket_connect_tcp_client()) {
    // if(!socket_connect_udp_client()) {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // 接收图片大小
    size_t length;
    std::cout << "[SUCCESS] 已接收数据：" << socket_recv_client(&length, sizeof(length)) << " Byte" << std::endl;
    std::cout << "[INFO] 接收图片大小：" << length << " Byte" << std::endl;

    // 先接收需要丢弃的数据
    unsigned char* buffer = (unsigned char*)malloc(4 * sizeof(unsigned char));
    if(0 != length % 4) {
        std::cout << "[SUCCESS] 已接收数据：" << socket_recv_client(buffer, (4 - (length % 4))) << " Byte" << std::endl;
    }

    // 接收图片数据
    size_t length_runtime = length, recv_length = 0;
    buffer = (unsigned char*)realloc(buffer, length_runtime * sizeof(unsigned char));
    unsigned char* recv_buffer = buffer;
    
    while(length_runtime > 0) {
        recv_length = socket_recv_client(recv_buffer, 
            (length_runtime < 32768 ? length_runtime : 32768));
        
        if (recv_length == -1) {
            // 接收失败，检查errno
            int err = errno;
            std::cerr << "[ERROR] 接收数据失败：(" << err << ") " << strerror(err) << std::endl;
            // 处理错误，例如关闭连接或重试
            break;
        }
        std::cout << "[SUCCESS] 已接收数据：" << recv_length << " Byte" << std::endl;
        
        length_runtime -= recv_length;
        recv_buffer += recv_length;
    }
    
    // 确保接收到的数据长度与预期相符
    if(length_runtime != 0) {
        std::cerr << "[ERROR] 接收到的数据长度不正确。" << std::endl;
        // 处理错误，例如关闭连接或重试
    } else {
        std::vector<uchar> encode_data(buffer, buffer + length);
        
        // 保存图片数据到文件
        cv::Mat frame = cv::imdecode(encode_data, cv::IMREAD_UNCHANGED);
        if(frame.empty())    std::cerr << "[ERROR] 图像解码失败" << std::endl;
        else    cv::imwrite("接收图片.png", frame);
    }

    socket_disconnect_client();

    std::cout << "[SUCCESS] 图片接收完成" << std::endl;
    return 0;
}

// ==================== functions ====================

bool socket_connect_tcp_client(void) {
    // 创建socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        std::cerr << "[ERROR] 创建socket失败" << std::endl;
        return false;
    }

    // 连接服务器
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr(IP_SERVER); // 服务器IP地址
    server_addr.sin_port = htons(PORT_TCP); // 服务器端口
    if (connect(client_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "[ERROR] 连接服务器失败" << std::endl;
        close(client_fd);
        return false;
    }

    flag_tcp = true;

    return true;
}

bool socket_connect_udp_client(void) {
    // 创建socket
    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd == -1) {
        std::cerr << "[ERROR] 创建socket失败" << std::endl;
        return false;
    }

    // 设置服务器地址信息
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr(IP_SERVER); // 服务器IP地址
    server_addr.sin_port = htons(PORT_UDP); // 服务器端口

    // UDP客户端可以选择性地使用connect，这样可以简化后续的send和recv调用
    // 如果不调用connect，则需要在使用sendto和recvfrom时每次都指定目的地址
    if (connect(client_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "[ERROR] 连接到服务器失败" << std::endl;
        close(client_fd);
        return false;
    }

    flag_udp = true;

    return true;
}

size_t socket_recv_client(void* buffer, size_t length) {
    if(flag_tcp) {
        recv(client_fd, buffer, length, 0);
    } else if(flag_udp) {

    } else {
        std::cerr << "[ERROR] 无连接不能接收" << std::endl;
        return -1;
    }
}

bool socket_disconnect_client(void) {
    // 关闭socket
    close(client_fd);

    flag_tcp = false;
    flag_udp = false;

    return true;
}
