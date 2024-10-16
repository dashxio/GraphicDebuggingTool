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
class Task;

inline auto& getCriticalSection() {
	static struct CriticalSection {
		std::atomic<bool> m_has_drawn = false;
		std::atomic<bool> m_stop_server = false;
        std::atomic<bool> m_mode_draw_new = true;
		MTObj<SOCKET> m_current_connetion_id;
		MTObj<std::string> m_brep_data;
        MTQueue<SOCKET> m_connection_list_to_delete;
		MTQueue<std::shared_ptr<Task>> m_task_deque;
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

		std::string getCurrentBrepData(){
			if(m_data_index >= 0 && m_data_index < m_brep_data_list.size())
				return m_brep_data_list.at(m_data_index);
			else
				return "";
		}

		void setCurrentIndexToLatest(){
			m_data_index = m_brep_data_list.size() - 1;
		}
    };

signals:
    void sigDrawDataReady();

public slots:
	void onMovePreviousBrep();
	void onMoveNextBrep();
	void onUpdateMode(bool selected);

public:
    MyServer& withListenPort(std::string ip, std::string port);
    void run();


    SOCKET m_id_ = INVALID_SOCKET;
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

	BrepDataSetTask(MyServer* boss) : m_boss_(boss) {}
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

	WaitingDrawTask(MyServer* boss) : m_boss_(boss){}
	std::vector<std::shared_ptr<Task>> run() override;

private:
	MyServer* m_boss_ = nullptr;
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

class PreviousBrepTask : public Task
{
public:
	PreviousBrepTask(MyServer* boss, SOCKET connection) : m_boss_(boss), m_connection_id_(connection) {}
	std::vector<std::shared_ptr<Task>> run() override{
		auto& connection = m_boss_->m_connection_map_[m_connection_id_];
		if(connection.m_data_index > 0){
			m_boss_->m_connection_map_[m_connection_id_].m_data_index--;
		}
		return {};
	}

private:
	MyServer* m_boss_ = nullptr;
	SOCKET m_connection_id_ = INVALID_SOCKET;

};

class NextBrepTask : public Task
{
public:
	NextBrepTask(MyServer* boss, SOCKET connection) : m_boss_(boss), m_connection_id_(connection) {}
	std::vector<std::shared_ptr<Task>> run() override{
		auto& connection = m_boss_->m_connection_map_[m_connection_id_];
		if(connection.m_data_index < connection.m_brep_data_list.size()-1){
			m_boss_->m_connection_map_[m_connection_id_].m_data_index++;
		}
		return {};
	}

private:
	MyServer* m_boss_ = nullptr;
	SOCKET m_connection_id_ = INVALID_SOCKET;

};


#endif
