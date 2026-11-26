#pragma once
#include <cstdint>
#include <vector>

#include <directx/d3d12.h>

namespace nori::graphics
{
class resource_states
{
public:
    resource_states();
    explicit resource_states(D3D12_RESOURCE_STATES initial_state, std::uint32_t subresource_count = 1);

    void transition(ID3D12GraphicsCommandList* command_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES target, std::uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

private:
    std::vector<D3D12_RESOURCE_STATES> states_;
};
} // namespace nori::graphics
