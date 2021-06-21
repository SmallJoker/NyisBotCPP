#pragma once

#include <mutex>
using MutexLock = std::unique_lock<std::mutex>;
using MutexLockRecursive = std::unique_lock<std::recursive_mutex>;
