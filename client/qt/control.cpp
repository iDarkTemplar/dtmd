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

#include "client/qt/control.hpp"

#include <QApplication>
#include <QMutexLocker>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>

#include <stdexcept>
#include <sstream>

#include <stddef.h>

#include "client/qt/qcustomdeviceaction.hpp"

const int Control::defaultTimeout = 5000;

Control::Control()
	: m_devices_initialized(false),
	m_icon_cdrom(DATA_PREFIX "/dtmd/cdrom.png"),
	m_icon_removable_disk(DATA_PREFIX "/dtmd/removable_disk.png"),
	m_icon_sd_card(DATA_PREFIX "/dtmd/sdcard.png"),
	m_icon_mounted_cdrom(DATA_PREFIX "/dtmd/cdrom.mounted.png"),
	m_icon_mounted_removable_disk(DATA_PREFIX "/dtmd/removable_disk.mounted.png"),
	m_icon_mounted_sd_card(DATA_PREFIX "/dtmd/sdcard.mounted.png")
{
	m_icons_map[normal]  = QIcon(DATA_PREFIX "/dtmd/normal.png");
	m_icons_map[notify]  = QIcon(DATA_PREFIX "/dtmd/notify.png");
	m_icons_map[working] = QIcon(DATA_PREFIX "/dtmd/working.png");
	m_icons_map[success] = QIcon(DATA_PREFIX "/dtmd/success.png");
	m_icons_map[fail]    = QIcon(DATA_PREFIX "/dtmd/fail.png");

	qRegisterMetaType<app_state>("app_state");
	qRegisterMetaType<QSystemTrayIcon::MessageIcon>("QSystemTrayIcon::MessageIcon");
	qRegisterMetaType<QSystemTrayIcon::ActivationReason>("QSystemTrayIcon::ActivationReason");
	qRegisterMetaType<std::string>("std::string");

	QObject::connect(this, SIGNAL(signalShowMessage(app_state,QString,QString,QSystemTrayIcon::MessageIcon,int)),
		this, SLOT(slotShowMessage(app_state,QString,QString,QSystemTrayIcon::MessageIcon,int)), Qt::QueuedConnection);

	QObject::connect(this, SIGNAL(signalBuildMenu()),
		this, SLOT(slotBuildMenu()), Qt::QueuedConnection);

	QObject::connect(this, SIGNAL(signalDtmdConnected()),
		this, SLOT(slotDtmdConnected()), Qt::QueuedConnection);

	QObject::connect(this, SIGNAL(signalDtmdDisconnected()),
		this, SLOT(slotDtmdDisconnected()), Qt::QueuedConnection);

	QObject::connect(this, SIGNAL(signalExitSignalled(QString,QString)),
		this, SLOT(slotExitSignalled(QString,QString)), Qt::QueuedConnection);

	QObject::connect(&m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(tray_activated(QSystemTrayIcon::ActivationReason)), Qt::QueuedConnection);

	QObject::connect(&m_tray, SIGNAL(messageClicked()),
		this, SLOT(tray_messageClicked()), Qt::QueuedConnection);

	m_tray.setIcon(m_icons_map.at(normal));

	m_lib.reset(new dtmd::library(&Control::dtmd_callback, &Control::dtmd_state_callback, this));

	{
		dtmd::removable_media_container temp_devices;

		dtmd_result_t result = m_lib->list_all_removable_devices(defaultTimeout, temp_devices);
		if (result != dtmd_ok)
		{
			if (m_lib->isStateInvalid())
			{
				std::stringstream errMsg;
				errMsg << "Couldn't obtain list of current removable devices, error code " << result;
				throw std::runtime_error(errMsg.str());
			}
		}
		else
		{ // lock
			QMutexLocker devices_locker(&m_devices_mutex);
			m_devices = temp_devices;
			m_devices_initialized = true;

			auto iter_end = m_saved_commands.end();
			for (auto iter = m_saved_commands.begin(); iter != iter_end; ++iter)
			{
				processCommand(*iter);
			}

			m_saved_commands.clear();
		} // unlock
	}

	BuildMenu();

	m_tray.show();
}

Control::~Control()
{
}

void Control::change_state_icon()
{ // lock
	QMutexLocker state_queue_locker(&m_state_queue_mutex);
	if (!m_state_queue.empty())
	{
		m_state_queue.pop_front();
	}

	if (m_state_queue.empty())
	{
		m_tray.setIcon(m_icons_map.at(normal));
	}
} // unlock

