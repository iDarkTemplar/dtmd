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

#ifndef DTMD_LOGS_H
#define DTMD_LOGS_H

#include "daemon/config_file.h"

#include <stdio.h>

#ifndef DISABLE_SYSLOG
#include <syslog.h>
#endif /* DISABLE_SYSLOG */

#ifndef DISABLE_SYSLOG
#define WRITE_LOG(priority, format) \
if (use_syslog) \
{ \
	syslog((priority), format); \
} \
else if (!daemonize) \
{ \
	fprintf(stderr, format "\n"); \
}
#else /* DISABLE_SYSLOG */
#define WRITE_LOG(priority, format) \
if (!daemonize) \
{ \
	fprintf(stderr, format "\n"); \
}
#endif /* DISABLE_SYSLOG */

#ifndef DISABLE_SYSLOG
#define WRITE_LOG_ARGS(priority, format, ...) \
if (use_syslog) \
{ \
	syslog((priority), format, __VA_ARGS__); \
} \
else if (!daemonize) \
{ \
	fprintf(stderr, format "\n", __VA_ARGS__); \
}
#else /* DISABLE_SYSLOG */
#define WRITE_LOG_ARGS(priority, format, ...) \
if (!daemonize) \
{ \
	fprintf(stderr, format "\n", __VA_ARGS__); \
}
#endif /* DISABLE_SYSLOG */

#endif /* DTMD_LOGS_H */
