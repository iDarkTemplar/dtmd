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

#ifndef DTMD_DEVICE_H
#define DTMD_DEVICE_H

#include <dtmd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dtmd_info_type
{
	dtmd_info_unknown = 0,
	dtmd_info_device,
	dtmd_info_partition,
	dtmd_info_stateful_device
} dtmd_info_type_t;

typedef struct dtmd_info
{
	dtmd_info_type_t type;

	/* common for all media */
	const char *path;
	dtmd_removable_media_type_t media_type;

	/* common for partition and non-empty cdroms */
	const char *fstype;
	const char *label;

	/* for partition only */
	const char *path_parent;

	/* for cdroms only */
	dtmd_removable_media_state_t state;

	/* common for all media */
	void *private_data;
} dtmd_info_t;

typedef enum dtmd_device_action_type
{
	dtmd_device_action_unknown = 0,
	dtmd_device_action_add,
	dtmd_device_action_online,
	dtmd_device_action_remove,
	dtmd_device_action_offline,
	dtmd_device_action_change
} dtmd_device_action_type_t;

typedef struct dtmd_device_system      dtmd_device_system_t;
typedef struct dtmd_device_enumeration dtmd_device_enumeration_t;
typedef struct dtmd_device_monitor     dtmd_device_monitor_t;

dtmd_device_system_t* device_system_init();
void device_system_deinit(dtmd_device_system_t *system);

dtmd_device_enumeration_t* device_system_enumerate_devices(dtmd_device_system_t *system);
void device_system_finish_enumerate_devices(dtmd_device_enumeration_t *enumeration);

int device_system_next_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t **device);
void device_system_free_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device);

dtmd_device_monitor_t* device_system_start_monitoring(dtmd_device_system_t *system);
void device_system_stop_monitoring(dtmd_device_monitor_t *monitor);

int device_system_get_monitor_fd(dtmd_device_monitor_t *monitor);

int device_system_monitor_get_device(dtmd_device_monitor_t *monitor, dtmd_info_t **device, dtmd_device_action_type_t *action);
void device_system_monitor_free_device(dtmd_device_monitor_t *monitor, dtmd_info_t *device);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_DEVICE_H */