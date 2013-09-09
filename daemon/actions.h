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

#ifndef DTMD_NOTIFY_H
#define DTMD_NOTIFY_H

#include "daemon/lists.h"

#include <dtmd-misc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
	Notification types:
	"add_disk(path, type)"
	"remove_disk(path)"
	"add_partition(path, fstype, label (optional, may be NULL), parent_path)"
	"remove_partition(path)"
	"mount(path, mount_point, mount_options)"
	"unmount(path, mount_point)"

	command responses start with "started" or "failed" or "succeeded"
	and finish with "finished" if "started"

	Commands:
	"enum_all"
		returns:
		devices: count
		device: "path, type, partitions_count"
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
		or "failed" on fail
	"list_device":
		input:
		"device path"
		returns:
		device: "path, type, partitions_count"
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
		or "failed" on fail
	"list_partition":
		input:
		"partition path"
		returns:
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
		or "failed" on fail
	"mount"
		input:
		"path, mount_point, mount_options"
		returns:
		"succeeded" or "failed"
	"unmount"
		input:
		"path, mount_point"
		returns:
		"succeeded" or "failed"
*/

int invoke_command(unsigned int client_number, dtmd_command_t *cmd);

int notify_add_disk(const char *path, dtmd_removable_media_type_t type);
int notify_remove_disk(const char *path);
int notify_add_partition(const char *path, const char *fstype, const char *label, const char *parent_path);
int notify_remove_partition(const char *path);
int notify_mount(const char *path, const char *mount_point, const char *mount_options);
int notify_unmount(const char *path, const char *mount_point);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_NOTIFY_H */
