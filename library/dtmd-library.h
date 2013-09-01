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

#include <dtmd.h>
#include <dtmd-commands.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dtmd_library dtmd_t;
typedef void (*dtmd_callback_t)(void *arg, const dtmd_command_t *cmd);

typedef enum dtmd_result
{
	dtmd_ok = 0,
	// non-fatal errors
	dtmd_command_failed = -1,
	dtmd_timeout = -2,
	// fatal errors
	dtmd_invalid_state = -3,
	dtmd_library_not_initialized = -4,
	dtmd_input_error = -5,
	dtmd_io_error = -6,
	dtmd_time_error = -7,
	dtmd_memory_error = -8
} dtmd_result_t;

typedef struct dtmd_partition
{
	char *path;
	char *type;
	char *label; // optional
	char *mnt_point; // optional
	char *mnt_opts; // optional
} dtmd_partition_t;

typedef struct dtmd_device
{
	char *path;
	dtmd_removable_media_type_t type;
	unsigned int partitions_count;
	dtmd_partition_t **partition;
} dtmd_device_t;

dtmd_t* dtmd_init(dtmd_callback_t callback, void *arg);
void dtmd_deinit(dtmd_t *handle);

// timeout is in milliseconds, negative for infinite
dtmd_result_t dtmd_enum_devices(dtmd_t *handle, int timeout, unsigned int *count, dtmd_device_t ***result);
dtmd_result_t dtmd_list_device(dtmd_t *handle, int timeout, const char *device_path, dtmd_device_t **result);
dtmd_result_t dtmd_list_partition(dtmd_t *handle, int timeout, const char *partition_path, dtmd_partition_t **result);
dtmd_result_t dtmd_mount(dtmd_t *handle, int timeout, const char *path, const char *mount_point, const char *mount_options);
dtmd_result_t dtmd_unmount(dtmd_t *handle, int timeout, const char *path, const char *mount_point);
int dtmd_is_state_invalid(dtmd_t *handle);

void dtmd_free_devices_array(dtmd_t *handle, unsigned int count, dtmd_device_t **devices);
void dtmd_free_device(dtmd_t *handle, dtmd_device_t *device);
void dtmd_free_partition(dtmd_t *handle, dtmd_partition_t *partition);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LIBRARY_H */