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

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

// ==================== define ====================

#define PATH_VIDEO "./test_640x480.mp4"

#define IP_SERVER "127.0.0.1"

#define PORT_TXD 12321
#define PORT_RXD 12322

#define SEND_PACKAGE_MAX_SIZE (size_t)(16384)

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

int server_fd;
int client_fd;

byte_t* buffer;

byte_t* uni_buff;
byte_t* sendBuff;
byte_t* ack_buffer;

int* ack_buffer_para_SN;
int* ack_buffer_para_RN;
size_t* ack_buffer_para_size;
byte_t* ack_buffer_para_data;

sockaddr_in server_addr;
socklen_t server_addr_len;
sockaddr_in remote_client_addr;
socklen_t remote_client_addr_len;

std::vector<uchar> encode_data;

byte_t frame_flag[] = {
    0xD0, 0xA0, 0xD0, 0xA0
};

stopAndWait_ARQ_t package_keep( (int)-1, (int)0, (size_t)-1, frame_flag );
stopAndWait_ARQ_t package_stop( (int)0, (int)-1, (size_t)-1, frame_flag );

// ==================== functions declaration ====================

bool socket_set_tcp_server(void);
bool socket_set_udp_server(void);

bool socket_sendFrame_server(cv::Mat& frame);
bool socket_sendKeep_server();
bool socket_sendStop_server();
bool socket_wait4ACK_server();

ssize_t socket_send_server(void* data, size_t len);
ssize_t socket_send_tcp_server(void* data, size_t len);
ssize_t socket_send_udp_server(void* data, size_t len);

ssize_t socket_recv_server(void* buffer, size_t len);
ssize_t socket_recv_tcp_server(void* buffer, size_t len);
ssize_t socket_recv_udp_server(void* buffer, size_t len);

bool socket_disconnect_server(void);

int log_frame(byte_t *data, size_t size, const char *filename);

// ==================== main ====================

int main() {
    std::cout << "[INFO] 进程PID：" << getpid() << std::endl;

    int key = 0;

    uni_buff = (byte_t*)malloc(128 * sizeof(byte_t));
    buffer = (byte_t*)malloc(128 * sizeof(byte_t));

    sendBuff = uni_buff;
    ack_buffer = uni_buff + sizeof(uni_buff) / 2;
    
    memset(buffer, 0, 2 * SEND_PACKAGE_MAX_SIZE); 
    ack_buffer_para_SN = (int*)ack_buffer;
    ack_buffer_para_RN = (int*)(ack_buffer + 4);
    ack_buffer_para_size = (size_t*)(ack_buffer + 8);
    ack_buffer_para_data = ack_buffer + 16; 

    // if(!socket_set_tcp_server()) {
    if(!socket_set_udp_server()) {
        std::cerr << "[ERROR] socket 创建失败" << std::endl;
        return 1;
    }

    // cv::VideoCapture video2send(PATH_VIDEO);
    // if(!video2send.isOpened())
    // {
    //     std::cerr << "[ERROR] 视频读入异常" << std::endl;
    //     return -1;
    // }

    av_register_all();
    AVFormatContext* formatContext = avformat_alloc_context();
    if (!formatContext) {
        // 错误处理
    }

    // 打开视频文件
    if (avformat_open_input(&formatContext, "video.mp4", NULL, NULL) < 0) {
        // 错误处理
    }

    // 查找视频流
    AVCodecParameters* codecParameters = NULL;
    AVCodec* codec = NULL;
    int videoStreamIndex = -1;
    for (int i = 0; i < formatContext->nb_streams; i++) {
        codecParameters = formatContext->streams[i]->codecpar;
        if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            codec = avcodec_find_decoder(codecParameters->codec_id);
            if (codec) {
                videoStreamIndex = i;
                break;
            }
        }
    }

    if (videoStreamIndex == -1) {
        // 错误处理
        std::cerr << "[ERROR] 无视频流" << std::endl;
    }

    // 创建解码器上下文
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParameters);

    // 打开解码器
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        // 错误处理
        std::cerr << "[ERROR] 解码器错误" << std::endl;
    }

    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();

    while(av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            // 解码视频帧
            if (avcodec_send_packet(codecContext, packet) < 0) {
                // 错误处理
                std::cerr << "[ERROR] 解码错误" << std::endl;
            }
            while (avcodec_receive_frame(codecContext, frame) == 0) {
                // 将AVFrame转换为cv::Mat
                cv::Mat mat(cv::Size(frame->width, frame->height), CV_8UC3, frame->data, 0UL);
                // 处理mat
                socket_sendFrame_server(mat);
                if(!socket_wait4ACK_server()) {
                    std::cerr << "[ERROR] 数据包传输失败" << std::endl;
                    break;
                } else {
                    std::cout << "[SUCCESS] 数据包到达" << std::endl;
                }
                
                socket_sendKeep_server();
                if(!socket_wait4ACK_server()) {
                    std::cerr << "[ERROR] 传输失败" << std::endl;
                    break;
                } else {
                    std::cout << "[SUCCESS] 保持请求到达" << std::endl;
                }
            }
        }
        av_packet_unref(packet);        

        key = cv::waitKey();
    }

    // 关闭socket
    free(uni_buff);
    free(buffer);
    socket_disconnect_server();

    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    return 0;
}

