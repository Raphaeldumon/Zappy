#include "net/client.hpp"
#include "zappy/protocol/ai_protocol.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace zappy::net;

static void test_drain_lines_complete()
{
    Client c;
    c.recv_buf = "hello\nworld\n";
    auto lines = c.drain_lines();
    assert(lines.size() == 2);
    assert(lines[0] == "hello");
    assert(lines[1] == "world");
    assert(c.recv_buf.empty());
}

static void test_drain_lines_partial()
{
    Client c;
    c.recv_buf = "hello\nwor";
    auto lines = c.drain_lines();
    assert(lines.size() == 1);
    assert(lines[0] == "hello");
    assert(c.recv_buf == "wor");
}

static void test_drain_lines_crlf()
{
    Client c;
    c.recv_buf = "hello\r\n";
    auto lines = c.drain_lines();
    assert(lines.size() == 1);
    assert(lines[0] == "hello");
}

static void test_drain_lines_empty()
{
    Client c;
    c.recv_buf = "incomplete";
    auto lines = c.drain_lines();
    assert(lines.empty());
    assert(c.recv_buf == "incomplete");
}

static void test_enqueue_appends_newline()
{
    Client c;
    c.enqueue("hello");
    assert(c.send_queue.size() == 1);
    assert(c.send_queue.front() == "hello\n");
}

static void test_enqueue_front_prepends()
{
    Client c;
    c.enqueue("second");
    c.enqueue_front("first");
    assert(c.send_queue.size() == 2);
    assert(c.send_queue.front() == "first\n");
}

static void test_has_pending_write()
{
    Client c;
    assert(!c.has_pending_write());
    c.enqueue("msg");
    assert(c.has_pending_write());
}

static void test_command_queue_ordering()
{
    Client c;
    c.command_queue.push_back("cmd1");
    c.command_queue.push_back("cmd2");
    assert(c.command_queue.front() == "cmd1");
    c.command_queue.pop_front();
    assert(c.command_queue.front() == "cmd2");
}

static void test_command_queue_max()
{
    // Simulate the MAX_COMMAND_QUEUE drop logic
    Client c;
    for (int i = 0; i < zappy::protocol::ai::MAX_COMMAND_QUEUE; ++i)
        c.command_queue.push_back("cmd");
    assert(static_cast<int>(c.command_queue.size()) == zappy::protocol::ai::MAX_COMMAND_QUEUE);
}

int main()
{
    test_drain_lines_complete();
    test_drain_lines_partial();
    test_drain_lines_crlf();
    test_drain_lines_empty();
    test_enqueue_appends_newline();
    test_enqueue_front_prepends();
    test_has_pending_write();
    test_command_queue_ordering();
    test_command_queue_max();
    std::cout << "network tests OK\n";
    return 0;
}
