/*
 * Copyright (C) 2016-2019 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSystemTrayIcon>

#include <list>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include <dtmd-library++.hpp>

class Control: public QObject
{
	Q_OBJECT

public:
	Control();
	~Control();

public slots:
	void change_state_icon();
	void triggeredOpen(const std::string &device_name);
	void triggeredMount(const std::string &device_name);
	void triggeredUnmount(const std::string &device_name);

#if (defined OS_Linux)
	void triggeredPoweroff(const std::string &device_name);
#endif /* (defined OS_Linux) */

private:
	Q_DISABLE_COPY(Control)

	void populate_devices();

	void buildMenuRecursive(QMenu &root_menu, const std::shared_ptr<dtmd::removable_media> &device_ptr);
	void BuildMenu();

	QIcon iconFromSubtype(dtmd_removable_media_subtype_t type, bool is_mounted);

	static void dtmd_callback(const dtmd::library &library_instance, void *arg, const dtmd::command &cmd);
	static void dtmd_state_callback(const dtmd::library &library_instance, void *arg, dtmd_state_t state);

	enum app_state
	{
		normal,
		notify,
		working,
		success,
		fail
	};

	void setIconState(app_state state, int millisecondsTimeoutHint);

	std::tuple<bool, QString, QString> processCommand(const dtmd::command &cmd);

	static const int defaultTimeout;

	QSystemTrayIcon m_tray;
	std::unique_ptr<dtmd::library> m_lib;
	dtmd::removable_media_container m_devices;
	std::list<dtmd::command> m_saved_commands;
	volatile bool m_devices_initialized;
	QMutex m_devices_mutex;

	std::map<app_state, QIcon> m_icons_map;
	std::unique_ptr<QMenu> m_menu;

	std::list<app_state> m_state_queue;
	QMutex m_state_queue_mutex;

	QIcon m_icon_cdrom;
	QIcon m_icon_removable_disk;
	QIcon m_icon_sd_card;

	QIcon m_icon_mounted_cdrom;
	QIcon m_icon_mounted_removable_disk;
	QIcon m_icon_mounted_sd_card;

private slots:
	void slotShowMessage(app_state state, QString title, QString message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint);
	void slotBuildMenu();

	void exit();
	void slotDtmdConnected();
	void slotDtmdDisconnected();
	void slotExitSignalled(QString title, QString message);

signals:
	void showMessage(app_state state, QString title, QString message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint);
	void triggerBuildMenu();

	void dtmdConnected();
	void dtmdDisconnected();
	void exitSignalled(QString title, QString message);
};

#endif /* QT_CONTROL_HPP */
