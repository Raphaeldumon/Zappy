#include "protocol/gui_emitter.hpp"
#include "protocol/gui_handler.hpp"

#include <cassert>
#include <iostream>

using namespace zappy::protocol;
using namespace zappy::core;

static void test_gui_emitter_msz()
{
    assert(GuiEmitter::msz(10, 8) == "msz 10 8");
}

static void test_gui_emitter_sgt()
{
    assert(GuiEmitter::sgt(100) == "sgt 100");
}

static void test_gui_emitter_tna()
{
    assert(GuiEmitter::tna("red") == "tna red");
}

static void test_gui_emitter_bct()
{
    ResourceSet r{};
    r[0] = 1;
    r[1] = 2;
    assert(GuiEmitter::bct(3, 4, r) == "bct 3 4 1 2 0 0 0 0 0");
}

static void test_gui_emitter_pnw()
{
    assert(GuiEmitter::pnw(1, 3, 4, Orientation::North, 1, "red") == "pnw #1 3 4 1 1 red");
    assert(GuiEmitter::pnw(2, 0, 0, Orientation::East, 2, "blue") == "pnw #2 0 0 2 2 blue");
}

static void test_gui_emitter_enw()
{
    assert(GuiEmitter::enw(5, 2, 3, 7) == "enw #5 #2 3 7");
}

static void test_gui_emitter_ppo()
{
    assert(GuiEmitter::ppo(1, 2, 3, Orientation::West) == "ppo #1 2 3 4");
}

static void test_gui_emitter_plv()
{
    assert(GuiEmitter::plv(3, 5) == "plv #3 5");
}

static void test_gui_emitter_pin()
{
    ResourceSet inv{};
    inv[0] = 10;
    assert(GuiEmitter::pin(1, 2, 3, inv) == "pin #1 2 3 10 0 0 0 0 0 0");
}

static void test_gui_emitter_events()
{
    assert(GuiEmitter::pex(7) == "pex #7");
    assert(GuiEmitter::pbc(2, "hello") == "pbc #2 hello");
    assert(GuiEmitter::pfk(4) == "pfk #4");
    assert(GuiEmitter::pdr(1, 2) == "pdr #1 2");
    assert(GuiEmitter::pgt(3, 0) == "pgt #3 0");
    assert(GuiEmitter::pdi(5) == "pdi #5");
    assert(GuiEmitter::ebo(10) == "ebo #10");
    assert(GuiEmitter::edi(11) == "edi #11");
    assert(GuiEmitter::sst(200) == "sst 200");
    assert(GuiEmitter::seg("blue") == "seg blue");
    assert(GuiEmitter::smg("hi") == "smg hi");
    assert(GuiEmitter::suc() == "suc");
    assert(GuiEmitter::sbp() == "sbp");
}

static void test_gui_emitter_pie()
{
    assert(GuiEmitter::pie(2, 3, true) == "pie 2 3 1");
    assert(GuiEmitter::pie(2, 3, false) == "pie 2 3 0");
}

static void test_gui_emitter_pic()
{
    std::vector<PlayerId> ids = {1, 2};
    assert(GuiEmitter::pic(0, 0, 3, ids) == "pic 0 0 3 #1 #2");
}

static void test_parse_gui_request_msz()
{
    auto r = parse_gui_request("msz");
    assert(r.type == GuiRequest::Msz);
}

static void test_parse_gui_request_bct()
{
    auto r = parse_gui_request("bct 3 7");
    assert(r.type == GuiRequest::Bct);
    assert(r.x == 3 && r.y == 7);

    auto bad = parse_gui_request("bct 3");
    assert(bad.type == GuiRequest::BadParam);
}

static void test_parse_gui_request_ppo()
{
    auto r = parse_gui_request("ppo #5");
    assert(r.type == GuiRequest::Ppo);
    assert(r.n == 5);

    // Without hash sign
    auto r2 = parse_gui_request("ppo 5");
    assert(r2.type == GuiRequest::Ppo);
    assert(r2.n == 5);

    auto bad = parse_gui_request("ppo");
    assert(bad.type == GuiRequest::BadParam);
}

static void test_parse_gui_request_sst()
{
    auto r = parse_gui_request("sst 200");
    assert(r.type == GuiRequest::Sst);
    assert(r.t == 200);

    auto bad = parse_gui_request("sst 0");
    assert(bad.type == GuiRequest::BadParam);
}

static void test_parse_gui_request_unknown()
{
    auto r = parse_gui_request("xyz");
    assert(r.type == GuiRequest::Unknown);
}

int main()
{
    test_gui_emitter_msz();
    test_gui_emitter_sgt();
    test_gui_emitter_tna();
    test_gui_emitter_bct();
    test_gui_emitter_pnw();
    test_gui_emitter_enw();
    test_gui_emitter_ppo();
    test_gui_emitter_plv();
    test_gui_emitter_pin();
    test_gui_emitter_events();
    test_gui_emitter_pie();
    test_gui_emitter_pic();
    test_parse_gui_request_msz();
    test_parse_gui_request_bct();
    test_parse_gui_request_ppo();
    test_parse_gui_request_sst();
    test_parse_gui_request_unknown();
    std::cout << "protocol_gui tests OK\n";
    return 0;
}
