// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

#include "ptz-manager.hpp"
#include "visca-ip.hpp"
#include "ndi-device.hpp"
#include "hybrid-device.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <obs-data.h>
#include <algorithm>

PtzManager &PtzManager::instance()
{
	static PtzManager mgr;
	return mgr;
}

PTZDevice *PtzManager::device(int id) const
{
	for (auto *d : devices_)
		if (d->id() == id)
			return d;
	return nullptr;
}

PTZDevice *PtzManager::create(const PTZConfig &cfg)
{
	switch (cfg.protocol) {
	case PTZProtocol::ViscaIP:
		return new ViscaIP(cfg);
	case PTZProtocol::NDI: {
		if (!cfg.ccu_host.isEmpty()) { /* hybrid: NDI motion + VISCA CCU */
			auto *h = new HybridDevice(cfg);
			if (!h->valid()) {
				h->deleteLater();
				return nullptr;
			}
			return h;
		}
		auto *d = new NDIDevice(cfg);
		if (!d->valid()) {
			d->deleteLater();
			return nullptr;
		}
		return d;
	}
	default:
		return nullptr;
	}
}

PTZDevice *PtzManager::addDevice(const PTZConfig &in)
{
	PTZConfig cfg = in;
	if (cfg.id <= 0)
		cfg.id = next_id_++;
	else
		next_id_ = std::max(next_id_, cfg.id + 1);

	PTZDevice *dev = create(cfg);
	if (!dev)
		return nullptr;
	devices_.push_back(dev);
	if (current_ < 0)
		current_ = cfg.id;
	save();
	emit devicesChanged();
	return dev;
}

void PtzManager::removeDevice(int id)
{
	for (size_t i = 0; i < devices_.size(); i++) {
		if (devices_[i]->id() == id) {
			devices_[i]->deleteLater();
			devices_.erase(devices_.begin() + i);
			break;
		}
	}
	if (current_ == id)
		current_ = devices_.empty() ? -1 : devices_.front()->id();
	save();
	emit devicesChanged();
}

void PtzManager::renameDevice(int id, const QString &name)
{
	if (PTZDevice *d = device(id)) {
		d->setName(name);
		save();
		emit devicesChanged();
	}
}

void PtzManager::editDevice(int id, const PTZConfig &in)
{
	for (size_t i = 0; i < devices_.size(); i++) {
		if (devices_[i]->id() != id)
			continue;
		PTZConfig cfg = in;
		cfg.id = id;
		if (cfg.preset_names.isEmpty()) /* preserve presets unless explicitly changed */
			cfg.preset_names = devices_[i]->config().preset_names;
		PTZDevice *nd = create(cfg);
		if (!nd) /* keep the old device if the new config can't be created */
			return;
		devices_[i]->deleteLater();
		devices_[i] = nd;
		save();
		emit devicesChanged();
		if (current_ == id)
			emit currentChanged(id);
		return;
	}
}

void PtzManager::setCurrent(int id)
{
	if (current_ == id)
		return;
	current_ = id;
	save();
	emit currentChanged(id);
}

void PtzManager::clear()
{
	for (auto *d : devices_)
		d->deleteLater();
	devices_.clear();
	current_ = -1;
}

/* ---- persistence ---- */

static char *config_file()
{
	return obs_module_get_config_path(obs_current_module(), "config.json");
}

void PtzManager::save()
{
	obs_data_t *root = obs_data_create();
	obs_data_set_int(root, "current", current_);
	obs_data_array_t *arr = obs_data_array_create();
	for (auto *d : devices_) {
		const PTZConfig &c = d->config();
		obs_data_t *o = obs_data_create();
		obs_data_set_int(o, "id", c.id);
		obs_data_set_string(o, "name", c.name.toUtf8().constData());
		obs_data_set_string(o, "type", ptz_protocol_id(c.protocol));
		obs_data_set_string(o, "transport", visca_transport_id(c.transport));
		obs_data_set_string(o, "host", c.host.toUtf8().constData());
		obs_data_set_int(o, "port", c.port);
		obs_data_set_int(o, "preset_max", c.preset_max);
		obs_data_set_bool(o, "pan_invert", c.pan_invert);
		obs_data_set_bool(o, "tilt_invert", c.tilt_invert);
		obs_data_set_string(o, "ccu_host", c.ccu_host.toUtf8().constData());
		obs_data_set_int(o, "ccu_port", c.ccu_port);
		obs_data_set_string(o, "ccu_transport", visca_transport_id(c.ccu_transport));
		obs_data_t *names = obs_data_create();
		for (auto it = c.preset_names.constBegin(); it != c.preset_names.constEnd(); ++it)
			obs_data_set_string(names, QString::number(it.key()).toUtf8().constData(),
					    it.value().toUtf8().constData());
		obs_data_set_obj(o, "preset_names", names);
		obs_data_release(names);
		obs_data_array_push_back(arr, o);
		obs_data_release(o);
	}
	obs_data_set_array(root, "devices", arr);

	char *path = config_file();
	if (path) {
		char *dir = obs_module_get_config_path(obs_current_module(), "");
		if (dir) {
			os_mkdirs(dir);
			bfree(dir);
		}
		obs_data_save_json(root, path);
		bfree(path);
	}
	obs_data_array_release(arr);
	obs_data_release(root);
}

void PtzManager::load()
{
	char *path = config_file();
	if (!path)
		return;
	obs_data_t *root = obs_data_create_from_json_file(path);
	bfree(path);
	if (!root)
		return;
	clear();
	const int savedCurrent = (int)obs_data_get_int(root, "current");
	obs_data_array_t *arr = obs_data_get_array(root, "devices");
	const size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		obs_data_t *o = obs_data_array_item(arr, i);
		PTZConfig c;
		c.id = (int)obs_data_get_int(o, "id");
		c.name = obs_data_get_string(o, "name");
		c.protocol = ptz_protocol_from_id(obs_data_get_string(o, "type"));
		c.transport = visca_transport_from_id(obs_data_get_string(o, "transport"));
		c.host = obs_data_get_string(o, "host");
		c.port = (int)obs_data_get_int(o, "port");
		if (c.port <= 0)
			c.port = visca_default_port(c.transport);
		c.preset_max = (int)obs_data_get_int(o, "preset_max");
		if (c.preset_max <= 0)
			c.preset_max = 16;
		c.pan_invert = obs_data_get_bool(o, "pan_invert");
		c.tilt_invert = obs_data_get_bool(o, "tilt_invert");
		c.ccu_host = obs_data_get_string(o, "ccu_host");
		c.ccu_port = (int)obs_data_get_int(o, "ccu_port");
		c.ccu_transport = visca_transport_from_id(obs_data_get_string(o, "ccu_transport"));
		obs_data_t *names = obs_data_get_obj(o, "preset_names");
		if (names) {
			for (obs_data_item_t *it = obs_data_first(names); it; obs_data_item_next(&it)) {
				const char *key = obs_data_item_get_name(it);
				const char *val = obs_data_item_get_string(it);
				if (key && val && *val)
					c.preset_names[QString(key).toInt()] = val;
			}
			obs_data_release(names);
		}
		PTZDevice *dev = create(c);
		if (dev) {
			devices_.push_back(dev);
			next_id_ = std::max(next_id_, c.id + 1);
		}
		obs_data_release(o);
	}
	obs_data_array_release(arr);
	obs_data_release(root);
	current_ = device(savedCurrent) ? savedCurrent : (devices_.empty() ? -1 : devices_.front()->id());
	emit devicesChanged();
}
