#include <algorithm>
#include "io_context_pool.hpp"

namespace nori::network
{
io_context_pool::io_context_pool(std::size_t thread_count) :
    thread_count_{std::max(thread_count, 1uz)},
    started_{false}
{
}

io_context_pool::~io_context_pool()
{
    stop();
    join();
}

void io_context_pool::start()
{
    if (started_)
    {
        return;
    }

    context_.restart();
    work_.emplace(boost::asio::make_work_guard(context_));
    started_ = true;
    threads_.reserve(thread_count_);
    for (std::size_t i{0}; i < thread_count_; ++i)
    {
        threads_.emplace_back(
            [this]
            {
                context_.run();
            }
        );
    }
}

void io_context_pool::stop()
{
    if (work_)
    {
        work_->reset();
        work_.reset();
    }
    context_.stop();
}

void io_context_pool::join()
{
    threads_.clear();
    work_.reset();
    started_ = false;
}

boost::asio::io_context& io_context_pool::context()
{
    return context_;
}

std::size_t io_context_pool::thread_count() const
{
    return thread_count_;
}
} // namespace nori::network
