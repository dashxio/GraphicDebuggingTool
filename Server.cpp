#include "Server.h"

#include "utf8_system_category.hpp"

#include <deque>
#include <vector>
#include <memory>
#include <thread>
#include <sstream>

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Shape.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Aspect_DisplayConnection.hxx>

#pragma comment(lib, "ws2_32.lib")

std::atomic<bool> server_wanted_close(false);
SOCKET current_connection_id;
MTQueue<SOCKET> connection_list;
std::string draw_brep_data;;

WSAContext::WSAContext() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
        std::cerr << ec.message();
        throw std::system_error(ec, "WSAStartup");
    }
}

WSAContext::~WSAContext() {
    WSACleanup();
}

template<typename T>
class EasyLogic {
public:
    T expression_result;
    std::function<void()> func;

    EasyLogic(const T& t) : expression_result(t) {}

    EasyLogic& execute(std::function<void()> f) {
        func = std::move(f);
        return *this;
    }

    EasyLogic& if_equal(const T& value) {
        if (func) {
            if (expression_result == value) {
                func();
            }
        }
        return *this;
    }

    EasyLogic& if_not_equal(const T& value) {
        if (func) {
            if (expression_result != value) {
                func();
            }
        }
        return *this;
    }

    EasyLogic& throw_if_equal(const T& value, std::string message = "error") {
        if (expression_result == value) {
            auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
            std::cerr << ec.message();
            throw std::system_error(ec, message);
        }
        return *this;
    }

    EasyLogic& throw_if_not_equal(const T& value, std::string message = "error") {
        if (expression_result != value) {
            auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
            std::cerr << ec.message();
            throw std::system_error(ec, message);
        }
        return *this;
    }

    T result() noexcept {
        return expression_result;
    }
};

template<typename T>
EasyLogic<T> convert_error(T t) {
    return EasyLogic<T>(t);
}

Server::~Server()
{
    server_wanted_close = true;
    if (m_id != INVALID_SOCKET) {
        closesocket(m_id);
    }

    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

Server& Server::withListenPort(std::string ip, std::string port)
{
    struct addrinfo* head = nullptr;
    convert_error(getaddrinfo(ip.c_str(), port.c_str(), NULL, &head))
        .throw_if_not_equal(0, "getaddrinfo");

    SOCKET temp_id = convert_error(socket(head->ai_family, head->ai_socktype, head->ai_protocol))
        .throw_if_equal(INVALID_SOCKET)
        .result();

    u_long mode = 1; //非阻塞的accept模式
    convert_error(ioctlsocket(temp_id, FIONBIO, &mode))
        .execute([temp_id] {closesocket(temp_id); }).if_not_equal(NO_ERROR)
        .throw_if_not_equal(NO_ERROR);

    convert_error(bind(temp_id, head->ai_addr, head->ai_addrlen))
        .execute([temp_id] {closesocket(temp_id); }).if_equal(SOCKET_ERROR)
        .throw_if_equal(SOCKET_ERROR);

    m_id = temp_id;
    return *this;
}

//Server& Server::withDrawContext(Handle(AIS_InteractiveContext) context)
//{
//    m_context = context;
//    return *this;
//}

void Server::startWork()
{
    //auto draw_worker = DrawWorker::makeWorker(m_context);
    //workers().push_back(std::move(draw_worker));
    auto accept_worker = AcceptConnectionWorker::makeWorker(this, m_id);
    workers().push_back(std::move(accept_worker));
}

void AcceptConnectionWorker::acceptConnection()
{
    listen(m_server_id, SOMAXCONN);
    while (!server_wanted_close) {
        SOCKET connection_id;
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        connection_id = accept(m_server_id, (struct sockaddr*)&client_addr, &addr_len);
        if (connection_id == INVALID_SOCKET) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
                throw std::system_error(ec, "accept");
            }
        }
        else {
            current_connection_id = connection_id;
            auto t = ReceiveDataWorker::makeWorker(m_boss, connection_id);
            m_boss->workers().push_back(std::move(t));
            connection_list.push(connection_id);
        }
    }
}

