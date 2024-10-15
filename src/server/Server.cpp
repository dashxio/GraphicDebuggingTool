#include "server/Server.h"

#include "common/utf8_system_category.hpp"
#include "common/convert_return.hpp"

#include <deque>
#include <vector>
#include <memory>
#include <thread>

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <V3d_Viewer.hxx>


#pragma comment(lib, "ws2_32.lib")

MyServer::WSAContext::WSAContext() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
        std::cerr << ec.message();
        throw std::system_error(ec, "WSAStartup");
    }
}

MyServer::WSAContext::~WSAContext() {
    WSACleanup();
}

MyServer::~MyServer()
{
    if (m_id_ != INVALID_SOCKET) {
        closesocket(m_id_);
    }

    m_work_thread_.join();
}


MyServer& MyServer::withListenPort(std::string ip, std::string port)
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

    m_id_ = temp_id;
	listen(m_id_, SOMAXCONN);
    return *this;
}

void MyServer::run()
{
	m_task_deque_.emplace_back(std::make_shared<ConnectionAcceptTask>(this));
	
	std::thread t([this] {
		while (!m_task_deque_.empty()) {
			auto t1 = m_task_deque_.front();
			m_task_deque_.pop_front();
			for (auto t2 : t1->run()) {
				m_task_deque_.emplace_back(t2);
			}
		}
	});

    m_work_thread_ = std::move(t);
}

std::vector<std::shared_ptr<Task>> ErrorThrowTask::run()
{
	auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
	std::cerr << ec.message();
	throw std::system_error(ec, m_str);
	return {};
}

std::vector<std::shared_ptr<Task>> ConnectionAcceptTask::run()
{
	sockaddr_in client_addr;
	int addr_len = sizeof(client_addr);
	auto res = convert_return(accept(m_boss_->m_id_, (struct sockaddr*)&client_addr, &addr_len))
		.to(AcceptOne).if_meet_condition([](auto socket) {return socket != INVALID_SOCKET; })
		.to(NeedReTry).if_meet_condition([](auto socket) {return socket == INVALID_SOCKET && WSAGetLastError() == WSAEWOULDBLOCK; })
		.to(Error);

	SOCKET m_recently_connected = res.result();

	// 状态转移
	switch(res)
	{
	case AcceptOne:
		m_boss_->m_connection_map_.emplace(m_recently_connected, MyServer::ConnectionInfo{ m_recently_connected });
		return { std::make_shared<BrepDataReceiveTask>(m_boss_, m_recently_connected),
			std::make_shared<BrepDataSetTask>(m_boss_, m_recently_connected) };

	case NeedReTry:
		return { std::make_shared<ConnectionAcceptTask>(m_boss_) };

	default:
		return { std::make_shared<ErrorThrowTask>("Accept") };
	}
}

std::vector<std::shared_ptr<Task>> BrepDataReceiveTask::run()
{
	auto& connection = m_boss_->m_connection_map_[m_connection_id_];
	auto& temp_data_buffer = connection.m_reserve_buffer;
	size_t old_size = temp_data_buffer.size();
	size_t new_size = old_size + 4096;
	temp_data_buffer.resize(new_size);

	auto res = convert_return(recv(m_connection_id_, temp_data_buffer.data()+old_size, 4096,0))
		.to(Received).if_meet_condition([](auto received) {return received > 0; })
		.to(ClientClosed).if_meet_condition([](auto received){return received == 0;})
		.to(NeedReTry).if_meet_condition([](auto received){return received < 0 && WSAGetLastError() == WSAEWOULDBLOCK;})
		.to(Error);

	switch(res){
	case Received:
		temp_data_buffer.resize(old_size+res.result());
		return { std::make_shared<BrepDataReceiveTask>(m_boss_, m_connection_id_)};
	case ClientClosed:
		return {std::make_shared<ConnectionCloseTask>(m_boss_, m_connection_id_)};
	case NeedReTry:
		temp_data_buffer.resize(old_size);
		return {std::make_shared<BrepDataReceiveTask>(m_boss_, m_connection_id_)}; 
	default:
		return {std::make_shared<ErrorThrowTask>("Recv")};
	}
}

std::vector<std::shared_ptr<Task>> BrepDataSetTask::run()
{
	if(m_boss_->m_connection_map_.find(m_connection_id_) == m_boss_->m_connection_map_.end()) {
		return {};
	}

	auto& connection = m_boss_->m_connection_map_[m_connection_id_];
	auto& temp_data_buffer = connection.m_reserve_buffer;
	const size_t HEADER_SIZE = sizeof(int32_t); 
	size_t buffer_size = temp_data_buffer.size();

	std::string draw_data;
	RunResult res = LackData;

	if(HEADER_SIZE < buffer_size){
		int32_t data_length_r = 0;
		memcpy(&data_length_r, temp_data_buffer.data(), HEADER_SIZE);
		int32_t data_length = ntohl(data_length_r);
		if (data_length < buffer_size) {
			draw_data.resize(data_length);
			memcpy(draw_data.data(), temp_data_buffer.data() + HEADER_SIZE, data_length); //这里可以优化，但目前没太大必要                        
			temp_data_buffer.erase(0, HEADER_SIZE + data_length);// 从缓冲区中移除已处理的数据
			res = EnoughData;
		}
	}

	if(res == EnoughData) {
		getCriticalSection().m_brep_data.setValue(std::move(draw_data));
		getCriticalSection().m_has_drawn = false;
		emit m_boss_->sigDrawDataReady();
		return {std::make_shared<WaitingDrawTask>(m_boss_, m_connection_id_)};
	}
	else{
		return {std::make_shared<BrepDataSetTask>(m_boss_, m_connection_id_)};
	}
}

std::vector<std::shared_ptr<Task>> WaitingDrawTask::run()
{
	if(getCriticalSection().m_has_drawn){
		return {std::make_shared<BrepDataSetTask>(m_boss_, m_connection_id_)};
	}
	else{
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		return {std::make_shared<WaitingDrawTask>(m_boss_, m_connection_id_)};
	}
}

std::vector<std::shared_ptr<Task>> ConnectionCloseTask::run()
{
	m_boss_->m_connection_map_.erase(m_connection_id_);
	closesocket(m_connection_id_);
	return {};
}

