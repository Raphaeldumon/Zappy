#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

namespace zappy::core {

// Deterministic time-ordered event queue driving the game loop.
//
// Time is measured in abstract "ticks" (derived from the server frequency f).
// Events scheduled for the same tick fire in insertion order (FIFO) so replays
// are reproducible. P1 owns the semantics; this is the working skeleton.
class EventScheduler {
public:
    using Callback = std::function<void()>;
    using Tick = std::uint64_t;

    // Schedule `cb` to run when the clock reaches `at_tick`. Returns an id usable
    // with cancel().
    std::uint64_t schedule(Tick at_tick, Callback cb);

    // Cancel a pending event. Returns true if it was still pending.
    bool cancel(std::uint64_t event_id);

    // Run every event whose tick is <= `now`, in (tick, insertion) order.
    void advance_to(Tick now);

    [[nodiscard]] Tick current_tick() const noexcept {
        return now_;
    }
    [[nodiscard]] bool empty() const noexcept {
        return queue_.empty();
    }
    [[nodiscard]] std::size_t pending() const noexcept {
        return queue_.size();
    }

private:
    struct Event {
        Tick tick;
        std::uint64_t seq; // insertion order, breaks tick ties
        std::uint64_t id;  // stable id for cancellation
        Callback cb;
    };
    struct Later {
        bool operator()(const Event& a, const Event& b) const noexcept {
            if (a.tick != b.tick) {
                return a.tick > b.tick;
            }
            return a.seq > b.seq;
        }
    };

    std::priority_queue<Event, std::vector<Event>, Later> queue_;
    std::vector<std::uint64_t> cancelled_;
    Tick now_{0};
    std::uint64_t next_seq_{0};
    std::uint64_t next_id_{1};
};

} // namespace zappy::core
