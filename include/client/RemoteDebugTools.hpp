#pragma once

#define WIN32_LEAN_AND_MEAN 
#include "Windows.h"
#include "WinSock2.h"
#include <ws2tcpip.h>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")

#include "common/utf8_system_category.hpp"
//#include "utf8_setup.hpp"

class WSAContext {
public:
    WSAContext() {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
            std::cerr << ec.message();
            throw std::system_error(ec, "WSAStartup");
        }
    }

    ~WSAContext() {
        WSACleanup();
    }
};

class Client {
public:
    Client& connectServer(std::string server_ip, int server_port) {
        // 创建套接字
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Failed to create socket. Error: " << WSAGetLastError() << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        // 设置服务器地址
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.c_str(), &serverAddr.sin_addr);

        // 连接到服务端
        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed. Error: " << WSAGetLastError() << std::endl;
            closesocket(sock);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        self_fd = sock;
        return *this;
    }

    void sendBrepData(const std::string& brepData) {
        // 发送数据长度（先转换为网络字节序）
        int32_t dataLength = htonl(brepData.size());
        int sentBytes = send(self_fd, reinterpret_cast<const char*>(&dataLength), sizeof(dataLength), 0);
        if (sentBytes == SOCKET_ERROR) {
            std::cerr << "Failed to send data length. Error: " << WSAGetLastError() << std::endl;
            closesocket(self_fd);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        // 发送实际的 BRep 数据
        sentBytes = send(self_fd, brepData.c_str(), brepData.size(), 0);
        if (sentBytes == SOCKET_ERROR) {
            std::cerr << "Failed to send data. Error: " << WSAGetLastError() << std::endl;
            closesocket(self_fd);
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        std::cout << "BRep data sent successfully." << std::endl;
    }

    ~Client() {
        closesocket(self_fd);
    }

private:
    WSAContext wsa;
    SOCKET self_fd;
};

#include <BRepTools.hxx>
#include <sstream>
#include <string>

// 将 TopoDS_Shape 转换为 BRep 格式的字符串
std::string shapeToBRep(const TopoDS_Shape& shape) {
    std::ostringstream oss;
    BRepTools::Write(shape, oss);  // 将形状写入到输出流
    return oss.str();  // 返回BRep格式的字符串
}

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS_Shape.hxx>

TopoDS_Shape createLine(gp_Pnt p1, gp_Pnt p2) {
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(p1, p2);
    return edge;  // 返回形状对象
}

TopoDS_Shape createBox(Standard_Real size) {
    TopoDS_Shape aBox = BRepPrimAPI_MakeBox(size, size, size).Shape();
    return aBox;  // 返回形状对象
}