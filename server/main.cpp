// ==================== includes ====================

#include <time.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>

// ==================== defines ====================

#define WINDOW_NAME "【摄像头捕获画面】"

#define PATH_UNILOG "log.md"

// ==================== global variables ====================

std::ofstream uni_log;

// ==================== functions ====================

void make_log(std::string str, bool print_sw);

// ==================== main ====================

int main()
{
    int key = 0;
    int fps = 0;

    time_t timer;
    clock_t start_time;

    std::string time_stamp;
    
    cv::Mat captureFrame;
    cv::VideoCapture cap(0);

    uni_log.open(PATH_UNILOG, std::ios::out);    // 日志

    while('q' != key && 'Q' != key)
    {
        start_time = std::clock();
        time(&timer);
        time_stamp = std::ctime(&timer);
        time_stamp += " fps:" + std::to_string(fps);

        cap >> captureFrame;    // 获取摄像头捕获画面
        cv::flip(captureFrame, captureFrame, 1);
        time_stamp += " fps:" + std::to_string(captureFrame.size().width) + 'x' + std::to_string(captureFrame.size().height);

        // 打印视频时间戳
        cv::putText(captureFrame, time_stamp, cv::Point(20,15), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(0, 0, 255));

        // 
        
        cv::imshow(WINDOW_NAME, captureFrame);    // 显示摄像头捕获内容
        
        key = cv::waitKey(1);    // 获取按键状态

        fps = (1/(float)(clock() - start_time) * CLOCKS_PER_SEC);    // 计算帧率
    }

    cv::destroyAllWindows();
    return 0;
}

void make_log(std::string str, bool print_sw)
{
    uni_log.write(str.c_str(), strlen(str.c_str()));
    if(print_sw)    std::cout << str.c_str();
}

