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

#include <dt-command.h>

#ifdef __cplusplus
extern "C" {
#endif

int invoke_command(struct client *client_ptr, dt_command_t *cmd);

void notify_removable_device_added(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts);

void notify_removable_device_removed(const char *path);

void notify_removable_device_changed(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts);

void notify_removable_device_mounted(const char *path, const char *mount_point, const char *mount_options);
void notify_removable_device_unmounted(const char *path, const char *mount_point);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_NOTIFY_H */
