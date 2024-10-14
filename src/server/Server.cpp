#include "server/Server.h"

#include "common/utf8_system_category.hpp"

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
    return *this;
}


void MyServer::run()
{
	m_task_deque_.emplace_back([this] {
		acceptConnection();
		});

    std::thread t([this]{
        while(!m_task_deque_.empty()){
            auto t = m_task_deque_.front();
            m_task_deque_.pop_front();
            t();
        }
        });

    m_work_thread_ = std::move(t);
}

void MyServer::acceptConnection()
{
    listen(m_id_, SOMAXCONN);

	SOCKET connection_id;
	sockaddr_in client_addr;
	int addr_len = sizeof(client_addr);
	connection_id = accept(m_id_, (struct sockaddr*)&client_addr, &addr_len);
	if (connection_id == INVALID_SOCKET) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
			throw std::system_error(ec, "accept");
		}
        else{
            m_task_deque_.emplace_back([this]{
                acceptConnection();
            });
        }
	}
	else {
		
		getCriticalSection().m_connection_list.push_back(connection_id);
        m_connection_map_.emplace(connection_id, ConnectionInfo{connection_id});
        getCriticalSection().m_current_connetion_id.setValue(connection_id);
		m_task_deque_.emplace_back([this, connection_id]
		{
			receiveBrepData(m_connection_map_[connection_id]);
		});
		
	}

}

void MyServer::receiveBrepData(ConnectionInfo& connection)
{
	if (!isConnectionIdValid(connection.m_id))
		return;

    const int HEADER_SIZE = sizeof(int32_t);  // 消息头长度
	auto& temp_data_buffer = connection.m_reserve_buffer;

	size_t old_size = temp_data_buffer.size();
	size_t new_size = old_size + 4096;
	temp_data_buffer.resize(new_size);

	int received = recv(connection.m_id, temp_data_buffer.data() + old_size, 4096, 0);

	if (received == 0)
	{
		std::cout << "Connection " << connection.m_id << " closed by client.\n";
		m_task_deque_.emplace_back([this, &connection]
			{
                //emit sigEraseSocket(connetion.m_id);
				m_connection_map_.erase(connection.m_id);
			});
        return;
	}

    if(received < 0)
    {
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			temp_data_buffer.resize(old_size);
			m_task_deque_.emplace_back([this, &connection]
				{
					receiveBrepData(connection);
				});
		}
		else
		{
			auto ec = std::error_code(WSAGetLastError(), utf8_system_category());
			throw std::system_error(ec, "recv");
		}
        return;
    }

	temp_data_buffer.resize(old_size + received);

	// 处理缓冲区中的数据

		// 检查是否有足够的数据读取消息长度
	if (temp_data_buffer.size() < HEADER_SIZE){
		m_task_deque_.emplace_back([this, &connection]
			{
				receiveBrepData(connection);
			});
        return;
    }

	// 读取消息长度（前4字节）
	int32_t data_length_r = 0;
	memcpy(&data_length_r, temp_data_buffer.data(), HEADER_SIZE);
	int32_t data_length = ntohl(data_length_r);

	// 检查是否有足够的数据读取完整的消息
	if (temp_data_buffer.size() < HEADER_SIZE + data_length){
		m_task_deque_.emplace_back([this, &connection]
			{
				receiveBrepData(connection);
			});
        return;
    }

	// 提取完整的消息数据
	std::string draw_data;
	draw_data.resize(data_length);
	memcpy(draw_data.data(), temp_data_buffer.data() + HEADER_SIZE, data_length); //这里可以优化，但目前没太大必要                        
	temp_data_buffer.erase(0, HEADER_SIZE + data_length);// 从缓冲区中移除已处理的数据

	// 处理消息数据
	connection.m_brep_data_list.push_back(draw_data);
	if (connection.m_id == getCriticalSection().m_current_connetion_id.value()) {
        if(getCriticalSection().m_mode_draw_new)
            connection.m_data_index = connection.m_brep_data_list.size() - 1;
		getCriticalSection().m_brep_data.setValue(connection.m_brep_data_list.at(connection.m_data_index));
		emit sigDrawDataReady();
        m_task_deque_.emplace_back([this, &connection]
        {
	        sleepUntilDrawn(connection);
        });
	}
    else
    {
		m_task_deque_.emplace_back([this, &connection]
			{
				receiveBrepData(connection);
			});
    }
}

void MyServer::sleepUntilDrawn(ConnectionInfo & connection)
{
	if (!getCriticalSection().m_has_drawn)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 暂停执行100毫秒
		m_task_deque_.emplace_back([this, &connection]
		{
			sleepUntilDrawn(connection);
		});
	}
	else
	{
		getCriticalSection().m_has_drawn = false;
		receiveBrepData(connection);
    }
}

bool MyServer::isConnectionIdValid(SOCKET connetion_id)
{
    return m_connection_map_.find(connetion_id) != m_connection_map_.end();
}


