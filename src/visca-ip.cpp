// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "visca-ip.hpp"
#include <algorithm>
#include <cmath>

ViscaIP::ViscaIP(const PTZConfig &cfg, QObject *parent) : PTZDevice(cfg, parent)
{
	transport_ = cfg.transport;
	addr_ = QHostAddress(cfg.host);

	if (transport_ == ViscaTransport::RawTCP) {
		tcp_.connectToHost(cfg.host, (quint16)cfg_.port);
	} else {
		udp_.bind(QHostAddress::AnyIPv4, 0);
		if (transport_ == ViscaTransport::SonyUDP)
			resetSequence();
	}
}

void ViscaIP::resetSequence()
{
	/* Sony VISCA-over-IP: reset the camera's sequence counter. */
	seq_ = 1;
	QByteArray ctrl = QByteArray::fromHex("0200000100000001");
	if (!addr_.isNull())
		udp_.writeDatagram(ctrl, addr_, (quint16)cfg_.port);
}

void ViscaIP::sendVisca(const QByteArray &payload, bool inquiry)
{
	switch (transport_) {
	case ViscaTransport::RawTCP:
		if (tcp_.state() == QAbstractSocket::UnconnectedState)
			tcp_.connectToHost(cfg_.host, (quint16)cfg_.port);
		tcp_.write(payload); /* raw VISCA, no header */
		tcp_.flush();
		return;
	case ViscaTransport::RawUDP:
		if (!addr_.isNull())
			udp_.writeDatagram(payload, addr_, (quint16)cfg_.port); /* raw, no header */
		return;
	case ViscaTransport::SonyUDP:
	default: {
		if (addr_.isNull())
			return;
		QByteArray p(8, '\0');
		p[0] = 0x01;
		p[1] = inquiry ? 0x10 : 0x00;
		p[2] = 0x00;
		p[3] = (char)payload.size();
		p[4] = (char)((seq_ >> 24) & 0xff);
		p[5] = (char)((seq_ >> 16) & 0xff);
		p[6] = (char)((seq_ >> 8) & 0xff);
		p[7] = (char)(seq_ & 0xff);
		seq_++;
		p += payload;
		udp_.writeDatagram(p, addr_, (quint16)cfg_.port);
		return;
	}
	}
}

static int scale(double v, int maxv)
{
	int s = (int)std::lround(std::fabs(v) * maxv);
	return std::clamp(s, 1, maxv);
}

void ViscaIP::pantilt(double pan, double tilt)
{
	if (cfg_.pan_invert)
		pan = -pan;
	if (cfg_.tilt_invert)
		tilt = -tilt;

	const int ps = scale(pan, pan_speed_max);
	const int ts = scale(tilt, tilt_speed_max);
	/* direction: pan 01=left 02=right 03=stop; tilt 01=up 02=down 03=stop */
	const uint8_t pd = pan < 0 ? 0x01 : pan > 0 ? 0x02 : 0x03;
	const uint8_t td = tilt > 0 ? 0x01 : tilt < 0 ? 0x02 : 0x03;

	QByteArray cmd;
	cmd.append((char)0x81).append((char)0x01).append((char)0x06).append((char)0x01);
	cmd.append((char)ps).append((char)ts).append((char)pd).append((char)td).append((char)0xff);
	sendVisca(cmd);
}

void ViscaIP::zoom(double speed)
{
	QByteArray cmd;
	cmd.append((char)0x81).append((char)0x01).append((char)0x04).append((char)0x07);
	if (speed > 0) /* tele / in */
		cmd.append((char)(0x20 | (scale(speed, zoom_speed_max) & 0x0f)));
	else if (speed < 0) /* wide / out */
		cmd.append((char)(0x30 | (scale(speed, zoom_speed_max) & 0x0f)));
	else
		cmd.append((char)0x00); /* stop */
	cmd.append((char)0xff);
	sendVisca(cmd);
}

void ViscaIP::focus(double speed)
{
	QByteArray cmd;
	cmd.append((char)0x81).append((char)0x01).append((char)0x04).append((char)0x08);
	if (speed > 0) /* far */
		cmd.append((char)(0x20 | (scale(speed, focus_speed_max) & 0x0f)));
	else if (speed < 0) /* near */
		cmd.append((char)(0x30 | (scale(speed, focus_speed_max) & 0x0f)));
	else
		cmd.append((char)0x00);
	cmd.append((char)0xff);
	sendVisca(cmd);
}

void ViscaIP::setAutofocus(bool on)
{
	QByteArray cmd = QByteArray::fromHex(on ? "8101043802ff" : "8101043803ff");
	sendVisca(cmd);
}

void ViscaIP::presetRecall(int index)
{
	QByteArray cmd = QByteArray::fromHex("8101043f0200ff");
	cmd[5] = (char)(index & 0x7f);
	sendVisca(cmd);
}

void ViscaIP::presetSet(int index)
{
	QByteArray cmd = QByteArray::fromHex("8101043f0100ff");
	cmd[5] = (char)(index & 0x7f);
	sendVisca(cmd);
}

void ViscaIP::presetClear(int index)
{
	QByteArray cmd = QByteArray::fromHex("8101043f0000ff"); /* memory reset */
	cmd[5] = (char)(index & 0x7f);
	sendVisca(cmd);
}

void ViscaIP::home()
{
	sendVisca(QByteArray::fromHex("81010604ff"));
}
