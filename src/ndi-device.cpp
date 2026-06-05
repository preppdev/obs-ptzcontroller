// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "ndi-device.hpp"
#include "ndi-runtime.hpp"
#include "ptz-probe.hpp"

#include <cstddef> // NDI headers reference NULL
#include <Processing.NDI.Lib.h>
#include <algorithm>

NDIDevice::NDIDevice(const PTZConfig &cfg, QObject *parent) : PTZDevice(cfg, parent)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (!ndi)
		return;

	const QByteArray name = cfg.host.toUtf8();
	NDIlib_source_t src;
	src.p_ndi_name = name.constData();
	src.p_url_address = nullptr;

	NDIlib_recv_create_v3_t s;
	s.source_to_connect_to = src;
	s.bandwidth = NDIlib_recv_bandwidth_metadata_only; /* control only; no video */
	s.allow_video_fields = false;
	s.p_ndi_recv_name = "PTZ Controller";
	recv_ = ndi->recv_create_v3(&s);
}

NDIDevice::~NDIDevice()
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_destroy((NDIlib_recv_instance_t)recv_);
}

void NDIDevice::pantilt(double pan, double tilt)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (!ndi || !recv_)
		return;
	/* NDI's positive pan_speed moves the camera screen-left, which is the
	 * opposite of our dock convention (right button = +pan); negate so the
	 * controls match the on-screen direction. */
	pan = -pan;
	if (cfg_.pan_invert)
		pan = -pan;
	if (cfg_.tilt_invert)
		tilt = -tilt;
	ndi->recv_ptz_pan_tilt_speed((NDIlib_recv_instance_t)recv_, (float)std::clamp(pan, -1.0, 1.0),
				     (float)std::clamp(tilt, -1.0, 1.0));
}

void NDIDevice::zoom(double speed)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_zoom_speed((NDIlib_recv_instance_t)recv_, (float)std::clamp(speed, -1.0, 1.0));
}

void NDIDevice::focus(double speed)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (!ndi || !recv_)
		return;
	if (speed == 0.0)
		ndi->recv_ptz_focus_speed((NDIlib_recv_instance_t)recv_, 0.0f);
	else
		ndi->recv_ptz_focus_speed((NDIlib_recv_instance_t)recv_, (float)std::clamp(speed, -1.0, 1.0));
}

void NDIDevice::setAutofocus(bool on)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_ && on)
		ndi->recv_ptz_auto_focus((NDIlib_recv_instance_t)recv_);
	/* Manual focus is engaged implicitly by sending focus_speed commands. */
}

void NDIDevice::presetRecall(int index)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_recall_preset((NDIlib_recv_instance_t)recv_, index, 0.5f);
}

void NDIDevice::presetSet(int index)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_store_preset((NDIlib_recv_instance_t)recv_, index);
}

void NDIDevice::home()
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_pan_tilt((NDIlib_recv_instance_t)recv_, 0.0f, 0.0f); /* absolute center */
}

/* ---- Image / CCU (NDI subset) ---- */

void NDIDevice::applyManualWB()
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_white_balance_manual((NDIlib_recv_instance_t)recv_, red_, blue_);
}

void NDIDevice::applyManualExposure()
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_exposure_manual_v2((NDIlib_recv_instance_t)recv_, iris_, gain_, shutter_);
}

void NDIDevice::setWhiteBalance(int mode)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (!ndi || !recv_)
		return;
	wbMode_ = mode;
	auto r = (NDIlib_recv_instance_t)recv_;
	switch (mode) {
	case 1:
		ndi->recv_ptz_white_balance_indoor(r);
		break;
	case 2:
		ndi->recv_ptz_white_balance_outdoor(r);
		break;
	case 3:
		ndi->recv_ptz_white_balance_oneshot(r);
		break;
	case 5:
		applyManualWB();
		break;
	default:
		ndi->recv_ptz_white_balance_auto(r);
		break;
	}
}

void NDIDevice::whiteBalanceTrigger()
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (ndi && recv_)
		ndi->recv_ptz_white_balance_oneshot((NDIlib_recv_instance_t)recv_);
}

void NDIDevice::setRedGain(int v)
{
	red_ = std::clamp(v / 255.0f, 0.0f, 1.0f);
	if (wbMode_ == 5)
		applyManualWB();
}

void NDIDevice::setBlueGain(int v)
{
	blue_ = std::clamp(v / 255.0f, 0.0f, 1.0f);
	if (wbMode_ == 5)
		applyManualWB();
}

void NDIDevice::setExposureMode(int mode)
{
	const NDIlib_v5 *ndi = ndi_lib();
	if (!ndi || !recv_)
		return;
	expManual_ = (mode != 0);
	if (expManual_)
		applyManualExposure(); /* NDI has no shutter/iris priority; treat as manual */
	else
		ndi->recv_ptz_exposure_auto((NDIlib_recv_instance_t)recv_);
}

void NDIDevice::stepShutter(int dir)
{
	shutter_ = std::clamp(shutter_ + dir * 0.05f, 0.0f, 1.0f);
	expManual_ = true;
	applyManualExposure();
}

void NDIDevice::stepIris(int dir)
{
	iris_ = std::clamp(iris_ + dir * 0.05f, 0.0f, 1.0f);
	expManual_ = true;
	applyManualExposure();
}

void NDIDevice::stepGain(int dir)
{
	gain_ = std::clamp(gain_ + dir * 0.05f, 0.0f, 1.0f);
	expManual_ = true;
	applyManualExposure();
}

QVector<ProbeResult> ndi_discover(int wait_ms)
{
	QVector<ProbeResult> out;
	const NDIlib_v5 *ndi = ndi_lib();
	if (!ndi)
		return out;

	NDIlib_find_create_t fc;
	fc.show_local_sources = true;
	fc.p_groups = nullptr;
	fc.p_extra_ips = nullptr;
	NDIlib_find_instance_t find = ndi->find_create_v2(&fc);
	if (!find)
		return out;

	ndi->find_wait_for_sources(find, (uint32_t)wait_ms);
	uint32_t n = 0;
	const NDIlib_source_t *src = ndi->find_get_current_sources(find, &n);
	for (uint32_t i = 0; i < n; i++) {
		ProbeResult r;
		r.protocol = PTZProtocol::NDI;
		r.host = QString::fromUtf8(src[i].p_ndi_name);
		r.model = "NDI source";
		out.push_back(r);
	}
	ndi->find_destroy(find);
	return out;
}
