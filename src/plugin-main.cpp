// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include <obs-module.h>
#include <obs-frontend-api.h>
#include "ptz-manager.hpp"
#include "dock.hpp"
#include "version.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("ptz-controller", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Control PTZ cameras (VISCA-over-IP, NDI) with auto-detection.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "PTZ Controller";
}

bool obs_module_load(void)
{
	PtzManager::instance().load();

	auto *dock = new PtzControlsDock();
	obs_frontend_add_dock_by_id("ptz_controller_dock", obs_module_text("PTZController"), dock);

	blog(LOG_INFO, "[ptz-controller] loaded version %s", PROJECT_VERSION);
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[ptz-controller] unloaded");
}
