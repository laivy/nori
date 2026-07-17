#pragma once
#include <cstddef>
#include <optional>
#include <thread>
#include <vector>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

namespace nori::network
{
class io_context_pool
{
public:
    explicit io_context_pool(std::size_t thread_count);
    ~io_context_pool();

    io_context_pool(const io_context_pool&) = delete;
    io_context_pool(io_context_pool&&) = delete;
    io_context_pool& operator=(const io_context_pool&) = delete;
    io_context_pool& operator=(io_context_pool&&) = delete;

    void start();
    void stop();
    void join();

    [[nodiscard]]
    boost::asio::io_context& context();

    [[nodiscard]]
    std::size_t thread_count() const;

private:
    using work_guard_type = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

private:
    boost::asio::io_context context_;
    std::optional<work_guard_type> work_;
    std::vector<std::jthread> threads_;
    std::size_t thread_count_;
    bool started_;
};
} // namespace nori::network
