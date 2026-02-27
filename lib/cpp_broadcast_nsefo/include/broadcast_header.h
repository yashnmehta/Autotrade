// DEPRECATED: This file is a legacy duplicate of protocol.h / nse_common.h.
//
// Use the following instead:
//   - BCAST_HEADER  →  #include "nse_common.h"   (used by all parsers)
//   - BroadcastHeader (protocol.h version) → #include "protocol.h"
//
// This header is kept to avoid breaking any old includes but should NOT be
// used in new code.

#ifndef BROADCAST_HEADER_H_LEGACY
#define BROADCAST_HEADER_H_LEGACY

#pragma message("broadcast_header.h is deprecated – include nse_common.h for BCAST_HEADER instead")

// Re-export via nse_common.h to avoid the duplicate definition
#include "nse_common.h"

#endif // BROADCAST_HEADER_H_LEGACY
