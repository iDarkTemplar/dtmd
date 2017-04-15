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

#include <QApplication>
#include <QTextCodec>
#include <QMessageBox>
#include <QScopedPointer>
#include <QLocale>

#include <signal.h>

#include "client/qt/control.hpp"

void signal_handler(int signum)
{
	switch (signum)
	{
	case SIGTERM:
	case SIGINT:
		QApplication::exit();
		break;
	}
}

int setSigHandlers(void)
{
	struct sigaction action;

	action.sa_handler = signal_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	if (sigaction(SIGTERM, &action, NULL) < 0)
	{
		return -1;
	}

	if (sigaction(SIGINT, &action, NULL) < 0)
	{
		return -1;
	}

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	QScopedPointer<Control> control;

#if QT_VERSION >= 0x050000
	QTextCodec::setCodecForLocale(
#else
	QTextCodec::setCodecForTr(
#endif
		QTextCodec::codecForName("UTF-8")
	);

	QLocale::setDefault(QLocale::system());

	if (!QSystemTrayIcon::isSystemTrayAvailable())
	{
		QMessageBox::critical(NULL, QObject::tr("Fatal error"), QObject::tr("System tray is not supported"));
		return 0;
	}

	if (setSigHandlers() != 0)
	{
		QMessageBox::critical(NULL, QObject::tr("Fatal error"), QObject::tr("Could not set signal handlers"));
		return 0;
	}

	try
	{
		control.reset(new Control());
	}
	catch (const std::exception &e)
	{
		QMessageBox::critical(NULL, QObject::tr("Fatal error"), QObject::tr("Initialization failed") + QString("\n") + QObject::tr("Error message: ") + QString::fromLocal8Bit(e.what()));
		return 0;
	}
	catch (...)
	{
		QMessageBox::critical(NULL, QObject::tr("Fatal error"), QObject::tr("Initialization failed"));
		return 0;
	}

	return app.exec();
}