void Control::triggeredOpen(const std::string &device_name)
{
	std::shared_ptr<dtmd::removable_media> media_ptr = dtmd::find_removable_media(device_name, m_devices);
	if (!media_ptr)
	{
		return;
	}

	QString mount_point;

	if (media_ptr->mnt_point.empty())
	{
		setIconState(working, Control::defaultTimeout);
		dtmd_result_t result = m_lib->mount(defaultTimeout, media_ptr->path);

		if (result == dtmd_ok)
		{
			setIconState(success, Control::defaultTimeout);
		}
		else
		{
			setIconState(fail, Control::defaultTimeout);
			return;
		}

		dtmd::removable_media_container temp;

		result = m_lib->list_removable_device(defaultTimeout, media_ptr->path, temp);
		if ((result == dtmd_ok) && (temp.size() == 1) && (media_ptr->path == (*temp.begin())->path))
		{
			mount_point = QString::fromLocal8Bit((*temp.begin())->mnt_point.c_str());
		}
		else
		{
			setIconState(fail, Control::defaultTimeout);
			return;
		}
	}
	else
	{
		mount_point = QString::fromLocal8Bit(media_ptr->mnt_point.c_str());
	}

	// In order to properly open directory, it should have odd number of '/' characters. Just leave first character if the string begins with more than one character
	{
		size_t last_index = 0;
		QChar char_looking_for('/');

		for (size_t mount_point_len = mount_point.size();
			(last_index < mount_point_len) && (mount_point.at(last_index) == char_looking_for);
			++last_index)
		{
		}

		if (last_index != 1)
		{
			mount_point.remove(0, last_index - 1);
		}
	}

	QDesktopServices::openUrl(QUrl(QString("file://") + mount_point));
}

void Control::triggeredMount(const std::string &device_name)
{
	std::shared_ptr<dtmd::removable_media> media_ptr = dtmd::find_removable_media(device_name, m_devices);
	if (!media_ptr)
	{
		return;
	}

	setIconState(working, Control::defaultTimeout);
	dtmd_result_t result = m_lib->mount(defaultTimeout, media_ptr->path);

	if (result == dtmd_ok)
	{
		setIconState(success, Control::defaultTimeout);
	}
	else
	{
		setIconState(fail, Control::defaultTimeout);
	}
}

void Control::triggeredUnmount(const std::string &device_name)
{
	std::shared_ptr<dtmd::removable_media> media_ptr = dtmd::find_removable_media(device_name, m_devices);
	if (!media_ptr)
	{
		return;
	}

	setIconState(working, Control::defaultTimeout);
	dtmd_result_t result = m_lib->unmount(defaultTimeout, media_ptr->path);

	if (result == dtmd_ok)
	{
		setIconState(success, Control::defaultTimeout);
	}
	else
	{
		setIconState(fail, Control::defaultTimeout);
	}
}

void Control::buildMenuRecursive(QMenu &root_menu, const std::shared_ptr<dtmd::removable_media> &device_ptr)
{
	bool device_is_ok = false;

	switch (device_ptr->type)
	{
	case dtmd_removable_media_type_device_partition:
		if (!device_ptr->fstype.empty())
		{
			device_is_ok = true;
		}
		break;

	case dtmd_removable_media_type_stateful_device:
		if ((device_ptr->state == dtmd_removable_media_state_ok) && (!(device_ptr->fstype.empty())))
		{
			device_is_ok = true;
		}
		break;

	case dtmd_removable_media_type_stateless_device:
	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		break;
	}

	if (device_is_ok)
	{
		bool is_mounted = !(device_ptr->mnt_point.empty());
		QMenu *menu = root_menu.addMenu(iconFromSubtype(device_ptr->subtype, is_mounted),
			QString::fromLocal8Bit(device_ptr->label.empty() ? device_ptr->path.c_str() : device_ptr->label.c_str()));

		QCustomDeviceAction *action;
		action = new QCustomDeviceAction(QObject::tr("Open device"),
			menu,
			device_ptr->path);

		QObject::connect(action, SIGNAL(triggered(const std::string &)),
			this, SLOT(triggeredOpen(const std::string &)), Qt::DirectConnection);

		menu->addAction(action);

		if (is_mounted)
		{
			action = new QCustomDeviceAction(QObject::tr("Unmount device"),
				menu,
				device_ptr->path);

			QObject::connect(action, SIGNAL(triggered(const std::string &)),
				this, SLOT(triggeredUnmount(const std::string &)), Qt::DirectConnection);

			menu->addAction(action);
		}
		else
		{
			action = new QCustomDeviceAction(QObject::tr("Mount device"),
				menu,
				device_ptr->path);

			QObject::connect(action, SIGNAL(triggered(const std::string &)),
				this, SLOT(triggeredMount(const std::string &)), Qt::DirectConnection);

			menu->addAction(action);
		}
	}

	auto iter_end = device_ptr->children.end();
	for (auto iter = device_ptr->children.begin(); iter != iter_end; ++iter)
	{
		buildMenuRecursive(root_menu, *iter);
	}
}

