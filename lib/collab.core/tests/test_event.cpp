#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

import collab.core;

using collab::core::event;
using collab::core::subscription;

// ─────────────────────────────────────────────────────────────────────────────
// 1–4: Basic connect / emit / disconnect
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("event<int>: single subscriber receives an emit", "[event][basic]") {
    event<int> evt;
    int        received = 0;
    auto       sub      = evt.connect([&](int x) { received = x; });

    evt(42);

    REQUIRE(received == 42);
}

TEST_CASE("event<>: multiple subscribers all fire in connection order",
          "[event][basic]") {
    event<>          evt;
    std::vector<int> order;

    auto s1 = evt.connect([&] { order.push_back(1); });
    auto s2 = evt.connect([&] { order.push_back(2); });
    auto s3 = evt.connect([&] { order.push_back(3); });

    evt();

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("event<>: emit with zero subscribers is a no-op", "[event][basic]") {
    event<int> evt;
    REQUIRE_NOTHROW(evt(7));
    REQUIRE(evt.subscriber_count() == 0);
}

TEST_CASE("subscriber_count() reflects connect and disconnect",
          "[event][basic]") {
    event<> evt;
    REQUIRE(evt.subscriber_count() == 0);

    auto s1 = evt.connect([] {});
    auto s2 = evt.connect([] {});
    REQUIRE(evt.subscriber_count() == 2);

    s1.disconnect();
    REQUIRE(evt.subscriber_count() == 1);

    {
        auto s3 = evt.connect([] {});
        REQUIRE(evt.subscriber_count() == 2);
    }  // s3's destructor disconnects
    REQUIRE(evt.subscriber_count() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5–8: subscription lifetime
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("subscription destructor disconnects", "[event][lifetime]") {
    event<> evt;
    int     calls = 0;
    {
        auto sub = evt.connect([&] { ++calls; });
        evt();
        REQUIRE(calls == 1);
    }
    evt();
    REQUIRE(calls == 1);
    REQUIRE(evt.subscriber_count() == 0);
}

TEST_CASE("Explicit disconnect() removes the handler", "[event][lifetime]") {
    event<> evt;
    int     calls = 0;
    auto    sub   = evt.connect([&] { ++calls; });

    evt();
    REQUIRE(calls == 1);

    sub.disconnect();
    evt();
    REQUIRE(calls == 1);
    REQUIRE_FALSE(sub.connected());
}

TEST_CASE("disconnect() is idempotent", "[event][lifetime]") {
    event<> evt;
    auto    sub = evt.connect([] {});

    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE(evt.subscriber_count() == 0);
}

TEST_CASE("subscription safely outlives its event", "[event][lifetime]") {
    subscription sub;
    {
        event<int> evt;
        sub = evt.connect([](int) {});
        REQUIRE(sub.connected());
    }
    // event is destroyed; subscription is now orphaned.
    REQUIRE_FALSE(sub.connected());
    REQUIRE_NOTHROW(sub.disconnect());
}  // sub destroyed here — its destructor must not crash

// ─────────────────────────────────────────────────────────────────────────────
// 9–11: Reentrancy from inside a handler
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Handler self-disconnects mid-emit", "[event][reentrancy]") {
    event<> evt;
    int     calls_a = 0;
    int     calls_b = 0;
    int     calls_c = 0;

    subscription sub_b;

    auto sub_a = evt.connect([&] { ++calls_a; });
    sub_b      = evt.connect([&] {
        ++calls_b;
        sub_b.disconnect();
    });
    auto sub_c = evt.connect([&] { ++calls_c; });

    evt();
    // First emit: snapshot included all three. b disconnects itself, but
    // c was already in the snapshot and still fires.
    REQUIRE(calls_a == 1);
    REQUIRE(calls_b == 1);
    REQUIRE(calls_c == 1);
    REQUIRE(evt.subscriber_count() == 2);

    evt();
    // Second emit: b is gone.
    REQUIRE(calls_a == 2);
    REQUIRE(calls_b == 1);
    REQUIRE(calls_c == 2);
}

TEST_CASE("Handler connects new handler mid-emit", "[event][reentrancy]") {
    event<>      evt;
    int          outer_calls = 0;
    int          inner_calls = 0;
    subscription inner_sub;

    auto outer_sub = evt.connect([&] {
        ++outer_calls;
        if (!inner_sub.connected())
            inner_sub = evt.connect([&] { ++inner_calls; });
    });

    evt();
    // Newly-connected inner is NOT in the in-flight snapshot.
    REQUIRE(outer_calls == 1);
    REQUIRE(inner_calls == 0);

    evt();
    // Now inner is in the snapshot.
    REQUIRE(outer_calls == 2);
    REQUIRE(inner_calls == 1);
}

TEST_CASE("Recursive emit on the same event does not deadlock",
          "[event][reentrancy]") {
    event<int> evt;
    int        total_calls = 0;

    auto sub = evt.connect([&](int depth) {
        ++total_calls;
        if (depth > 0) evt(depth - 1);
    });

    evt(3);
    // depth=3 → 2 → 1 → 0  (4 invocations)
    REQUIRE(total_calls == 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// 12–16: Type-erased payloads
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("event<>: void signature", "[event][payload]") {
    event<> evt;
    int     calls = 0;
    auto    sub   = evt.connect([&] { ++calls; });
    evt();
    evt();
    REQUIRE(calls == 2);
}

TEST_CASE("event<int>: primitive arg", "[event][payload]") {
    event<int> evt;
    int        received = 0;
    auto       sub      = evt.connect([&](int x) { received = x; });
    evt(123);
    REQUIRE(received == 123);
}

TEST_CASE("event<const std::string&>: by-const-ref arg", "[event][payload]") {
    event<const std::string&> evt;
    std::string               received;
    const void*               seen_address = nullptr;

    auto sub = evt.connect([&](const std::string& s) {
        received     = s;
        seen_address = &s;
    });

    std::string source = "hello";
    evt(source);

    REQUIRE(received == "hello");
    REQUIRE(seen_address != nullptr);
}

TEST_CASE("event<int, double, const std::string&>: multi-arg",
          "[event][payload]") {
    event<int, double, const std::string&> evt;
    int                                    i = 0;
    double                                 d = 0.0;
    std::string                            s;

    auto sub = evt.connect(
        [&](int a, double b, const std::string& c) {
            i = a;
            d = b;
            s = c;
        });

    evt(7, 3.14, std::string{"pi"});

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

TEST_CASE("event<MyStruct>: user-defined type by value", "[event][payload]") {
    event<payload> evt;
    payload        received{};

    auto sub = evt.connect([&](payload p) { received = std::move(p); });
    evt(payload{.n = 9, .label = "nine"});

    REQUIRE(received.n == 9);
    REQUIRE(received.label == "nine");
}

// ─────────────────────────────────────────────────────────────────────────────
// 17–18: Concurrency
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Concurrent connect / emit / subscriber_count under thrash",
          "[event][concurrency]") {
    event<int>        evt;
    std::atomic<bool> stop{false};

    // Keep one always-on subscriber so emit always has work to do.
    std::atomic<int> baseline_calls{0};
    auto baseline_sub = evt.connect([&](int) { baseline_calls.fetch_add(1); });

    // Connector: continuously connects + drops subscriptions.
    std::thread connector([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto s = evt.connect([](int) {});
            // s drops at end of scope → auto-disconnect
        }
    });

    // Emitter: continuously fires.
    std::thread emitter([&] {
        int i = 0;
        while (!stop.load(std::memory_order_relaxed)) evt(i++);
    });

    // Counter: continuously queries subscriber_count.
    std::thread counter([&] {
        std::size_t total = 0;
        while (!stop.load(std::memory_order_relaxed))
            total += evt.subscriber_count();
        REQUIRE(total >= 0);  // exists only to prevent the loop being elided
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    stop.store(true, std::memory_order_relaxed);

    connector.join();
    emitter.join();
    counter.join();

    REQUIRE(baseline_calls.load() > 0);
}

TEST_CASE("Concurrent emit() from multiple threads", "[event][concurrency]") {
    event<int>       evt;
    std::atomic<int> calls{0};

    auto sub = evt.connect([&](int) { calls.fetch_add(1); });

    constexpr int    iterations = 5'000;
    std::thread      a([&] {
        for (int i = 0; i < iterations; ++i) evt(i);
    });
    std::thread      b([&] {
        for (int i = 0; i < iterations; ++i) evt(i);
    });

    a.join();
    b.join();

    REQUIRE(calls.load() == iterations * 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// 19: Compile-time behavior
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("connect() is [[nodiscard]] (signature check)",
          "[event][compile_time]") {
    // The [[nodiscard]] attribute itself can only be observed via compiler
    // diagnostics — there is no portable trait for it. This test pins the
    // signature so any silent change to the return type is caught, and
    // documents the contract: discarding connect()'s return value MUST
    // produce a compiler diagnostic.
    event<int> evt;
    using ReturnT = decltype(evt.connect([](int) {}));
    STATIC_REQUIRE(std::is_same_v<ReturnT, subscription>);

    // Sanity: subscription is move-only.
    STATIC_REQUIRE(std::is_move_constructible_v<subscription>);
    STATIC_REQUIRE(std::is_move_assignable_v<subscription>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<subscription>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<subscription>);

    // event is pinned (neither copyable nor movable).
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<event<int>>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<event<int>>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<event<int>>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<event<int>>);
}
