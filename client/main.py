import socket
import struct
import cv2
import numpy as np

# 定义服务器地址和端口
SERVER_IP = 'localhost'
SERVER_PORT = 3000

# 创建socket连接
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((SERVER_IP, SERVER_PORT))

while True:
    # 接收数据包大小（假设数据包大小用4字节整数表示）
    packet_size_data = client_socket.recv(4)
    if not packet_size_data:
        break
    packet_size = struct.unpack('!I', packet_size_data)[0]

    # 接收宽度和高度（每个都是4字节整数）
    width_height_data = client_socket.recv(8)
    width, height = struct.unpack('!II', width_height_data)

    # 接收剩余的帧数据
    frame_data = b''
    while len(frame_data) < packet_size - 12:  # 减去size, width, height三个int字段的长度
        frame_data += client_socket.recv(packet_size - 12 - len(frame_data))

    # 将字节数据转换为numpy数组
    frame = np.frombuffer(frame_data, dtype=np.uint8)

    # 解码图像
    frame = cv2.imdecode(frame, cv2.IMREAD_COLOR)

    # 显示图像
    cv2.imshow('Received Frame', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# 关闭连接和窗口
client_socket.close()
cv2.destroyAllWindows()
