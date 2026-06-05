// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 preppdev — PTZ Controller (obs-ptz-controller)

/*
 * PTZ control dock. Scrollable so it can shrink. Top: a row of quick-select
 * camera buttons (nameable) + a settings gear. Middle: pan/tilt pad, zoom/focus
 * rockers, speed. Bottom: a collapsible (accordion) preset bank whose buttons
 * have right-click Save / Clear / Rename and show the name beneath. Camera
 * add/rename/remove live in the Settings dialog.
 */
#pragma once

#include <QFrame>

class QSlider;
class QGridLayout;
class QHBoxLayout;
class QWidget;
class QPushButton;
class QButtonGroup;
class PTZDevice;

class PtzControlsDock : public QFrame {
	Q_OBJECT

public:
	explicit PtzControlsDock(QWidget *parent = nullptr);

private slots:
	void refreshCameras();
	void onSettings();
	void onPresetClicked(int index);
	void togglePresets();

private:
	PTZDevice *current() const;
	double speed() const;
	void buildCameraBar();
	void buildPresetBank(int count);
	void presetContextMenu(int index, QWidget *anchor);
	void addCameraDialog();

	QWidget *camBar_;
	QHBoxLayout *camLayout_;
	QButtonGroup *camGroup_;
	QSlider *speed_;
	QPushButton *presetToggle_;
	QWidget *presetPanel_;
	QGridLayout *presetGrid_;
	bool presetsCollapsed_ = false;
};
