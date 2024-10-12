//#pragma once
#ifndef SERVER_H
#define SERVER_H

#include "common/bytes_buffer.hpp"
#include "common/MTQueue.hpp"
#include <unordered_map>

#include <QObject>
#include <AIS_InteractiveContext.hxx>

#include <ws2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

class WSAContext {
public:
    WSAContext();
    ~WSAContext();
};


class Server : public QObject {
    Q_OBJECT
public:
    Server(QObject* parent) : QObject(parent) { }

    ~Server();

signals:
    void sigDrawDataReady();

public:
    Server& withListenPort(std::string ip, std::string port);
    //Server& withDrawContext(Handle(AIS_InteractiveContext) context);
    void startWork();
    std::vector<std::thread>& workers() {
        return m_workers;
    }

private:
    WSAContext m_wsa;
    SOCKET m_id = INVALID_SOCKET;
    std::vector<std::thread> m_workers;
};

class ReceiveDataWorker {
public:
    void receiveData();

    static std::thread makeWorker(Server* boss, SOCKET connection_id) {
        std::thread t([=] {
            ReceiveDataWorker r(boss, connection_id);
            r.receiveData();
        });
        return std::move(t);
    }

    ReceiveDataWorker(Server* boss, SOCKET connection_id) : m_boss(boss), m_connection_id(connection_id) {}

    Server* m_boss;
    SOCKET m_connection_id;
    bytes_buffer m_data_buffer;
    std::deque<std::string> m_draw_data_list;
};


class AcceptConnectionWorker {
public:
    void acceptConnection();

    static std::thread makeWorker(Server* boss, SOCKET server_id) {
        std::thread t([=] {
            AcceptConnectionWorker a(boss, server_id);
            a.acceptConnection();
        });
        return std::move(t);
    }

    AcceptConnectionWorker(Server* boss, SOCKET server_id) : m_boss(boss), m_server_id(server_id) {}

    Server* m_boss;
    SOCKET m_server_id;
};

//class DrawWorker {
//public:
//    void draw();
//
//    static std::thread makeWorker(Handle(AIS_InteractiveContext) context) {
//        std::thread t([=] {
//            DrawWorker d;
//            d.m_context = context;
//            d.draw();
//        });
//        return std::move(t);
//    }
//
//    Handle(AIS_InteractiveContext) m_context;
//};


//class Server : public QObject {
//    Q_OBJECT
//public:
//    Server(QObject* parent) : QObject(parent) { }
//    std::string getBrepData() {
//        return brep_data_to_draw;
//    }
//
//signals:
//    void sigCreateListenSocket(std::string name, std::string port);
//    void sigAcceptClientSocket(SOCKET server);
//    void sigReceiveBrepData(SOCKET client_id);
//    void sigDrawBrepData(QByteArray brep_data);
//    void sigRemoveClientFromMap(SOCKET client_id);
//
//public slots:
//    void onCreateListenSocket(std::string name, std::string port);
//    void onAcceptClientSocket(SOCKET server);
//    void onReceiveBrepData(SOCKET client_id);
//    void onRemoveClientFromMap(SOCKET client_id);
//
//public:
//	//SOCKET listen_id;
//    std::unordered_map<SOCKET, ClientInfo> client_map;
//    WSAContext wsa_context;
//    bool server_wanted_close = false;
//    std::string brep_data_to_draw;
//    //void init();
//};

#endif
