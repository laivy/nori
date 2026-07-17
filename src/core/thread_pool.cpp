#include <exception>
#include <stdexcept>
#include <utility>
#include "thread_pool.hpp"

namespace nori::core
{
thread_pool::thread_pool(std::size_t thread_count) :
    running_{true}
{
    if (thread_count == 0)
    {
        throw std::invalid_argument{"thread_pool requires at least one worker thread"};
    }
    workers_.reserve(thread_count);
    for (std::size_t i{0}; i < thread_count; ++i)
    {
        workers_.emplace_back(std::bind_front(&thread_pool::run, this));
    }
}

thread_pool::~thread_pool()
{
    stop();
}

void thread_pool::post(std::move_only_function<void()> task)
{
    {
        std::lock_guard lock{mutex_};
        if (!running_)
        {
            throw std::runtime_error{"thread_pool is stopped"};
        }
        tasks_.push(std::move(task));
    }
    task_available_.notify_one();
}

void thread_pool::stop()
{
    {
        std::lock_guard lock{mutex_};
        if (!running_)
        {
            return;
        }
        running_ = false;
    }
    for (auto& worker : workers_)
    {
        worker.request_stop();
    }
    task_available_.notify_all();
}

bool thread_pool::is_running() const
{
    std::lock_guard lock{mutex_};
    return running_;
}

void thread_pool::run(const std::stop_token& stop_token)
{
    while (true)
    {
        std::move_only_function<void()> task;
        {
            std::unique_lock lock{mutex_};
            task_available_.wait(
                lock,
                stop_token,
                [this]
                {
                    return !running_ || !tasks_.empty();
                }
            );
            if (tasks_.empty())
            {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        if (task)
        {
            task();
        }
    }
}
} // namespace nori::core
