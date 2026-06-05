// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "dock.hpp"
#include "ptz-manager.hpp"
#include "ptz-device.hpp"
#include "ptz-probe.hpp"
#include "ndi-device.hpp"

#include <obs-module.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QGroupBox>
#include <QMenu>
#include <QInputDialog>
#include <QCheckBox>
#include <QNetworkInterface>
#include <QHostAddress>
#include <functional>

/* Compact button style so labels render predictably (the native macOS bezel
 * clips small labels and adds heavy padding). */
static const char *kCellStyle =
	"QPushButton{min-width:26px;min-height:26px;padding:2px 6px;border:1px solid #4a4f57;"
	"border-radius:4px;background:#2c2f36;color:#e6e6e6;}"
	"QPushButton:hover{background:#3a3f48;}"
	"QPushButton:checked{background:#2d6cdf;border-color:#2d6cdf;}"
	"QPushButton:pressed{background:#1f2228;}";

static QPushButton *holdButton(const QString &label, std::function<void()> onPress, std::function<void()> onRelease)
{
	auto *b = new QPushButton(label);
	b->setFocusPolicy(Qt::NoFocus);
	QObject::connect(b, &QPushButton::pressed, b, [onPress]() { onPress(); });
	QObject::connect(b, &QPushButton::released, b, [onRelease]() { onRelease(); });
	return b;
}

static QString localIPv4()
{
	for (const QHostAddress &a : QNetworkInterface::allAddresses())
		if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
			return a.toString();
	return "192.168.1.10";
}

