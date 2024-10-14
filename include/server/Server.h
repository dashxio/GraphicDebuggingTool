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

inline auto& getCriticalSection() {
	static struct CriticalSection {
		std::atomic<bool> m_has_drawn;
        MTObj<SOCKET> m_current_connetion_id;
        std::atomic<bool> m_mode_draw_new = true;
		MTObj<std::string> m_brep_data;
        std::deque<SOCKET> m_connection_list;
        MTQueue<SOCKET> m_connection_list_to_delete;

	} c;

    return c;
}

class MyServer : public QObject {
    Q_OBJECT
public:
    MyServer(QObject* parent) : QObject(parent) { }
    MyServer& operator=(MyServer&&) = delete;
    ~MyServer() override;

	class WSAContext {
	public:
		WSAContext();
		~WSAContext();
	};

    struct ConnectionInfo
    {
		SOCKET m_id;
        bytes_buffer m_reserve_buffer;

        int m_data_index = 0;
        std::vector<std::string> m_brep_data_list;
    };

signals:
    void sigDrawDataReady();
    void sigEraseSocket(SOCKET id);

public:
    MyServer& withListenPort(std::string ip, std::string port);
    void run();
    void acceptConnection();
    void receiveBrepData(ConnectionInfo& connection);
    void sleepUntilDrawn(ConnectionInfo& connection);
	bool isConnectionIdValid(SOCKET connetion_id);


private:

    WSAContext m_wsa_;
    SOCKET m_id_ = INVALID_SOCKET;
    std::thread m_work_thread_;
    std::deque<std::function<void()>> m_task_deque_;
    std::unordered_map<SOCKET, ConnectionInfo> m_connection_map_;
};


#endif
