#include "core/event_scheduler.hpp"

#include <algorithm>

namespace zappy::core
{

std::uint64_t EventScheduler::schedule(Tick at_tick, Callback cb)
{
    std::uint64_t id = next_id_++;
    queue_.push(Event{at_tick, next_seq_++, id, std::move(cb)});
    return id;
}

bool EventScheduler::cancel(std::uint64_t event_id)
{
    if (std::find(cancelled_.begin(), cancelled_.end(), event_id) != cancelled_.end())
    {
        return false;
    }
    cancelled_.push_back(event_id);
    return true;
}

void EventScheduler::advance_to(Tick now)
{
    now_ = now;
    while (!queue_.empty() && queue_.top().tick <= now_)
    {
        Event ev = queue_.top();
        queue_.pop();

        auto it = std::find(cancelled_.begin(), cancelled_.end(), ev.id);
        if (it != cancelled_.end())
        {
            cancelled_.erase(it);
            continue;
        }
        ev.cb();
    }
}

} // namespace zappy::core
