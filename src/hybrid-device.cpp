// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "hybrid-device.hpp"
#include "ndi-device.hpp"
#include "visca-ip.hpp"

HybridDevice::HybridDevice(const PTZConfig &cfg, QObject *parent) : PTZDevice(cfg, parent)
{
	ndi_ = new NDIDevice(cfg, this);

	PTZConfig vc = cfg;
	vc.protocol = PTZProtocol::ViscaIP;
	vc.host = cfg.ccu_host;
	vc.transport = cfg.ccu_transport;
	vc.port = cfg.ccu_port > 0 ? cfg.ccu_port : visca_default_port(cfg.ccu_transport);
	ccu_ = new ViscaIP(vc, this);

	/* Surface the VISCA readback as this device's image state. */
	connect(ccu_, &PTZDevice::imageState, this, [this](const ImageState &s) { emit imageState(s); });
}

HybridDevice::~HybridDevice() = default;

bool HybridDevice::valid() const
{
	return ndi_ && ndi_->valid();
}

void HybridDevice::pantilt(double pan, double tilt)
{
	ndi_->pantilt(pan, tilt);
}
void HybridDevice::zoom(double speed)
{
	ndi_->zoom(speed);
}
void HybridDevice::focus(double speed)
{
	ndi_->focus(speed);
}
void HybridDevice::setAutofocus(bool on)
{
	ndi_->setAutofocus(on);
}
void HybridDevice::presetRecall(int index)
{
	ndi_->presetRecall(index);
}
void HybridDevice::presetSet(int index)
{
	ndi_->presetSet(index);
}
void HybridDevice::presetClear(int index)
{
	ndi_->presetClear(index);
}
void HybridDevice::home()
{
	ndi_->home();
}

void HybridDevice::setWhiteBalance(int mode)
{
	ccu_->setWhiteBalance(mode);
}
void HybridDevice::whiteBalanceTrigger()
{
	ccu_->whiteBalanceTrigger();
}
void HybridDevice::setRedGain(int v)
{
	ccu_->setRedGain(v);
}
void HybridDevice::setBlueGain(int v)
{
	ccu_->setBlueGain(v);
}
void HybridDevice::setExposureMode(int mode)
{
	ccu_->setExposureMode(mode);
}
void HybridDevice::stepShutter(int dir)
{
	ccu_->stepShutter(dir);
}
void HybridDevice::stepIris(int dir)
{
	ccu_->stepIris(dir);
}
void HybridDevice::stepGain(int dir)
{
	ccu_->stepGain(dir);
}
void HybridDevice::stepBright(int dir)
{
	ccu_->stepBright(dir);
}
void HybridDevice::setExposureComp(bool on)
{
	ccu_->setExposureComp(on);
}
void HybridDevice::setBacklight(bool on)
{
	ccu_->setBacklight(on);
}
void HybridDevice::requestImageState()
{
	ccu_->requestImageState();
}
