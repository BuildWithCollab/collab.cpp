// Base exception type for collab libraries.
//
// Each library derives its own base from collab::core::error, and specific
// error types from that. The result is a catch hierarchy:
//
//   try { ... }
//   catch (const mylib::errors::specific& e) { /* leaf */ }
//   catch (const mylib::error& e)            { /* any mylib error */ }
//   catch (const collab::core::error& e)     { /* any collab error */ }
//   catch (const std::exception& e)          { /* anything */ }
//
// The same struct is also a valid std::expected<T, E> error type — no
// wrapping, no slicing. Throw it, or return it; the caller chooses per API.

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace collab::core {

struct error : std::runtime_error {
    using std::runtime_error::runtime_error;  // const char*, const std::string&

    explicit error(std::string_view msg)
        : std::runtime_error(std::string(msg)) {}

    template <typename... Args>
    explicit error(fmt::format_string<Args...> fmt_str, Args&&... args)
        : std::runtime_error(fmt::format(fmt_str, std::forward<Args>(args)...)) {}
};

}  // namespace collab::core