//void DrawWorker::draw()
//{
//    while (!server_wanted_close) {
//        std::string draw_data = draw_data_list.pop();
//
//        TopoDS_Shape shape;
//        std::istringstream iss(draw_data);
//        BRep_Builder builder;
//        BRepTools::Read(shape, iss, builder);
//
//        if (shape.IsNull()) {
//            throw;
//        }
//
//        // 显示形状
//        Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
//        m_context->EraseAll(Standard_False);  // 清除之前的显示
//        m_context->Display(aisShape, Standard_True);  // 显示新形状
//    }
//}


void ReceiveDataWorker::receiveData()
{
    const int HEADER_SIZE = sizeof(int32_t);  // 消息头长度
    while (!server_wanted_close) {
        //接收一批新数据
        auto& temp_data_buffer = m_data_buffer;

        size_t old_size = temp_data_buffer.size();
        size_t new_size = old_size + 4096;
        temp_data_buffer.resize(new_size);

        int received = recv(m_connection_id, temp_data_buffer.data() + old_size, 4096, 0);
        if (received <= 0) {
            if (received == 0) {
                std::cout << "Connection closed by client." << std::endl;
                break;
            }
            else if (WSAGetLastError() == WSAEWOULDBLOCK) {
                temp_data_buffer.resize(old_size);
                continue; //客户端未发送任何数据
            }
            else {
                auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
                throw std::system_error(ec, "recv");
            }
        }
        temp_data_buffer.resize(old_size + received);

        // 处理缓冲区中的数据
        while (!server_wanted_close) {
            // 检查是否有足够的数据读取消息长度
            if (temp_data_buffer.size() < HEADER_SIZE)
                break;  // 继续接收数据

            // 读取消息长度（前4字节）
            int32_t data_length_r = 0;
            memcpy(&data_length_r, temp_data_buffer.data(), HEADER_SIZE);
            int32_t data_length = ntohl(data_length_r);

            // 检查是否有足够的数据读取完整的消息
            if (temp_data_buffer.size() < HEADER_SIZE + data_length)
                break;

            // 提取完整的消息数据
            std::string draw_data;
            draw_data.resize(data_length);
            memcpy(draw_data.data(), temp_data_buffer.data() + HEADER_SIZE, data_length); //这里可以优化，但目前没太大必要                        
            temp_data_buffer.erase(0, HEADER_SIZE + data_length);// 从缓冲区中移除已处理的数据

            // 处理消息数据
            m_draw_data_list.push_back(draw_data);
            if (m_connection_id == current_connection_id) {
                //draw_data_list.push(m_draw_data_list.back());
                draw_brep_data = std::move(draw_data);
                emit m_boss->sigDrawDataReady();
            }                
        }
    }
}