void Control::BuildMenu()
{
	QScopedPointer<QMenu> new_menu(new QMenu());

	{ // lock
		QMutexLocker devices_locker(&m_devices_mutex);

		auto iter_end = m_devices.end();
		for (auto iter = m_devices.begin(); iter != iter_end; ++iter)
		{
			buildMenuRecursive(*new_menu, *iter);
		}
	} // unlock

	// TODO: cache menu

	if (!new_menu->isEmpty())
	{
		new_menu->addSeparator();
	}

	new_menu->addAction(QObject::tr("Exit"), this, SLOT(exit()));

	m_tray.setContextMenu(new_menu.data());
	m_menu.reset(new_menu.take());
}

QIcon Control::iconFromSubtype(dtmd_removable_media_subtype_t type, bool is_mounted)
{
	switch (type)
	{
	case dtmd_removable_media_subtype_cdrom:
		if (is_mounted)
		{
			return m_icon_mounted_cdrom;
		}
		else
		{
			return m_icon_cdrom;
		}

	case dtmd_removable_media_subtype_removable_disk:
		if (is_mounted)
		{
			return m_icon_mounted_removable_disk;
		}
		else
		{
			return m_icon_removable_disk;
		}

	case dtmd_removable_media_subtype_sd_card:
		if (is_mounted)
		{
			return m_icon_mounted_sd_card;
		}
		else
		{
			return m_icon_sd_card;
		}

	//case unknown_or_persistent:
	}

	return QIcon();
}

void Control::dtmd_callback(const dtmd::library &library_instance, void *arg, const dtmd::command &cmd)
{
	Control *ptr = (Control*) arg;

	try
	{
		std::tuple<bool, QString, QString> result(false, QString(), QString());

		{ // lock
			QMutexLocker devices_locker(&(ptr->m_devices_mutex));

			if (ptr->m_devices_initialized)
			{
				result = ptr->processCommand(cmd);
			}
			else
			{
				ptr->m_saved_commands.push_back(cmd);
			}
		} // unlock

		if (std::get<0>(result))
		{
			ptr->triggerBuildMenu();

			const QString &title = std::get<1>(result);
			const QString &message = std::get<2>(result);

			if ((!title.isEmpty()) || (!message.isEmpty()))
			{
				ptr->showMessage(notify, title, message, QSystemTrayIcon::Information, Control::defaultTimeout);
			}
		}
	}
	catch (const std::exception &e)
	{
		ptr->exitSignalled(QObject::tr("Fatal error"), QObject::tr("Runtime error") + QString("\n") + QObject::tr("Error message: ") + QString::fromLocal8Bit(e.what()));
	}
	catch (...)
	{
		ptr->exitSignalled(QObject::tr("Fatal error"), QObject::tr("Runtime error"));
	}
}

void Control::dtmd_state_callback(const dtmd::library &library_instance, void *arg, dtmd_state_t state)
{
	Control *ptr = (Control*) arg;

	switch (state)
	{
	case dtmd_state_connected:
		ptr->dtmdConnected();
		break;

	case dtmd_state_disconnected:
		ptr->dtmdDisconnected();
		break;

	case dtmd_state_failure:
		ptr->exitSignalled(QObject::tr("Exiting"), QObject::tr("Daemon sent exit message"));
		break;

	default:
		ptr->exitSignalled(QObject::tr("Exiting"), QObject::tr("Got unknown state from daemon"));
		break;
	}
}

void Control::showMessage(app_state state, const QString &title,
	const QString &message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint)
{
	emit signalShowMessage(state, title, message, icon, millisecondsTimeoutHint);
}

void Control::setIconState(app_state state, int millisecondsTimeoutHint)
{ // lock
	QMutexLocker state_queue_locker(&m_state_queue_mutex);
	if (m_state_queue.empty()
		|| ((m_state_queue.back() != success)
			&& (m_state_queue.back() != fail)))
	{
		m_state_queue.push_back(state);
		m_tray.setIcon(m_icons_map.at(state));

		QTimer::singleShot(millisecondsTimeoutHint, this, SLOT(change_state_icon()));
	}
} // unlock

