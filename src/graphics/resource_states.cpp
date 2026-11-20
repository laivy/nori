#include <algorithm>

#include <directx/d3dx12.h>

#include "resource_states.hpp"

namespace nori::graphics
{
resource_states::resource_states() :
    states_{D3D12_RESOURCE_STATE_COMMON}
{
}

resource_states::resource_states(D3D12_RESOURCE_STATES initial_state, std::uint32_t subresource_count) :
    states_(subresource_count, initial_state)
{
}

void resource_states::transition(ID3D12GraphicsCommandList* command_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES target, std::uint32_t subresource)
{
    const auto emit = [command_list, resource, target](D3D12_RESOURCE_STATES& state, std::uint32_t index)
    {
        constexpr auto read_states{D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_RESOLVE_SOURCE};
        auto next{target};
        if (state != D3D12_RESOURCE_STATE_COMMON && target != D3D12_RESOURCE_STATE_COMMON && (state & ~read_states) == 0 && (target & ~read_states) == 0)
        {
            // 연속 읽기의 접근 상태를 보존하여 다음 쓰기 전이가 모두 동기화하도록 함
            next = static_cast<D3D12_RESOURCE_STATES>(state | target);
        }
        if (state != next)
        {
            const auto barrier{CD3DX12_RESOURCE_BARRIER::Transition(resource, state, next, index)};
            command_list->ResourceBarrier(1, &barrier);
            state = next;
        }
        return state;
    };
    if (subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        emit(states_[subresource], subresource);
        return;
    }
    if (std::ranges::all_of(states_, [this](D3D12_RESOURCE_STATES state)
    {
        return state == states_.front();
    }))
    {
        const auto next{emit(states_.front(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)};
        std::ranges::fill(states_, next);
        return;
    }
    for (std::uint32_t index{0}; index < states_.size(); ++index)
    {
        emit(states_[index], index);
    }
}
} // namespace nori::graphics