PtzControlsDock::PtzControlsDock(QWidget *parent) : QFrame(parent)
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	outer->addWidget(scroll);

	auto *content = new QWidget();
	scroll->setWidget(content);
	auto *root = new QVBoxLayout(content);
	root->setContentsMargins(6, 6, 6, 6);

	/* --- Top: camera quick-select bar + settings gear --- */
	auto *top = new QHBoxLayout();
	camBar_ = new QWidget(content);
	camLayout_ = new QHBoxLayout(camBar_);
	camLayout_->setContentsMargins(0, 0, 0, 0);
	camLayout_->setSpacing(3);
	camGroup_ = new QButtonGroup(this);
	camGroup_->setExclusive(true);
	top->addWidget(camBar_, 1);
	auto *gear = new QPushButton("⚙", content);
	gear->setFixedWidth(34);
	gear->setFocusPolicy(Qt::NoFocus);
	gear->setToolTip(obs_module_text("Settings"));
	connect(gear, &QPushButton::clicked, this, &PtzControlsDock::onSettings);
	top->addWidget(gear);
	root->addLayout(top);

	/* --- Pan/Tilt pad + zoom/focus --- */
	auto *mid = new QHBoxLayout();
	auto *pad = new QGridLayout();
	pad->setSpacing(3);
	struct Dir {
		int r, c;
		double dx, dy;
		const char *glyph;
	};
	const Dir dirs[] = {{0, 0, -1, 1, "↖"}, {0, 1, 0, 1, "↑"},  {0, 2, 1, 1, "↗"}, {1, 0, -1, 0, "←"},
			    {1, 2, 1, 0, "→"},  {2, 0, -1, -1, "↙"}, {2, 1, 0, -1, "↓"}, {2, 2, 1, -1, "↘"}};
	for (const Dir &d : dirs) {
		auto *b = holdButton(
			d.glyph,
			[this, d]() {
				if (auto *dev = current())
					dev->pantilt(d.dx * speed(), d.dy * speed());
			},
			[this]() {
				if (auto *dev = current())
					dev->pantilt(0, 0);
			});
		b->setFixedSize(44, 40);
		pad->addWidget(b, d.r, d.c);
	}
	auto *homeBtn = new QPushButton("⌂", content);
	homeBtn->setFixedSize(44, 40);
	homeBtn->setFocusPolicy(Qt::NoFocus);
	connect(homeBtn, &QPushButton::clicked, this, [this]() {
		if (auto *dev = current())
			dev->home();
	});
	pad->addWidget(homeBtn, 1, 1);
	mid->addLayout(pad);
	mid->addStretch();

	auto *zf = new QGridLayout();
	zf->setSpacing(3);
	zf->addWidget(new QLabel(obs_module_text("Zoom")), 0, 0);
	zf->addWidget(holdButton(
			      "＋", [this]() { if (auto *d = current()) d->zoom(speed()); },
			      [this]() { if (auto *d = current()) d->zoom(0); }),
		      0, 1);
	zf->addWidget(holdButton(
			      "－", [this]() { if (auto *d = current()) d->zoom(-speed()); },
			      [this]() { if (auto *d = current()) d->zoom(0); }),
		      0, 2);
	zf->addWidget(new QLabel(obs_module_text("Focus")), 1, 0);
	zf->addWidget(holdButton(
			      obs_module_text("Far"), [this]() { if (auto *d = current()) d->focus(speed()); },
			      [this]() { if (auto *d = current()) d->focus(0); }),
		      1, 1);
	zf->addWidget(holdButton(
			      obs_module_text("Near"), [this]() { if (auto *d = current()) d->focus(-speed()); },
			      [this]() { if (auto *d = current()) d->focus(0); }),
		      1, 2);
	auto *af = new QPushButton(obs_module_text("AutoFocus"), content);
	af->setCheckable(true);
	af->setChecked(true);
	af->setFocusPolicy(Qt::NoFocus);
	connect(af, &QPushButton::toggled, this, [this](bool on) {
		if (auto *d = current())
			d->setAutofocus(on);
	});
	zf->addWidget(af, 2, 0, 1, 3);
	mid->addLayout(zf);
	root->addLayout(mid);

	/* --- Speed --- */
	auto *spd = new QHBoxLayout();
	spd->addWidget(new QLabel(obs_module_text("Speed")));
	speed_ = new QSlider(Qt::Horizontal, content);
	speed_->setRange(1, 100);
	speed_->setValue(60);
	speed_->setFocusPolicy(Qt::NoFocus);
	spd->addWidget(speed_, 1);
	root->addLayout(spd);

	/* --- Presets (accordion) --- */
	presetToggle_ = new QPushButton(content);
	presetToggle_->setFlat(true);
	presetToggle_->setStyleSheet("text-align:left; font-weight:bold; padding:4px;");
	presetToggle_->setFocusPolicy(Qt::NoFocus);
	connect(presetToggle_, &QPushButton::clicked, this, &PtzControlsDock::togglePresets);
	root->addWidget(presetToggle_);

	presetPanel_ = new QWidget(content);
	presetGrid_ = new QGridLayout(presetPanel_);
	presetGrid_->setSpacing(4);
	root->addWidget(presetPanel_);

	/* --- Image / CCU (accordion, collapsed by default) --- */
	imageToggle_ = new QPushButton(content);
	imageToggle_->setFlat(true);
	imageToggle_->setStyleSheet("text-align:left; font-weight:bold; padding:4px;");
	imageToggle_->setFocusPolicy(Qt::NoFocus);
	connect(imageToggle_, &QPushButton::clicked, this, &PtzControlsDock::toggleImage);
	root->addWidget(imageToggle_);

	imagePanel_ = new QWidget(content);
	auto *img = new QFormLayout(imagePanel_);
	img->setContentsMargins(2, 2, 2, 2);
	wbCombo_ = new QComboBox(imagePanel_);
	wbCombo_->addItems({obs_module_text("WB.Auto"), obs_module_text("WB.Indoor"), obs_module_text("WB.Outdoor"),
			    obs_module_text("WB.OnePush"), obs_module_text("WB.ATW"), obs_module_text("WB.Manual")});
	connect(wbCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int m) {
		if (auto *d = current())
			d->setWhiteBalance(m);
	});
	img->addRow(obs_module_text("WhiteBalance"), wbCombo_);
	expCombo_ = new QComboBox(imagePanel_);
	expCombo_->addItems({obs_module_text("Exp.Auto"), obs_module_text("Exp.Manual"), obs_module_text("Exp.ShutterPri"),
			     obs_module_text("Exp.IrisPri"), obs_module_text("Exp.Bright")});
	connect(expCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int m) {
		if (auto *d = current())
			d->setExposureMode(m);
	});
	img->addRow(obs_module_text("Exposure"), expCombo_);
	ccuSummary_ = new QLabel(imagePanel_);
	ccuSummary_->setStyleSheet("color:#bbb;");
	img->addRow(obs_module_text("Values"), ccuSummary_);
	auto *moreBtn = new QPushButton(obs_module_text("MoreControls"), imagePanel_);
	moreBtn->setFocusPolicy(Qt::NoFocus);
	connect(moreBtn, &QPushButton::clicked, this, &PtzControlsDock::openCcuWindow);
	img->addRow(QString(), moreBtn);
	imagePanel_->setVisible(false);
	root->addWidget(imagePanel_);

	root->addStretch();

	connect(&PtzManager::instance(), &PtzManager::devicesChanged, this, &PtzControlsDock::refreshCameras);
	connect(&PtzManager::instance(), &PtzManager::currentChanged, this, [this](int) { refreshCameras(); });

	refreshCameras();
}