// ==================== functions ====================

  // ==================== 图传相关 ====================

/**
 * @brief 发送一帧数据
*/
bool socket_sendFrame_server(cv::Mat& frame) { 
    cv::imencode(".png", frame, encode_data);
    
    size_t len = encode_data.end() - encode_data.begin();
    size_t package_len = len;
    if(0 != len % 4)    package_len = len + (4 - (len % 4));    // 凑够 4 整除
    buffer = (byte_t*)realloc(buffer, package_len * sizeof(byte_t));
    std::copy(encode_data.begin(), encode_data.end(), buffer + (4 - (len % 4)));
    log_frame(buffer, package_len, "log");

    // 发送图片数据到客户端
    ssize_t send_len = 0;
    
    // std::cout << "[INFO] 发送图片大小：" << len << " Byte" << std::endl;
    send_len = socket_send_server(&len, sizeof(len));
    if(-1 != send_len) {
        // std::cout << "[SUCCESS] 已发送大小数据：" << send_len << " Byte" << std::endl; // 先发送图片大小
    } else {    // 发送失败，处理错误
        int err = errno;    
        // std::cerr << "[ERROR] 发送大小数据失败：" << strerror(err) << std::endl;
        
        // 这里可以添加错误处理代码，例如关闭连接或重试
        return false;
    }

    if(flag_udp) {
        ssize_t ret;
        do {
            // std::cout << "[INFO] 等待 UDP 客户端响应" << std::endl;
            ret = socket_recv_udp_server(buffer, 4);
            // ret = recvfrom(server_fd, buffer, 4, 0, (sockaddr*)&remote_client_addr, &remote_client_addr_len);

            if(ret > 0 && ret <= 4) {
                // std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
                // std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息(";
                // std::cout << ret << " Byte)：";
                // printf("0x%x 0x%x 0x%x 0x%x",buffer[0], buffer[1], buffer[2], buffer[3]);
                // std::cout << std::endl;
                break;
            }
            else    // std::cerr << "[ERROR] 接收错误" << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } while(1);
        // std::cout << "[SUCCESS] UDP服务端已响应 " << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // UDP 直接分包发送不可靠，故这里运用停等式ARQ进行分包发送
        stopAndWait_ARQ_t<byte_t> send_package_para;    // 停等式ARQ发包缓冲区
        byte_t* send_package = (byte_t*)malloc(SEND_PACKAGE_MAX_SIZE * sizeof(byte_t));
        size_t max_len_send_package_para = SEND_PACKAGE_MAX_SIZE - 2*sizeof(int) - sizeof(size_t);

        send_package_para.data = buffer;
        send_package_para.SN = 0;
        send_package_para.RN = 0;

        byte_t* udp_recv_cache = (byte_t*)malloc(20 * sizeof(byte_t));
        ssize_t recv_len = -1;
        int recv_RN;
        short retry;
        bool recv_notOK;
        bool send_debug = false;

        while(package_len > 0) {
            recv_notOK = false;
            retry = 0;

            // 发送数据块，每次最多 SEND_PACKAGE_MAX_SIZE - 2*sizeof(int) - sizeof(size_t) == 32752 字节
            send_package_para.size = (package_len < max_len_send_package_para) ? package_len + 16 : SEND_PACKAGE_MAX_SIZE;
            send_package_para.SN += 1;

            *((int*)send_package) = send_package_para.SN;
            *(int*)(send_package + 4) = send_package_para.RN;
            *(size_t*)(send_package + 8) = send_package_para.size;
            memcpy(send_package + 16, send_package_para.data, (package_len < max_len_send_package_para) ? package_len : SEND_PACKAGE_MAX_SIZE);

            send_len = socket_send_server(send_package, send_package_para.size);
            if (send_len == -1) {    // 发送失败，处理错误
                int err = errno;    
                std::cerr << "[ERROR] 发送数据失败：" << strerror(err) << std::endl;
                
                // 这里可以添加错误处理代码，例如关闭连接或重试
                return false;
            }
            // std::cout << "[SUCCESS] 已发送数据(" << send_package_para.SN << ", " << send_package_para.RN << ")：" << send_len << " Byte\n";

            // 接收响应
            do {
                recv_len = socket_recv_server(udp_recv_cache, 20);
                if(-1 != recv_len) {
                    recv_RN = *((int*)(udp_recv_cache + 4));
                    if(recv_RN == send_package_para.SN) {    // 应该服务端发SN是几，响应的RN就是几，服务端一直有 RN <= SN
                        send_package_para.RN += 1;
                        break;
                    }
                } else {
                    // std::cerr << "[ERROR] 接收客户端响应出错，重试" << retry + 1 << "次" << std::endl;
                    retry += 1;
                    if(40 == retry) {
                        // std::cerr << "[ERROR] 重试全部失败，数据重传" << std::endl;
                        send_package_para.SN -= 1;
                        recv_notOK = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } while(1);

            if(recv_notOK)    continue;
            send_package_para.data += (send_len - 16);    // 直接使用send_len作为偏移量
            package_len -= (send_len - 16);
            // std::cout << "[SUCCESS] 响应接收成功(" << retry << ")，累计发送数据：" << len - package_len << " Byte\n";
        }

    } else if(flag_tcp) {
        byte_t* now_inbuff = buffer;
        while(package_len > 0) {
            // 发送数据块，每次最多SEND_PACKAGE_MAX_SIZE字节
            send_len = socket_send_server(now_inbuff, (package_len < SEND_PACKAGE_MAX_SIZE ? package_len : SEND_PACKAGE_MAX_SIZE));
            if (send_len == -1) {    // 发送失败，处理错误
                int err = errno;    
                // std::cerr << "[ERROR] 发送数据失败：" << strerror(err) << std::endl;
                
                // 这里可以添加错误处理代码，例如关闭连接或重试
                return false;
            }
            // std::cout << "[SUCCESS] 已发送数据：" << send_len << " Byte";
            now_inbuff += send_len; // 直接使用send_len作为偏移量
            package_len -= send_len;
            // std::cout << "累计接收数据：" << len - package_len << " Byte\n";
    
            // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return true;
}

/**
 * @brief 发送继续信号
*/
bool socket_sendKeep_server() {
    *(int*)sendBuff = package_keep.SN;
    *(int*)(sendBuff + 4) = package_keep.RN;
    *(size_t*)(sendBuff + 8) = package_keep.size;
    memcpy(sendBuff + 16, package_keep.data, 4);
    socket_send_server(sendBuff, 20);

    return true;
}

/**
 * @brief 发送停止信号
*/
bool socket_sendStop_server() {
    *(int*)sendBuff = package_stop.SN;
    *(int*)(sendBuff + 4) = package_stop.RN;
    *(size_t*)(sendBuff + 8) = package_stop.size;
    memcpy(sendBuff + 16, package_stop.data, 4);
    socket_send_server(sendBuff, 20);

    return true;
}

/**
 * @brief 等待响应
*/
bool socket_wait4ACK_server() {
    ssize_t ret;
    short retry = 0;
    bool flag_ack_ok = false;

    do {
        memset(ack_buffer, 0, 20);

        if(40 == retry) {
            std::cerr << "[ERROR] 未收到响应" << std::endl;
            return false;
        }

        ret = socket_recv_server(ack_buffer, 20);

        if( -1 != ret &&
            -1 == *ack_buffer_para_SN &&
            -1 == *ack_buffer_para_RN &&
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

  // ==================== socket 相关 ====================

/**
 * @brief  server 端以 `TCP` 设置 socket
*/
bool socket_set_tcp_server(void) {
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        // std::cerr << "[ERROR] 创建socket失败" << std::endl;
        return false;
    }

    // 绑定socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // 使用IPv4
    server_addr.sin_addr.s_addr = inet_addr(IP_SERVER); // 服务器IP地址
    server_addr.sin_port = htons(PORT_RXD); // 服务器端口
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        // std::cerr << "[ERROR] 绑定端口失败：" << std::endl;
        close(server_fd);
        return false;
    }

    // 监听socket
    if (listen(server_fd, 5) == -1) {
        // std::cerr << "[ERROR] 监听socket失败" << std::endl;
        close(server_fd);
        return false;
    }

    // 接受客户端连接
    remote_client_addr_len = sizeof(remote_client_addr);
    client_fd = accept(server_fd, (sockaddr*)&remote_client_addr, &remote_client_addr_len);
    if (client_fd == -1) {
        // std::cerr << "[ERROR] 接受客户端连接失败" << std::endl;
        close(server_fd);
        return false;
    }

    // std::cout << "[SUCCESS] 建立一条 TCP 链接到 " << inet_ntoa(remote_client_addr.sin_addr);
    // std::cout << ":" << ntohs(remote_client_addr.sin_port) << std::endl;

    flag_tcp = true;

    return true;
}

/**
 * @brief  server 端以 `UDP` 设置 socket
*/
bool socket_set_udp_server(void) {
    // 创建socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(-1==server_fd) {
        // std::cerr << "[ERROR] socket 创建错误" << std::endl;
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
        // std::cerr << "[ERROR] 端口绑定错误" << std::endl;
        close(server_fd);
        return false;
    }

    remote_client_addr_len = sizeof(remote_client_addr);

    ssize_t ret;
    do {
        memset(&remote_client_addr, 0, sizeof(remote_client_addr));
        std::cout << "[INFO] 等待 UDP 客户端连接" << std::endl;
        ret = socket_recv_udp_server(buffer, 4);
        // ret = recvfrom(server_fd, buffer, 4, 0, (sockaddr*)&remote_client_addr, &remote_client_addr_len);
        if(ret > 0 && ret <= 4) {
            // std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
            // std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息(";
            // std::cout << ret << " Byte)：";
            // printf("0x%x 0x%x 0x%x 0x%x",buffer[0], buffer[1], buffer[2], buffer[3]);
            // std::cout << std::endl;
            break;
        } else {
            // std::cerr << "[ERROR] 接收错误" << std::endl
        };
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    } while(1);
    std::cout << "[SUCCESS] 建立一条 UDP 链接到 " << inet_ntoa(remote_client_addr.sin_addr);
    std::cout << ":" << ntohs(remote_client_addr.sin_port) << std::endl;

    flag_udp = true;

    return true;
}

/**
 * @brief server 端向 clinet 端发送数据
*/
ssize_t socket_send_server(void* data, size_t len) {
    if(flag_tcp)    return socket_send_tcp_server(data, len);
    else if(flag_udp)    return socket_send_udp_server(data, len);
    else {
        // std::cerr << "[ERROR] 无连接不能发送" << std::endl;
        return -1;
    }
}

/**
 * @brief server 端以`TCP`方式向 clinet 端发送数据
*/
ssize_t socket_send_tcp_server(void* data, size_t len) {
    if(0 != len % 4) {
        // std::cout << "[WARNING] 发送数据非4位整倍" << std::endl;
    };
    
    return send(client_fd, data, len, 0);
}

/**
 * @brief server 端以`UDP`方式向 clinet 端发送数据
*/
ssize_t socket_send_udp_server(void* data, size_t len) {
    if(0 != len % 4) {
        // std::cout << "[WARNING] 发送数据非4位整倍" << std::endl;
    };
    
    return sendto(server_fd, data, len, 0,
        (sockaddr*)&remote_client_addr, remote_client_addr_len);
}

/**
 * @brief server 端接收来自 clinet 端的数据
*/
ssize_t socket_recv_server(void* buffer, size_t len) {
    if(flag_tcp)    return socket_recv_tcp_server(buffer, len);
    else if(flag_udp)    return socket_recv_udp_server(buffer, len);
    else {
        // std::cerr << "[ERROR] 无连接不能接收" << std::endl;
        return -1;
    }
}

/**
 * @brief server 端以`TCP`方式接收来自 clinet 端的数据
*/
ssize_t socket_recv_tcp_server(void* buffer, size_t len) {
    return recv(client_fd, buffer, len, 0);
}

/**
 * @brief server 端以`UDP`方式接收来自 clinet 端的数据
*/
ssize_t socket_recv_udp_server(void* buffer, size_t len) {
    auto ret = recvfrom(server_fd, buffer, len, 0, 
        (sockaddr*)&remote_client_addr, &remote_client_addr_len);
    if(-1 != ret) {
        // std::cout << "[INFO] 收到来自" << inet_ntoa(remote_client_addr.sin_addr);
        // std::cout << "(" << ntohs(remote_client_addr.sin_port) << ")的消息: ";
        // std::cout << ret << " Byte" << std::endl;
    }

    return ret;
}

/**
 * @brief socket 拆除连接
*/
bool socket_disconnect_server(void) {
    close(client_fd);
    close(server_fd);

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
