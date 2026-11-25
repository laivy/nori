#pragma once
#include <nori/core/handle.hpp>

namespace nori::io::detail
{
struct context_tag;

class context_deleter
{
public:
    void operator()(core::basic_handle<context_tag> handle) const noexcept;
};
} // namespace nori::io::detail

namespace nori::io
{
using context_handle = core::basic_handle<detail::context_tag>;
using unique_context_handle = core::unique_handle<context_handle, detail::context_deleter>;

[[nodiscard]] unique_context_handle create_context();
/// @pre run·poll과 관련 실행 스레드의 사용 종료, 외부에 보관한 I/O 연산의 완료 또는 폐기
void destroy(context_handle ctx);
void run(context_handle ctx);
void poll(context_handle ctx);
void stop(context_handle ctx);
} // namespace nori::io
