#include <catch2/catch_test_macros.hpp>
#include <cutecoro/cutecoro.hpp>

#include "counted.hpp"

using namespace cutecoro;

// 编译期调试类型 (clang)
// template <typename, typename...>
// struct [[deprecated]] Dump {};

// 编译期调试类型 (gcc)
template <typename, typename...>
struct Dump;

SCENARIO("test result T") {
    // 禁止拷贝和移动赋值
    using TestCounted = Counted<{.move_assignable = false, .copy_assignable = false}>;
    TestCounted::reset_count();

    // 为结果设置左值
    GIVEN("result set lvalue") {
        Result<TestCounted> res;
        REQUIRE(!res.HasValue());
        {
            TestCounted c;
            REQUIRE(TestCounted::construct_counts() == 1);
            REQUIRE(TestCounted::copy_construct_counts_ == 0);
            res.SetValue(c);
            REQUIRE(TestCounted::construct_counts() == 2);
            REQUIRE(TestCounted::copy_construct_counts_ == 1);
        }
        REQUIRE(TestCounted::alive_counts() == 1);
        REQUIRE(res.HasValue());
    }

    // 为结果设置右值
    GIVEN("result set rvalue") {
        Result<TestCounted> res;
        REQUIRE(!res.HasValue());
        {
            TestCounted c;
            REQUIRE(TestCounted::construct_counts() == 1);
            REQUIRE(TestCounted::move_construct_counts_ == 0);
            res.SetValue(std::move(c));
            REQUIRE(TestCounted::construct_counts() == 2);
            REQUIRE(TestCounted::move_construct_counts_ == 1);
        }
        REQUIRE(TestCounted::alive_counts() == 1);
        REQUIRE(res.HasValue());
    }

    // 取左值结果
    GIVEN("lvalue result") {
        Result<TestCounted> res;
        res.SetValue(TestCounted{});  // 右值 (调用移动构造函数)
        REQUIRE(res.HasValue());
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::move_construct_counts_ == 1);
        REQUIRE(TestCounted::copy_construct_counts_ == 0);
        {
            {
                auto&& r = res.GetResult();
                REQUIRE(TestCounted::default_construct_counts_ == 1);
                REQUIRE(TestCounted::move_construct_counts_ == 1);
                REQUIRE(TestCounted::copy_construct_counts_ == 1);
            }
            {
                auto r = res.GetResult();
                REQUIRE(TestCounted::default_construct_counts_ == 1);
                REQUIRE(TestCounted::copy_construct_counts_ == 2);
            }
        }
        REQUIRE(TestCounted::alive_counts() == 1);
    }

    // 取右值结果
    GIVEN("rvalue result") {
        Result<TestCounted> res;
        res.SetValue(TestCounted{});
        REQUIRE(res.HasValue());
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::move_construct_counts_ == 1);
        {
            auto r = std::move(res).GetResult();
            REQUIRE(TestCounted::move_construct_counts_ == 2);
            REQUIRE(TestCounted::alive_counts() == 2);
        }
        REQUIRE(TestCounted::alive_counts() == 1);
    }
}

SCENARIO("test Counted for Task") {
    using TestCounted = Counted<default_counted_policy>;
    TestCounted::reset_count();

    auto build_count = []() -> Task<TestCounted> { co_return TestCounted{}; };
    bool called{false};

    GIVEN("return a counted") {
        Run([&]() -> Task<> {
            auto c = co_await build_count();
            REQUIRE(TestCounted::alive_counts() == 1);
            REQUIRE(TestCounted::move_construct_counts_ == 2);
            REQUIRE(TestCounted::default_construct_counts_ == 1);
            REQUIRE(TestCounted::copy_construct_counts_ == 0);
            called = true;
        }());
        REQUIRE(called);
    }

    GIVEN("return a lvalue counted") {
        Run([&]() -> Task<> {
            auto t = build_count();
            {
                auto c = co_await t;
                REQUIRE(TestCounted::alive_counts() == 2);
                REQUIRE(TestCounted::move_construct_counts_ == 1);
                REQUIRE(TestCounted::default_construct_counts_ == 1);
                REQUIRE(TestCounted::copy_construct_counts_ == 1);
            }

            {
                auto c = co_await std::move(t);
                REQUIRE(TestCounted::alive_counts() == 2);
                REQUIRE(TestCounted::move_construct_counts_ == 2);
                REQUIRE(TestCounted::default_construct_counts_ == 1);
                REQUIRE(TestCounted::copy_construct_counts_ == 1);
            }

            called = true;
        }());
        REQUIRE(called);
    }

    GIVEN("rvalue task: get_result") {
        auto c = Run(build_count());
        REQUIRE(TestCounted::alive_counts() == 1);
        REQUIRE(TestCounted::move_construct_counts_ == 2);
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::copy_construct_counts_ == 0);
    }

    GIVEN("lvalue task: get_result") {
        auto t = build_count();
        auto c = Run(t);
        REQUIRE(TestCounted::alive_counts() == 2);
        REQUIRE(TestCounted::move_construct_counts_ == 1);
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::copy_construct_counts_ == 1);
    }
}

SCENARIO("test pass parameters to the coroutine frame") {
    using TestCounted = Counted<{.move_assignable = false, .copy_assignable = false}>;
    TestCounted::reset_count();

    GIVEN("pass by rvalue") {
        auto coro = [](TestCounted count) -> Task<> {
            REQUIRE(count.alive_counts() == 2);
            co_return;
        };
        Run(coro(TestCounted{}));
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::move_construct_counts_ == 1);
        REQUIRE(TestCounted::alive_counts() == 0);
    }

    GIVEN("pass by lvalue") {
        auto coro = [](TestCounted count) -> Task<> {
            REQUIRE(TestCounted::copy_construct_counts_ == 1);
            REQUIRE(TestCounted::move_construct_counts_ == 1);
            REQUIRE(count.alive_counts() == 3);
            co_return;
        };
        TestCounted count;
        Run(coro(count));

        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::copy_construct_counts_ == 1);
        REQUIRE(TestCounted::move_construct_counts_ == 1);
        REQUIRE(TestCounted::alive_counts() == 1);
        REQUIRE(count.id_ != -1);
    }

    GIVEN("pass by xvalue") {
        auto coro = [](TestCounted count) -> Task<> {
            REQUIRE(TestCounted::copy_construct_counts_ == 0);
            REQUIRE(TestCounted::move_construct_counts_ == 2);
            REQUIRE(count.alive_counts() == 3);
            REQUIRE(count.id_ != -1);
            co_return;
        };
        TestCounted count;
        Run(coro(std::move(count)));

        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::copy_construct_counts_ == 0);
        REQUIRE(TestCounted::move_construct_counts_ == 2);
        REQUIRE(TestCounted::alive_counts() == 1);
        REQUIRE(count.id_ == -1);
    }

    GIVEN("pass by lvalue ref") {
        TestCounted count;
        auto coro = [&](TestCounted& cnt) -> Task<> {
            REQUIRE(cnt.alive_counts() == 1);
            REQUIRE(&cnt == &count);
            co_return;
        };
        Run(coro(count));
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::construct_counts() == 1);
        REQUIRE(TestCounted::alive_counts() == 1);
    }

    GIVEN("pass by rvalue ref") {
        auto coro = [](TestCounted&& count) -> Task<> {
            REQUIRE(count.alive_counts() == 1);
            co_return;
        };
        Run(coro(TestCounted{}));
        REQUIRE(TestCounted::default_construct_counts_ == 1);
        REQUIRE(TestCounted::construct_counts() == 1);
        REQUIRE(TestCounted::alive_counts() == 0);
    }
}
