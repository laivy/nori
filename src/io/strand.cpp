#include <memory>
#include <utility>

#include "context.hpp"
#include "strand.hpp"

namespace nori::io
{
strand::strand(const std::shared_ptr<context>& ctx) :
    context_{ctx},
    native_{asio::make_strand(ctx->native_executor())}
{
}

bool strand::post(std::move_only_function<void()> function)
{
    const auto ctx{context_.lock()};
    if (!ctx)
    {
        return false;
    }
    const auto self{shared_from_this()};
    return ctx->submit(native_, [self, function = std::move(function)]() mutable
    {
        executor_scope scope{self};
        std::move(function)();
    });
}

bool strand::is_stopped() const
{
    const auto ctx{context_.lock()};
    return !ctx || ctx->is_stopped();
}

std::shared_ptr<context> strand::root()
{
    return context_.lock();
}

asio::any_io_executor strand::native_executor() const
{
    return native_;
}
} // namespace nori::io
