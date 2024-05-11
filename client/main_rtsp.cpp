#include <iostream>

#define IP "127.0.0.1"
#define PORT_TCP 8554
#define PORT_UDP 10201

int udp_pullFlow() {
    std::cout << ("ffplay -f h264 \"udp://" + std::string(IP) + ":" + std::to_string(PORT_UDP) + "\"") << std::endl;
    return system(("ffplay -f h264 \"udp://" + std::string(IP) + ":" + std::to_string(PORT_UDP) + "\"").c_str());
}

int tcp_pullFlow() {
    std::cout << ("ffplay rtsp://" + std::string(IP) + ":" + std::to_string(PORT_TCP) + "/stream") << std::endl;
    return system(("ffplay rtsp://" + std::string(IP) + ":" + std::to_string(PORT_TCP) + "/stream").c_str());
}

int main() {
    tcp_pullFlow();
    udp_pullFlow();

    return 0;
}