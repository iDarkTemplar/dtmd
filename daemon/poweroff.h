/*
 * Copyright (C) 2019 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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

#ifndef POWEROFF_H
#define POWEROFF_H

#include <dtmd.h>

#include "daemon/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

int invoke_poweroff(struct client *client_ptr, const char *path, dtmd_error_code_t *error_code);

#ifdef __cplusplus
}
#endif

#endif /* POWEROFF_H */
