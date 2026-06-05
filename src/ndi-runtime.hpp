// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

/*
 * NDI runtime loader. The NDI library is loaded dynamically at runtime (dlopen
 * + NDIlib_v5_load) so the plugin has no hard link-time dependency on libndi —
 * it works if the NDI runtime is present, and degrades gracefully if not.
 */
#pragma once

#include <cstddef> // NDI headers reference NULL
#include <Processing.NDI.Lib.h>

/* Returns the loaded NDI v5 function table (initialized), or nullptr if the NDI
 * runtime isn't installed. Cached after first call. */
const NDIlib_v5 *ndi_lib();
