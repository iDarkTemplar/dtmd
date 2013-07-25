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

#ifndef DTMD_LIBRARY_H
#define DTMD_LIBRARY_H

#include "dtmd-commands.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dtmd_library dtmd_t;
typedef void (*dtmd_callback)(void *arg, struct dtmd_command *cmd);

enum dtmd_result
{
	dtmd_ok = 0,
	dtmd_library_not_initialized = -1,
	dtmd_input_error = -2,
	dtmd_io_error = -3
};

dtmd_t* dtmd_init(dtmd_callback callback, void *arg);
void dtmd_deinit(dtmd_t *handle);

int dtmd_enum_devices(dtmd_t *handle);
int dtmd_list_device(dtmd_t *handle, const char *device_path);
int dtmd_mount(dtmd_t *handle, const char *path, const char *mount_point, const char *mount_options);
int dtmd_unmount(dtmd_t *handle, const char *path, const char *mount_point);

/*
	"add_disk(path, type)"
	"remove_disk(path)"
	"add_partition(path, fstype, label (optional, may be NULL), parent_path)"
	"remove_partition(path)"
	"mount(path, mount_point, mount_options?)" or mount_failed
	"unmount(path, mount_point)" or unmount_failed
*/

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LIBRARY_H */
