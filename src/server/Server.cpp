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
	getCriticalSection().m_stop_server = true;
    m_work_thread_.join();
    if (m_id_ != INVALID_SOCKET) {
        closesocket(m_id_);
    }

}

void MyServer::onMovePreviousBrep()
{
	if(!getCriticalSection().m_mode_draw_new){
		auto& task_deque = getCriticalSection().m_task_deque;
		task_deque.push(std::make_shared<PreviousBrepTask>(this, getCriticalSection().m_current_connetion_id.value()));
	}
}

void MyServer::onMoveNextBrep()
{
	if(!getCriticalSection().m_mode_draw_new){
		auto& task_deque = getCriticalSection().m_task_deque;
		task_deque.push(std::make_shared<NextBrepTask>(this, getCriticalSection().m_current_connetion_id.value()));
	}
}

void MyServer::onUpdateMode(bool selected)
{
	getCriticalSection().m_mode_draw_new = selected;
	auto current_id = getCriticalSection().m_current_connetion_id.value();
	m_connection_map_[current_id].setCurrentIndexToLatest();
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
	auto guardFunc = [this]{
		auto& task_deque = getCriticalSection().m_task_deque;
		task_deque.push(std::make_shared<ConnectionAcceptTask>(this));
		while (!task_deque.getAccessor().value().empty()) {
			if (getCriticalSection().m_stop_server) /*[[unlikely]]*/ {
				task_deque.getAccessor().value().clear();
				return;
			}

			auto t1 = task_deque.pop();
			task_deque.push_many(t1->run());
		}
	};
	
	
	std::thread t([guardFunc] {
		guardFunc();
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
		getCriticalSection().m_current_connetion_id.setValue(m_recently_connected);
		m_boss_->m_connection_map_.emplace(m_recently_connected, MyServer::ConnectionInfo{ m_recently_connected });
		return { std::make_shared<BrepDataReceiveTask>(m_boss_, m_recently_connected),
			std::make_shared<BrepDataSetTask>(m_boss_) };

	case NeedReTry:
		return { std::make_shared<ConnectionAcceptTask>(m_boss_) };

	default:
		return { std::make_shared<ErrorThrowTask>("Accept") };
	}
}

std::vector<std::shared_ptr<Task>> BrepDataReceiveTask::run()
{

	auto recvBrepDataFromSocket = [](MyServer::ConnectionInfo& connection){
		auto& temp_data_buffer = connection.m_reserve_buffer;
		size_t old_size = temp_data_buffer.size();
		size_t new_size = old_size + 4096;
		temp_data_buffer.resize(new_size);

		auto res = convert_return(recv(connection.m_id, temp_data_buffer.data()+old_size, 4096,0))
		.to(Received).if_meet_condition([](auto received) {return received > 0; })
		.to(ClientClosed).if_meet_condition([](auto received){return received == 0;})
		.to(NeedReTry).if_meet_condition([](auto received){return received < 0 && WSAGetLastError() == WSAEWOULDBLOCK;})
		.to(Error);

		if(res == Received){
			temp_data_buffer.resize(old_size + res.result());
		}
		else if(res == NeedReTry){
			temp_data_buffer.resize(old_size);
		}

		return res.value();
	};

	auto addBrepDataToList = [](MyServer::ConnectionInfo& connection) {
		auto& temp_data_buffer = connection.m_reserve_buffer;
		const size_t HEADER_SIZE = sizeof(int32_t);
		size_t buffer_size = temp_data_buffer.size();

		std::string draw_data;
		if (HEADER_SIZE < buffer_size) {
			int32_t data_length_r = 0;
			memcpy(&data_length_r, temp_data_buffer.data(), HEADER_SIZE);
			int32_t data_length = ntohl(data_length_r);
			if (data_length < buffer_size) {
				draw_data.resize(data_length);
				memcpy(draw_data.data(), temp_data_buffer.data() + HEADER_SIZE, data_length); //这里可以优化，但目前没太大必要                        
				temp_data_buffer.erase(0, HEADER_SIZE + data_length);// 从缓冲区中移除已处理的数据
				connection.m_brep_data_list.push_back(draw_data);
				if (getCriticalSection().m_mode_draw_new) {
					connection.setCurrentIndexToLatest();
				}
			}
		}
	};

	auto& connection = m_boss_->m_connection_map_[m_connection_id_];
	auto res = recvBrepDataFromSocket(connection);
	addBrepDataToList(connection);

	switch(res){
	case Received:
		return { std::make_shared<BrepDataReceiveTask>(m_boss_, m_connection_id_)};
	case ClientClosed:
		return {std::make_shared<ConnectionCloseTask>(m_boss_, m_connection_id_)};
	case NeedReTry:
		return {std::make_shared<BrepDataReceiveTask>(m_boss_, m_connection_id_)}; 
	default:
		return {std::make_shared<ErrorThrowTask>("Recv")};
	}
}

std::vector<std::shared_ptr<Task>> BrepDataSetTask::run()
{
	SOCKET current_id = getCriticalSection().m_current_connetion_id.value();
	if(m_boss_->m_connection_map_.find(current_id) == m_boss_->m_connection_map_.end()) {
		return {};
	}

	auto& connection = m_boss_->m_connection_map_[current_id];

	auto next_draw_data = connection.getCurrentBrepData();
	if(getCriticalSection().m_brep_data.value() != next_draw_data) {
		getCriticalSection().m_brep_data.setValue(next_draw_data);
		getCriticalSection().m_has_drawn = false;
		emit m_boss_->sigDrawDataReady();
		return {std::make_shared<WaitingDrawTask>(m_boss_)};
	}
	else{
		return {std::make_shared<BrepDataSetTask>(m_boss_)};
	}
}

std::vector<std::shared_ptr<Task>> WaitingDrawTask::run()
{
	if(getCriticalSection().m_has_drawn){
		return {std::make_shared<BrepDataSetTask>(m_boss_)};
	}
	else{
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		return {std::make_shared<WaitingDrawTask>(m_boss_)};
	}
}

std::vector<std::shared_ptr<Task>> ConnectionCloseTask::run()
{
	m_boss_->m_connection_map_.erase(m_connection_id_);
	closesocket(m_connection_id_);
	return {};
}

