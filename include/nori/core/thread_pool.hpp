#pragma once
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace nori::core
{
class thread_pool
{
public:
    explicit thread_pool(std::size_t thread_count);
    ~thread_pool();

    thread_pool(const thread_pool&) = delete;
    thread_pool(thread_pool&&) noexcept = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool& operator=(thread_pool&&) noexcept = delete;

    void post(std::move_only_function<void()> task);
    void stop();

    [[nodiscard]]
    bool is_running() const;

private:
    void run(const std::stop_token& stop_token);

private:
    std::vector<std::jthread> workers_;
    std::queue<std::move_only_function<void()>> tasks_;

    mutable std::mutex mutex_;
    bool running_;
    std::condition_variable_any task_available_;
};
} // namespace nori::core