PTZDevice *PtzControlsDock::current() const
{
	return PtzManager::instance().current();
}

double PtzControlsDock::speed() const
{
	return speed_->value() / 100.0;
}

void PtzControlsDock::buildCameraBar()
{
	/* Clear existing camera buttons. */
	for (auto *b : camGroup_->buttons()) {
		camGroup_->removeButton(b);
		delete b;
	}
	QLayoutItem *it;
	while ((it = camLayout_->takeAt(0)) != nullptr) {
		delete it->widget();
		delete it;
	}

	const int curId = PtzManager::instance().currentId();
	for (auto *d : PtzManager::instance().devices()) {
		const int id = d->id();
		auto *b = new QPushButton(d->config().name.isEmpty() ? d->config().host : d->config().name, camBar_);
		b->setStyleSheet(kCellStyle);
		b->setCheckable(true);
		b->setChecked(id == curId);
		b->setFocusPolicy(Qt::NoFocus);
		b->setContextMenuPolicy(Qt::CustomContextMenu);
		camGroup_->addButton(b);
		camLayout_->addWidget(b);
		connect(b, &QPushButton::clicked, this, [id]() { PtzManager::instance().setCurrent(id); });
		connect(b, &QPushButton::customContextMenuRequested, this, [this, id, b](const QPoint &) {
			QMenu m;
			if (m.addAction(obs_module_text("Rename")) == m.exec(b->mapToGlobal(QPoint(0, b->height())))) {
				bool ok = false;
				QString n = QInputDialog::getText(this, obs_module_text("Rename"), obs_module_text("Name"),
								  QLineEdit::Normal,
								  PtzManager::instance().device(id)
									  ? PtzManager::instance().device(id)->config().name
									  : QString(),
								  &ok);
				if (ok)
					PtzManager::instance().renameDevice(id, n);
			}
		});
	}
	camLayout_->addStretch();
}

void PtzControlsDock::refreshCameras()
{
	buildCameraBar();
	auto *d = current();
	buildPresetBank(d ? d->config().preset_max : 0);

	const bool img = d && d->hasImageControls();
	imageToggle_->setVisible(img);
	imageToggle_->setText((imageCollapsed_ ? "▶ " : "▼ ") + QString(obs_module_text("Image")));
	imagePanel_->setVisible(img && !imageCollapsed_);

	/* Track the current device's image-state readback for the inline display. */
	disconnect(imageConn_);
	ccuSummary_->setText("—");
	if (d && d->hasImageControls()) {
		imageConn_ = connect(d, &PTZDevice::imageState, this, &PtzControlsDock::updateImageInline);
		if (!imageCollapsed_)
			d->requestImageState();
	}
}

void PtzControlsDock::updateImageInline(const ImageState &st)
{
	if (st.wbMode >= 0 && st.wbMode < wbCombo_->count())
		wbCombo_->setCurrentIndex(st.wbMode);
	if (st.exposureMode >= 0 && st.exposureMode < expCombo_->count())
		expCombo_->setCurrentIndex(st.exposureMode);
	auto vs = [](int v) { return v >= 0 ? QString::number(v) : QStringLiteral("—"); };
	ccuSummary_->setText(QString("R %1  B %2   Sh %3  Ir %4  Gn %5")
				     .arg(vs(st.redGain), vs(st.blueGain), vs(st.shutter), vs(st.iris), vs(st.gain)));
}

void PtzControlsDock::toggleImage()
{
	imageCollapsed_ = !imageCollapsed_;
	imageToggle_->setText((imageCollapsed_ ? "▶ " : "▼ ") + QString(obs_module_text("Image")));
	imagePanel_->setVisible(!imageCollapsed_);
	if (!imageCollapsed_) {
		if (auto *d = current())
			d->requestImageState(); /* refresh values when opened */
	}
}

