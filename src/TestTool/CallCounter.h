#pragma once

#include <algorithm>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace TestTool
{
class FunctionCallCounter
{
public:
    static FunctionCallCounter& Instance()
    {
        static FunctionCallCounter instance;
        return instance;
    }

    void Record(const char* functionName)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++callCounts_[functionName];
    }

    void DumpToStdout()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (hasDumped_)
        {
            return;
        }
        hasDumped_ = true;

        if (callCounts_.empty())
        {
            std::cout << "[CallCounter] No function call records." << std::endl;
            return;
        }

        std::vector<std::pair<std::string, std::size_t>> sortedCounts(
            callCounts_.begin(),
            callCounts_.end());

        std::sort(sortedCounts.begin(), sortedCounts.end(), [](const auto& left, const auto& right) {
            if (left.second != right.second)
            {
                return left.second > right.second;
            }
            return left.first < right.first;
        });

        std::cout << "\n========== Function Call Statistics ==========" << std::endl;
        for (const auto& [name, count] : sortedCounts)
        {
            std::cout << name << " : " << count << std::endl;
        }
        std::cout << "=============================================\n" << std::endl;
    }

private:
    FunctionCallCounter() = default;

    std::mutex mutex_;
    std::unordered_map<std::string, std::size_t> callCounts_;
    bool hasDumped_ = false;
};

class AutoDumpCallStats
{
public:
    ~AutoDumpCallStats()
    {
        FunctionCallCounter::Instance().DumpToStdout();
    }
};
} // namespace TestTool

#if defined(_MSC_VER)
#define CALL_COUNTER_FUNCTION_NAME __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define CALL_COUNTER_FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define CALL_COUNTER_FUNCTION_NAME __func__
#endif

// Place this macro inside a function body to count each invocation.
#define COUNT_FUNCTION_CALL \
    ::TestTool::FunctionCallCounter::Instance().Record(CALL_COUNTER_FUNCTION_NAME);
