// ==================== includes ====================

#include <time.h>
#include <thread>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <opencv2/opencv.hpp>

// ==================== defines ====================

#define WINDOW_NAME "【捕获画面】"
#define PATH_VIDEO "./src.png"

#define SIZE_RECVBUF 1024
#define MAX_LEN_BUFF_FRAME 1

#define PATH_UNILOG "log.md"

#define VALUE_CHECK(value, min, max) ((value >= min) ? (value <= max ? value : max) : min) 

// ==================== struct ====================

struct SEND_PACKAGE_t
{
    uint32_t size;
    unsigned char data[3840*2160];
};

struct RETVAL_t
{
    bool res;
    SEND_PACKAGE_t* data;
};

// ==================== class ====================

class BUFF_t
{
public:
    BUFF_t() {
        buff = (SEND_PACKAGE_t*)malloc(MAX_LEN_BUFF_FRAME * sizeof(SEND_PACKAGE_t));
    };
    ~BUFF_t() {
        free(buff);
    };

private:
    SEND_PACKAGE_t* buff;
    
public:
    SEND_PACKAGE_t* getData_inBuff(int num){
        return &buff[num];
    }

    void storeData_inBuff(SEND_PACKAGE_t* tar, int num){
        memcpy(getData_inBuff(num), tar, sizeof(SEND_PACKAGE_t));
    }

    void clearData_inBuff(int num){
        memset(getData_inBuff(num), 0x00, sizeof(SEND_PACKAGE_t));
    }

    int getIndex_first_needSend(){
        if(allFull())    return -1;
        else    
            for(int i=0; i<MAX_LEN_BUFF_FRAME; i++){
                if(0 != buff[i].size)    return i;
            }
        return -1;
    }

    bool allFull(void){
        for(int i=0; i<MAX_LEN_BUFF_FRAME; i++){
            if(0 == buff[i].size)    return false;
        }
        return true;    // 全满了
    }
};

// ==================== global variables ====================

int key = 0;
int fps = 0;
int clientfd = -1;
int index_buff_store = 0;
int index_buff_send = 0;

bool flag_program_end = false;

time_t timer;
BUFF_t buff_frame;
clock_t start_time;
std::ofstream uni_log;

cv::Mat captureFrame;
cv::VideoCapture cap(PATH_VIDEO);

int serverfd;    // 创建 socket
struct sockaddr_in addr_server;
struct sockaddr_in addr_client;
socklen_t len_addr_client = sizeof(addr_client);

// ==================== functions ====================

RETVAL_t captureFrame_once(void);
RETVAL_t sendFrame_socket(int buff_index, int clientfd);
void make_log(std::string str, std::ostream & stdFlow, bool print_sw);

// ==================== main ====================

int main()
{
    uni_log.open(PATH_UNILOG, std::ios::out);    // 日志
    make_log("[INFO] 当前进程号：" + std::to_string(getpid()) + '\n', std::cout, true);

    // cap.set(3, 640);
    // cap.set(4, 480);
    {
        serverfd = socket(AF_INET, SOCK_STREAM, 0);    // 流模式，TCP
        // serverfd = socket(AF_INET, SOCK_DGRAM, 0);    // 数据报模式，UDP
	    if(-1 == serverfd) {
	    	make_log("[ERROR] socket 创建失败，程序退出\n", std::cerr, true);
	    	return -1;
	    }
        
        // 绑定端口
        memset(&addr_server, 0, sizeof(addr_server));
	    addr_server.sin_family = AF_INET;
	    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);    // 接收任何地址的数据
	    addr_server.sin_port = htons(3000);    // 3000 端口
	    if(-1 == bind(serverfd, (struct sockaddr *)&addr_server, sizeof(addr_server))) 
        {
	    	make_log("[ERROR] 端口绑定错误，程序退出\n", std::cerr, true);
	    	return -1;
	    }
	    
	    // 开始监听
	    if(-1 == listen(serverfd, 2)) 
        {
	    	make_log("[ERROR] socket 监听错误，程序退出\n", std::cerr, true);
	    	return -1;
	    }
    }
    make_log("[SUCCESS] socket开始监听3000端口\n", std::cout, true);

    time(&timer);

    while('q' != key && 'Q' != key)
    {
        // 准备链接
        clientfd = accept(serverfd, (struct sockaddr *)&addr_client, &len_addr_client);

        if(-1 == clientfd)
        {
            close(clientfd);    // 清除链接 
            make_log("[INFO] 链接未建立\n", std::cout, true);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        // else if(!buff_frame.allFull())
        // {
        //     make_log("[ERROR] 缓冲区全满\n", std::cerr, true);
        //     std::this_thread::sleep_for(std::chrono::seconds(1));
        // }
        else
        {
            // ==================== 循环开始 ====================

            make_log("[SUCCESS] 链接建立：" + std::to_string(clientfd) + '\n', std::cout, true);
    
            // ==================== socket 相关 ====================
    
            captureFrame_once();    // 捕获画面
            sendFrame_socket(index_buff_store, clientfd);
    
            // ==================== 循环结束 ====================
        }
        
        key = cv::waitKey(1);    // 获取按键状态
    }

    flag_program_end = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));    // 睡10ms给子线程一点时间反应

    close(serverfd);    // 清除链接 

    cv::destroyAllWindows();
    uni_log.close();
    return 0;
}