//void Server::onCreateListenSocket(std::string name, std::string port) 
//{
//    struct addrinfo* head = nullptr;
//    convert_error(getaddrinfo(name.c_str(), port.c_str(), NULL, &head))
//        .throw_if_not_equal(0, "getaddrinfo");
//
//    SOCKET temp_id = convert_error(socket(head->ai_family, head->ai_socktype, head->ai_protocol))
//        .throw_if_equal(INVALID_SOCKET)
//        .result();
//
//    u_long mode = 1; //非阻塞的accept模式
//    convert_error(ioctlsocket(temp_id, FIONBIO, &mode))
//        .execute([temp_id] {closesocket(temp_id); }).if_not_equal(NO_ERROR)
//        .throw_if_not_equal(NO_ERROR);
//
//    convert_error(bind(temp_id, head->ai_addr, head->ai_addrlen))
//        .execute([temp_id] {closesocket(temp_id); }).if_equal(SOCKET_ERROR)
//        .throw_if_equal(SOCKET_ERROR);
//
//    emit sigAcceptClientSocket(temp_id);
//}
//
//void Server::onAcceptClientSocket(SOCKET server_id)
//{
//    QtConcurrent::run([=] {
//        SOCKET client_socket;
//        sockaddr_in client_addr;
//        int addr_len = sizeof(client_addr);
//        //SOCKET server_id = server.server_id;
//        listen(server_id, SOMAXCONN);
//        while (!server_wanted_close) {
//            client_socket = accept(server_id, (struct sockaddr*)&client_addr, &addr_len);
//            if (client_socket == INVALID_SOCKET) {
//                if (WSAGetLastError() != WSAEWOULDBLOCK) {
//                    auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
//                    throw std::system_error(ec, "accept");
//                }
//            }
//            else {
//                client_map.emplace(client_socket, client_socket);
//                emit sigReceiveBrepData(client_socket);
//            }
//            QThread::sleep(1);
//        }
//        });
//
//}
//
//void Server::onReceiveBrepData(SOCKET client_id)
//{
//    QtConcurrent::run([client_id, this] {
//        const int HEADER_SIZE = sizeof(int32_t);  // 消息头长度
//        auto& client = client_map[client_id];
//        while (!client.wanted_close) {
//            //接收一批新数据
//            auto& temp_data_buffer = client.received_data_buffer;
//            size_t old_size = temp_data_buffer.size();
//            size_t new_size = old_size + 4096;
//            temp_data_buffer.resize(new_size);
//            int received = recv(client.client_id, temp_data_buffer.data() + old_size, 4096, 0);
//            if (received <= 0) {
//                if (received == 0) {
//                    std::cout << "Connection closed by client." << std::endl;
//                }
//                else {
//                    auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
//                    throw std::system_error(ec, "recv");
//                }
//                break;
//            }
//            temp_data_buffer.resize(received);
//
//            // 处理缓冲区中的数据
//            while (!client.wanted_close) {
//                // 检查是否有足够的数据读取消息长度
//                if (temp_data_buffer.size() < HEADER_SIZE)
//                    break;  // 继续接收数据
//
//                // 读取消息长度（前4字节）
//                int32_t data_length_r = 0;
//                memcpy(&data_length_r, temp_data_buffer.data(), HEADER_SIZE);
//                int32_t data_length = ntohl(data_length_r);
//
//                // 检查是否有足够的数据读取完整的消息
//                if (temp_data_buffer.size() < HEADER_SIZE + data_length)
//                    break;
//
//                // 提取完整的消息数据
//                QByteArray draw_data;
//                draw_data.resize(data_length);
//                memcpy(draw_data.data(), temp_data_buffer.data() + HEADER_SIZE, data_length); //这里可以优化，但目前没太大必要                        
//                temp_data_buffer.erase(0, HEADER_SIZE + data_length);// 从缓冲区中移除已处理的数据
//
//                // 处理消息数据
//                if (client.wanted_send_data)
//                    emit sigDrawBrepData(std::move(draw_data));
//            }
//            QThread::sleep(1);
//        }   
//        emit sigRemoveClientFromMap(client_id);
//    });
//}
//
//void Server::onRemoveClientFromMap(SOCKET client_id)
//{
//    client_map.erase(client_id);
//    closesocket(client_id);
//}

