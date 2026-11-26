#pragma once
#include <functional>
#include <memory>

#include <asio/any_io_executor.hpp>
#include <asio/strand.hpp>

#include "executor.hpp"

namespace nori::io
{
class context;

class strand final : public detail::executor, public std::enable_shared_from_this<strand>
{
public:
    explicit strand(const std::shared_ptr<context>& ctx);

    [[nodiscard]] bool post(std::move_only_function<void()> function) override;
    [[nodiscard]] bool is_stopped() const override;
    [[nodiscard]] std::shared_ptr<context> root() override;
    asio::any_io_executor native_executor() const;

private:
    std::weak_ptr<context> context_;
    asio::strand<asio::any_io_executor> native_;
};
} // namespace nori::io
