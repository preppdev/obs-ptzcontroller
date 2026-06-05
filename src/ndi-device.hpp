// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#pragma once

#include "ptz-device.hpp"
#include <QVector>

struct ProbeResult; // from ptz-probe.hpp

/* NDI PTZ device: control happens over a metadata-only NDI receiver connected
 * to the camera's NDI source (cfg.host = NDI source name). */
class NDIDevice : public PTZDevice {
	Q_OBJECT

public:
	explicit NDIDevice(const PTZConfig &cfg, QObject *parent = nullptr);
	~NDIDevice() override;

	bool valid() const { return recv_ != nullptr; }

	void pantilt(double pan, double tilt) override;
	void zoom(double speed) override;
	void focus(double speed) override;
	void setAutofocus(bool on) override;
	void presetRecall(int index) override;
	void presetSet(int index) override;
	void home() override;

private:
	void *recv_ = nullptr; // NDIlib_recv_instance_t
};

/* Discover NDI sources on the network (those that look like PTZ cameras).
 * Returns results tagged PTZProtocol::NDI with host = NDI source name. */
QVector<ProbeResult> ndi_discover(int wait_ms = 1200);
