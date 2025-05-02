#include <catch2/catch_test_macros.hpp>
#include <cutecoro/event_loop.hpp>

using namespace cutecoro;
using namespace std::chrono_literals;

SCENARIO("test selector") {
    EventLoop loop;
    Selector selector;
    auto before_wait = loop.time();
    selector.Select(300);
    auto after_wait = loop.time();
    REQUIRE(after_wait - before_wait >= 300ms);
}
