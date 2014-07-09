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

#ifndef DTMD_H
#define DTMD_H

#define dtmd_daemon_socket_addr "/var/run/dtmd.socket"
#define dtmd_command_max_length 4096

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dtmd_removable_media_type
{
	dtmd_removable_media_unknown_or_persistent = 0,
	dtmd_removable_media_cdrom                 = 1,
	dtmd_removable_media_removable_disk        = 2,
	dtmd_removable_media_sd_card               = 3
} dtmd_removable_media_type_t;

typedef enum dtmd_removable_media_state
{
	dtmd_removable_media_state_unknown = 0,
	dtmd_removable_media_state_empty   = 1,
	dtmd_removable_media_state_clear   = 2,
	dtmd_removable_media_state_ok      = 3
} dtmd_removable_media_state_t;

#ifdef __cplusplus
}
#endif

#define dtmd_string_device_unknown_or_persistent "unknown"
#define dtmd_string_device_cdrom                 "cdrom"
#define dtmd_string_device_removable_disk        "removable disk"
#define dtmd_string_device_sd_card               "sdcard"

#define dtmd_string_state_unknown "unknown"
#define dtmd_string_state_empty   "empty"
#define dtmd_string_state_clear   "clear"
#define dtmd_string_state_ok      "ok"

/* Notification types */

#define dtmd_notification_add_disk "add_disk"
/* parameters: path, type */

#define dtmd_notification_remove_disk "remove_disk"
/* parameters: path */

#define dtmd_notification_disk_changed "disk_changed"
/* parameters: path, type */

#define dtmd_notification_add_partition "add_partition"
/* parameters: path, fstype (or NULL), label (or NULL), parent_path */

#define dtmd_notification_remove_partition "remove_partition"
/* parameters: path */

#define dtmd_notification_partition_changed "partition_changed"
/* parameters: path, fstype (or NULL), label (or NULL), parent_path */

#define dtmd_notification_add_stateful_device "add_stateful_device"
/* parameters: path, type, state, fstype (or NULL), label (or NULL) */

#define dtmd_notification_remove_stateful_device "remove_stateful_device"
/* parameters: path */

#define dtmd_notification_stateful_device_changed "stateful_device_changed"
/* parameters: path, type, state, fstype (or NULL), label (or NULL) */

#define dtmd_notification_mount "mount"
/* parameters: path, mount_point, mount_options */

#define dtmd_notification_unmount "unmount"
/* parameters: path, mount_point */

/* Commands and responses */

#define dtmd_command_enum_all "enum_all"
/*
 *	input: none
 *
 *	returns:
 *		devices: count
 *		device: "path, type, partitions_count"
 *		partition: "path, fstype (or NULL), label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
 *		stateful_devices: count
 *		stateful_device: "path, type, state, fstype (or NULL), label (or NULL), mount_point (or NULL), mount_options (or NULL)"
 *
 *		or "failed" on fail
 */

#define dtmd_command_list_device "list_device"
/*
 *	input:
 *		"device path"
 *
 *	returns:
 *		device: "path, type, partitions_count"
 *		partition: "path, fstype (or NULL), label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
 *
 *		or "failed" on fail
 */

#define dtmd_command_list_partition "list_partition"
/*
 *	input:
 *		"partition path"
 *
 *	returns:
 *		partition: "path, fstype (or NULL), label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
 *
 *		or "failed" on fail
 */

#define dtmd_command_list_stateful_device "list_stateful_device"
/*
 *	input:
 *		"device path"
 *
 *	returns:
 *		stateful device: "path, type, state, fstype (or NULL), label (or NULL), mount_point (or NULL), mount_options (or NULL)"
 *
 *		or "failed" on fail
 */

#define dtmd_command_mount "mount"
/*
 *	input:
 *		"path, mount_options"
 *
 *	returns:
 *		"succeeded" or "failed"
 */

#define dtmd_command_unmount "unmount"
/*
 *	input:
 *		"path"
 *
 *	returns:
 *		"succeeded" or "failed"
 */

#define dtmd_command_list_supported_filesystems "list_supported_filesystems"
/*
 *	input: none
 *
 *	returns:
 *		list of filesystem names
 *
 *		or "failed" on fail
 */

#define dtmd_command_list_supported_filesystem_options "list_supported_filesystem_options"
/*
 *	input:
 *		"filesystem"
 *
 *	returns:
 *		list of filesystem option names
 *
 *		or "failed" on fail
 */

#define dtmd_response_started "started"
#define dtmd_response_finished "finished"
#define dtmd_response_succeeded "succeeded"
#define dtmd_response_failed "failed"

#define dtmd_response_argument_devices "devices"
#define dtmd_response_argument_device "device"
#define dtmd_response_argument_partition "partition"
#define dtmd_response_argument_stateful_devices "stateful_devices"
#define dtmd_response_argument_stateful_device "stateful_device"
#define dtmd_response_argument_supported_filesystems_lists "supported_filesystems_list"
#define dtmd_response_argument_supported_filesystem_options_lists "supported_filesystem_options_list"

#endif /* DTMD_H */
