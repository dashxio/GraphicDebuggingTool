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

class Task;

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


    SOCKET m_id_ = INVALID_SOCKET;
    std::deque<std::shared_ptr<Task>> m_task_deque_;
    std::unordered_map<SOCKET, ConnectionInfo> m_connection_map_;

private:
    WSAContext m_wsa_;
    std::thread m_work_thread_;
};

class Task
{
public:
	virtual std::vector<std::shared_ptr<Task>> run() { return {}; }
	virtual ~Task() = default;
};

class ConnectionAcceptTask : public Task
{
public:
	enum RunResult : int {
		Error,
		NeedReTry,
		AcceptOne,
	};

	ConnectionAcceptTask(MyServer* boss) : m_boss_(boss) {}
	std::vector<std::shared_ptr<Task>> run() override;

private:
	MyServer* m_boss_;
};

class BrepDataReceiveTask : public Task
{
public:
	enum RunResult : int {
		Error,
		ClientClosed,
		NeedReTry,
		Received,
		EnoughData,
		LackData
	};

	BrepDataReceiveTask(MyServer* boss, SOCKET connection) : m_boss_(boss), m_connection_id_(connection) {}
	std::vector<std::shared_ptr<Task>> run() override;

private:
	MyServer* m_boss_ = nullptr;
	SOCKET m_connection_id_ = INVALID_SOCKET;
};

class BrepDataSetTask : public Task
{
public:
	enum RunResult : int {
		Error,
		EnoughData,
		LackData,
	};

	BrepDataSetTask(MyServer* boss, SOCKET connection) : m_boss_(boss), m_connection_id_(connection) {}
	std::vector<std::shared_ptr<Task>> run() override;

private:
	MyServer* m_boss_ = nullptr;
	SOCKET m_connection_id_ = INVALID_SOCKET;
};

class WaitingDrawTask : public Task
{
public:
	enum RunResult : int {
		Error,
		EnoughData,
		LackData,
	};

	WaitingDrawTask(MyServer* boss, SOCKET connection) : m_boss_(boss), m_connection_id_(connection) {}
	std::vector<std::shared_ptr<Task>> run() override;

private:
	MyServer* m_boss_ = nullptr;
	SOCKET m_connection_id_ = INVALID_SOCKET;
};


class ConnectionCloseTask : public Task
{
public:
	ConnectionCloseTask(MyServer* boss, SOCKET connection) : m_boss_(boss), m_connection_id_(connection) {}
	std::vector<std::shared_ptr<Task>> run() override;

private:
	MyServer* m_boss_ = nullptr;
	SOCKET m_connection_id_ = INVALID_SOCKET;

};

class ErrorThrowTask : public Task
{
public:
	ErrorThrowTask(std::string str = "error") : m_str(str) {}
	std::vector<std::shared_ptr<Task>> run() override;
private:
	std::string m_str = "error";
};
#endif
