/*
 * Copyright (C) 2016 i.Dark_Templar <darktemplar@dark-templar-archives.net>
 *
 * This file is part of DTMD, Dark Templar Mount Daemon.
 *
 * DTMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DTMD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DTMD.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef QT_CONTROL_HPP
#define QT_CONTROL_HPP

#include <QObject>
#include <QSystemTrayIcon>
#include <QScopedPointer>
#include <QMenu>
#include <QMutex>

#include <vector>
#include <map>
#include <list>

#include <dtmd-library++.hpp>

class Control: public QObject
{
	Q_OBJECT

public:
	Control();
	~Control();

public slots:
	void change_state_icon();
	void triggeredOpen(unsigned int device, unsigned int partition, QString partition_name);
	void triggeredOpen(unsigned int stateful_device, QString device_name);
	void triggeredMount(unsigned int device, unsigned int partition, QString partition_name);
	void triggeredMount(unsigned int stateful_device, QString device_name);
	void triggeredUnmount(unsigned int device, unsigned int partition, QString partition_name);
	void triggeredUnmount(unsigned int stateful_device, QString device_name);

private:
	Q_DISABLE_COPY(Control)

	void BuildMenu();

	QIcon iconFromType(dtmd_removable_media_type_t type, bool is_mounted);

	static void dtmd_callback(void *arg, const dtmd::command &cmd);

	enum app_state
	{
		normal,
		notify,
		working,
		success,
		fail
	};

	void showMessage(app_state state, const QString &title, const QString &message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint);

	void setIconState(app_state state, int millisecondsTimeoutHint);
	void triggerBuildMenu();

	static const int defaultTimeout;

	QSystemTrayIcon m_tray;
	QScopedPointer<dtmd::library> m_lib;
	// TODO: implement stateful devices
	std::vector<dtmd::stateful_device> m_stateful_devices;
	std::vector<dtmd::device> m_devices;
	QMutex m_devices_mutex;

	std::map<app_state, QIcon> m_icons_map;
	QScopedPointer<QMenu> m_menu;

	std::list<app_state> m_state_queue;
	QMutex m_state_queue_mutex;

	QIcon m_icon_cdrom;
	QIcon m_icon_removable_disk;
	QIcon m_icon_sd_card;

	QIcon m_icon_mounted_cdrom;
	QIcon m_icon_mounted_removable_disk;
	QIcon m_icon_mounted_sd_card;

private slots:
	void tray_activated(QSystemTrayIcon::ActivationReason reason);
	void tray_messageClicked();

	void exit();

	void slotShowMessage(app_state state, QString title, QString message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint);
	void slotBuildMenu();

signals:
	void signalShowMessage(app_state state, QString title, QString message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint);
	void signalBuildMenu();
};

#endif /* QT_CONTROL_HPP */
