#pragma once

// Marker for test gates: present whenever <collab.hpp> has been included
// directly, so tests can distinguish "pure-import" mode from include/dual
// mode and apply the right conditional compilation.
#define COLLAB_HEADER_INCLUDED 1

#include <collab/error.hpp>
#include <collab/fixed_string.hpp>
#include <collab/identifier.hpp>
#include <collab/log.hpp>
#include <collab/manifest.hpp>
#include <collab/publisher.hpp>
#include <collab/semver.hpp>
#include <collab/term.hpp>
