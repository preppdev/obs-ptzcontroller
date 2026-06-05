// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "ptz-probe.hpp"
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QNetworkInterface>

static const QByteArray kViscaInquiry = QByteArray::fromHex("81090002ff"); // version inquiry

PtzProber::PtzProber(QObject *parent) : QObject(parent)
{
	sock_.bind(QHostAddress::AnyIPv4, 0);
	connect(&sock_, &QUdpSocket::readyRead, this, &PtzProber::onReadyRead);
	timer_.setSingleShot(true);
	connect(&timer_, &QTimer::timeout, this, &PtzProber::onTimeout);
	connect(&batch_, &QTimer::timeout, this, &PtzProber::sendBatch);
}

void PtzProber::sendUdpProbesAddr(quint32 ipv4)
{
	sendUdpProbes(QHostAddress(ipv4).toString());
}

/* A VISCA reply byte (0x90 = reply from camera address 1) confirms a device,
 * whether it's a version reply (0x90 0x50 …) or an error (0x90 0x60 …). */
static bool isViscaReply(const QByteArray &visca)
{
	return !visca.isEmpty() && (uint8_t)visca[0] == 0x90;
}

void PtzProber::sendUdpProbes(const QString &host)
{
	QHostAddress addr(host);
	if (addr.isNull())
		return;

	/* Sony VISCA-over-IP: wrap the inquiry in an 8-byte header on :52381. */
	QByteArray sony(8, '\0');
	sony[0] = 0x01;
	sony[1] = 0x10;
	sony[3] = (char)kViscaInquiry.size();
	sony[7] = (char)(seq_ & 0xff);
	sony[6] = (char)((seq_ >> 8) & 0xff);
	seq_++;
	sony += kViscaInquiry;
	sock_.writeDatagram(sony, addr, SONY_PORT);

	/* Raw VISCA (no header) on :1259 — PTZOptics and many others. */
	sock_.writeDatagram(kViscaInquiry, addr, RAWUDP_PORT);
}

void PtzProber::probeTcp(const QString &host)
{
	auto *t = new QTcpSocket(this);
	tcpProbes_.append(t);
	connect(t, &QTcpSocket::connected, t, [t]() { t->write(kViscaInquiry); });
	connect(t, &QTcpSocket::readyRead, this, [this, t, host]() {
		const QByteArray d = t->readAll();
		if (isViscaReply(d))
			report(host, RAWTCP_PORT, ViscaTransport::RawTCP, d);
		t->disconnectFromHost();
	});
	t->connectToHost(host, RAWTCP_PORT);
}

void PtzProber::report(const QString &host, int port, ViscaTransport tr, const QByteArray &reply)
{
	const QString key = host + ":" + visca_transport_id(tr);
	if (seen_.contains(key))
		return;
	seen_.insert(key);

	ProbeResult r;
	r.host = host;
	r.port = port;
	r.protocol = PTZProtocol::ViscaIP;
	r.transport = tr;
	/* If it's a Sony version reply we can read vendor/model; otherwise label
	 * by transport. */
	const QByteArray visca = (tr == ViscaTransport::SonyUDP && reply.size() > 8) ? reply.mid(8) : reply;
	if (visca.size() >= 6 && (uint8_t)visca[0] == 0x90 && (uint8_t)visca[1] == 0x50) {
		const uint16_t vendor = ((uint8_t)visca[2] << 8) | (uint8_t)visca[3];
		const uint16_t model = ((uint8_t)visca[4] << 8) | (uint8_t)visca[5];
		r.model = QString::asprintf("VISCA camera (vendor %04x / model %04x)", vendor, model);
	} else {
		r.model = QString("VISCA camera (%1)").arg(visca_transport_id(tr));
	}
	emit detected(r);
}

void PtzProber::probeHost(const QString &host)
{
	seen_.clear();
	sendUdpProbes(host);
	probeTcp(host);
	timer_.start(1800);
}

/* Find the prefix length of the local interface whose network contains
 * `sampleIp`; default /24 if not found. */
static int prefixForHost(quint32 sampleIp)
{
	for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
		if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
			continue;
		for (const QNetworkAddressEntry &e : iface.addressEntries()) {
			if (e.ip().protocol() != QAbstractSocket::IPv4Protocol)
				continue;
			const int p = e.prefixLength();
			if (p <= 0 || p > 32)
				continue;
			const quint32 mask = (p == 0) ? 0u : (0xFFFFFFFFu << (32 - p));
			if ((e.ip().toIPv4Address() & mask) == (sampleIp & mask))
				return p;
		}
	}
	return 24;
}

void PtzProber::scanSubnet(const QString &sampleHost)
{
	seen_.clear();
	QHostAddress sample(sampleHost);
	if (sample.isNull() || sample.protocol() != QAbstractSocket::IPv4Protocol) {
		emit finished();
		return;
	}
	const quint32 ip = sample.toIPv4Address();
	int prefix = prefixForHost(ip);
	if (prefix < 16) /* cap huge sweeps to a /16 around the host */
		prefix = 16;
	const quint32 mask = (prefix == 0) ? 0u : (0xFFFFFFFFu << (32 - prefix));
	const quint32 network = ip & mask;
	const quint32 hosts = (prefix >= 31) ? 2u : ((1u << (32 - prefix)) - 2u);

	scanBase_ = network + 1;
	scanCount_ = hosts;
	scanIdx_ = 0;
	batch_.start(8); /* pace the sweep so we don't flood the socket buffer */
}

void PtzProber::sendBatch()
{
	const quint32 perTick = 768; /* hosts per 8ms tick */
	quint32 sent = 0;
	while (scanIdx_ < scanCount_ && sent < perTick) {
		sendUdpProbesAddr(scanBase_ + scanIdx_);
		scanIdx_++;
		sent++;
	}
	emit progress((int)scanIdx_, (int)scanCount_);
	if (scanIdx_ >= scanCount_) {
		batch_.stop();
		timer_.start(2000); /* listen window after the last probe */
	}
}

void PtzProber::onReadyRead()
{
	while (sock_.hasPendingDatagrams()) {
		QNetworkDatagram dg = sock_.receiveDatagram();
		const QByteArray d = dg.data();
		const QString host = dg.senderAddress().toString();
		const quint16 sport = dg.senderPort();
		if (sport == SONY_PORT) {
			if (d.size() >= 9 && (uint8_t)d[8] == 0x90)
				report(host, SONY_PORT, ViscaTransport::SonyUDP, d);
		} else if (sport == RAWUDP_PORT) {
			if (isViscaReply(d))
				report(host, RAWUDP_PORT, ViscaTransport::RawUDP, d);
		}
	}
}

void PtzProber::onTimeout()
{
	for (auto *t : tcpProbes_)
		t->deleteLater();
	tcpProbes_.clear();
	emit finished();
}
