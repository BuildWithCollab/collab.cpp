module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

export module collab.core:event;

// ─────────────────────────────────────────────────────────────────────────────
// 📡 collab::core::event — multi-subscriber, thread-safe event/handler.
//
// Emission is `operator()`, not `emit()`. Qt's qtmetamacros.h defines `emit`
// as an empty preprocessor macro, which silently mangles any `evt.emit(args)`
// call in a TU that also includes a Qt header. Using call syntax sidesteps
// that collision entirely — `evt(args)` reads naturally and survives Qt.
//
// Threading contract
// ──────────────────
//   • connect(), operator(), disconnect(), and subscriber_count() are all
//     safe to call concurrently from any thread on the same event.
//   • operator() does not hold the internal lock while invoking handlers. It
//     snapshots the slot list under a shared lock, releases the lock, then
//     iterates. Reentrancy and recursive emission are deadlock-free.
//   • A handler invoked from operator() may freely call connect(),
//     disconnect(), or operator() (including on the same event).
//   • Disconnect during an in-flight emission affects subsequent emissions,
//     not the current one. Handlers already captured in the current snapshot
//     still fire — their slots are kept alive by the snapshot's shared_ptrs.
//   • A subscription may safely outlive its event. Disconnect becomes a
//     no-op once the event is destroyed. No use-after-free is possible.
//
// Convention
// ──────────
//   Only the class that owns the event invokes it. The type system does
//   not enforce this — code review and discipline do.
// ─────────────────────────────────────────────────────────────────────────────

namespace collab::core {

namespace detail {

struct slot_base {
    virtual ~slot_base() = default;
};

template <typename... Args>
struct slot final : slot_base {
    std::function<void(Args...)> handler;
    explicit slot(std::function<void(Args...)> h) : handler(std::move(h)) {}
};

struct event_control_block {
    std::shared_mutex                       mutex;
    std::vector<std::shared_ptr<slot_base>> slots;
};

}  // namespace detail

// 🎟 RAII subscription token. Move-only. Auto-disconnects on destruction.
// Safe to outlive its event — disconnect becomes a no-op then.
export class subscription {
public:
    subscription() noexcept = default;

    subscription(std::function<void()> disconnect_fn,
                 std::function<bool()> connected_fn) noexcept
        : disconnect_fn_{std::move(disconnect_fn)},
          connected_fn_{std::move(connected_fn)} {}

    subscription(subscription&& other) noexcept
        : disconnect_fn_{std::move(other.disconnect_fn_)},
          connected_fn_{std::move(other.connected_fn_)} {
        other.disconnect_fn_ = nullptr;
        other.connected_fn_  = nullptr;
    }

    subscription& operator=(subscription&& other) noexcept {
        if (this != &other) {
            disconnect();
            disconnect_fn_       = std::move(other.disconnect_fn_);
            connected_fn_        = std::move(other.connected_fn_);
            other.disconnect_fn_ = nullptr;
            other.connected_fn_  = nullptr;
        }
        return *this;
    }

    subscription(const subscription&)            = delete;
    subscription& operator=(const subscription&) = delete;

    ~subscription() { disconnect(); }

    void disconnect() noexcept {
        if (disconnect_fn_) {
            disconnect_fn_();
            disconnect_fn_ = nullptr;
            connected_fn_  = nullptr;
        }
    }

    bool connected() const noexcept {
        return static_cast<bool>(connected_fn_) && connected_fn_();
    }

private:
    std::function<void()> disconnect_fn_;
    std::function<bool()> connected_fn_;
};

// 📡 Multi-subscriber event. See top-of-file contract for semantics.
export template <typename... Args>
class event {
public:
    using handler = std::function<void(Args...)>;

    event() : control_{std::make_shared<detail::event_control_block>()} {}

    event(const event&)            = delete;
    event& operator=(const event&) = delete;
    event(event&&)                 = delete;
    event& operator=(event&&)      = delete;

    [[nodiscard]] subscription connect(handler fn) {
        auto slot_ptr =
            std::make_shared<detail::slot<Args...>>(std::move(fn));
        {
            std::unique_lock lock{control_->mutex};
            control_->slots.push_back(slot_ptr);
        }

        std::weak_ptr<detail::event_control_block> weak_ctrl = control_;
        std::weak_ptr<detail::slot_base>           weak_slot = slot_ptr;

        return subscription{
            [weak_ctrl, weak_slot]() {
                auto ctrl = weak_ctrl.lock();
                if (!ctrl) return;
                auto sp = weak_slot.lock();
                if (!sp) return;
                std::unique_lock lock{ctrl->mutex};
                auto&            v  = ctrl->slots;
                auto             it = std::find(v.begin(), v.end(), sp);
                if (it != v.end()) v.erase(it);
            },
            [weak_ctrl, weak_slot]() -> bool {
                auto ctrl = weak_ctrl.lock();
                if (!ctrl) return false;
                auto sp = weak_slot.lock();
                if (!sp) return false;
                std::shared_lock lock{ctrl->mutex};
                const auto&      v = ctrl->slots;
                return std::find(v.begin(), v.end(), sp) != v.end();
            }};
    }

    void operator()(Args... args) {
        // Local strong ref keeps the control block alive if `*this` is
        // destroyed on another thread mid-emission. Without it, the lock and
        // snapshot lines below would access a freed control block.
        auto ctrl = control_;
        std::vector<std::shared_ptr<detail::slot_base>> snapshot;
        {
            std::shared_lock lock{ctrl->mutex};
            snapshot = ctrl->slots;
        }
        for (auto& base : snapshot) {
            // Every slot in our control_ is slot<Args...> by construction.
            auto* typed = static_cast<detail::slot<Args...>*>(base.get());
            typed->handler(args...);
        }
    }

    std::size_t subscriber_count() const {
        std::shared_lock lock{control_->mutex};
        return control_->slots.size();
    }

private:
    std::shared_ptr<detail::event_control_block> control_;
};

}  // namespace collab::core
