#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

import collab.core;

using collab::core::signal;
using collab::core::subscription;

// ─────────────────────────────────────────────────────────────────────────────
// 1–4: Basic connect / emit / disconnect
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("signal<int>: single subscriber receives an emit", "[signal][basic]") {
    signal<int> sig;
    int         received = 0;
    auto        sub      = sig.connect([&](int x) { received = x; });

    sig(42);

    REQUIRE(received == 42);
}

TEST_CASE("signal<>: multiple subscribers all fire in connection order",
          "[signal][basic]") {
    signal<>         sig;
    std::vector<int> order;

    auto s1 = sig.connect([&] { order.push_back(1); });
    auto s2 = sig.connect([&] { order.push_back(2); });
    auto s3 = sig.connect([&] { order.push_back(3); });

    sig();

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("signal<>: emit with zero subscribers is a no-op", "[signal][basic]") {
    signal<int> sig;
    REQUIRE_NOTHROW(sig(7));
    REQUIRE(sig.subscriber_count() == 0);
}

TEST_CASE("subscriber_count() reflects connect and disconnect",
          "[signal][basic]") {
    signal<> sig;
    REQUIRE(sig.subscriber_count() == 0);

    auto s1 = sig.connect([] {});
    auto s2 = sig.connect([] {});
    REQUIRE(sig.subscriber_count() == 2);

    s1.disconnect();
    REQUIRE(sig.subscriber_count() == 1);

    {
        auto s3 = sig.connect([] {});
        REQUIRE(sig.subscriber_count() == 2);
    }  // s3's destructor disconnects
    REQUIRE(sig.subscriber_count() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5–8: subscription lifetime
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("subscription destructor disconnects", "[signal][lifetime]") {
    signal<> sig;
    int      calls = 0;
    {
        auto sub = sig.connect([&] { ++calls; });
        sig();
        REQUIRE(calls == 1);
    }
    sig();
    REQUIRE(calls == 1);
    REQUIRE(sig.subscriber_count() == 0);
}

TEST_CASE("Explicit disconnect() removes the handler", "[signal][lifetime]") {
    signal<> sig;
    int      calls = 0;
    auto     sub   = sig.connect([&] { ++calls; });

    sig();
    REQUIRE(calls == 1);

    sub.disconnect();
    sig();
    REQUIRE(calls == 1);
    REQUIRE_FALSE(sub.connected());
}

TEST_CASE("disconnect() is idempotent", "[signal][lifetime]") {
    signal<> sig;
    auto     sub = sig.connect([] {});

    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE_NOTHROW(sub.disconnect());
    REQUIRE(sig.subscriber_count() == 0);
}

TEST_CASE("subscription safely outlives its signal", "[signal][lifetime]") {
    subscription sub;
    {
        signal<int> sig;
        sub = sig.connect([](int) {});
        REQUIRE(sub.connected());
    }
    // signal is destroyed; subscription is now orphaned.
    REQUIRE_FALSE(sub.connected());
    REQUIRE_NOTHROW(sub.disconnect());
}  // sub destroyed here — its destructor must not crash

// ─────────────────────────────────────────────────────────────────────────────
// 9–11: Reentrancy from inside a handler
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Handler self-disconnects mid-emit", "[signal][reentrancy]") {
    signal<> sig;
    int      calls_a = 0;
    int      calls_b = 0;
    int      calls_c = 0;

    subscription sub_b;

    auto sub_a = sig.connect([&] { ++calls_a; });
    sub_b      = sig.connect([&] {
        ++calls_b;
        sub_b.disconnect();
    });
    auto sub_c = sig.connect([&] { ++calls_c; });

    sig();
    // First emit: snapshot included all three. b disconnects itself, but
    // c was already in the snapshot and still fires.
    REQUIRE(calls_a == 1);
    REQUIRE(calls_b == 1);
    REQUIRE(calls_c == 1);
    REQUIRE(sig.subscriber_count() == 2);

    sig();
    // Second emit: b is gone.
    REQUIRE(calls_a == 2);
    REQUIRE(calls_b == 1);
    REQUIRE(calls_c == 2);
}

TEST_CASE("Handler connects new handler mid-emit", "[signal][reentrancy]") {
    signal<>     sig;
    int          outer_calls = 0;
    int          inner_calls = 0;
    subscription inner_sub;

    auto outer_sub = sig.connect([&] {
        ++outer_calls;
        if (!inner_sub.connected())
            inner_sub = sig.connect([&] { ++inner_calls; });
    });

    sig();
    // Newly-connected inner is NOT in the in-flight snapshot.
    REQUIRE(outer_calls == 1);
    REQUIRE(inner_calls == 0);

    sig();
    // Now inner is in the snapshot.
    REQUIRE(outer_calls == 2);
    REQUIRE(inner_calls == 1);
}

TEST_CASE("Recursive emit on the same signal does not deadlock",
          "[signal][reentrancy]") {
    signal<int> sig;
    int         total_calls = 0;

    auto sub = sig.connect([&](int depth) {
        ++total_calls;
        if (depth > 0) sig(depth - 1);
    });

    sig(3);
    // depth=3 → 2 → 1 → 0  (4 invocations)
    REQUIRE(total_calls == 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// 12–16: Type-erased payloads
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("signal<>: void signature", "[signal][payload]") {
    signal<> sig;
    int      calls = 0;
    auto     sub   = sig.connect([&] { ++calls; });
    sig();
    sig();
    REQUIRE(calls == 2);
}

TEST_CASE("signal<int>: primitive arg", "[signal][payload]") {
    signal<int> sig;
    int         received = 0;
    auto        sub      = sig.connect([&](int x) { received = x; });
    sig(123);
    REQUIRE(received == 123);
}

TEST_CASE("signal<const std::string&>: by-const-ref arg", "[signal][payload]") {
    signal<const std::string&> sig;
    std::string                received;
    const void*                seen_address = nullptr;

    auto sub = sig.connect([&](const std::string& s) {
        received     = s;
        seen_address = &s;
    });

    std::string source = "hello";
    sig(source);

    REQUIRE(received == "hello");
    REQUIRE(seen_address != nullptr);
}

TEST_CASE("signal<int, double, const std::string&>: multi-arg",
          "[signal][payload]") {
    signal<int, double, const std::string&> sig;
    int                                     i = 0;
    double                                  d = 0.0;
    std::string                             s;

    auto sub = sig.connect(
        [&](int a, double b, const std::string& c) {
            i = a;
            d = b;
            s = c;
        });

    sig(7, 3.14, std::string{"pi"});

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

TEST_CASE("signal<MyStruct>: user-defined type by value", "[signal][payload]") {
    signal<payload> sig;
    payload         received{};

    auto sub = sig.connect([&](payload p) { received = std::move(p); });
    sig(payload{.n = 9, .label = "nine"});

    REQUIRE(received.n == 9);
    REQUIRE(received.label == "nine");
}

// ─────────────────────────────────────────────────────────────────────────────
// 17–18: Concurrency
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Concurrent connect / emit / subscriber_count under thrash",
          "[signal][concurrency]") {
    signal<int>       sig;
    std::atomic<bool> stop{false};

    // Keep one always-on subscriber so emit always has work to do.
    std::atomic<int> baseline_calls{0};
    auto baseline_sub = sig.connect([&](int) { baseline_calls.fetch_add(1); });

    // Connector: continuously connects + drops subscriptions.
    std::thread connector([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto s = sig.connect([](int) {});
            // s drops at end of scope → auto-disconnect
        }
    });

    // Emitter: continuously fires.
    std::thread emitter([&] {
        int i = 0;
        while (!stop.load(std::memory_order_relaxed)) sig(i++);
    });

    // Counter: continuously queries subscriber_count.
    std::thread counter([&] {
        std::size_t total = 0;
        while (!stop.load(std::memory_order_relaxed))
            total += sig.subscriber_count();
        REQUIRE(total >= 0);  // exists only to prevent the loop being elided
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    stop.store(true, std::memory_order_relaxed);

    connector.join();
    emitter.join();
    counter.join();

    REQUIRE(baseline_calls.load() > 0);
}

TEST_CASE("Concurrent emit() from multiple threads", "[signal][concurrency]") {
    signal<int>      sig;
    std::atomic<int> calls{0};

    auto sub = sig.connect([&](int) { calls.fetch_add(1); });

    constexpr int    iterations = 5'000;
    std::thread      a([&] {
        for (int i = 0; i < iterations; ++i) sig(i);
    });
    std::thread      b([&] {
        for (int i = 0; i < iterations; ++i) sig(i);
    });

    a.join();
    b.join();

    REQUIRE(calls.load() == iterations * 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// 19: Compile-time behavior
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("connect() is [[nodiscard]] (signature check)",
          "[signal][compile_time]") {
    // The [[nodiscard]] attribute itself can only be observed via compiler
    // diagnostics — there is no portable trait for it. This test pins the
    // signature so any silent change to the return type is caught, and
    // documents the contract: discarding connect()'s return value MUST
    // produce a compiler diagnostic.
    signal<int> sig;
    using ReturnT = decltype(sig.connect([](int) {}));
    STATIC_REQUIRE(std::is_same_v<ReturnT, subscription>);

    // Sanity: subscription is move-only.
    STATIC_REQUIRE(std::is_move_constructible_v<subscription>);
    STATIC_REQUIRE(std::is_move_assignable_v<subscription>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<subscription>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<subscription>);

    // signal is pinned (neither copyable nor movable).
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<signal<int>>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<signal<int>>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<signal<int>>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<signal<int>>);
}
