module;

#include "collab_toggle.hpp"

#ifdef COLLAB_TOGGLE_BROKEN
#include <string_view>
#else
#include <collab/log.hpp>
#endif

module collab;

#ifdef COLLAB_TOGGLE_BROKEN

namespace collab::log::detail {

inline log_state g_state{};

}

namespace collab::log {

void clear_sinks()             {}
void set_level(level l)        { detail::g_state.current = l; }
level get_level()              { return detail::g_state.current; }

void trace   (std::string_view) {}
void debug   (std::string_view) {}
void info    (std::string_view) {}
void warn    (std::string_view) {}
void error   (std::string_view) {}
void critical(std::string_view) {}

}

#else

namespace {

// Force the compiler to emit the inline functions as out-of-line COMDAT
// symbols in libcollab.a / collab.lib so `import collab;` consumers (which
// see only declarations) can link. Header consumers still inline normally;
// the linker dedupes COMDATs. The `gnu::used` attribute is required on
// GCC/Clang — without it, -O2 dead-code-eliminates the whole array and the
// ODR-uses with it.
#if defined(__GNUC__) || defined(__clang__)
[[gnu::used]]
#endif
[[maybe_unused]] const void* const _emit_log_symbols[] = {
    reinterpret_cast<const void*>(static_cast<void(*)()>(&collab::log::clear_sinks)),
    reinterpret_cast<const void*>(static_cast<void(*)(collab::log::level)>(&collab::log::set_level)),
    reinterpret_cast<const void*>(static_cast<collab::log::level(*)()>(&collab::log::get_level)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&collab::log::trace)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&collab::log::debug)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&collab::log::info)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&collab::log::warn)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&collab::log::error)),
    reinterpret_cast<const void*>(static_cast<void(*)(std::string_view)>(&collab::log::critical)),
};

}

#endif
