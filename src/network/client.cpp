#include <utility>
#include "client_impl.hpp"

namespace nori::network
{
client::client(specification spec) :
    impl_{std::make_unique<impl>(std::move(spec))}
{
}

client::~client() = default;

client::client(client&&) noexcept = default;

client& client::operator=(client&&) noexcept = default;

bool client::start()
{
    return impl_->start();
}

void client::stop()
{
    impl_->stop();
}

void client::send(out_packet packet)
{
    impl_->send(std::move(packet));
}

void client::set_disconnect_handler(disconnect_handler handler)
{
    impl_->set_disconnect_handler(std::move(handler));
}

void client::set_packet_handler(packet_handler handler)
{
    impl_->set_packet_handler(std::move(handler));
}

bool client::is_connected() const
{
    return impl_->is_connected();
}

std::string client::address() const
{
    return impl_->address();
}

std::uint16_t client::port() const
{
    return impl_->port();
}
} // namespace nori::network
