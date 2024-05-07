// ==================== includes ====================

#include <vector>
#include <thread>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <opencv2/opencv.hpp>

// ==================== struct ====================


// ==================== global variables ====================

int server_fd;
int client_fd;
sockaddr_in server_addr;
sockaddr_in client_addr;
socklen_t client_addr_len;

cv::Mat frame;
std::vector<uchar> encode_data;

// ==================== functions declaration ====================

bool socket_connect_tcp_server(void);
bool socket_connect_udp_server(void);

bool socket_disconnect(void);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;

    if(!socket_connect_tcp_server())
    {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // 5. 读取图片数据
    frame = cv::imread("./src.png");
    
    cv::imencode(".png", frame, encode_data);
    size_t length = encode_data.end() - encode_data.begin();
    unsigned char* buffer = (unsigned char*)malloc(length * sizeof(unsigned char));
    std::copy(encode_data.begin(), encode_data.end(), buffer);

    // 6. 发送图片数据到客户端
    std::cout << "[INFO] 发送图片大小：" << length << " Byte" << std::endl;
    std::cout << "[INFO] 发送大小数据：" << send(client_fd, (void*)&length, sizeof(length), 0) << " Byte" << std::endl; // 先发送图片大小

    size_t send_length = 0;
    unsigned char* now_inbuff = buffer;
    while(length > 0)
    {
        // 发送数据块，每次最多32768字节
        send_length = send(client_fd, now_inbuff, (length > 32768 ? 32768 : length), 0);
        if (send_length == -1) 
        {    // 发送失败，处理错误
            int err = errno;    
            std::cerr << "[ERROR] 发送数据失败：" << strerror(err) << std::endl;
            // 这里可以添加错误处理代码，例如关闭连接或重试
            break;
        }
        std::cout << "[INFO] 已发送数据：" << send_length << " Byte" << std::endl;
        now_inbuff += send_length; // 直接使用send_length作为偏移量
        length -= send_length;
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    frame = cv::imdecode(encode_data, cv::IMREAD_UNCHANGED);
    cv::imwrite("发送图像.png", frame);

    // 7. 关闭socket
    free(buffer);
    socket_disconnect();

    return 0;
}

// ==================== functions ====================

bool socket_connect_tcp_server(void)
{
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "创建socket失败" << std::endl;
        return false;
    }

    // 绑定socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 服务器IP地址
    server_addr.sin_port = htons(8080); // 服务器端口
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "绑定socket失败" << std::endl;
        close(server_fd);
        return false;
    }

    // 监听socket
    if (listen(server_fd, 5) == -1) {
        std::cerr << "监听socket失败" << std::endl;
        close(server_fd);
        return false;
    }

    // 接受客户端连接
    client_addr_len = sizeof(client_addr);
    client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_addr_len);
    if (client_fd == -1) {
        std::cerr << "接受客户端连接失败" << std::endl;
        close(server_fd);
        return false;
    }

    return true;
}

bool socket_connect_udp_server(void) {
    // 创建socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        std::cerr << "创建socket失败" << std::endl;
        return false;
    }

    // 绑定socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 服务器IP地址
    server_addr.sin_port = htons(8080); // 服务器端口
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "绑定socket失败" << std::endl;
        close(server_fd);
        return false;
    }

    return true;
}

bool socket_disconnect(void)
{
    close(client_fd);
    close(server_fd);

    return true;
}
