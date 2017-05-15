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
#include <dtmd-misc.h>

#include <dt-command.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define dtmd_library_timeout_infinite (-1)

typedef enum dtmd_state
{
	dtmd_state_connected,
	dtmd_state_disconnected,
	dtmd_state_failure
} dtmd_state_t;

typedef struct dtmd_library dtmd_t;
typedef void (*dtmd_callback_t)(dtmd_t *library, void *arg, const dt_command_t *cmd);
typedef void (*dtmd_state_callback_t)(dtmd_t *library, void *arg, dtmd_state_t state);

typedef enum dtmd_result
{
	dtmd_ok = 0,
	// non-fatal errors
	dt_command_failed = -1,
	dtmd_timeout = -2,
	dtmd_not_connected = -3,
	dtmd_io_error = -4,
	// fatal errors
	dtmd_invalid_state = -5,
	dtmd_fatal_io_error = -6,
	dtmd_library_not_initialized = -7,
	dtmd_input_error = -8,
	dtmd_time_error = -9,
	dtmd_memory_error = -10,
	dtmd_internal_initialization_error = -11,
	dtmd_daemon_not_responding_error = -12,
	dtmd_label_decoding_error = -13
} dtmd_result_t;

typedef enum dtmd_fill_type
{
	dtmd_fill_copy = 0, // copy the data from dt_command to structure
	dtmd_fill_link = 1 // link fields in structure to fields in dt_commands, thus structure remains valid only while dt_command exists unmodified
} dtmd_fill_type_t;

dtmd_t* dtmd_init(dtmd_callback_t callback, dtmd_state_callback_t state_callback, void *arg, dtmd_result_t *result);
void dtmd_deinit(dtmd_t *handle);

// timeout is in milliseconds, negative for infinite
dtmd_result_t dtmd_list_all_removable_devices(dtmd_t *handle, int timeout, dtmd_removable_media_t **result_list);
dtmd_result_t dtmd_list_removable_device(dtmd_t *handle, int timeout, const char *device_path, dtmd_removable_media_t **result_list);
dtmd_result_t dtmd_mount(dtmd_t *handle, int timeout, const char *path, const char *mount_options);
dtmd_result_t dtmd_unmount(dtmd_t *handle, int timeout, const char *path);
dtmd_result_t dtmd_list_supported_filesystems(dtmd_t *handle, int timeout, size_t *supported_filesystems_count, const char ***supported_filesystems_list);
dtmd_result_t dtmd_list_supported_filesystem_options(dtmd_t *handle, int timeout, const char *filesystem, size_t *supported_filesystem_options_count, const char ***supported_filesystem_options_list);

dtmd_result_t dtmd_fill_removable_device_from_notification(dtmd_t *handle, const dt_command_t *cmd, dtmd_fill_type_t fill_type, dtmd_removable_media_t **result);

int dtmd_is_state_invalid(dtmd_t *handle);
int dtmd_is_notification_valid_removable_device(dtmd_t *handle, const dt_command_t *cmd);

// if error is command_failed, detailed error code can be with following function
dtmd_error_code_t dtmd_get_code_of_command_fail(dtmd_t *handle);

void dtmd_free_removable_devices(dtmd_t *handle, dtmd_removable_media_t *devices_list);
void dtmd_free_supported_filesystems_list(dtmd_t *handle, size_t supported_filesystems_count, const char **supported_filesystems_list);
void dtmd_free_supported_filesystem_options_list(dtmd_t *handle, size_t supported_filesystem_options_count, const char **supported_filesystem_options_list);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LIBRARY_H */
