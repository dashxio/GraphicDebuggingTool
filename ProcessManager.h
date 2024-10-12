#pragma once

#include "MTQueue.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>

extern std::atomic<bool> running;

class ProcessManager {
public:

	static std::unordered_map<std::string, std::function<void()>> func_map;
	static MTQueue<std::string> key_list;
	static std::vector<std::thread> thread_list;

};
