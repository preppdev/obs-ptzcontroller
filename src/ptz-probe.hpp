// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

/*
 * Auto-detect PTZ cameras and their control protocol. Rather than making the
 * user type host/port/protocol, the prober fires each protocol's harmless
 * identity query and reports what answers. Phase 1 detects VISCA-over-IP (UDP
 * :52381) via the VISCA version inquiry; the reply also carries vendor/model so
 * detected cameras get a sensible name. Designed to extend to VISCA-TCP / ONVIF
 * / NDI discovery.
 */
#pragma once

#include "ptz-device.hpp"
#include <QObject>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QTimer>
#include <QSet>
#include <QString>
#include <QList>

struct ProbeResult {
	QString host;
	int port = 52381;
	PTZProtocol protocol = PTZProtocol::ViscaIP;
	ViscaTransport transport = ViscaTransport::SonyUDP;
	QString model; // e.g. "VISCA camera (vendor 0020 / model 0513)"
};

class PtzProber : public QObject {
	Q_OBJECT

public:
	explicit PtzProber(QObject *parent = nullptr);

	/* Probe a single host for a supported protocol. */
	void probeHost(const QString &host);

	/* Scan the /24 around a sample host (e.g. "10.2.242.94" -> 10.2.242.1-254)
	 * for VISCA-over-IP cameras. Emits detected() per responder, finished()
	 * after the listen window. */
	void scanSubnet(const QString &sampleHost);

signals:
	void detected(ProbeResult result);
	void finished();

private slots:
	void onReadyRead();
	void onTimeout();
	void sendBatch();

private:
	void sendUdpProbes(const QString &host); // Sony :52381 + raw :1259
	void sendUdpProbesAddr(quint32 ipv4);
	void probeTcp(const QString &host);      // raw VISCA :5678 (single host)
	void report(const QString &host, int port, ViscaTransport t, const QByteArray &reply);

	QUdpSocket sock_;
	QTimer timer_;     // listen-window finish timer
	QTimer batch_;     // paces large subnet sweeps
	uint32_t seq_ = 1;
	QSet<QString> seen_;          // host:transport keys already reported
	QList<QTcpSocket *> tcpProbes_;

	/* Batched scan state (whole detected network, capped to /16). */
	quint32 scanBase_ = 0;  // first host address (host order)
	quint32 scanCount_ = 0; // number of hosts to probe
	quint32 scanIdx_ = 0;

	static constexpr int SONY_PORT = 52381;
	static constexpr int RAWUDP_PORT = 1259;
	static constexpr int RAWTCP_PORT = 5678;
};
