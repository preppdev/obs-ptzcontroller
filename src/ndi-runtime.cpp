// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "ndi-runtime.hpp"

#include <obs-module.h>
#include <dlfcn.h>
#include <cstdlib>
#include <string>
#include <vector>

const NDIlib_v5 *ndi_lib()
{
	static const NDIlib_v5 *lib = nullptr;
	static bool tried = false;
	if (tried)
		return lib;
	tried = true;

	/* Candidate locations for the NDI runtime, in order of preference. */
	std::vector<std::string> candidates;
	for (const char *env : {"NDI_RUNTIME_DIR_V6", "NDI_RUNTIME_DIR_V5"}) {
		if (const char *dir = getenv(env))
			candidates.push_back(std::string(dir) + "/libndi.dylib");
	}
	candidates.push_back("/usr/local/lib/libndi.dylib");
	candidates.push_back("/Library/NDI SDK for Apple/lib/macOS/libndi.dylib");
	candidates.push_back("libndi.dylib"); /* rely on dyld search path */

	void *h = nullptr;
	for (const auto &path : candidates) {
		h = dlopen(path.c_str(), RTLD_LOCAL | RTLD_LAZY);
		if (h)
			break;
	}
	if (!h) {
		blog(LOG_INFO, "[ptz-controller] NDI runtime not found; NDI control disabled");
		return nullptr;
	}

	typedef const NDIlib_v5 *(*load_t)(void);
	auto load = (load_t)dlsym(h, "NDIlib_v5_load");
	if (!load) {
		blog(LOG_WARNING, "[ptz-controller] NDIlib_v5_load missing in NDI runtime");
		return nullptr;
	}
	lib = load();
	if (lib && !lib->initialize()) {
		blog(LOG_WARNING, "[ptz-controller] NDIlib initialize() failed");
		lib = nullptr;
	}
	if (lib)
		blog(LOG_INFO, "[ptz-controller] NDI runtime loaded");
	return lib;
}
