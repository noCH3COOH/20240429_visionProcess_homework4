// ==================== includes ====================

#include <vector>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <opencv2/opencv.hpp>

// ==================== define ====================

#define IP_SERVER "127.0.0.1"
#define PORT_TCP 3000
#define PORT_UDP 3000

// ==================== struct ====================


// ==================== global variables ====================

bool flag_tcp = false;
bool flag_udp = false;

int server_fd;
int client_fd;

unsigned char* buffer;

sockaddr_in server_addr;
sockaddr_in remote_client_addr;
socklen_t remote_client_addr_len;

cv::Mat frame;
std::vector<uchar> encode_data;

unsigned char frame_flag[] = {
    0xD0, 0xA0, 0xD0, 0xA0
};

// ==================== functions declaration ====================

bool socket_connect_tcp_server(void);
bool socket_connect_udp_server(void);

bool socket_sendFrame_server(cv::Mat& frame);

size_t socket_send_server(void* data, size_t length);
size_t socket_send_tcp_server(void* data, size_t length);
size_t socket_send_udp_server(void* data, size_t length);

size_t socket_recv_server(void* buffer, size_t length);
size_t socket_recv_tcp_server(void* buffer, size_t length);
size_t socket_recv_udp_server(void* buffer, size_t length);

bool socket_disconnect_server(void);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;

    // if(!socket_connect_tcp_server()) {
    if(!socket_connect_udp_server()) {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // UDP 接收 client 端数据
    if(flag_udp) {
        unsigned char frame_start[4];
        auto ret = socket_recv_server(frame_start, 4);
        do {
            std::cout << "[INFO] 等待 UDP 客户端连接" << std::endl;
            ret = recvfrom(client_fd, frame_start, 4, 0, NULL, NULL);
            if(-1 == ret)    std::cerr << "[ERROR] 接收错误" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while( -1 == ret || !strcmp((const char*)frame_start, (const char*)frame_flag) );
        std::cout << "[SUCCESS] 建立一条 UDP 链接到 " << inet_ntoa(remote_client_addr.sin_addr);
        std::cout << ":" << ntohs(remote_client_addr.sin_port) << std::endl;
    }

    // 读取图片数据
    frame = cv::imread("./src.png");
    socket_sendFrame_server(frame);   

    // 关闭socket
    free(buffer);
    socket_disconnect_server();

    return 0;
}

// ==================== functions ====================

bool socket_connect_tcp_server(void) {
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "[ERROR] 创建socket失败" << std::endl;
        return false;
    }

    // 绑定socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr(IP_SERVER); // 服务器IP地址
    server_addr.sin_port = htons(PORT_TCP); // 服务器端口
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "[ERROR] 绑定端口失败：" << std::endl;
        close(server_fd);
        return false;
    }

    // 监听socket
    if (listen(server_fd, 5) == -1) {
        std::cerr << "[ERROR] 监听socket失败" << std::endl;
        close(server_fd);
        return false;
    }

    // 接受客户端连接
    remote_client_addr_len = sizeof(remote_client_addr);
    client_fd = accept(server_fd, (sockaddr*)&remote_client_addr, &remote_client_addr_len);
    if (client_fd == -1) {
        std::cerr << "[ERROR] 接受客户端连接失败" << std::endl;
        close(server_fd);
        return false;
    }

    std::cout << "[SUCCESS] 建立一条 TCP 链接到 " << inet_ntoa(remote_client_addr.sin_addr);
    std::cout << ":" << ntohs(remote_client_addr.sin_port) << std::endl;

    flag_tcp = true;

    return true;
}

bool socket_connect_udp_server(void) {
    // 创建socket
    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_fd == -1) {
        std::cerr << "[ERROR] 创建socket失败" << std::endl;
        return false;
    }

    // 绑定socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 服务器IP地址
    server_addr.sin_port = htons(PORT_UDP); // 服务器端口
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "[ERROR] 绑定socket失败" << std::endl;
        close(server_fd);
        return false;
    }
    remote_client_addr_len = sizeof(remote_client_addr);
    memset(&remote_client_addr, 0, sizeof(remote_client_addr));

    flag_udp = true;

    return true;
}