// ==================== functions ====================

RETVAL_t captureFrame_once(void)
{
    // cap >> captureFrame;    // 获取摄像头捕获画面
    // cv::flip(captureFrame, captureFrame, 1);
    captureFrame = cv::imread(PATH_VIDEO);
    
    std::string time_stamp;    // 时间戳
    std::vector<uchar> encode_data;
    SEND_PACKAGE_t* send_package = buff_frame.getData_inBuff(index_buff_store);
    
    time_stamp = std::ctime(&timer);
    time_stamp += " res:" + std::to_string(captureFrame.size().width) + 'x' + std::to_string(captureFrame.size().height);

    // 打印视频时间戳
    cv::putText(captureFrame, time_stamp, cv::Point(20,15), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(0, 0, 255));

    // 压缩视频打包
    cv::imencode(".jpg", captureFrame, encode_data);

    if(encode_data.size() <= sizeof(send_package->data))
        std::copy(encode_data.begin(), encode_data.end(), send_package->data);
    else
    {
        make_log("[ERROR] 缓冲区容量不足", std::cerr, true);
        return {false, NULL};
    }
    send_package->size = sizeof(*send_package);

    cv::imshow(WINDOW_NAME, captureFrame);    // 显示捕获内容

    return {true, send_package};
}

RETVAL_t sendFrame_socket(int buff_index, int clientfd)
{
	if (-1 == clientfd) 
    {
        return {false, NULL};
    }

    // memset(recvBuf, 0, sizeof(recvBuf));
    // 
	// // 接收数据
	// if (recv(clientfd, recvBuf, SIZE_RECVBUF, 0) > 0) 
    // {
	// 	make_log("[INFO] 收到数据：" + std::string(recvBuf) + '\n', false);
    //
    //     // 数据回传
    //     sendBuf = "[INFO] " + std::string(recvBuf);
	// 	if (send(clientfd, sendBuf.c_str(), sendBuf.length(), 0) != sendBuf.length()) 
    //     {
	// 		make_log("[ERROR] 回传错误\n", true);
	// 	}
	// } 
    // else 
    // {
	// 	make_log("[ERROR] 接收错误\n", true);
	// }

    SEND_PACKAGE_t* target = buff_frame.getData_inBuff(buff_index);
    buff_frame.clearData_inBuff(buff_index);

    if(-1 != send(clientfd, target, target->size, 0))    // 发送一帧数据包  
    {
        make_log("[SUCCESS] 发送成功", std::cout, false);
        return {true, target};
    }
    else
    {
        make_log("[ERROR] 发送失败", std::cerr, true);
        return {false, target};
    }
}

void make_log(std::string str, std::ostream & stdFlow, bool print_sw)
{
    uni_log.write(str.c_str(), str.length());
    if(print_sw)    stdFlow << str.c_str();
}

