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

// ==================== global variables ====================

int client_fd;
sockaddr_in server_addr;

// ==================== functions declaration ====================

bool socket_connect_tcp_client(void);

bool socket_disconnect_client(void);

// ==================== main ====================

int main() 
{
    if(!socket_connect_tcp_client())
    {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // 3. 接收图片大小
    size_t length;
    std::cout << "[INFO] 已接收数据：" << recv(client_fd, (void*)&length, sizeof(length), 0) << " Byte" << std::endl;
    std::cout << "[INFO] 接收图片大小：" << length << " Byte" << std::endl;

    // 4. 接收图片数据
    size_t length_runtime = length, recv_length = 0;
    unsigned char* buffer = (unsigned char*)malloc(length_runtime * sizeof(unsigned char));
    unsigned char* recv_buffer = (unsigned char*)malloc(size_t(32768));
    
    while(length_runtime > 0)
    {
        recv_length = recv(client_fd, recv_buffer, 32768, 0);
        if (recv_length == -1) {
            // 接收失败，检查errno
            int err = errno;
            std::cerr << "[ERROR] 接收数据失败：(" << err << ") " << strerror(err) << std::endl;
            // 处理错误，例如关闭连接或重试
            break;
        }
        std::cout << "[INFO] 已接收数据：" << recv_length << " Byte" << std::endl;
        // 将接收到的数据复制到buffer中
        memcpy(buffer + (length - length_runtime), recv_buffer, recv_length);
        length_runtime -= recv_length;
    }
    
    // 释放内存
    free(recv_buffer);
    
    // 确保接收到的数据长度与预期相符
    if (length_runtime != 0) 
    {
        std::cerr << "[ERROR] 接收到的数据长度不正确。" << std::endl;
        // 处理错误，例如关闭连接或重试
    }
    
    std::vector<uchar> encode_data(buffer, buffer + length);

    // 5. 保存图片数据到文件
    cv::Mat frame = cv::imdecode(encode_data, cv::IMREAD_UNCHANGED);
    if(frame.empty())    std::cerr << "[ERROR] 图像解码失败" << std::endl;
    else    cv::imwrite("接收图片.png", frame);

    socket_disconnect_client();

    std::cout << "图片接收完成" << std::endl;
    return 0;
}

// ==================== functions ====================

bool socket_connect_tcp_client(void)
{
    // 创建socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        std::cerr << "创建socket失败" << std::endl;
        return false;
    }

    // 连接服务器
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 服务器IP地址
    server_addr.sin_port = htons(8080); // 服务器端口
    if (connect(client_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "连接服务器失败" << std::endl;
        close(client_fd);
        return false;
    }

    return true;
}

bool socket_disconnect_client(void)
{
    // 关闭socket
    close(client_fd);
    return true;
}
