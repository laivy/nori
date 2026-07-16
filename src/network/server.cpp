#include <memory>
#include <stop_token>
#include <utility>
#include "server_impl.hpp"

namespace nori::network
{
server::server(specification spec) :
    impl_{std::make_unique<impl>(std::move(spec))}
{
}

server::~server() = default;

server::server(server&&) noexcept = default;

server& server::operator=(server&&) noexcept = default;

void server::start()
{
    impl_->start();
}

void server::stop()
{
    impl_->stop();
}

void server::send(client_id id, out_packet packet)
{
    impl_->send(id, std::move(packet));
}

void server::disconnect(client_id id)
{
    impl_->disconnect(id);
}

std::unique_ptr<server::packet_event> server::wait_packet(std::stop_token stop_token)
{
    return impl_->wait_packet(stop_token);
}

bool server::is_connected(client_id id) const
{
    return impl_->is_connected(id);
}

std::size_t server::connection_count() const
{
    return impl_->connection_count();
}

std::string server::remote_endpoint(client_id id) const
{
    return impl_->remote_endpoint(id);
}

void server::set_connect_handler(connect_handler handler)
{
    impl_->set_connect_handler(std::move(handler));
}

void server::set_disconnect_handler(disconnect_handler handler)
{
    impl_->set_disconnect_handler(std::move(handler));
}

std::uint16_t server::port() const
{
    return impl_->port();
}

std::string server::address() const
{
    return impl_->address();
}
} // namespace nori::network
