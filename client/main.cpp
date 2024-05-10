// ==================== includes ====================

#include <vector>
#include <thread>
#include <fcntl.h>
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
#define PORT_RXD 12321
#define PORT_TXD 12322

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

int client_fd;
sockaddr_in remote_server_addr;
socklen_t remote_server_addr_len;

unsigned char frame_flag[] = {
    0xD0, 0xA0, 0xD0, 0xA0
};

// ==================== functions declaration ====================

bool socket_connect_tcp_client(void);
bool socket_connect_udp_client(void);

bool socket_recvFrame_client(size_t length);

ssize_t socket_send_client(void* data, size_t length);
ssize_t socket_send_tcp_client(void* data, size_t length);
ssize_t socket_send_udp_client(void* data, size_t length);

ssize_t socket_recv_client(void* buffer, size_t length);
ssize_t socket_recv_tcp_client(void* buffer, size_t length);
ssize_t socket_recv_udp_client(void*& buffer, size_t length);

bool socket_disconnect_client(void);

int writeHexToFile(unsigned char *data, size_t size, const char *filename);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;

    // if(!socket_connect_tcp_client()) {
    if(!socket_connect_udp_client()) {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // 接收图片大小
    ssize_t length = -1;
    if(flag_tcp) {
        std::cout << "[SUCCESS] 已接收数据：" << socket_recv_client(&length, sizeof(length)) << " Byte" << std::endl;
    } else if(flag_udp) {
        ssize_t ret;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            socket_send_client(frame_flag, 4);
            ret = socket_recv_client(&length, sizeof(length));
            std::cout << "[SUCCESS] 已接收数据：" << ret << " Byte" << std::endl;
        } while( -1 == ret );
        std::cout << "[SUCCESS] 建立一条 UDP 链接到 " << inet_ntoa(remote_server_addr.sin_addr);
        std::cout << ":" << ntohs(remote_server_addr.sin_port) << std::endl;
    }

    std::cout << "[INFO] 接收图片大小：" << length << " Byte" << std::endl;

    if(flag_udp) {
        socket_send_client(frame_flag, 4);
        std::cout << "[SUCCESS] 响应发送成功 " << inet_ntoa(remote_server_addr.sin_addr);
        std::cout << ":" << ntohs(remote_server_addr.sin_port) << std::endl;
    }

    // // 先接收需要丢弃的数据
    // unsigned char* buffer = (unsigned char*)malloc(4 * sizeof(unsigned char));
    // if(0 != length % 4) {
    //     std::cout << "[SUCCESS] 已接收数据：" << socket_recv_client(buffer, (4 - (length % 4))) << " Byte\n";
    // }

    socket_recvFrame_client(length);

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
    memset(&remote_server_addr, 0, sizeof(remote_server_addr));
    remote_server_addr.sin_family = AF_INET;    // 使用IPv4
    remote_server_addr.sin_addr.s_addr = inet_addr(IP_SERVER);    // 服务器IP地址
    remote_server_addr.sin_port = htons(PORT_TXD);    // 服务器端口
    if (connect(client_fd, (sockaddr*)&remote_server_addr, sizeof(remote_server_addr)) == -1) {
        std::cerr << "[ERROR] 端口绑定失败" << std::endl;
        close(client_fd);
        return false;
    }
    remote_server_addr_len = sizeof(remote_server_addr);

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
    memset(&remote_server_addr, 0, sizeof(remote_server_addr));
    remote_server_addr.sin_family = AF_INET;    // 使用IPv4
    remote_server_addr.sin_addr.s_addr = inet_addr(IP_SERVER);    // 服务器IP地址
    remote_server_addr.sin_port = htons(PORT_TXD);    // 服务器端口
    
    remote_server_addr_len = sizeof(remote_server_addr);

    // 设置超时
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 200000;  // 200 ms
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));

    remote_server_addr.sin_port = htons(PORT_RXD);
    if(bind(client_fd, (struct sockaddr*)&remote_server_addr, remote_server_addr_len) == -1) {
        std::cerr << "[ERROR] RXD 端口绑定失败" << std::endl;
        close(client_fd);
        return false;
    }
    remote_server_addr.sin_port = htons(PORT_TXD);

    flag_udp = true;

    std::cout << "[SUCCESS] UDP 客户端创建成功" << std::endl;

    return true;
}

