// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#pragma once

#include "ptz-device.hpp"
#include <QObject>
#include <vector>

/* Owns the configured PTZ devices, the current selection, and persistence
 * (plugin config dir / ptz-controller/config.json). UI-thread singleton. */
class PtzManager : public QObject {
	Q_OBJECT

public:
	static PtzManager &instance();

	const std::vector<PTZDevice *> &devices() const { return devices_; }
	PTZDevice *device(int id) const;
	PTZDevice *current() const { return device(current_); }
	int currentId() const { return current_; }
	void setCurrent(int id);

	PTZDevice *addDevice(const PTZConfig &cfg); // creates, persists, returns it
	void removeDevice(int id);
	void renameDevice(int id, const QString &name);
	void editDevice(int id, const PTZConfig &cfg); // recreate with new config (keeps id/presets)

	void load();
	void save();
	void clear();

signals:
	void devicesChanged();
	void currentChanged(int id);

private:
	explicit PtzManager(QObject *parent = nullptr) : QObject(parent) {}
	PTZDevice *create(const PTZConfig &cfg);

	std::vector<PTZDevice *> devices_;
	int current_ = -1;
	int next_id_ = 1;
};
