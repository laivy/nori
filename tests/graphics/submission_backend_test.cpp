#include "command_encoder_test_helpers.hpp"
#include "command_queue.hpp"
#include "context.hpp"

namespace
{
class submission_backend_test : public transfer_order_test
{
};

TEST_F(submission_backend_test, submission_queries_do_not_wait_for_later_work)
{
    const std::uint64_t first{submit()};
    ASSERT_NE(first, 0u);
    wait(first);
    Microsoft::WRL::ComPtr<ID3D12Fence> gate_fence;
    ASSERT_TRUE(SUCCEEDED(ctx()->device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate_fence))));
    std::uint64_t pending{};
    {
        const submission_queue_gate gate{gate_fence};
        ASSERT_TRUE(SUCCEEDED(ctx()->queue()->native()->Wait(gate_fence.Get(), 1)));
        const std::uint64_t second{submit()};
        ASSERT_NE(second, 0u);
        pending = second;
        EXPECT_FALSE(poll(pending));
        wait(first);
    }
    wait(pending);
    EXPECT_TRUE(poll(pending));
}
} // namespace