void PtzControlsDock::buildPresetBank(int count)
{
	QLayoutItem *item;
	while ((item = presetGrid_->takeAt(0)) != nullptr) {
		delete item->widget();
		delete item;
	}
	presetToggle_->setText((presetsCollapsed_ ? "▶ " : "▼ ") + QString(obs_module_text("Presets")));
	presetPanel_->setVisible(!presetsCollapsed_);

	auto *d = current();
	const int cols = 8;
	for (int i = 0; i < count; i++) {
		const int idx = i + 1;
		auto *cell = new QWidget(presetPanel_);
		auto *cv = new QVBoxLayout(cell);
		cv->setContentsMargins(0, 0, 0, 0);
		cv->setSpacing(1);

		auto *b = new QPushButton(QString::number(idx), cell);
		b->setStyleSheet(kCellStyle);
		b->setFocusPolicy(Qt::NoFocus);
		b->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(b, &QPushButton::clicked, this, [this, idx]() { onPresetClicked(idx); });
		connect(b, &QPushButton::customContextMenuRequested, this,
			[this, idx, b](const QPoint &) { presetContextMenu(idx, b); });
		cv->addWidget(b, 0, Qt::AlignHCenter);

		auto *lbl = new QLabel(d ? d->presetName(idx) : QString(), cell);
		lbl->setStyleSheet("color:#aaa; font-size:10px;");
		lbl->setAlignment(Qt::AlignHCenter);
		lbl->setVisible(!lbl->text().isEmpty());
		cv->addWidget(lbl, 0, Qt::AlignHCenter);

		presetGrid_->addWidget(cell, i / cols, i % cols);
	}
}

void PtzControlsDock::togglePresets()
{
	presetsCollapsed_ = !presetsCollapsed_;
	presetToggle_->setText((presetsCollapsed_ ? "▶ " : "▼ ") + QString(obs_module_text("Presets")));
	presetPanel_->setVisible(!presetsCollapsed_);
}

void PtzControlsDock::onPresetClicked(int index)
{
	if (auto *d = current())
		d->presetRecall(index);
}

void PtzControlsDock::presetContextMenu(int index, QWidget *anchor)
{
	auto *d = current();
	if (!d)
		return;
	QMenu m;
	QAction *save = m.addAction(obs_module_text("PresetSave"));
	QAction *rename = m.addAction(obs_module_text("PresetRename"));
	QAction *clear = m.addAction(obs_module_text("PresetClear"));
	QAction *chosen = m.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
	if (chosen == save) {
		d->presetSet(index);
	} else if (chosen == rename) {
		bool ok = false;
		QString n = QInputDialog::getText(this, obs_module_text("PresetRename"), obs_module_text("Name"),
						  QLineEdit::Normal, d->presetName(index), &ok);
		if (ok) {
			d->setPresetName(index, n);
			PtzManager::instance().save();
			buildPresetBank(d->config().preset_max);
		}
	} else if (chosen == clear) {
		d->presetClear(index);
		d->setPresetName(index, QString());
		PtzManager::instance().save();
		buildPresetBank(d->config().preset_max);
	}
}

/* ---- Settings dialog: camera management ---- */

