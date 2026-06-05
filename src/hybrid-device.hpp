// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

/*
 * Hybrid device: motion (pan/tilt/zoom/focus/presets) over NDI, image/CCU
 * (white balance, exposure, gain, …) and readback over VISCA. Lets cameras
 * like OBSBOT — whose NDI implementation lacks CCU — get full control by
 * pairing their NDI source with their VISCA-over-IP endpoint.
 */
#pragma once

#include "ptz-device.hpp"

class NDIDevice;
class ViscaIP;

class HybridDevice : public PTZDevice {
	Q_OBJECT

public:
	explicit HybridDevice(const PTZConfig &cfg, QObject *parent = nullptr);
	~HybridDevice() override;

	bool valid() const; // motion (NDI) must be usable

	/* Motion → NDI */
	void pantilt(double pan, double tilt) override;
	void zoom(double speed) override;
	void focus(double speed) override;
	void setAutofocus(bool on) override;
	void presetRecall(int index) override;
	void presetSet(int index) override;
	void presetClear(int index) override;
	void home() override;

	/* Image/CCU → VISCA */
	bool hasImageControls() const override { return true; }
	void setWhiteBalance(int mode) override;
	void whiteBalanceTrigger() override;
	void setRedGain(int v) override;
	void setBlueGain(int v) override;
	void setExposureMode(int mode) override;
	void stepShutter(int dir) override;
	void stepIris(int dir) override;
	void stepGain(int dir) override;
	void stepBright(int dir) override;
	void setExposureComp(bool on) override;
	void setBacklight(bool on) override;
	void requestImageState() override;

private:
	NDIDevice *ndi_ = nullptr;
	ViscaIP *ccu_ = nullptr;
};
