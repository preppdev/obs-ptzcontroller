// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

/*
 * Abstract PTZ device. Concrete backends (VISCA-over-IP, NDI, …) implement the
 * control verbs. Devices live on the UI thread and use async Qt networking, so
 * control calls never block OBS.
 */
#pragma once

#include <QObject>
#include <QString>
#include <QMap>

enum class PTZProtocol {
	Unknown = 0,
	ViscaIP, // Sony VISCA over UDP (port 52381)
	NDI,     // NDI PTZ control (phase 2)
};

inline const char *ptz_protocol_id(PTZProtocol p)
{
	switch (p) {
	case PTZProtocol::ViscaIP:
		return "visca-over-ip";
	case PTZProtocol::NDI:
		return "ndi";
	default:
		return "unknown";
	}
}

inline PTZProtocol ptz_protocol_from_id(const QString &s)
{
	if (s == "visca-over-ip")
		return PTZProtocol::ViscaIP;
	if (s == "ndi")
		return PTZProtocol::NDI;
	return PTZProtocol::Unknown;
}

/* VISCA transport variants. Sony VISCA-over-IP wraps each command in an 8-byte
 * header on UDP :52381; PTZOptics and many others speak raw VISCA (no header)
 * over UDP :1259 or TCP :5678. */
enum class ViscaTransport { SonyUDP, RawUDP, RawTCP };

inline const char *visca_transport_id(ViscaTransport t)
{
	switch (t) {
	case ViscaTransport::RawUDP:
		return "raw-udp";
	case ViscaTransport::RawTCP:
		return "raw-tcp";
	default:
		return "sony-udp";
	}
}

inline ViscaTransport visca_transport_from_id(const QString &s)
{
	if (s == "raw-udp")
		return ViscaTransport::RawUDP;
	if (s == "raw-tcp")
		return ViscaTransport::RawTCP;
	return ViscaTransport::SonyUDP;
}

inline int visca_default_port(ViscaTransport t)
{
	switch (t) {
	case ViscaTransport::RawUDP:
		return 1259;
	case ViscaTransport::RawTCP:
		return 5678;
	default:
		return 52381;
	}
}

/* Current image/CCU values read back from the camera (-1 = unknown). */
struct ImageState {
	int wbMode = -1;
	int exposureMode = -1;
	int redGain = -1;
	int blueGain = -1;
	int shutter = -1;
	int iris = -1;
	int gain = -1;
};

struct PTZConfig {
	int id = 0;
	QString name;
	PTZProtocol protocol = PTZProtocol::ViscaIP;
	QString host;        // VISCA-IP host / NDI source name
	int port = 52381;    // VISCA-IP port
	ViscaTransport transport = ViscaTransport::SonyUDP;
	int preset_max = 16;            // number of preset slots
	bool pan_invert = false;
	bool tilt_invert = false;
	QMap<int, QString> preset_names; // index -> user label
};

class PTZDevice : public QObject {
	Q_OBJECT

public:
	explicit PTZDevice(const PTZConfig &cfg, QObject *parent = nullptr) : QObject(parent), cfg_(cfg) {}
	~PTZDevice() override = default;

	const PTZConfig &config() const { return cfg_; }
	void setName(const QString &n) { cfg_.name = n; }
	int id() const { return cfg_.id; }

	QString presetName(int i) const { return cfg_.preset_names.value(i); }
	void setPresetName(int i, const QString &n)
	{
		if (n.isEmpty())
			cfg_.preset_names.remove(i);
		else
			cfg_.preset_names[i] = n;
	}

	/* Continuous moves; speeds normalized to [-1, 1] (0 = stop that axis). */
	virtual void pantilt(double pan, double tilt) = 0;
	virtual void zoom(double speed) = 0;
	virtual void focus(double speed) = 0;
	virtual void stopAll()
	{
		pantilt(0, 0);
		zoom(0);
		focus(0);
	}

	virtual void setAutofocus(bool on) = 0;
	virtual void presetRecall(int index) = 0;
	virtual void presetSet(int index) = 0;
	virtual void presetClear(int index) { (void)index; } // optional
	virtual void home() = 0;

	/* ---- Image / CCU controls (optional; default no-ops) ---- */
	virtual bool hasImageControls() const { return false; }
	/* White balance: 0 Auto, 1 Indoor, 2 Outdoor, 3 One-Push, 4 Auto-Tracing, 5 Manual */
	virtual void setWhiteBalance(int mode) { (void)mode; }
	virtual void whiteBalanceTrigger() {}        // one-push trigger
	virtual void setRedGain(int v0_255) { (void)v0_255; }
	virtual void setBlueGain(int v0_255) { (void)v0_255; }
	/* Exposure mode: 0 Auto, 1 Manual, 2 Shutter-pri, 3 Iris-pri, 4 Bright */
	virtual void setExposureMode(int mode) { (void)mode; }
	virtual void stepShutter(int dir) { (void)dir; } // -1 down, +1 up, 0 reset
	virtual void stepIris(int dir) { (void)dir; }
	virtual void stepGain(int dir) { (void)dir; }
	virtual void stepBright(int dir) { (void)dir; }
	virtual void setExposureComp(bool on) { (void)on; }
	virtual void setBacklight(bool on) { (void)on; }

	/* Ask the device to report current image values; it emits imageState()
	 * when known. VISCA queries the camera; NDI reports last-set values. */
	virtual void requestImageState() {}

signals:
	void imageState(const ImageState &state);

protected:
	PTZConfig cfg_;
};