bool socket_recvFrame_client(size_t length) {
    // 接收图片数据
    ssize_t length_runtime = length;
    ssize_t recv_length = -1;
    unsigned char* buffer = (unsigned char*)malloc(length_runtime * sizeof(unsigned char));
    unsigned char* recv_buffer = buffer;

    if(flag_tcp) {   
        // 先接收需要丢弃的数据
        if(0 != length % 4) {
            std::cout << "[SUCCESS] 已接收数据：" << socket_recv_client(buffer, (4 - (length % 4))) << " Byte\n";
        }

        while(length_runtime > 0) {
            recv_length = socket_recv_client(recv_buffer, 
                (length_runtime < 32768 ? length_runtime : 32768));
            
            if (recv_length == -1) {
                // 接收失败，检查errno
                int err = errno;
                std::cerr << "[ERROR] 接收数据失败：(" << err << ") " << strerror(err) << std::endl;
                // 处理错误，例如关闭连接或重试
                return false;
            }
            length_runtime -= recv_length;
            recv_buffer += recv_length;
            
            std::cout << "[SUCCESS] 已接收数据：" << recv_length << " Byte，";
            std::cout << "累计接收数据：" << length - length_runtime << " Byte\n";
        }

    } else if(flag_udp) {
        bool flag_uselessData = false;    // 所有数据为保证4位发送，前有小于4个字节的无用数据
        short retry = 0;

        stopAndWait_ARQ_t<unsigned char> send_buff;
        stopAndWait_ARQ_t<unsigned char> recv_buff_para;
        unsigned char* recv_buff = (unsigned char*)malloc(32768);

        memset(&send_buff, 0, sizeof(send_buff));
        
        // 构造响应包
        send_buff.SN = 0;
        send_buff.RN = 0;
        send_buff.size = 20;
        
        send_buff.data = (unsigned char*)malloc(4);    // 只分配四个字节
        *(send_buff.data) = frame_flag[0];
        *(send_buff.data + 1) = frame_flag[1];
        *(send_buff.data + 2) = frame_flag[2];
        *(send_buff.data + 3) = frame_flag[3];

        memset(&recv_buff_para, 0, sizeof(stopAndWait_ARQ_t<unsigned char>));

        while(length_runtime > 0) {
            memset(recv_buff, 0, 32768);
            recv_length = socket_recv_client(recv_buff, 
                (length_runtime < 32768 ? length_runtime + 16 : 32768));
            if((~recv_length) == 0) {
                retry += 1;
                std::cerr << "[ERROR] 接收数据失败，重试" << retry << "次，";
                std::cout << "发送响应包(" << send_buff.SN << ", " << send_buff.RN << ")" << std::endl;
                socket_send_client(&send_buff, 20);
                if(20 == retry) {
                    // 接收失败，检查errno
                    int err = errno;
                    std::cerr << "[ERROR] 接收数据失败：(" << err << ") " << strerror(err) << std::endl;
                    // 处理错误，例如关闭连接或重试
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            send_buff.RN += 1;

            std::cout << "[INFO] 已接收数据(" << send_buff.SN << ", " << send_buff.RN << ")：" << recv_length << " Byte\n";

            recv_buff_para.SN = *((int*)recv_buff);
            recv_buff_para.RN = *(int*)(recv_buff + 4);
            recv_buff_para.size = *(int*)(recv_buff + 8) - 16;
            recv_buff_para.data = recv_buff + 16;

            if(!flag_uselessData) {
                std::cout << "[INFO] 无用位数：" << 4 - (length % 4) << std::endl;
                recv_buff_para.size -= (4 - (length % 4));
                recv_buff_para.data += (4 - (length % 4));
                flag_uselessData = true;
            }

            if( recv_buff_para.SN == send_buff.RN && recv_buff_para.size <= (length_runtime < 32768 ? length_runtime : 32768) ) {
                std::cout << "[SUCCESS] 数据正确(" << recv_buff_para.SN << ", " << recv_buff_para.RN << ")：" << recv_buff_para.size << " Byte\n";
                memcpy(recv_buffer, recv_buff_para.data, recv_buff_para.size);
                recv_buffer += recv_buff_para.size;
                send_buff.SN += 1; 
                std::cout << "[INFO] 发送响应包(" << send_buff.SN << ", " << send_buff.RN << ")" << std::endl;
                socket_send_client(&send_buff, 20);
            } else {
                std::cerr << "[ERROR] 接收无效(" << recv_buff_para.SN << ", " << recv_buff_para.RN << ")" << std::endl;
                send_buff.RN -= 1;
                continue;
            }
            
            length_runtime -= recv_buff_para.size;
            recv_buff_para.data += recv_buff_para.size;
            std::cout << "[SUCCESS] 累计接收数据：" << length - length_runtime << " Byte\n";
            retry = 0;
        }

        writeHexToFile(buffer, length, "log");

    } else {
        std::cerr << "[ERROR] 无连接不能接收帧" << std::endl;
        return false;
    }

    // 确保接收到的数据长度与预期相符
    if(length_runtime != 0 || (recv_buffer - buffer != length)) {
        std::cerr << "[ERROR] 接收到的数据长度不正确。" << std::endl;
        // 处理错误，例如关闭连接或重试
        return false;
    } else {
        std::vector<uchar> encode_data(buffer, buffer + length);
        
        // 保存图片数据到文件
        cv::Mat frame = cv::imdecode(encode_data, cv::IMREAD_UNCHANGED);
        if(frame.empty())    std::cerr << "[ERROR] 图像解码失败" << std::endl;
        else    cv::imwrite("接收图片.png", frame);
    }

    return true;
}

ssize_t socket_send_client(void* data, size_t length) {
    if(flag_tcp) {
        return socket_send_tcp_client(data, length);
    } else if(flag_udp) {
        return socket_send_udp_client(data, length);
    } else {
        std::cerr << "[ERROR] 无连接不能发送" << std::endl;
        return -1;
    }
    
}

ssize_t socket_send_tcp_client(void* data, size_t length) {
    return send(client_fd, data, length, 0);
}

ssize_t socket_send_udp_client(void* data, size_t length) {
    auto ret = sendto(client_fd, data, length, 0,
        (sockaddr*)&remote_server_addr, remote_server_addr_len);
    if(-1 != ret) {
        std::cout << "[INFO] 发送消息到" << inet_ntoa(remote_server_addr.sin_addr);
        std::cout << "(" << ntohs(remote_server_addr.sin_port) << ")的消息: ";
        std::cout << ret << " Byte" << std::endl;
    }
    return ret;
}


ssize_t socket_recv_client(void* buffer, size_t length) {
    if(flag_tcp) {
        return socket_recv_tcp_client(buffer, length);
    } else if(flag_udp) {
        return socket_recv_udp_client(buffer, length);
    } else {
        std::cerr << "[ERROR] 无连接不能接收" << std::endl;
        return -1;
    }
}

ssize_t socket_recv_tcp_client(void* buffer, size_t length) {
    return recv(client_fd, buffer, length, 0);
}

ssize_t socket_recv_udp_client(void*& buffer, size_t length) {
    return recvfrom(client_fd, buffer, length, 0,
        (sockaddr*)&remote_server_addr, &remote_server_addr_len);
}

bool socket_disconnect_client(void) {
    // 关闭socket
    close(client_fd);

    flag_tcp = false;
    flag_udp = false;

    return true;
}

int writeHexToFile(unsigned char *data, size_t size, const char *filename) {
    // 打开文件
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("无法打开文件");
        return -1;
    }

    // 写入十六进制数据
    for (size_t i = 0; i < size; ++i) {
        // 将每个字节转换为十六进制
        fprintf(file, "%02x", data[i]);
    }

    // 关闭文件
    fclose(file);
    return 0;
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
