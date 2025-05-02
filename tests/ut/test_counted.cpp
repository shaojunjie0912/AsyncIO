#include <catch2/catch_test_macros.hpp>

#include "counted.hpp"

SCENARIO("test Counted") {
    using TestCounted = Counted<default_counted_policy>;
    TestCounted::reset_count();
    GIVEN("move counted") {
        {
            TestCounted c1;
            TestCounted c2(std::move(c1));
            REQUIRE(TestCounted::construct_counts() == 2);
            REQUIRE(TestCounted::move_construct_counts_ == 1);
            REQUIRE(TestCounted::alive_counts() == 2);
        }
        REQUIRE(TestCounted::alive_counts() == 0);
    }

    GIVEN("copy counted") {
        {
            TestCounted c1;
            TestCounted c2(c1);
            REQUIRE(TestCounted::construct_counts() == 2);
            REQUIRE(TestCounted::copy_construct_counts_ == 1);
            REQUIRE(TestCounted::alive_counts() == 2);
        }
        REQUIRE(TestCounted::alive_counts() == 0);
    }

    GIVEN("copy counted") {
        TestCounted c1;
        {
            TestCounted c2(std::move(c1));
            REQUIRE(TestCounted::construct_counts() == 2);
            REQUIRE(TestCounted::move_construct_counts_ == 1);
            REQUIRE(TestCounted::alive_counts() == 2);
        }
        REQUIRE(TestCounted::alive_counts() == 1);
        REQUIRE(c1.id_ == -1);
    }
}