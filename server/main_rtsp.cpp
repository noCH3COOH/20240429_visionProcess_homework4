#include <iostream>

#define IP "127.0.0.1"
#define PORT_TCP 10202
#define PORT_UDP 10201
#define PATH_VIDEO "test_1280x720.mp4"

int udp_pushFlow(std::string video) {
    std::cout << ("[INFO] ffmpeg -re -stream_loop -1 -i " + video + " -vcodec copy -pkt_size 1300 -f h264 \"udp://" + std::string(IP) + ":" + std::to_string(PORT_UDP) + "\"") << std::endl;
    return system(("ffmpeg -re -stream_loop -1 -i " + video + " -vcodec copy -pkt_size 1300 -f h264 \"udp://" + std::string(IP) + ":" + std::to_string(PORT_UDP) + "\"").c_str());
}

int tcp_pushFlow(std::string video) {
    std::cout << ("[INFO] ffmpeg -re -stream_loop -1 -i " + video +  " -c copy -rtsp_transport tcp -f rtsp rtsp://" + IP + ":" + std::to_string(PORT_TCP) + "/stream") << std::endl;
    return system(("ffmpeg -re -stream_loop -1 -i " + video +  " -c copy -rtsp_transport tcp -f rtsp rtsp://" + IP + ":" + std::to_string(PORT_TCP) + "/stream").c_str());
}

int main() {
    tcp_pushFlow(PATH_VIDEO);
    udp_pushFlow(PATH_VIDEO);

    return 0;
}