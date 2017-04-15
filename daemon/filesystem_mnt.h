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

#ifndef FILESYSTEM_MNT_H
#define FILESYSTEM_MNT_H

#include <dtmd.h>
#include "daemon/config_file.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: invoke_unmount and invoke_unmount_all can take -1 as client number meaning the client is daemon itself */

int invoke_mount(int client_number, const char *path, const char *mount_options, enum mount_by_value_enum mount_type, dtmd_error_code_t *error_code);
int invoke_unmount(int client_number, const char *path, dtmd_error_code_t *error_code);

int invoke_unmount_all(int client_number);

#ifdef __cplusplus
}
#endif

#endif /* FILESYSTEM_MNT_H */
