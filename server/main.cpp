// ==================== includes ====================

#include <time.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <opencv2/opencv.hpp>

// ==================== defines ====================

#define WINDOW_NAME "【摄像头捕获画面】"

#define SIZE_RECVBUF 1024

#define PATH_UNILOG "log.md"

// ==================== struct ====================

struct SEND_PACKAGE_t
{
    int size;
    int width;
    int height;
    std::vector<uchar> data;
};

// ==================== global variables ====================

std::ofstream uni_log;

// ==================== functions ====================

void make_log(std::string str, bool print_sw);

// ==================== main ====================

int main()
{
    uni_log.open(PATH_UNILOG, std::ios::out);    // 日志
    make_log("[INFO] 当前进程号：" + std::to_string(getpid()) + '\n', true);
    
    int key = 0;
    int fps = 0;

    time_t timer;
    clock_t start_time;

    std::string time_stamp;

    cv::Mat captureFrame;
    cv::VideoCapture cap(0);
    cap.set(3, 640);
    cap.set(4, 480);

    std::vector<uchar> compress_buff;


    // 创建 socket
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);    // 流模式，TCP
    // int serverfd = socket(AF_INET, SOCK_DGRAM, 0);    // 数据报模式，UDP
	if(-1 == serverfd) {
		make_log("[ERROR] socket 创建失败，程序退出\n", true);
		return -1;
	}

    // 绑定端口
    struct sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = htonl(INADDR_ANY);    // 接收任何地址的数据
	addr_server.sin_port = htons(3000);    // 3000 端口
	if(-1 == bind(serverfd, (struct sockaddr *)&addr_server, sizeof(addr_server))) 
    {
		make_log("[ERROR] 端口绑定错误，程序退出\n", true);
		return -1;
	}
	
	// 开始监听
	if(-1 == listen(serverfd, 2)) 
    {
		make_log("[ERROR] socket 监听错误，程序退出\n", true);
		return -1;
	}

    struct sockaddr_in addr_client;
    socklen_t len_addr_client = sizeof(addr_client);
    
    char recvBuf[SIZE_RECVBUF];
    // std::string sendBuf;
    SEND_PACKAGE_t send_package;

    while('q' != key && 'Q' != key)
    {
        // ==================== 循环开始 ====================

        start_time = std::clock();
        time(&timer);
        time_stamp = std::ctime(&timer);
        time_stamp += " fps:" + std::to_string(fps);

        cap >> captureFrame;    // 获取摄像头捕获画面
        cv::flip(captureFrame, captureFrame, 1);
        time_stamp += " fps:" + std::to_string(captureFrame.size().width) + 'x' + std::to_string(captureFrame.size().height);

        // 打印视频时间戳
        cv::putText(captureFrame, time_stamp, cv::Point(20,15), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(0, 0, 255));

        // 压缩视频打包
        cv::imencode(".jpg", captureFrame, compress_buff);
        send_package.data = compress_buff;
        send_package.width = captureFrame.size().width;
        send_package.height = captureFrame.size().height;
        send_package.size = sizeof(send_package);

        // ==================== socket 相关 ====================

        // 准备链接
		int clientfd = accept(serverfd, (struct sockaddr *)&addr_client, &len_addr_client);
		if (clientfd != -1) 
        {
            memset(recvBuf, 0, sizeof(recvBuf));
        
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
        
            send(clientfd, &send_package, sizeof(send_package), 0);    // 发送一帧数据包
        
            close(clientfd);    // 清除链接
		}

        // ==================== 循环结束 ====================

        cv::imshow(WINDOW_NAME, captureFrame);    // 显示摄像头捕获内容
        
        key = cv::waitKey(1);    // 获取按键状态

        fps = (1/(float)(clock() - start_time) * CLOCKS_PER_SEC);    // 计算帧率
    }

    close(serverfd);

    cv::destroyAllWindows();
    uni_log.close();
    return 0;
}

void make_log(std::string str, bool print_sw)
{
    uni_log.write(str.c_str(), str.length());
    if(print_sw)    std::cout << str.c_str();
}

