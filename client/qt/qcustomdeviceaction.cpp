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

#include "client/qt/qcustomdeviceaction.hpp"

QCustomDeviceAction::QCustomDeviceAction(QObject *parent)
	: QAction(parent)
{
	QObject::connect(this, SIGNAL(triggered()), this, SLOT(retrigger()), Qt::DirectConnection);
}

QCustomDeviceAction::QCustomDeviceAction(const QString &text, QObject *parent, const std::string &device_name)
	: QAction(text, parent),
	m_device_name(device_name)
{
	QObject::connect(this, SIGNAL(triggered()), this, SLOT(retrigger()), Qt::DirectConnection);
}

void QCustomDeviceAction::retrigger()
{
	emit triggered(m_device_name);
}
