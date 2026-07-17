#include "application.hpp"
#include "input.hpp"

namespace nori::core
{
void application::on_event(event& /*e*/)
{
}

void application::on_tick(float /*delta_seconds*/)
{
}

void application::process_event(event& e)
{
    update_input_state(e);
    on_event(e);
    event_dispatcher_.dispatch(e);
}
} // namespace nori::core
