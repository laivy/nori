#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <thread>
#include <vector>

#include <nori/io.hpp>

namespace
{
class context_lifetime
{
public:
    context_lifetime() :
        handle_{nori::io::create_context()}
    {
    }

    ~context_lifetime()
    {
        reset();
    }

    context_lifetime(const context_lifetime&) = delete;
    context_lifetime& operator=(const context_lifetime&) = delete;

    nori::io::context_handle handle() const noexcept
    {
        return handle_.get();
    }

    void reset()
    {
        if (handle_)
        {
            nori::io::stop(handle_.get());
            handle_.reset();
        }
    }

private:
    nori::io::unique_context_handle handle_;
};

class context_runner
{
public:
    context_runner(nori::io::context_handle context, std::size_t count) :
        context_{context}
    {
        workers_.reserve(count);
        for (std::size_t i{0}; i < count; ++i)
        {
            workers_.emplace_back([this]()
            {
                nori::io::run(context_);
            });
        }
    }

    ~context_runner()
    {
        nori::io::stop(context_);
    }

private:
    nori::io::context_handle context_;
    std::vector<std::jthread> workers_;
};

template <typename T>
bool wait_until_ready(std::future<T>& future)
{
    return future.wait_for(std::chrono::seconds{3}) == std::future_status::ready;
}

enum class test_packet_type : std::uint16_t
{
    value = 0x1234
};
} // namespace
