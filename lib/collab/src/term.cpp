module;

#include <iostream>
#include <rang.hpp>

module collab;

import :term;

namespace collab::term {

std::ostream& operator<<(std::ostream& os, color c) {
    switch (c) {
        case color::black:   return os << rang::fg::black;
        case color::red:     return os << rang::fg::red;
        case color::green:   return os << rang::fg::green;
        case color::yellow:  return os << rang::fg::yellow;
        case color::blue:    return os << rang::fg::blue;
        case color::magenta: return os << rang::fg::magenta;
        case color::cyan:    return os << rang::fg::cyan;
        case color::gray:    return os << rang::fg::gray;
        case color::reset:   return os << rang::fg::reset;
        default:             return os;
    }
}

std::ostream& operator<<(std::ostream& os, style s) {
    switch (s) {
        case style::bold:      return os << rang::style::bold;
        case style::dim:       return os << rang::style::dim;
        case style::italic:    return os << rang::style::italic;
        case style::underline: return os << rang::style::underline;
        case style::blink:     return os << rang::style::blink;
        case style::reversed:  return os << rang::style::reversed;
        case style::crossed:   return os << rang::style::crossed;
        case style::reset:     return os << rang::style::reset;
        default:               return os;
    }
}

}  // namespace collab::term