bool socket_sendFrame_server(cv::Mat& frame) { 
    cv::imencode(".png", frame, encode_data);
    
    size_t length = encode_data.end() - encode_data.begin();
    size_t package_length = length;
    if(0 != length % 4)    package_length = length + (4 - (length % 4));    // 凑够 4 整除
    buffer = (unsigned char*)malloc(package_length * sizeof(unsigned char));
    std::copy(encode_data.begin(), encode_data.end(), buffer + (4 - (length % 4)));

    // 发送图片数据到客户端
    size_t send_length = 0;
    
    std::cout << "[INFO] 发送图片大小：" << length << " Byte" << std::endl;
    send_length = socket_send_server(&length, sizeof(length));
    if(-1 != send_length)
        std::cout << "[SUCCESS] 已发送大小数据：" << send_length << " Byte" << std::endl; // 先发送图片大小
    else{    // 发送失败，处理错误
        int err = errno;    
        std::cerr << "[ERROR] 发送大小数据失败：" << strerror(err) << std::endl;
        
        // 这里可以添加错误处理代码，例如关闭连接或重试
        return false;
    }

    unsigned char* now_inbuff = buffer;
    while(package_length > 0) {
        // 发送数据块，每次最多32768字节
        send_length = socket_send_server(now_inbuff, (package_length < 32768 ? package_length : 32768));
        if (send_length == -1) {    // 发送失败，处理错误
            int err = errno;    
            std::cerr << "[ERROR] 发送数据失败：" << strerror(err) << std::endl;
            
            // 这里可以添加错误处理代码，例如关闭连接或重试
            return false;
        }
        std::cout << "[SUCCESS] 已发送数据：" << send_length << " Byte" << std::endl;
        now_inbuff += send_length; // 直接使用send_length作为偏移量
        package_length -= send_length;
    }

    return true;
}

size_t socket_send_server(void* data, size_t length) {
    if(flag_tcp)    return socket_send_tcp_server(data, length);
    else if(flag_udp)    return socket_send_udp_server(data, length);
    else {
        std::cerr << "[ERROR] 无连接不能发送" << std::endl;
        return -1;
    }
}

size_t socket_send_tcp_server(void* data, size_t length) {
    if(0 != length % 4) std::cout << "[WARNING] 发送数据非4位整倍" << std::endl;
    
    return send(client_fd, data, length, 0);
}

size_t socket_send_udp_server(void* data, size_t length) {
    if(0 != length % 4) std::cout << "[WARNING] 发送数据非4位整倍" << std::endl;
    
    return sendto(server_fd, data, length, 0,
        (sockaddr*)&remote_client_addr, remote_client_addr_len);
}

size_t socket_recv_server(void* buffer, size_t length) {
    if(flag_tcp)    return socket_recv_tcp_server(buffer, length);
    else if(flag_udp)    return socket_recv_udp_server(buffer, length);
    else {
        std::cerr << "[ERROR] 无连接不能接收" << std::endl;
        return -1;
    }
}

size_t socket_recv_tcp_server(void* buffer, size_t length) {
    return recv(client_fd, buffer, length, 0);
}

size_t socket_recv_udp_server(void* buffer, size_t length) {
    auto ret = recvfrom(client_fd, buffer, length, 0, 
        (sockaddr*)&remote_client_addr, &remote_client_addr_len);
    if(-1 != ret) {
        std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
        std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息: ";
        std::cout << ret << " Byte" << std::endl;
        memset(&remote_client_addr, 0, sizeof(remote_client_addr));
    } else {
        std::cout << "[INFO] 接收来自" << inet_ntoa(remote_client_addr.sin_addr);
        std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息出现问题: ";
        std::cout << ret << " Byte" << std::endl;
    }

    return ret;
}


bool socket_disconnect_server(void) {
    close(client_fd);
    close(server_fd);

    return true;
}
