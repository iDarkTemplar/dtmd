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

#include "qcustomaction.hpp"

const int Control::defaultTimeout = 5000;

Control::Control()
	: m_icon_cdrom(DATA_PREFIX "/dtmd/cdrom.png"),
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

	QObject::connect(this, SIGNAL(signalShowMessage(app_state,QString,QString,QSystemTrayIcon::MessageIcon,int)),
		this, SLOT(slotShowMessage(app_state,QString,QString,QSystemTrayIcon::MessageIcon,int)), Qt::QueuedConnection);

	QObject::connect(this, SIGNAL(signalBuildMenu()),
		this, SLOT(slotBuildMenu()), Qt::QueuedConnection);

	QObject::connect(&m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(tray_activated(QSystemTrayIcon::ActivationReason)), Qt::QueuedConnection);

	QObject::connect(&m_tray, SIGNAL(messageClicked()),
		this, SLOT(tray_messageClicked()), Qt::QueuedConnection);

	m_tray.setIcon(m_icons_map.at(normal));

	m_lib.reset(new dtmd::library(&Control::dtmd_callback, this));

	{ // lock
		QMutexLocker devices_locker(&m_devices_mutex);
		dtmd_result_t result = m_lib->enum_devices(dtmd::timeout_infinite, m_devices);
		if (result != dtmd_ok)
		{
			std::stringstream errMsg;
			errMsg << "Couldn't obtain list of current removable devices, error code " << result;
			throw std::runtime_error(errMsg.str());
		}
	} // unlock

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

void Control::triggeredOpen(unsigned int device, unsigned int partition, QString partition_name)
{
	if ((device >= m_devices.size())
		|| (partition >= m_devices.at(device).partitions.size())
		|| (QString::fromLocal8Bit(m_devices.at(device).partitions.at(partition).path.c_str()) != partition_name))
	{
		return;
	}

	dtmd::partition &part = m_devices[device].partitions[partition];
	QString mount_point;

	if (part.mnt_point.empty())
	{
		setIconState(working, Control::defaultTimeout);
		dtmd_result_t result = m_lib->mount(dtmd::timeout_infinite, part.path);

		if (result == dtmd_ok)
		{
			setIconState(success, Control::defaultTimeout);
		}
		else
		{
			setIconState(fail, Control::defaultTimeout);
			return;
		}

		dtmd::partition temp;

		m_lib->list_partition(dtmd::timeout_infinite, part.path, temp);

		mount_point = QString::fromLocal8Bit(temp.mnt_point.c_str());
	}
	else
	{
		mount_point = QString::fromLocal8Bit(part.mnt_point.c_str());
	}

	QDesktopServices::openUrl(QUrl(QString("file:///") + mount_point));
}

void Control::triggeredMount(unsigned int device, unsigned int partition, QString partition_name)
{
	if ((device >= m_devices.size())
		|| (partition >= m_devices.at(device).partitions.size())
		|| (QString::fromLocal8Bit(m_devices.at(device).partitions.at(partition).path.c_str()) != partition_name))
	{
		return;
	}

	setIconState(working, Control::defaultTimeout);
	dtmd_result_t result = m_lib->mount(dtmd::timeout_infinite, m_devices.at(device).partitions.at(partition).path);

	if (result == dtmd_ok)
	{
		setIconState(success, Control::defaultTimeout);
	}
	else
	{
		setIconState(fail, Control::defaultTimeout);
	}
}

void Control::triggeredUnmount(unsigned int device, unsigned int partition, QString partition_name)
{
	if ((device >= m_devices.size())
		|| (partition >= m_devices.at(device).partitions.size())
		|| (QString::fromLocal8Bit(m_devices.at(device).partitions.at(partition).path.c_str()) != partition_name))
	{
		return;
	}

	setIconState(working, Control::defaultTimeout);
	dtmd_result_t result = m_lib->unmount(dtmd::timeout_infinite, m_devices.at(device).partitions.at(partition).path);

	if (result == dtmd_ok)
	{
		setIconState(success, Control::defaultTimeout);
	}
	else
	{
		setIconState(fail, Control::defaultTimeout);
	}
}

