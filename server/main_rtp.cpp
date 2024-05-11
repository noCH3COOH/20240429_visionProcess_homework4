#include <iostream>

#define IP "127.0.0.1"
#define PORT_TCP 8554
#define PORT_UDP 10201
#define PATH_VIDEO "test.mp4"

int udp_pushFlow(std::string video) {
    std::string cmd = "ffmpeg ";
    cmd += "-re -stream_loop -1 ";
    cmd += "-i " + video + " ";
    cmd += "-vcodec copy ";
    cmd += "-pkt_size 1300 ";
    cmd += "-f h264 ";
    cmd += "\"udp://" + std::string(IP) + ":" + std::to_string(PORT_UDP) + "\"";

    std::cout << "[INFO] " + cmd << std::endl;
    return system(cmd.c_str());
}

int tcp_pushFlow(std::string video) {
    std::string cmd = "ffmpeg ";
    cmd += "-re -stream_loop -1 ";
    cmd += "-i " + video + " ";
    cmd += "-vcodec copy ";
    cmd += "-rtsp_transport tcp ";
    cmd += "-f rtsp ";
    cmd += "rtsp://" + std::string(IP) + ":" + std::to_string(PORT_TCP) + "/stream";

    std::cout << "[INFO] " + cmd << std::endl;
    return system(cmd.c_str());
}

int main() {
    // tcp_pushFlow(PATH_VIDEO);
    udp_pushFlow(PATH_VIDEO);

    return 0;
}