void Control::triggerBuildMenu()
{
	emit signalBuildMenu();
}

std::tuple<bool, QString, QString> Control::processCommand(const dtmd::command &cmd)
{
	bool modified = false;
	QString title;
	QString message;

	if ((cmd.cmd == dtmd_notification_removable_device_added) && (m_lib->isNotificationValidRemovableDevice(cmd)))
	{
		// first search if device with such path already exists
		std::shared_ptr<dtmd::removable_media> device_ptr = dtmd::find_removable_media(cmd.args[1], m_devices);
		if (!device_ptr)
		{
			// Such device doesn't exist, search for parent

			std::shared_ptr<dtmd::removable_media> obtained_device;
			dtmd_result_t result = m_lib->fill_removable_device_from_notification(cmd, obtained_device);
			if (result == dtmd_ok)
			{
				if (cmd.args[0] == dtmd_root_device_path)
				{
					m_devices.insert(obtained_device);
					modified = true;
				}
				else
				{
					device_ptr = dtmd::find_removable_media(cmd.args[0], m_devices);
					if (device_ptr)
					{
						obtained_device->parent = device_ptr;
						device_ptr->children.insert(obtained_device);
						modified = true;
					}
				}

				if (modified)
				{
					switch (obtained_device->type)
					{
					case dtmd_removable_media_type_device_partition:
						title = QObject::tr("Device attached");
						message = QString::fromLocal8Bit(obtained_device->label.empty() ? obtained_device->path.c_str() : obtained_device->label.c_str());
						break;

					case dtmd_removable_media_type_stateful_device:
						switch (obtained_device->state)
						{
						case dtmd_removable_media_state_ok:
							title = QObject::tr("Device attached, state: available");
							break;

						default:
							title = QObject::tr("Device attached, state: unavailable");
							break;
						}

						message = QString::fromLocal8Bit(obtained_device->label.empty() ? obtained_device->path.c_str() : obtained_device->label.c_str());
						break;
					}
				}
			}
		}
	}
	else if ((cmd.cmd == dtmd_notification_removable_device_removed) && (cmd.args.size() == 1) && (!cmd.args[0].empty()))
	{
		// first search if device with such path already exists
		std::shared_ptr<dtmd::removable_media> device_ptr = dtmd::find_removable_media(cmd.args[0], m_devices);
		if (device_ptr)
		{
			// search for parent
			auto parent = device_ptr->parent.lock();
			if (parent)
			{
				if (parent->children.erase(device_ptr) > 0)
				{
					modified = true;
				}
			}
			else
			{
				// check root devices
				if (m_devices.erase(device_ptr) > 0)
				{
					modified = true;
				}
			}

			if (modified)
			{
				switch (device_ptr->type)
				{
				case dtmd_removable_media_type_device_partition:
				case dtmd_removable_media_type_stateful_device:
					title = QObject::tr("Device removed");
					message = QString::fromLocal8Bit(device_ptr->label.empty() ? device_ptr->path.c_str() : device_ptr->label.c_str());
					break;
				}
			}
		}
	}
	else if ((cmd.cmd == dtmd_notification_removable_device_changed) && (m_lib->isNotificationValidRemovableDevice(cmd)))
	{
		// first search if device with such path already exists
		std::shared_ptr<dtmd::removable_media> device_ptr = dtmd::find_removable_media(cmd.args[1], m_devices);
		if (device_ptr)
		{
			dtmd_removable_media_state_t last_state = device_ptr->state;
			std::string last_name = (device_ptr->label.empty() ? device_ptr->path : device_ptr->label);

			std::shared_ptr<dtmd::removable_media> obtained_device;
			dtmd_result_t result = m_lib->fill_removable_device_from_notification(cmd, obtained_device);
			if (result == dtmd_ok)
			{
				device_ptr->copyFromRemovableMedia(*obtained_device);
				modified = true;
			}

			if (modified)
			{
				switch (device_ptr->type)
				{
				case dtmd_removable_media_type_device_partition:
					title = QObject::tr("Device changed");
					message = QString::fromLocal8Bit(device_ptr->label.empty() ? device_ptr->path.c_str() : device_ptr->label.c_str());
					break;

				case dtmd_removable_media_type_stateful_device:
					if ((last_state != dtmd_removable_media_state_ok)
						&& (device_ptr->state == dtmd_removable_media_state_ok))
					{
						title = QObject::tr("Device changed to state: available");
						message = QString::fromLocal8Bit(device_ptr->label.empty() ? device_ptr->path.c_str() : device_ptr->label.c_str());
					}
					else if ((last_state == dtmd_removable_media_state_ok)
						&& (device_ptr->state != dtmd_removable_media_state_ok))
					{
						title = QObject::tr("Device changed to state: unavailable");
						message = QString::fromLocal8Bit(last_name.c_str());
					}
					break;
				}
			}
		}
	}
	else if ((cmd.cmd == dtmd_notification_removable_device_mounted) && (cmd.args.size() == 3) && (!cmd.args[0].empty())
		&& (!cmd.args[1].empty()) && (!cmd.args[2].empty()))
	{
		// first search if device with such path exists
		std::shared_ptr<dtmd::removable_media> device_ptr = dtmd::find_removable_media(cmd.args[0], m_devices);
		if (device_ptr)
		{
			device_ptr->mnt_point = cmd.args[1];
			device_ptr->mnt_opts  = cmd.args[2];

			modified = true;
			title = QObject::tr("Device mounted");
			message = QString::fromLocal8Bit(device_ptr->label.empty() ? device_ptr->path.c_str() : device_ptr->label.c_str());
		}
	}
	else if ((cmd.cmd == dtmd_notification_removable_device_unmounted) && (cmd.args.size() == 2) && (!cmd.args[0].empty()) && (!cmd.args[1].empty()))
	{
		// first search if device with such path exists
		std::shared_ptr<dtmd::removable_media> device_ptr = dtmd::find_removable_media(cmd.args[0], m_devices);
		if (device_ptr)
		{
			device_ptr->mnt_point.clear();
			device_ptr->mnt_opts.clear();

			modified = true;
			title = QObject::tr("Device unmounted");
			message = QString::fromLocal8Bit(device_ptr->label.empty() ? device_ptr->path.c_str() : device_ptr->label.c_str());
		}
	}

	return std::make_tuple(modified, title, message);
}

