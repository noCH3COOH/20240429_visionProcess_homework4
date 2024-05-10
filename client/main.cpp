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

#define SEND_PACKAGE_MAX_SIZE (size_t)(16384)
#define NAME_SHOW_WINDOW "【接收数据】"

#define byte_t unsigned char

// ==================== class ====================

template <typename T>
class stopAndWait_ARQ_t {
public:
    int SN, RN;
    size_t size;
    T* data;

public:
    stopAndWait_ARQ_t();
    stopAndWait_ARQ_t(int SN, int RN, size_t size, T* data);
    ~stopAndWait_ARQ_t();
};

// ==================== global variables ====================

bool flag_tcp = false;
bool flag_udp = false;

int client_fd;
sockaddr_in remote_server_addr;
socklen_t remote_server_addr_len;

byte_t frame_flag[] = {
    0xD0, 0xA0, 0xD0, 0xA0
};

byte_t* ack_buffer;

int* ack_buffer_para_SN;
int* ack_buffer_para_RN;
size_t* ack_buffer_para_size;
byte_t* ack_buffer_para_data;

byte_t* sendBuff;

stopAndWait_ARQ_t package_keep( (int)-1, (int)0, (size_t)-1, frame_flag );
stopAndWait_ARQ_t package_stop( (int)0, (int)-1, (size_t)-1, frame_flag );
stopAndWait_ARQ_t package_ack( (int)-1, (int)-1, (size_t)-1, frame_flag );

// ==================== functions declaration ====================

bool socket_set_tcp_client(void);
bool socket_set_udp_client(void);

bool socket_recvFrame_client();
bool socket_wait4Signal_client();
bool socket_sendACK_client();

ssize_t socket_send_client(void* data, size_t len);
ssize_t socket_send_tcp_client(void* data, size_t len);
ssize_t socket_send_udp_client(void* data, size_t len);

ssize_t socket_recv_client(void* buffer, size_t len);
ssize_t socket_recv_tcp_client(void* buffer, size_t len);
ssize_t socket_recv_udp_client(void*& buffer, size_t len);

bool socket_disconnect_client(void);

int log_frame(byte_t *data, size_t size, const char *filename);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;

    cv::namedWindow(NAME_SHOW_WINDOW);    

    ack_buffer = (byte_t*)malloc(20 * sizeof(byte_t));
    ack_buffer_para_SN = (int*)ack_buffer;
    ack_buffer_para_RN = (int*)(ack_buffer + 4);
    ack_buffer_para_size = (size_t*)(ack_buffer + 8);
    ack_buffer_para_data = ack_buffer + 16;

    sendBuff = (byte_t*)malloc(20 * sizeof(byte_t));

    // if(!socket_set_tcp_client()) {
    if(!socket_set_udp_client()) {
        // std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    } 

    do {
        socket_recvFrame_client();
        socket_sendACK_client();

        if(socket_wait4Signal_client()) {
            socket_sendACK_client();
        } else {
            break;
        }      
    } while(1);

    // std::cout << "[SUCCESS] 图片接收完成" << std::endl;
    socket_disconnect_client();
    cv::destroyAllWindows();
    cv::waitKey(0);

    free(ack_buffer);

    return 0;
}

// ==================== functions ====================

  // ==================== 图传相关 ====================

