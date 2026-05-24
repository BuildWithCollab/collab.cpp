module;

#include <collab/log.hpp>

module collab;

namespace {

// Force MSVC to emit the inline functions as out-of-line COMDAT symbols in
// collab.lib so `import collab;` consumers (which see only declarations) can
// link. Header consumers still inline normally; the linker dedupes COMDATs.
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