void Control::BuildMenu()
{
	QScopedPointer<QMenu> new_menu(new QMenu());

	{ // lock
		QMutexLocker devices_locker(&m_devices_mutex);

		if (!m_devices.empty())
		{
			for (std::vector<dtmd::device>::iterator dev = m_devices.begin(); dev != m_devices.end(); ++dev)
			{
				for (std::vector<dtmd::partition>::iterator it = dev->partitions.begin(); it != dev->partitions.end(); ++it)
				{
					bool is_mounted = !(it->mnt_point.empty());
					QMenu *menu = new_menu->addMenu(iconFromType(dev->type, is_mounted),
						QString::fromLocal8Bit(it->label.empty() ? it->path.c_str() : it->label.c_str()));

					QCustomAction *action;
					action = new QCustomAction(QObject::trUtf8("Open device"),
						menu,
						dev - m_devices.begin(),
						it - dev->partitions.begin(),
						QString::fromLocal8Bit(it->path.c_str()));

					QObject::connect(action, SIGNAL(triggered(uint,uint,QString)),
						this, SLOT(triggeredOpen(uint,uint,QString)), Qt::DirectConnection);

					menu->addAction(action);

					if (is_mounted)
					{
						action = new QCustomAction(QObject::trUtf8("Unmount device"),
							menu,
							dev - m_devices.begin(),
							it - dev->partitions.begin(),
							QString::fromLocal8Bit(it->path.c_str()));

						QObject::connect(action, SIGNAL(triggered(uint,uint,QString)),
							this, SLOT(triggeredUnmount(uint,uint,QString)), Qt::DirectConnection);

						menu->addAction(action);
					}
					else
					{
						action = new QCustomAction(QObject::trUtf8("Mount device"),
							menu,
							dev - m_devices.begin(),
							it - dev->partitions.begin(),
							QString::fromLocal8Bit(it->path.c_str()));

						QObject::connect(action, SIGNAL(triggered(uint,uint,QString)),
							this, SLOT(triggeredMount(uint,uint,QString)), Qt::DirectConnection);

						menu->addAction(action);
					}
				}
			}
			// TODO: cache menu
			// TODO: build menu
			new_menu->addSeparator();
		}
	} // unlock

	new_menu->addAction(QObject::trUtf8("Exit"), this, SLOT(exit()));

	m_tray.setContextMenu(new_menu.data());
	m_menu.reset(new_menu.take());
}

QIcon Control::iconFromType(dtmd_removable_media_type_t type, bool is_mounted)
{
	switch (type)
	{
	case cdrom:
		if (is_mounted)
		{
			return m_icon_mounted_cdrom;
		}
		else
		{
			return m_icon_cdrom;
		}

	case removable_disk:
		if (is_mounted)
		{
			return m_icon_mounted_removable_disk;
		}
		else
		{
			return m_icon_removable_disk;
		}

	case sd_card:
		if (is_mounted)
		{
			return m_icon_mounted_sd_card;
		}
		else
		{
			return m_icon_mounted_sd_card;
		}

	//case unknown_or_persistent:
	}

	return QIcon();
}