/**
 * @brief 接收一帧数据
*/
bool socket_recvFrame_client() {
    // 接收图片数据
    ssize_t len;
    ssize_t len_remain;
    ssize_t recv_len = -1;
    byte_t* buffer;
    byte_t* p_toBuffer;

    if(flag_tcp) {   
        auto ret = socket_recv_client(&len, sizeof(len));
        // std::cout << "[SUCCESS] 已接收数据：" << ret << " Byte" << std::endl;
        if(ret != -1) {
            // std::cout << "[INFO] 接收图片大小：" << len << " Byte" << std::endl;
        } else {
            return false;
        }

        len_remain = len;
        buffer = (byte_t*)malloc(len_remain * sizeof(byte_t));
        p_toBuffer = buffer;

        // 先接收需要丢弃的数据
        if(0 != len % 4) {
            auto ret = socket_recv_client(buffer, (4 - (len % 4)));
            // std::cout << "[SUCCESS] 已接收数据：" << ret << " Byte\n";
        }

        while(len_remain > 0) {
            recv_len = socket_recv_client(p_toBuffer, 
                (len_remain < SEND_PACKAGE_MAX_SIZE ? len_remain : SEND_PACKAGE_MAX_SIZE));
            
            if (recv_len == -1) {
                // 接收失败，检查errno
                int err = errno;
                std::cerr << "[ERROR] 接收数据失败：(" << err << ") " << strerror(err) << std::endl;
                // 处理错误，例如关闭连接或重试
                return false;
            }
            len_remain -= recv_len;
            p_toBuffer += recv_len;
            
            // std::cout << "[SUCCESS] 已接收数据：" << recv_len << " Byte\n";
            // std::cout << "[SUCCESS] 累计接收数据：" << len - len_remain << " Byte\n";
        }

    } else if(flag_udp) {
        ssize_t ret;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            socket_send_client(frame_flag, 4);
            ret = socket_recv_client(&len, sizeof(len));
            // std::cout << "[SUCCESS] 已接收数据：" << ret << " Byte" << std::endl;
        } while( -1 == ret );
        // std::cout << "[INFO] 接收图片大小：" << len << " Byte" << std::endl;
        std::cout << "[SUCCESS] 建立一条 UDP 链接到 " << inet_ntoa(remote_server_addr.sin_addr);
        std::cout << ":" << ntohs(remote_server_addr.sin_port) << std::endl;

        socket_send_client(frame_flag, 4);
        // std::cout << "[SUCCESS] 响应发送成功 " << inet_ntoa(remote_server_addr.sin_addr);
        // std::cout << ":" << ntohs(remote_server_addr.sin_port) << std::endl;

        len_remain = len;
        buffer = (byte_t*)malloc(len_remain * sizeof(byte_t));
        p_toBuffer = buffer;
    

        bool flag_uselessData = false;    // 所有数据为保证4位发送，前有小于4个字节的无用数据
        short retry = 0;

        stopAndWait_ARQ_t<byte_t> response_buff;
        stopAndWait_ARQ_t<byte_t> udp_recv_cache_para;
        byte_t* udp_recv_cache = (byte_t*)malloc(SEND_PACKAGE_MAX_SIZE * sizeof(byte_t));

        memset(&response_buff, 0, sizeof(response_buff));
        
        // 构造响应包
        response_buff.SN = 0;
        response_buff.RN = 0;
        response_buff.size = 20;
        
        response_buff.data = (byte_t*)malloc(4 * sizeof(byte_t));    // 只分配四个字节
        *(response_buff.data) = frame_flag[0];
        *(response_buff.data + 1) = frame_flag[1];
        *(response_buff.data + 2) = frame_flag[2];
        *(response_buff.data + 3) = frame_flag[3];

        memset(&udp_recv_cache_para, 0, sizeof(stopAndWait_ARQ_t<byte_t>));

        while(len_remain > 0) {
            memset(udp_recv_cache, 0, SEND_PACKAGE_MAX_SIZE);
            recv_len = socket_recv_client(udp_recv_cache, 
                (len_remain < SEND_PACKAGE_MAX_SIZE ? len_remain + 16 : SEND_PACKAGE_MAX_SIZE));
            if((~recv_len) == 0) {
                retry += 1;
                // std::cerr << "[ERROR] 接收数据失败，重试" << retry << "次，";
                // std::cout << "发送响应包(" << response_buff.SN << ", " << response_buff.RN << ")" << std::endl;
                socket_send_client(&response_buff, 20);
                if(20 == retry) {
                    // 接收失败，检查errno
                    int err = errno;
                    std::cerr << "[ERROR] 接收数据失败：(" << err << ") " << strerror(err) << std::endl;
                    // 处理错误，例如关闭连接或重试
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            response_buff.RN += 1;

            // std::cout << "[INFO] 已接收数据(" << response_buff.SN << ", " << response_buff.RN << ")：" << recv_len << " Byte\n";

            udp_recv_cache_para.SN = *((int*)udp_recv_cache);
            udp_recv_cache_para.RN = *(int*)(udp_recv_cache + 4);
            udp_recv_cache_para.size = *(int*)(udp_recv_cache + 8) - 16;
            udp_recv_cache_para.data = udp_recv_cache + 16;

            if(!flag_uselessData) {
                // std::cout << "[INFO] 无用位数：" << 4 - (len % 4) << std::endl;
                udp_recv_cache_para.size -= (4 - (len % 4));
                udp_recv_cache_para.data += (4 - (len % 4));
                flag_uselessData = true;
            }

            if( udp_recv_cache_para.SN == response_buff.RN && udp_recv_cache_para.size <= (len_remain < SEND_PACKAGE_MAX_SIZE ? len_remain : SEND_PACKAGE_MAX_SIZE) ) {
                // std::cout << "[SUCCESS] 数据正确(" << udp_recv_cache_para.SN << ", " << udp_recv_cache_para.RN << ")：" << udp_recv_cache_para.size << " Byte\n";
                memcpy(p_toBuffer, udp_recv_cache_para.data, udp_recv_cache_para.size);
                p_toBuffer += udp_recv_cache_para.size;
                response_buff.SN += 1; 
                // std::cout << "[INFO] 发送响应包(" << response_buff.SN << ", " << response_buff.RN << ")" << std::endl;
                socket_send_client(&response_buff, 20);
            } else {
                // std::cerr << "[ERROR] 接收无效(" << udp_recv_cache_para.SN << ", " << udp_recv_cache_para.RN << ")" << std::endl;
                response_buff.RN -= 1;
                continue;
            }
            
            len_remain -= udp_recv_cache_para.size;
            udp_recv_cache_para.data += udp_recv_cache_para.size;
            // std::cout << "[SUCCESS] 累计接收数据：" << len - len_remain << " Byte\n";
            retry = 0;
        }

        // 不知道为什么前两位不对，因为前几位数据一般为格式头，所有针对格式打个补丁解决算了
        if(buffer[1] != 0x50 || buffer[2] != 0x4E || buffer[3] != 0x47) {    // 这是针对 .png 格式的
            buffer[0] = 0x89;
            buffer[1] = 0x50;
            buffer[2] = 0x4E;
            buffer[3] = 0x47;
        }
        log_frame(buffer, len, "log");

    } else {
        // std::cerr << "[ERROR] 无连接不能接收帧" << std::endl;
        return false;
    }

    // 确保接收到的数据长度与预期相符
    if(len_remain != 0 || (p_toBuffer - buffer != len)) {
        // std::cerr << "[ERROR] 接收到的数据长度不正确。" << std::endl;
        // 处理错误，例如关闭连接或重试
        return false;
    } else {
        std::vector<uchar> encode_data(buffer, buffer + len);
        
        // 保存图片数据到文件
        cv::Mat frame = cv::imdecode(encode_data, cv::IMREAD_UNCHANGED);
        if(frame.empty())    std::cerr << "[ERROR] 图像解码失败" << std::endl;
        else {
            cv::imwrite("接收图片.png", frame);
            cv::imshow(NAME_SHOW_WINDOW, frame);
            std::cout << "[SUCCESS] 完成一帧接收" << std::endl;
        }
    }

    return true;
}

/**
 * @brief 等待 server 端信号发送
*/
bool socket_wait4Signal_client() {
    ssize_t ret;
    short retry = 0;
    bool flag_ack_ok = false;

    do {
        memset(ack_buffer, 0, 20);

        if(40 == retry) {
            std::cerr << "[ERROR] 未收到响应" << std::endl;
            return false;
        }

        ret = socket_recv_client(ack_buffer, 20);

        if( -1 != ret && 
            -1 == *ack_buffer_para_SN &&
            -1 == (ssize_t)(*ack_buffer_para_size)) {


            flag_ack_ok = true;
        } else {
            retry += 1;
            std::cerr << "[ERROR] 未收到帧间响应(" << ret << ")，重试" << retry << "次" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } while(!flag_ack_ok);

    return true;
}

/**
 * @brief 发送ACK
*/
bool socket_sendACK_client() {
    *(int*)sendBuff = package_ack.SN;
    *(int*)(sendBuff + 4) = package_ack.RN;
    *(size_t*)(sendBuff + 8) = package_ack.size;
    memcpy(sendBuff + 16, package_ack.data, 4);
    socket_send_client(sendBuff, 20);

    return true;
}

  // ==================== socket 相关 ====================

/**
 * @brief  clinet 端以 `TCP` 设置 socket
*/
bool socket_set_tcp_client(void) {
    // 创建socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        // std::cerr << "[ERROR] 创建socket失败" << std::endl;
        return false;
    }

    // 连接服务器
    memset(&remote_server_addr, 0, sizeof(remote_server_addr));
    remote_server_addr.sin_family = AF_INET;    // 使用IPv4
    remote_server_addr.sin_addr.s_addr = inet_addr(IP_SERVER);    // 服务器IP地址
    remote_server_addr.sin_port = htons(PORT_TXD);    // 服务器端口
    if (connect(client_fd, (sockaddr*)&remote_server_addr, sizeof(remote_server_addr)) == -1) {
        // std::cerr << "[ERROR] 端口绑定失败" << std::endl;
        close(client_fd);
        return false;
    }
    remote_server_addr_len = sizeof(remote_server_addr);

    flag_tcp = true;

    return true;
}

/**
 * @brief  clinet 端以 `UDP` 设置 socket
*/
bool socket_set_udp_client(void) {
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

    // std::cout << "[SUCCESS] UDP 客户端创建成功" << std::endl;

    return true;
}

/**
 * @brief clinet 端向 server 端端发送数据
*/
ssize_t socket_send_client(void* data, size_t len) {
    if(flag_tcp) {
        return socket_send_tcp_client(data, len);
    } else if(flag_udp) {
        return socket_send_udp_client(data, len);
    } else {
        // std::cerr << "[ERROR] 无连接不能发送" << std::endl;
        return -1;
    }
    
}

/**
 * @brief clinet 端以`TCP`方式向 server 端端发送数据
*/
ssize_t socket_send_tcp_client(void* data, size_t len) {
    return send(client_fd, data, len, 0);
}

/**
 * @brief clinet 端以`UDP`方式向 server 端端发送数据
*/
ssize_t socket_send_udp_client(void* data, size_t len) {
    auto ret = sendto(client_fd, data, len, 0,
        (sockaddr*)&remote_server_addr, remote_server_addr_len);
    if(-1 != ret) {
        // std::cout << "[INFO] 发送消息到" << inet_ntoa(remote_server_addr.sin_addr);
        // std::cout << "(" << ntohs(remote_server_addr.sin_port) << ")的消息: ";
        // std::cout << ret << " Byte" << std::endl;
    }
    return ret;
}

/**
 * @brief clinet 端接收来自 server 端的数据
*/
ssize_t socket_recv_client(void* buffer, size_t len) {
    if(flag_tcp) {
        return socket_recv_tcp_client(buffer, len);
    } else if(flag_udp) {
        return socket_recv_udp_client(buffer, len);
    } else {
        // std::cerr << "[ERROR] 无连接不能接收" << std::endl;
        return -1;
    }
}

/**
 * @brief clinet 端以`TCP`方式接收来自 server 端的数据
*/
ssize_t socket_recv_tcp_client(void* buffer, size_t len) {
    return recv(client_fd, buffer, len, 0);
}

/**
 * @brief clinet 端以`UDP`方式接收来自 server 端的数据
*/
ssize_t socket_recv_udp_client(void*& buffer, size_t len) {
    return recvfrom(client_fd, buffer, len, 0,
        (sockaddr*)&remote_server_addr, &remote_server_addr_len);
}

/**
 * @brief socket 拆除连接
*/
bool socket_disconnect_client(void) {
    // 关闭socket
    close(client_fd);

    flag_tcp = false;
    flag_udp = false;

    return true;
}

/**
 * @brief 记录一帧
*/
int log_frame(byte_t *data, size_t size, const char *filename) {
    // 打开文件
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("无法打开文件");
        return -1;
    }

    // 写入十六进制数据
    for (size_t i = 0; i < size; ++i) {
        fputc(data[i], file);
    }

    // 关闭文件
    fclose(file);
    return 0;
}

// ==================== class implement ====================

template <typename T>
stopAndWait_ARQ_t<T>::stopAndWait_ARQ_t() {
    SN = 0; // 初始化序列号
    RN = 0; // 初始化确认号
    size = 0;
}

template <typename T>
stopAndWait_ARQ_t<T>::stopAndWait_ARQ_t(int SN, int RN, size_t size, T* data)
    : SN(SN), RN(RN), size(size), data(data) {

}

template <typename T>
stopAndWait_ARQ_t<T>::~stopAndWait_ARQ_t() {}
