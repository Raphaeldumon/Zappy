// Skeleton unit tests for EventScheduler. Migrate to Catch2 once vcpkg is wired.
#include "core/event_scheduler.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace zappy::core;

static void test_fifo_within_same_tick() {
    EventScheduler s;
    std::vector<int> order;
    s.schedule(5, [&] { order.push_back(1); });
    s.schedule(5, [&] { order.push_back(2); });
    s.schedule(5, [&] { order.push_back(3); });
    s.advance_to(5);
    assert((order == std::vector<int>{1, 2, 3}));
}

static void test_tick_ordering() {
    EventScheduler s;
    std::vector<int> order;
    s.schedule(10, [&] { order.push_back(10); });
    s.schedule(1, [&] { order.push_back(1); });
    s.schedule(5, [&] { order.push_back(5); });
    s.advance_to(4); // only the tick-1 event fires
    assert((order == std::vector<int>{1}));
    s.advance_to(100);
    assert((order == std::vector<int>{1, 5, 10}));
}

static void test_cancel() {
    EventScheduler s;
    bool fired = false;
    std::uint64_t id = s.schedule(3, [&] { fired = true; });
    assert(s.cancel(id));
    s.advance_to(10);
    assert(!fired);
}

int main() {
    test_fifo_within_same_tick();
    test_tick_ordering();
    test_cancel();
    std::cout << "event_scheduler tests OK\n";
    return 0;
}
