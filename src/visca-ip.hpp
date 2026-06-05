// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#pragma once

#include "ptz-device.hpp"
#include <QUdpSocket>
#include <QTcpSocket>
#include <QHostAddress>
#include <cstdint>

/* VISCA over IP. Supports three transports (see ViscaTransport):
 *   SonyUDP – 8-byte VISCA-over-IP header + sequence, UDP :52381 (Sony)
 *   RawUDP  – raw VISCA bytes, no header, UDP :1259 (PTZOptics & others)
 *   RawTCP  – raw VISCA bytes over a persistent TCP connection :5678 */
class ViscaIP : public PTZDevice {
	Q_OBJECT

public:
	explicit ViscaIP(const PTZConfig &cfg, QObject *parent = nullptr);

	void pantilt(double pan, double tilt) override;
	void zoom(double speed) override;
	void focus(double speed) override;
	void setAutofocus(bool on) override;
	void presetRecall(int index) override;
	void presetSet(int index) override;
	void presetClear(int index) override;
	void home() override;

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

	int pan_speed_max = 0x18;
	int tilt_speed_max = 0x14;
	int zoom_speed_max = 7;
	int focus_speed_max = 7;

private slots:
	void onUdpReadyRead();
	void onTcpReadyRead();

private:
	void sendVisca(const QByteArray &payload, bool inquiry = false);
	void resetSequence();
	void sendInquiry(int type, const char *hex);
	void handleReply(const QByteArray &visca);

	ViscaTransport transport_;
	QUdpSocket udp_;
	QTcpSocket tcp_;
	QHostAddress addr_;
	uint32_t seq_ = 1;

	QList<int> pendingInq_; // expected inquiry types, in order
	QByteArray tcpBuf_;
	ImageState acc_;
};