void Control::tray_activated(QSystemTrayIcon::ActivationReason reason)
{
	// TODO: implement
}

void Control::tray_messageClicked()
{
	// TODO: implement
}

void Control::exit()
{
	QApplication::exit();
}

void Control::dtmdConnected()
{
	emit signalDtmdConnected();
}

void Control::dtmdDisconnected()
{
	emit signalDtmdDisconnected();
}

void Control::exitSignalled(QString title, QString message)
{
	emit signalExitSignalled(title, message);
}

void Control::slotShowMessage(app_state state, QString title, QString message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint)
{
	m_tray.showMessage(title, message, icon, millisecondsTimeoutHint);
	setIconState(state, millisecondsTimeoutHint);
}

void Control::slotBuildMenu()
{
	BuildMenu();
}

void Control::slotDtmdConnected()
{
	{
		dtmd::removable_media_container temp_devices;

		dtmd_result_t result = m_lib->list_all_removable_devices(defaultTimeout, temp_devices);
		if (result != dtmd_ok)
		{
			if (m_lib->isStateInvalid())
			{
				std::stringstream errMsg;
				errMsg << "Couldn't obtain list of current removable devices, error code " << result;
				throw std::runtime_error(errMsg.str());
			}
		}
		else
		{ // lock
			QMutexLocker devices_locker(&m_devices_mutex);
			m_devices = temp_devices;
			m_devices_initialized = true;

			auto iter_end = m_saved_commands.end();
			for (auto iter = m_saved_commands.begin(); iter != iter_end; ++iter)
			{
				processCommand(*iter);
			}

			m_saved_commands.clear();
		} // unlock
	}

	BuildMenu();

	this->showMessage(success, QObject::tr("Connected to DTMD daemon"), QString(), QSystemTrayIcon::Information, Control::defaultTimeout);
}

void Control::slotDtmdDisconnected()
{
	{ // lock
		QMutexLocker devices_locker(&m_devices_mutex);
		m_devices.clear();
		m_devices_initialized = false;
	} // unlock

	BuildMenu();

	this->showMessage(fail, QObject::tr("Disconnected from DTMD daemon"), QString(), QSystemTrayIcon::Information, Control::defaultTimeout);
}

void Control::slotExitSignalled(QString title, QString message)
{
	QMessageBox::critical(NULL, title, message);
	this->exit();
}