//void Server::init()
//{
//
//    ProcessManager::func_map.emplace("createListenSocket", [this]() {
//        struct addrinfo* head = nullptr;
//        convert_error(getaddrinfo(server_name.c_str(), server_port.c_str(), NULL, &head))
//            .throw_if_not_equal(0, "getaddrinfo");
//
//        SOCKET temp_id = convert_error(socket(head->ai_family, head->ai_socktype, head->ai_protocol))
//            .throw_if_equal(INVALID_SOCKET)
//            .result();
//
//        u_long mode = 1; //非阻塞的accept模式
//        convert_error(ioctlsocket(temp_id, FIONBIO, &mode))
//            .execute([temp_id] {closesocket(temp_id); }).if_not_equal(NO_ERROR)
//            .throw_if_not_equal(NO_ERROR);
//
//        convert_error(bind(temp_id, head->ai_addr, head->ai_addrlen))
//            .execute([temp_id] {closesocket(temp_id); }).if_equal(SOCKET_ERROR)
//            .throw_if_equal(SOCKET_ERROR);
//
//        listen_id = temp_id;
//        ProcessManager::key_list.push("listen");
//        });
//
//    ProcessManager::func_map.emplace("listen", [this]() {
//        std::thread t([this] {
//            SOCKET client_socket;
//            sockaddr_in client_addr;
//            int addr_len = sizeof(client_addr);
//            listen(listen_id, SOMAXCONN);
//            client_socket = accept(listen_id, (struct sockaddr*)&client_addr, &addr_len);
//            if (client_socket == INVALID_SOCKET) {
//                if (WSAGetLastError() == WSAEWOULDBLOCK) {
//                    if (running)
//                        ProcessManager::key_list.push("listen");
//                    else
//                        closesocket(listen_id);
//                }
//                else {
//                    auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
//                    throw std::system_error(ec, "accept");
//                }
//            }
//            else {
//                current_connected_id = client_socket;
//                connected_ids.push(client_socket);
//                ProcessManager::key_list.push("asyncReceiveData");
//            }
//            });
//
//        ProcessManager::thread_list.emplace_back(std::move(t));
//        });
//
//    ProcessManager::func_map.emplace("asyncReceiveData", [this]() {
//        std::thread t([this] {
//            const int HEADER_SIZE = sizeof(int32_t);  // 消息头长度
//            while (running) {
//                //接收一批新数据
//                size_t old_size = temp_data_buffer.size();
//                size_t new_size = old_size + 4096;
//                temp_data_buffer.resize(new_size);
//                int received = recv(current_connected_id, temp_data_buffer.data() + old_size, 4096, 0);
//                if (received <= 0) {
//                    if (received == 0) {
//                        //std::cout << "Connection closed by client." << std::endl;
//                        //ProcessManager::key_list.push_back("closeClientSocket");
//                    }
//                    else {
//                        auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
//                        throw std::system_error(ec, "recv");
//                    }
//                    break;
//                }
//                temp_data_buffer.resize(received);
//
//                // 处理缓冲区中的数据
//                while (running) {
//                    // 检查是否有足够的数据读取消息长度
//                    if (temp_data_buffer.size() < HEADER_SIZE)
//                        break;  // 继续接收数据
//
//                    // 读取消息长度（前4字节）
//                    int32_t data_length_r = 0;
//                    memcpy(&data_length_r, temp_data_buffer.data(), HEADER_SIZE);
//                    int32_t data_length = ntohl(data_length_r);
//
//                    // 检查是否有足够的数据读取完整的消息
//                    if (temp_data_buffer.size() < HEADER_SIZE + data_length)
//                        break;
//
//                    // 提取完整的消息数据
//                    draw_data.resize(data_length);
//                    memcpy(draw_data.data(), temp_data_buffer.data() + HEADER_SIZE, data_length); //这里可以优化，但目前没太大必要                        
//                    temp_data_buffer.erase(0, HEADER_SIZE + data_length);// 从缓冲区中移除已处理的数据
//
//                    // 处理消息数据
//                    ProcessManager::key_list.push("handleDrawData");
//                }
//            }}
//        );
//        ProcessManager::thread_list.emplace_back(std::move(t));
//        });
//
//    ProcessManager::func_map.emplace("handleDrawData", [this]() {
//        std::thread t([this] {
//            // 反序列化为 TopoDS_Shape
//            TopoDS_Shape shape;
//            std::string s(draw_data);
//            std::istringstream iss(s);
//            BRep_Builder builder;
//            BRepTools::Read(shape, iss, builder);
//
//            if (shape.IsNull()) {
//                throw;
//            }
//
//            // 显示形状
//            Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
//            draw_context->EraseAll(Standard_False);  // 清除之前的显示
//            draw_context->Display(aisShape, Standard_True);  // 显示新形状
//            //draw_context->CurrentViewer()->Update();
//            });
//
//        ProcessManager::thread_list.emplace_back(std::move(t));
//        });
//}