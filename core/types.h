#pragma once

#include <mutex>
#include <string>

typedef std::unique_lock<std::recursive_mutex> MutexLockR;
typedef std::unique_lock<std::mutex> MutexLock;
typedef const std::string cstr_t;
