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

#ifndef QCUSTOMACTION_HPP
#define QCUSTOMACTION_HPP

#include <QAction>

class QCustomAction : public QAction
{
	Q_OBJECT

public:
	explicit QCustomAction(QObject *parent = 0);
	QCustomAction(const QString &text, QObject *parent, unsigned int device, unsigned int partition, const QString &partition_name);

signals:
	void triggered(unsigned int, unsigned int, QString);

public slots:

protected:
	unsigned int m_device;
	unsigned int m_partition;
	QString m_partition_name;

protected slots:
	void retrigger();
};

#endif // QCUSTOMACTION_HPP
