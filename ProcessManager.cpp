#include "ProcessManager.h"

std::atomic<bool> running(true);

std::unordered_map<std::string, std::function<void()>> ProcessManager::func_map = {};
MTQueue<std::string> ProcessManager::key_list = MTQueue<std::string>();
std::vector<std::thread> ProcessManager::thread_list = {};