void PtzControlsDock::onSettings()
{
	QDialog dlg(this);
	dlg.setWindowTitle(obs_module_text("Settings"));
	auto *lay = new QVBoxLayout(&dlg);

	auto *box = new QGroupBox(obs_module_text("Cameras"), &dlg);
	auto *bl = new QVBoxLayout(box);
	auto *list = new QListWidget(&dlg);
	list->setMinimumHeight(140);
	bl->addWidget(list);

	auto rebuild = [&]() {
		list->clear();
		for (auto *d : PtzManager::instance().devices()) {
			const auto &c = d->config();
			QString proto = (c.protocol == PTZProtocol::NDI) ? "NDI" : visca_transport_id(c.transport);
			auto *it = new QListWidgetItem(QString("%1   —   %2  (%3)").arg(c.name, c.host, proto));
			it->setData(Qt::UserRole, c.id);
			list->addItem(it);
		}
	};
	rebuild();

	auto *btns = new QHBoxLayout();
	auto *addB = new QPushButton(obs_module_text("AddCamera"), &dlg);
	auto *editB = new QPushButton(obs_module_text("EditCamera"), &dlg);
	auto *renB = new QPushButton(obs_module_text("Rename"), &dlg);
	auto *delB = new QPushButton(obs_module_text("Remove"), &dlg);
	btns->addWidget(addB);
	btns->addWidget(editB);
	btns->addWidget(renB);
	btns->addWidget(delB);
	btns->addStretch();
	bl->addLayout(btns);
	lay->addWidget(box);

	auto selId = [&]() -> int {
		auto *it = list->currentItem();
		return it ? it->data(Qt::UserRole).toInt() : -1;
	};
	connect(addB, &QPushButton::clicked, &dlg, [&]() {
		addCameraDialog();
		rebuild();
	});
	connect(editB, &QPushButton::clicked, &dlg, [&]() {
		int id = selId();
		if (id > 0) {
			editCameraDialog(id);
			rebuild();
		}
	});
	connect(renB, &QPushButton::clicked, &dlg, [&]() {
		int id = selId();
		auto *d = PtzManager::instance().device(id);
		if (!d)
			return;
		bool ok = false;
		QString n = QInputDialog::getText(&dlg, obs_module_text("Rename"), obs_module_text("Name"),
						  QLineEdit::Normal, d->config().name, &ok);
		if (ok) {
			PtzManager::instance().renameDevice(id, n);
			rebuild();
		}
	});
	connect(delB, &QPushButton::clicked, &dlg, [&]() {
		int id = selId();
		if (id > 0) {
			PtzManager::instance().removeDevice(id);
			rebuild();
		}
	});

	auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
	connect(close, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
	connect(close, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	lay->addWidget(close);

	dlg.exec();
	refreshCameras();
}

/* ---- Add-camera dialog (auto-detect first) ---- */

void PtzControlsDock::addCameraDialog()
{
	QDialog dlg(this);
	dlg.setWindowTitle(obs_module_text("AddCamera"));
	auto *lay = new QVBoxLayout(&dlg);

	auto *detectBox = new QGroupBox(obs_module_text("AutoDetect"), &dlg);
	auto *dv = new QVBoxLayout(detectBox);
	auto *hostRow = new QHBoxLayout();
	auto *hostEdit = new QLineEdit(localIPv4(), &dlg);
	auto *probeBtn = new QPushButton(obs_module_text("ProbeHost"), &dlg);
	auto *scanBtn = new QPushButton(obs_module_text("ScanSubnet"), &dlg);
	auto *ndiBtn = new QPushButton(obs_module_text("ScanNDI"), &dlg);
	hostRow->addWidget(hostEdit, 1);
	hostRow->addWidget(probeBtn);
	hostRow->addWidget(scanBtn);
	hostRow->addWidget(ndiBtn);
	dv->addLayout(hostRow);
	auto *results = new QListWidget(&dlg);
	results->setMinimumHeight(120);
	dv->addWidget(results);
	auto *status = new QLabel(&dlg);
	status->setStyleSheet("color:#888;");
	dv->addWidget(status);
	lay->addWidget(detectBox);

	auto *prober = new PtzProber(&dlg);
	auto setBusy = [&](bool busy, const QString &msg) {
		probeBtn->setEnabled(!busy);
		scanBtn->setEnabled(!busy);
		ndiBtn->setEnabled(!busy);
		status->setText(msg);
	};
	auto addResult = [results](const ProbeResult &r) {
		auto *it = new QListWidgetItem(QString("%1  —  %2").arg(r.host, r.model));
		it->setData(Qt::UserRole + 0, r.host);
		it->setData(Qt::UserRole + 1, r.port);
		it->setData(Qt::UserRole + 2, (int)r.protocol);
		it->setData(Qt::UserRole + 3, (int)r.transport);
		results->addItem(it);
	};
	connect(prober, &PtzProber::detected, &dlg, [addResult](ProbeResult r) { addResult(r); });
	connect(prober, &PtzProber::finished, &dlg, [&]() {
		setBusy(false, results->count() ? obs_module_text("ScanDone") : obs_module_text("NoneFound"));
	});
	connect(probeBtn, &QPushButton::clicked, &dlg, [&]() {
		results->clear();
		setBusy(true, obs_module_text("Probing"));
		prober->probeHost(hostEdit->text().trimmed());
	});
	connect(scanBtn, &QPushButton::clicked, &dlg, [&]() {
		results->clear();
		setBusy(true, obs_module_text("Scanning"));
		prober->scanSubnet(hostEdit->text().trimmed());
	});
	connect(ndiBtn, &QPushButton::clicked, &dlg, [&]() {
		results->clear();
		setBusy(true, obs_module_text("ScanningNDI"));
		const QVector<ProbeResult> found = ndi_discover();
		for (const ProbeResult &r : found)
			addResult(r);
		setBusy(false, found.isEmpty() ? obs_module_text("NoneFound") : obs_module_text("ScanDone"));
	});

	auto *manualBox = new QGroupBox(obs_module_text("Manual"), &dlg);
	auto *mf = new QFormLayout(manualBox);
	auto *nameEdit = new QLineEdit(&dlg);
	auto *protoCombo = new QComboBox(&dlg);
	protoCombo->addItem("VISCA over IP", (int)PTZProtocol::ViscaIP);
	protoCombo->addItem("NDI", (int)PTZProtocol::NDI);
	auto *portSpin = new QSpinBox(&dlg);
	portSpin->setRange(1, 65535);
	portSpin->setValue(52381);
	auto *ccuEdit = new QLineEdit(&dlg);
	ccuEdit->setPlaceholderText(obs_module_text("CcuHostHint"));
	mf->addRow(obs_module_text("Name"), nameEdit);
	mf->addRow(obs_module_text("Protocol"), protoCombo);
	mf->addRow(obs_module_text("Port"), portSpin);
	mf->addRow(obs_module_text("CcuHost"), ccuEdit);
	lay->addWidget(manualBox);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	lay->addWidget(buttons);

	if (dlg.exec() != QDialog::Accepted)
		return;

	PTZConfig cfg;
	QListWidgetItem *sel = results->currentItem();
	if (sel) {
		cfg.host = sel->data(Qt::UserRole + 0).toString();
		cfg.port = sel->data(Qt::UserRole + 1).toInt();
		cfg.protocol = (PTZProtocol)sel->data(Qt::UserRole + 2).toInt();
		cfg.transport = (ViscaTransport)sel->data(Qt::UserRole + 3).toInt();
		cfg.name = nameEdit->text().isEmpty() ? cfg.host : nameEdit->text();
	} else {
		cfg.host = hostEdit->text().trimmed();
		cfg.protocol = (PTZProtocol)protoCombo->currentData().toInt();
		cfg.transport = ViscaTransport::SonyUDP;
		cfg.port = portSpin->value();
		cfg.name = nameEdit->text().isEmpty() ? cfg.host : nameEdit->text();
	}
	/* Optional VISCA CCU host pairs with an NDI camera for hybrid control. */
	const QString ccu = ccuEdit->text().trimmed();
	if (!ccu.isEmpty()) {
		cfg.ccu_host = ccu;
		cfg.ccu_transport = ViscaTransport::SonyUDP; /* OBSBOT etc. use Sony VISCA-over-IP :52381 */
		cfg.ccu_port = 52381;
	}

	if (cfg.host.isEmpty())
		return;
	if (PTZDevice *d = PtzManager::instance().addDevice(cfg))
		PtzManager::instance().setCurrent(d->id());
}

/* ---- Edit camera ---- */

static QComboBox *transportCombo(QWidget *parent, ViscaTransport sel)
{
	auto *c = new QComboBox(parent);
	c->addItem("Sony VISCA-over-IP (UDP :52381)", (int)ViscaTransport::SonyUDP);
	c->addItem("Raw VISCA (UDP :1259)", (int)ViscaTransport::RawUDP);
	c->addItem("Raw VISCA (TCP :5678)", (int)ViscaTransport::RawTCP);
	c->setCurrentIndex(c->findData((int)sel));
	return c;
}

void PtzControlsDock::editCameraDialog(int id)
{
	PTZDevice *d = PtzManager::instance().device(id);
	if (!d)
		return;
	const PTZConfig c = d->config();

	QDialog dlg(this);
	dlg.setWindowTitle(obs_module_text("EditCamera"));
	auto *form = new QFormLayout(&dlg);

	auto *nameEdit = new QLineEdit(c.name, &dlg);
	auto *protoCombo = new QComboBox(&dlg);
	protoCombo->addItem("VISCA over IP", (int)PTZProtocol::ViscaIP);
	protoCombo->addItem("NDI", (int)PTZProtocol::NDI);
	protoCombo->setCurrentIndex(protoCombo->findData((int)c.protocol));
	auto *hostEdit = new QLineEdit(c.host, &dlg);
	auto *portSpin = new QSpinBox(&dlg);
	portSpin->setRange(1, 65535);
	portSpin->setValue(c.port > 0 ? c.port : 52381);
	auto *trans = transportCombo(&dlg, c.transport);
	auto *ccuEdit = new QLineEdit(c.ccu_host, &dlg);
	auto *ccuTrans = transportCombo(&dlg, c.ccu_transport);
	auto *panInv = new QCheckBox(obs_module_text("InvertPan"), &dlg);
	panInv->setChecked(c.pan_invert);
	auto *tiltInv = new QCheckBox(obs_module_text("InvertTilt"), &dlg);
	tiltInv->setChecked(c.tilt_invert);

	form->addRow(obs_module_text("Name"), nameEdit);
	form->addRow(obs_module_text("Protocol"), protoCombo);
	form->addRow(obs_module_text("Host"), hostEdit);
	form->addRow(obs_module_text("Port"), portSpin);
	form->addRow(obs_module_text("Transport"), trans);
	form->addRow(obs_module_text("CcuHost"), ccuEdit);
	form->addRow(obs_module_text("CcuTransport"), ccuTrans);
	form->addRow(QString(), panInv);
	form->addRow(QString(), tiltInv);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	form->addRow(buttons);

	if (dlg.exec() != QDialog::Accepted)
		return;

	PTZConfig nc = c; /* keep id + preset_names */
	nc.name = nameEdit->text();
	nc.protocol = (PTZProtocol)protoCombo->currentData().toInt();
	nc.host = hostEdit->text().trimmed();
	nc.port = portSpin->value();
	nc.transport = (ViscaTransport)trans->currentData().toInt();
	nc.ccu_host = ccuEdit->text().trimmed();
	nc.ccu_transport = (ViscaTransport)ccuTrans->currentData().toInt();
	nc.ccu_port = visca_default_port(nc.ccu_transport);
	nc.pan_invert = panInv->isChecked();
	nc.tilt_invert = tiltInv->isChecked();
	PtzManager::instance().editDevice(id, nc);
}

/* ---- Full CCU window ---- */

/* A label + [−][⟳][+] stepper row + a current-value readout. Returns the value
 * label so readback can update it. Buttons use the compact stylesheet so their
 * glyphs render (the native macOS bezel clips small labels). */
static QLabel *addStepperRow(QFormLayout *form, const QString &label, std::function<void(int)> step)
{
	auto *row = new QHBoxLayout();
	auto *down = new QPushButton("−");
	auto *reset = new QPushButton("⟳");
	auto *up = new QPushButton("+");
	for (auto *b : {down, reset, up}) {
		b->setFixedWidth(36);
		b->setFocusPolicy(Qt::NoFocus);
		b->setStyleSheet(kCellStyle);
	}
	QObject::connect(down, &QPushButton::clicked, down, [step]() { step(-1); });
	QObject::connect(reset, &QPushButton::clicked, reset, [step]() { step(0); });
	QObject::connect(up, &QPushButton::clicked, up, [step]() { step(1); });
	auto *val = new QLabel("—");
	val->setMinimumWidth(44);
	val->setAlignment(Qt::AlignCenter);
	val->setStyleSheet("color:#ddd; font-weight:bold;");
	row->addWidget(down);
	row->addWidget(reset);
	row->addWidget(up);
	row->addSpacing(10);
	row->addWidget(val);
	row->addStretch();
	form->addRow(label, row);
	return val;
}

void PtzControlsDock::openCcuWindow()
{
	auto *d = current();
	if (!d || !d->hasImageControls())
		return;
	const int devId = d->id();
	auto dev = [devId]() { return PtzManager::instance().device(devId); }; // re-resolve each use

	QDialog dlg(this);
	dlg.setWindowTitle(obs_module_text("ImageSettings"));
	auto *lay = new QVBoxLayout(&dlg);

	/* White balance. */
	auto *wbBox = new QGroupBox(obs_module_text("WhiteBalance"), &dlg);
	auto *wf = new QFormLayout(wbBox);
	auto *wb = new QComboBox(&dlg);
	wb->addItems({obs_module_text("WB.Auto"), obs_module_text("WB.Indoor"), obs_module_text("WB.Outdoor"),
		      obs_module_text("WB.OnePush"), obs_module_text("WB.ATW"), obs_module_text("WB.Manual")});
	auto *red = new QSlider(Qt::Horizontal, &dlg);
	red->setRange(0, 255);
	red->setValue(128);
	auto *redVal = new QLabel("128", &dlg);
	redVal->setMinimumWidth(36);
	auto *blue = new QSlider(Qt::Horizontal, &dlg);
	blue->setRange(0, 255);
	blue->setValue(128);
	auto *blueVal = new QLabel("128", &dlg);
	blueVal->setMinimumWidth(36);
	auto *trigger = new QPushButton(obs_module_text("OnePushTrigger"), &dlg);
	auto syncWb = [=](int m) {
		const bool manual = (m == 5);
		red->setEnabled(manual);
		blue->setEnabled(manual);
	};
	connect(wb, QOverload<int>::of(&QComboBox::activated), &dlg, [=](int m) {
		if (auto *x = dev())
			x->setWhiteBalance(m);
		syncWb(m);
	});
	connect(red, &QSlider::valueChanged, &dlg, [=](int v) {
		redVal->setText(QString::number(v));
		if (auto *x = dev())
			x->setRedGain(v);
	});
	connect(blue, &QSlider::valueChanged, &dlg, [=](int v) {
		blueVal->setText(QString::number(v));
		if (auto *x = dev())
			x->setBlueGain(v);
	});
	connect(trigger, &QPushButton::clicked, &dlg, [=]() {
		if (auto *x = dev())
			x->whiteBalanceTrigger();
	});
	syncWb(0);
	wf->addRow(obs_module_text("Mode"), wb);
	wf->addRow(QString(), trigger);
	auto *redRow = new QHBoxLayout();
	redRow->addWidget(red, 1);
	redRow->addWidget(redVal);
	wf->addRow(obs_module_text("RedGain"), redRow);
	auto *blueRow = new QHBoxLayout();
	blueRow->addWidget(blue, 1);
	blueRow->addWidget(blueVal);
	wf->addRow(obs_module_text("BlueGain"), blueRow);
	lay->addWidget(wbBox);

	/* Exposure. */
	auto *exBox = new QGroupBox(obs_module_text("Exposure"), &dlg);
	auto *ef = new QFormLayout(exBox);
	auto *ex = new QComboBox(&dlg);
	ex->addItems({obs_module_text("Exp.Auto"), obs_module_text("Exp.Manual"), obs_module_text("Exp.ShutterPri"),
		      obs_module_text("Exp.IrisPri"), obs_module_text("Exp.Bright")});
	connect(ex, QOverload<int>::of(&QComboBox::activated), &dlg, [=](int m) {
		if (auto *x = dev())
			x->setExposureMode(m);
	});
	ef->addRow(obs_module_text("Mode"), ex);
	QLabel *shLbl = addStepperRow(ef, obs_module_text("Shutter"), [dev](int s) { if (auto *x = dev()) x->stepShutter(s); });
	QLabel *irLbl = addStepperRow(ef, obs_module_text("Iris"), [dev](int s) { if (auto *x = dev()) x->stepIris(s); });
	QLabel *gnLbl = addStepperRow(ef, obs_module_text("Gain"), [dev](int s) { if (auto *x = dev()) x->stepGain(s); });
	addStepperRow(ef, obs_module_text("Brightness"), [dev](int s) { if (auto *x = dev()) x->stepBright(s); });
	lay->addWidget(exBox);

	/* Toggles. */
	auto *togRow = new QHBoxLayout();
	auto *expc = new QPushButton(obs_module_text("ExposureComp"), &dlg);
	expc->setCheckable(true);
	auto *bl = new QPushButton(obs_module_text("Backlight"), &dlg);
	bl->setCheckable(true);
	connect(expc, &QPushButton::toggled, &dlg, [=](bool on) {
		if (auto *x = dev())
			x->setExposureComp(on);
	});
	connect(bl, &QPushButton::toggled, &dlg, [=](bool on) {
		if (auto *x = dev())
			x->setBacklight(on);
	});
	togRow->addWidget(expc);
	togRow->addWidget(bl);
	togRow->addStretch();
	lay->addLayout(togRow);

	auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
	connect(close, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
	lay->addWidget(close);

	/* Live readback: populate controls from the camera's reported state. */
	connect(d, &PTZDevice::imageState, &dlg, [=](const ImageState &st) {
		if (st.wbMode >= 0 && st.wbMode < wb->count()) {
			wb->setCurrentIndex(st.wbMode);
			syncWb(st.wbMode);
		}
		if (st.exposureMode >= 0 && st.exposureMode < ex->count())
			ex->setCurrentIndex(st.exposureMode);
		if (st.redGain >= 0) {
			red->blockSignals(true);
			red->setValue(st.redGain);
			red->blockSignals(false);
			redVal->setText(QString::number(st.redGain));
		}
		if (st.blueGain >= 0) {
			blue->blockSignals(true);
			blue->setValue(st.blueGain);
			blue->blockSignals(false);
			blueVal->setText(QString::number(st.blueGain));
		}
		shLbl->setText(st.shutter >= 0 ? QString::number(st.shutter) : "—");
		irLbl->setText(st.iris >= 0 ? QString::number(st.iris) : "—");
		gnLbl->setText(st.gain >= 0 ? QString::number(st.gain) : "—");
	});
	d->requestImageState();

	dlg.exec();
}