void Control::dtmd_callback(void *arg, const dtmd::command &cmd)
{
	Control *ptr = (Control*) arg;

	if (!cmd.isEmpty())
	{
		try
		{
			bool modified = false;
			QString title;
			QString message;

			if ((cmd.cmd == "add_disk") && (cmd.args.size() == 2) && (!cmd.args[0].empty()) && (!cmd.args[1].empty()))
			{ // lock
				QMutexLocker devices_locker(&(ptr->m_devices_mutex));

				std::vector<dtmd::device>::iterator it;
				for (it = ptr->m_devices.begin(); it != ptr->m_devices.end(); ++it)
				{
					if (cmd.args[0] == it->path)
					{
						break;
					}
				}

				if (it == ptr->m_devices.end())
				{
					ptr->m_devices.push_back(dtmd::device(cmd.args[0], dtmd_string_to_device_type(cmd.args[1].c_str())));
				}
			} // unlock
			else if ((cmd.cmd == "remove_disk") && (cmd.args.size() == 1) && (!cmd.args[0].empty()))
			{ // lock
				QMutexLocker devices_locker(&(ptr->m_devices_mutex));

				std::vector<dtmd::device>::iterator it;
				for (it = ptr->m_devices.begin(); it != ptr->m_devices.end(); ++it)
				{
					if (cmd.args[0] == it->path)
					{
						break;
					}
				}

				if (it != ptr->m_devices.end())
				{
					ptr->m_devices.erase(it);
				}
			} // unlock
			else if ((cmd.cmd == "add_partition") && (cmd.args.size() == 4)
				&& (!cmd.args[0].empty()) && (!cmd.args[1].empty()) && (!cmd.args[3].empty()))
			{ // lock
				QMutexLocker devices_locker(&(ptr->m_devices_mutex));

				std::vector<dtmd::device>::iterator dev;
				for (dev = ptr->m_devices.begin(); dev != ptr->m_devices.end(); ++dev)
				{
					if (cmd.args[3] == dev->path)
					{
						break;
					}
				}

				if (dev != ptr->m_devices.end())
				{
					std::vector<dtmd::partition>::iterator it;
					for (it = dev->partitions.begin(); it != dev->partitions.end(); ++it)
					{
						if (cmd.args[0] == it->path)
						{
							break;
						}
					}

					if (it == dev->partitions.end())
					{
						dev->partitions.push_back(dtmd::partition(cmd.args[0], cmd.args[1], cmd.args[2]));
						title = QObject::trUtf8("Device attached");
						message = QString::fromLocal8Bit(cmd.args[2].empty() ? cmd.args[0].c_str() : cmd.args[2].c_str());
						modified = true;
					}
				}
			} // unlock
			else if ((cmd.cmd == "remove_partition") && (cmd.args.size() == 1) && (!cmd.args[0].empty()))
			{ // lock
				QMutexLocker devices_locker(&(ptr->m_devices_mutex));

				std::vector<dtmd::device>::iterator dev;
				for (dev = ptr->m_devices.begin(); dev != ptr->m_devices.end(); ++dev)
				{
					if (cmd.args[3] == dev->path)
					{
						break;
					}
				}

				if (dev != ptr->m_devices.end())
				{
					std::vector<dtmd::partition>::iterator it;
					for (it = dev->partitions.begin(); it != dev->partitions.end(); ++it)
					{
						if (cmd.args[0] == it->path)
						{
							break;
						}
					}

					if (it != dev->partitions.end())
					{
						dev->partitions.erase(it);
						title = QObject::trUtf8("Device removed");
						message = QString::fromLocal8Bit(it->label.empty() ? it->path.c_str() : it->label.c_str());
						modified = true;
					}
				}
			} // unlock
			else if ((cmd.cmd == "mount") && (cmd.args.size() == 3) && (!cmd.args[0].empty())
				&& (!cmd.args[1].empty()) && (!cmd.args[2].empty()))
			{ // lock
				QMutexLocker devices_locker(&(ptr->m_devices_mutex));

				std::vector<dtmd::device>::iterator dev;
				for (dev = ptr->m_devices.begin(); (!modified) && (dev != ptr->m_devices.end()); ++dev)
				{
					std::vector<dtmd::partition>::iterator it;
					for (it = dev->partitions.begin(); it != dev->partitions.end(); ++it)
					{
						if (cmd.args[0] == it->path)
						{
							it->mnt_point = cmd.args[1];
							it->mnt_opts  = cmd.args[2];
							title = QObject::trUtf8("Device mounted");
							message = QString::fromLocal8Bit(it->label.empty() ? it->path.c_str() : it->label.c_str());
							modified = true;
							break;
						}
					}
				}
			} // unlock
			else if ((cmd.cmd == "unmount") && (cmd.args.size() == 2) && (!cmd.args[0].empty()) && (!cmd.args[1].empty()))
			{ // lock
				QMutexLocker devices_locker(&(ptr->m_devices_mutex));

				std::vector<dtmd::device>::iterator dev;
				for (dev = ptr->m_devices.begin(); (!modified) && (dev != ptr->m_devices.end()); ++dev)
				{
					std::vector<dtmd::partition>::iterator it;
					for (it = dev->partitions.begin(); it != dev->partitions.end(); ++it)
					{
						if (cmd.args[0] == it->path)
						{
							it->mnt_point.clear();
							it->mnt_opts.clear();
							title = QObject::trUtf8("Device unmounted");
							message = QString::fromLocal8Bit(it->label.empty() ? it->path.c_str() : it->label.c_str());
							modified = true;
							break;
						}
					}
				}
			} // unlock

			if (modified)
			{
				ptr->triggerBuildMenu();
				ptr->showMessage(notify, title, message, QSystemTrayIcon::Information, Control::defaultTimeout);
			}
		}
		catch (const std::exception &e)
		{
			QMessageBox::critical(NULL, QObject::trUtf8("Fatal error"), QObject::trUtf8("Runtime error") + QString("\n") + QObject::trUtf8("Error message: ") + QString::fromLocal8Bit(e.what()));
			ptr->exit();
		}
		catch (...)
		{
			QMessageBox::critical(NULL, QObject::trUtf8("Fatal error"), QObject::trUtf8("Runtime error"));
			ptr->exit();
		}
	}
	else
	{
		ptr->exit();
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

void Control::slotShowMessage(app_state state, QString title, QString message, QSystemTrayIcon::MessageIcon icon, int millisecondsTimeoutHint)
{
	m_tray.showMessage(title, message, icon, millisecondsTimeoutHint);
	setIconState(state, millisecondsTimeoutHint);
}

void Control::slotBuildMenu()
{
	BuildMenu();
}