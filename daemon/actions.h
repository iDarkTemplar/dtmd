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

#include "library/dtmd-commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
	Notification types:
	"add_disk(path, type)"
	"remove_disk(path)"
	"add_partition(path, fstype, label (optional, may be NULL), parent_path)"
	"remove_partition(path)"
	"mount(path, mount_point, mount_options?)" or mount_failed
	"unmount(path, mount_point)" or unmount_failed

	Commands:
	"enum_all"
		returns:
		device: "path, type"
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
	"list_device":
		input:
		"device path"
		returns:
		device: "path, type"
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
	"mount"
		input:
		"path, mount_point, mount_options?"
		returns:
		broadcast "mount"
		or
		single "mount_failed"
	"unmount"
		input:
		"path, mount_point"
		returns:
		broadcast "unmount"
		or
		single "mount_failed"
*/

int invoke_command(int client_number, struct dtmd_command *cmd);

int send_notification(const char *type, const char *device);

int print_device(int client_number, struct removable_media *media);

int print_partition(int client_number, struct removable_media *media, unsigned int partition);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_NOTIFY_H */
