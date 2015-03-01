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

int invoke_command(int client_number, dtmd_command_t *cmd);

void notify_add_disk(const char *path, dtmd_removable_media_type_t type);
void notify_remove_disk(const char *path);
void notify_disk_changed(const char *path, dtmd_removable_media_type_t type);

void notify_add_partition(const char *path, const char *fstype, const char *label, const char *parent_path);
void notify_remove_partition(const char *path);
void notify_partition_changed(const char *path, const char *fstype, const char *label, const char *parent_path);

void notify_add_stateful_device(const char *path, dtmd_removable_media_type_t type, dtmd_removable_media_state_t state, const char *fstype, const char *label);
void notify_remove_stateful_device(const char *path);
void notify_stateful_device_changed(const char *path, dtmd_removable_media_type_t type, dtmd_removable_media_state_t state, const char *fstype, const char *label);

void notify_mount(const char *path, const char *mount_point, const char *mount_options);
void notify_unmount(const char *path, const char *mount_point);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_NOTIFY_H */
