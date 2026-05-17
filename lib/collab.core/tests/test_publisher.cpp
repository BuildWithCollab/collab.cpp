#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

import collab.core;

using namespace collab::core;

// ─────────────────────────────────────────────────────────────────────────────
// 1–4: Basic connect / publish / disconnect
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("publisher<int>: single subscriber receives a publication",
          "[publisher][basic]") {
    publisher<int> pub;
    int            received = 0;
    auto           sub      = pub.connect([&](int x) { received = x; });

    pub(42);

    REQUIRE(received == 42);
}

TEST_CASE("publisher<>: multiple subscribers all fire in subscription order",
          "[publisher][basic]") {
    publisher<>      pub;
    std::vector<int> order;

    auto s1 = pub.connect([&] { order.push_back(1); });
    auto s2 = pub.connect([&] { order.push_back(2); });
    auto s3 = pub.connect([&] { order.push_back(3); });

    pub();

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("publisher<>: publish with zero subscribers is a no-op",
          "[publisher][basic]") {
    publisher<int> pub;
    REQUIRE_NOTHROW(pub(7));
    REQUIRE(pub.subscriber_count() == 0);
}

TEST_CASE("subscriber_count() reflects connect and disconnect",
          "[publisher][basic]") {
    publisher<> pub;
    REQUIRE(pub.subscriber_count() == 0);

    auto s1 = pub.connect([] {});
    auto s2 = pub.connect([] {});
    REQUIRE(pub.subscriber_count() == 2);

    s1.disconnect();
    REQUIRE(pub.subscriber_count() == 1);

    {
        auto s3 = pub.connect([] {});
        REQUIRE(pub.subscriber_count() == 2);
    }  // s3's destructor disconnects
    REQUIRE(pub.subscriber_count() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5–8: subscription lifetime
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("subscription destructor disconnects", "[publisher][lifetime]") {
    publisher<> pub;
    int         calls = 0;
    {
        auto sub = pub.connect([&] { ++calls; });
        pub();
        REQUIRE(calls == 1);
    }
    pub();
    REQUIRE(calls == 1);
    REQUIRE(pub.subscriber_count() == 0);
}

TEST_CASE("Explicit disconnect() removes the handler",
          "[publisher][lifetime]") {
    publisher<> pub;
    int         calls = 0;
    auto        sub   = pub.connect([&] { ++calls; });

    pub();
    REQUIRE(calls == 1);

    sub.disconnect();
    pub();
    REQUIRE(calls == 1);
    REQUIRE_FALSE(sub.connected());
}

TEST_CASE("disconnect() is idempotent", "[publisher][lifetime]") {
    publisher<> pub;
    auto        sub = pub.connect([] {});

    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE(pub.subscriber_count() == 0);
}

TEST_CASE("subscription safely outlives its publisher",
          "[publisher][lifetime]") {
    subscription sub;
    {
        publisher<int> pub;
        sub = pub.connect([](int) {});
        REQUIRE(sub.connected());
    }
    // publisher is destroyed; subscription is now orphaned.
    REQUIRE_FALSE(sub.connected());
    REQUIRE_NOTHROW(sub.disconnect());
}  // sub destroyed here — its destructor must not crash

// ─────────────────────────────────────────────────────────────────────────────
// 9–11: Reentrancy from inside a handler
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Handler self-disconnects mid-publish", "[publisher][reentrancy]") {
    publisher<> pub;
    int         calls_a = 0;
    int         calls_b = 0;
    int         calls_c = 0;

    subscription sub_b;

    auto sub_a = pub.connect([&] { ++calls_a; });
    sub_b      = pub.connect([&] {
        ++calls_b;
        sub_b.disconnect();
    });
    auto sub_c = pub.connect([&] { ++calls_c; });

    pub();
    // First publish: snapshot included all three. b disconnects itself, but
    // c was already in the snapshot and still fires.
    REQUIRE(calls_a == 1);
    REQUIRE(calls_b == 1);
    REQUIRE(calls_c == 1);
    REQUIRE(pub.subscriber_count() == 2);

    pub();
    // Second publish: b is gone.
    REQUIRE(calls_a == 2);
    REQUIRE(calls_b == 1);
    REQUIRE(calls_c == 2);
}

TEST_CASE("Handler connects new handler mid-publish",
          "[publisher][reentrancy]") {
    publisher<>  pub;
    int          outer_calls = 0;
    int          inner_calls = 0;
    subscription inner_sub;

    auto outer_sub = pub.connect([&] {
        ++outer_calls;
        if (!inner_sub.connected())
            inner_sub = pub.connect([&] { ++inner_calls; });
    });

    pub();
    // Newly-connected inner is NOT in the in-flight snapshot.
    REQUIRE(outer_calls == 1);
    REQUIRE(inner_calls == 0);

    pub();
    // Now inner is in the snapshot.
    REQUIRE(outer_calls == 2);
    REQUIRE(inner_calls == 1);
}

TEST_CASE("Recursive publish on the same publisher does not deadlock",
          "[publisher][reentrancy]") {
    publisher<int> pub;
    int            total_calls = 0;

    auto sub = pub.connect([&](int depth) {
        ++total_calls;
        if (depth > 0) pub(depth - 1);
    });

    pub(3);
    // depth=3 → 2 → 1 → 0  (4 invocations)
    REQUIRE(total_calls == 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// 12–16: Type-erased payloads
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("publisher<>: void signature", "[publisher][payload]") {
    publisher<> pub;
    int         calls = 0;
    auto        sub   = pub.connect([&] { ++calls; });
    pub();
    pub();
    REQUIRE(calls == 2);
}

TEST_CASE("publisher<int>: primitive arg", "[publisher][payload]") {
    publisher<int> pub;
    int            received = 0;
    auto           sub      = pub.connect([&](int x) { received = x; });
    pub(123);
    REQUIRE(received == 123);
}

TEST_CASE("publisher<const std::string&>: by-const-ref arg",
          "[publisher][payload]") {
    publisher<const std::string&> pub;
    std::string                   received;
    const void*                   seen_address = nullptr;

    auto sub = pub.connect([&](const std::string& s) {
        received     = s;
        seen_address = &s;
    });

    std::string source = "hello";
    pub(source);

    REQUIRE(received == "hello");
    REQUIRE(seen_address != nullptr);
}

TEST_CASE("publisher<int, double, const std::string&>: multi-arg",
          "[publisher][payload]") {
    publisher<int, double, const std::string&> pub;
    int                                        i = 0;
    double                                     d = 0.0;
    std::string                                s;

    auto sub = pub.connect(
        [&](int a, double b, const std::string& c) {
            i = a;
            d = b;
            s = c;
        });

    pub(7, 3.14, std::string{"pi"});

    REQUIRE(i == 7);
    REQUIRE(d == 3.14);
    REQUIRE(s == "pi");
}

namespace {
struct payload {
    int         n;
    std::string label;
};
}  // namespace

TEST_CASE("publisher<MyStruct>: user-defined type by value",
          "[publisher][payload]") {
    publisher<payload> pub;
    payload            received{};

    auto sub = pub.connect([&](payload p) { received = std::move(p); });
    pub(payload{.n = 9, .label = "nine"});

    REQUIRE(received.n == 9);
    REQUIRE(received.label == "nine");
}

// ─────────────────────────────────────────────────────────────────────────────
// 17–18: Concurrency
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Concurrent connect / publish / subscriber_count under thrash",
          "[publisher][concurrency]") {
    publisher<int>    pub;
    std::atomic<bool> stop{false};

    // Keep one always-on subscriber so publish always has work to do.
    std::atomic<int> baseline_calls{0};
    auto baseline_sub = pub.connect([&](int) { baseline_calls.fetch_add(1); });

    // Connector: continuously connects + drops subscriptions.
    std::thread connector([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto s = pub.connect([](int) {});
            // s drops at end of scope → auto-disconnect
        }
    });

    // Publisher thread: continuously fires.
    std::thread publisher_thread([&] {
        int i = 0;
        while (!stop.load(std::memory_order_relaxed)) pub(i++);
    });

    // Counter: continuously queries subscriber_count.
    std::thread counter([&] {
        std::size_t total = 0;
        while (!stop.load(std::memory_order_relaxed))
            total += pub.subscriber_count();
        REQUIRE(total >= 0);  // exists only to prevent the loop being elided
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    stop.store(true, std::memory_order_relaxed);

    connector.join();
    publisher_thread.join();
    counter.join();

    REQUIRE(baseline_calls.load() > 0);
}

TEST_CASE("Concurrent publish from multiple threads",
          "[publisher][concurrency]") {
    publisher<int>   pub;
    std::atomic<int> calls{0};

    auto sub = pub.connect([&](int) { calls.fetch_add(1); });

    constexpr int iterations = 5'000;
    std::thread   a([&] {
        for (int i = 0; i < iterations; ++i) pub(i);
    });
    std::thread   b([&] {
        for (int i = 0; i < iterations; ++i) pub(i);
    });

    a.join();
    b.join();

    REQUIRE(calls.load() == iterations * 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// 19: Compile-time behavior
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("connect() is [[nodiscard]] (signature check)",
          "[publisher][compile_time]") {
    // The [[nodiscard]] attribute itself can only be observed via compiler
    // diagnostics — there is no portable trait for it. This test pins the
    // signature so any silent change to the return type is caught, and
    // documents the contract: discarding connect()'s return value MUST
    // produce a compiler diagnostic.
    publisher<int> pub;
    using ReturnT = decltype(pub.connect([](int) {}));
    STATIC_REQUIRE(std::is_same_v<ReturnT, subscription>);

    // Sanity: subscription is move-only.
    STATIC_REQUIRE(std::is_move_constructible_v<subscription>);
    STATIC_REQUIRE(std::is_move_assignable_v<subscription>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<subscription>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<subscription>);

    // publisher is pinned (neither copyable nor movable).
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<publisher<int>>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<publisher<int>>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<publisher<int>>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<publisher<int>>);
}
