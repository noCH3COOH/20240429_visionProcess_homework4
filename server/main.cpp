// ==================== includes ====================

#include <vector>
#include <thread>
#include <cstdio>
#include <iomanip>
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
#define PORT_TXD 12321
#define PORT_RXD 12322

// ==================== class ====================

template <typename T>
class stopAndWait_ARQ_t {
public:
    int SN, RN;
    size_t size;
    T* data;

public:
    stopAndWait_ARQ_t();
    ~stopAndWait_ARQ_t();
};

// ==================== global variables ====================

bool flag_tcp = false;
bool flag_udp = false;

int server_fd;
int client_fd;

unsigned char* buffer;

sockaddr_in server_addr;
socklen_t server_addr_len;
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

ssize_t socket_send_server(void* data, size_t length);
ssize_t socket_send_tcp_server(void* data, size_t length);
ssize_t socket_send_udp_server(void* data, size_t length);

ssize_t socket_recv_server(void* buffer, size_t length);
ssize_t socket_recv_tcp_server(void* buffer, size_t length);
ssize_t socket_recv_udp_server(void* buffer, size_t length);

bool socket_disconnect_server(void);

template <typename T>
bool stopAndWait_ARQ_init(stopAndWait_ARQ_t<T>*& tar);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;
    buffer = (unsigned char*)malloc(128 * sizeof(unsigned char));
    memset(buffer, 0, 128);  

    // if(!socket_connect_tcp_server()) {
    if(!socket_connect_udp_server()) {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // UDP 接收 client 端数据
    if(flag_udp) {
        ssize_t ret;
        do {
            memset(&remote_client_addr, 0, sizeof(remote_client_addr));

            std::cout << "[INFO] 等待 UDP 客户端连接" << std::endl;
            ret = recvfrom(server_fd, buffer, 4, 0, (sockaddr*)&remote_client_addr, &remote_client_addr_len);

            if(ret > 0 && ret <= 4) {
                std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
                std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息(";
                std::cout << ret << " Byte)：";
                printf("0x%x 0x%x 0x%x 0x%x",buffer[0], buffer[1], buffer[2], buffer[3]);
                std::cout << std::endl;
                break;
            }
            else    std::cerr << "[ERROR] 接收错误" << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } while(1);
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
    server_addr.sin_port = htons(PORT_RXD); // 服务器端口
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
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(-1==server_fd) {
        std::cerr << "[ERROR] socket 创建错误" << std::endl;
        return false;
    }

    // 设置地址与端口
    server_addr_len = sizeof(server_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;       // Use IPV4
    server_addr.sin_port = htons(PORT_RXD);    //
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
   // Time out
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 200000;  // 200 ms
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));

    // Bind 端口，用来接受之前设定的地址与端口发来的信息,作为接受一方必须bind端口，并且端口号与发送方一致
    if(bind(server_fd, (struct sockaddr*)&server_addr, server_addr_len) == -1) {
        std::cerr << "[ERROR] 端口绑定错误" << std::endl;
        close(server_fd);
        return false;
    }

    remote_client_addr_len = sizeof(remote_client_addr);

    flag_udp = true;

    return true;
}

bool socket_sendFrame_server(cv::Mat& frame) { 
    cv::imencode(".png", frame, encode_data);
    
    size_t length = encode_data.end() - encode_data.begin();
    size_t package_length = length;
    if(0 != length % 4)    package_length = length + (4 - (length % 4));    // 凑够 4 整除
    buffer = (unsigned char*)realloc(buffer, package_length * sizeof(unsigned char));
    std::copy(encode_data.begin(), encode_data.end(), buffer + (4 - (length % 4)));

    // 发送图片数据到客户端
    ssize_t send_length = 0;
    
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

    if(flag_udp) {
        ssize_t ret;
        do {
            std::cout << "[INFO] 等待 UDP 客户端响应" << std::endl;
            ret = recvfrom(server_fd, buffer, 4, 0, (sockaddr*)&remote_client_addr, &remote_client_addr_len);

            if(ret > 0 && ret <= 4) {
                std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
                std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息(";
                std::cout << ret << " Byte)：";
                printf("0x%x 0x%x 0x%x 0x%x",buffer[0], buffer[1], buffer[2], buffer[3]);
                std::cout << std::endl;
                break;
            }
            else    std::cerr << "[ERROR] 接收错误" << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } while(1);
        std::cout << "[SUCCESS] UDP服务端已响应 " << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // UDP 直接分包发送不可靠，故这里运用停等式ARQ进行分包发送
        stopAndWait_ARQ_t<unsigned char> send_package_para;    // 停等式ARQ发包缓冲区
        unsigned char* send_package = (unsigned char*)malloc(32768);
        size_t max_len_send_package_para = 32768 - 2*sizeof(int) - sizeof(size_t);

        send_package_para.data = buffer;
        send_package_para.SN = 0;
        send_package_para.RN = 0;

        unsigned char* recv_buff = (unsigned char*)malloc(20);
        ssize_t recv_len = -1;
        int recv_RN;
        short retry;
        bool recv_notOK;

        while(package_length > 0) {
            recv_notOK = false;
            retry = 0;

            // 发送数据块，每次最多 32768 - 2*sizeof(int) - sizeof(size_t) == 32752 字节
            send_package_para.size = (package_length < max_len_send_package_para) ? package_length + 16 : 32768;
            send_package_para.SN += 1;

            *((int*)send_package) = send_package_para.SN;
            *(int*)(send_package + 4) = send_package_para.RN;
            *(size_t*)(send_package + 8) = send_package_para.size;
            memcpy(send_package + 16, send_package_para.data, (package_length < max_len_send_package_para) ? package_length : 32768);

            send_length = socket_send_server(send_package, send_package_para.size);
            if (send_length == -1) {    // 发送失败，处理错误
                int err = errno;    
                std::cerr << "[ERROR] 发送数据失败：" << strerror(err) << std::endl;
                
                // 这里可以添加错误处理代码，例如关闭连接或重试
                return false;
            }
            std::cout << "[SUCCESS] 已发送数据(" << send_package_para.SN << ", " << send_package_para.RN << ")：" << send_length << " Byte\n";

            // 接收响应
            do {
                recv_len = socket_recv_server(recv_buff, 20);
                if(-1 != recv_len) {
                    recv_RN = *((int*)(recv_buff + 4));
                    if(recv_RN == send_package_para.SN) {    // 应该服务端发SN是几，响应的RN就是几，服务端一直有 RN <= SN
                        send_package_para.RN += 1;
                        break;
                    }
                } else {
                    std::cerr << "[ERROR] 接收客户端响应出错，重试" << retry + 1 << "次" << std::endl;
                    retry += 1;
                    if(40 == retry) {
                        std::cerr << "[ERROR] 重试全部失败，数据重传" << std::endl;
                        send_package_para.SN -= 1;
                        recv_notOK = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } while(1);

            if(recv_notOK)    continue;
            send_package_para.data += send_length - 16;    // 直接使用send_length作为偏移量
            package_length -= send_length - 16;
            std::cout << "[SUCCESS] 响应接收成功(" << retry << ")，累计发送数据：" << length - package_length << " Byte\n";
        }

    } else if(flag_tcp) {
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
            std::cout << "[SUCCESS] 已发送数据：" << send_length << " Byte";
            now_inbuff += send_length; // 直接使用send_length作为偏移量
            package_length -= send_length;
            std::cout << "累计接收数据：" << length - package_length << " Byte\n";
    
            // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return true;
}

ssize_t socket_send_server(void* data, size_t length) {
    if(flag_tcp)    return socket_send_tcp_server(data, length);
    else if(flag_udp)    return socket_send_udp_server(data, length);
    else {
        std::cerr << "[ERROR] 无连接不能发送" << std::endl;
        return -1;
    }
}

ssize_t socket_send_tcp_server(void* data, size_t length) {
    if(0 != length % 4) std::cout << "[WARNING] 发送数据非4位整倍" << std::endl;
    
    return send(client_fd, data, length, 0);
}

ssize_t socket_send_udp_server(void* data, size_t length) {
    if(0 != length % 4) std::cout << "[WARNING] 发送数据非4位整倍" << std::endl;
    
    return sendto(server_fd, data, length, 0,
        (sockaddr*)&remote_client_addr, remote_client_addr_len);
}

ssize_t socket_recv_server(void* buffer, size_t length) {
    if(flag_tcp)    return socket_recv_tcp_server(buffer, length);
    else if(flag_udp)    return socket_recv_udp_server(buffer, length);
    else {
        std::cerr << "[ERROR] 无连接不能接收" << std::endl;
        return -1;
    }
}

ssize_t socket_recv_tcp_server(void* buffer, size_t length) {
    return recv(client_fd, buffer, length, 0);
}

ssize_t socket_recv_udp_server(void* buffer, size_t length) {
    auto ret = recvfrom(server_fd, buffer, length, 0, 
        (sockaddr*)&remote_client_addr, &remote_client_addr_len);
    if(-1 != ret) {
        std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
        std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息: ";
        std::cout << ret << " Byte" << std::endl;
    }

    return ret;
}


bool socket_disconnect_server(void) {
    close(client_fd);
    close(server_fd);

    return true;
}

// ==================== class ====================

template <typename T>
stopAndWait_ARQ_t<T>::stopAndWait_ARQ_t() {
    SN = 0; // 初始化序列号
    RN = 0; // 初始化确认号

    size = 0;
}

template <typename T>
stopAndWait_ARQ_t<T>::~stopAndWait_ARQ_t() {}
