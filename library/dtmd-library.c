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

#if (defined OS_FreeBSD)
#define _WITH_DPRINTF
#endif /* (defined OS_FreeBSD) */

#include <dtmd-library.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <poll.h>
#include <stdio.h>
#include <time.h>

#define dtmd_removable_media_internal_state_fields_are_linked  (1<<0)

typedef enum dtmd_library_state
{
	dtmd_state_default,
	dtmd_state_in_list_all_removable_devices,
	dtmd_state_in_list_removable_device,
	dtmd_state_in_list_supported_filesystems,
	dtmd_state_in_list_supported_filesystem_options
} dtmd_library_state_t;

typedef enum dtmd_internal_fill_type
{
	dtmd_internal_fill_copy = 0,
	dtmd_internal_fill_link = 1,
	dtmd_internal_fill_move = 2
} dtmd_internal_fill_type_t;

struct dtmd_library
{
	dtmd_callback_t callback;
	void *callback_arg;
	pthread_t worker;
	int pipes[2];
	int feedback[2];
	int socket_fd;
	dtmd_result_t result_state;
	dtmd_error_code_t error_code;
	dtmd_library_state_t library_state;

	sem_t caller_socket;

	size_t cur_pos;
	char buffer[dtmd_command_max_length + 1];
};

typedef enum dtmd_helper_result
{
	dtmd_helper_result_ok,
	dtmd_helper_result_exit,
	dtmd_helper_result_error
} dtmd_helper_result_t;

typedef struct dtmd_helper_params_list_removable_device
{
	const char *device_path;
} dtmd_helper_params_list_removable_device_t;

typedef struct dtmd_helper_params_mount
{
	const char *path;
	const char *mount_options;
} dtmd_helper_params_mount_t;

typedef struct dtmd_helper_params_unmount
{
	const char *path;
} dtmd_helper_params_unmount_t;

typedef struct dtmd_helper_params_list_supported_filesystem_options
{
	const char *filesystem;
} dtmd_helper_params_list_supported_filesystem_options_t;

typedef struct dtmd_helper_state_list_all_removable_devices
{
	int got_started;
	dtmd_removable_media_t *result;
} dtmd_helper_state_list_all_removable_devices_t;

typedef struct dtmd_helper_state_list_removable_device
{
	int got_started;
	dtmd_removable_media_t *result;
} dtmd_helper_state_list_removable_device_t;

typedef struct dtmd_helper_state_list_supported_filesystems
{
	int got_started;
	int got_result;
	size_t result_count;
	const char **result_list;
} dtmd_helper_state_list_supported_filesystems_t;

typedef struct dtmd_helper_state_list_supported_filesystem_options
{
	int got_started;
	int got_result;
	size_t result_count;
	const char **result_list;
} dtmd_helper_state_list_supported_filesystem_options_t;

static void* dtmd_worker_function(void *arg);

static int dtmd_helper_fill_data(char **where, char **from, dtmd_internal_fill_type_t internal_fill_type);
static dtmd_result_t dtmd_fill_removable_device_from_notification_implementation(dtmd_t *handle, dt_command_t *cmd, dtmd_internal_fill_type_t internal_fill_type, dtmd_removable_media_t **result);

static dtmd_result_t dtmd_helper_handle_cmd(dtmd_t *handle, dt_command_t *cmd);
static dtmd_result_t dtmd_helper_handle_callback_cmd(dtmd_t *handle, dt_command_t *cmd);
static dtmd_result_t dtmd_helper_wait_for_input(int handle, int timeout);
static int dtmd_helper_is_state_invalid(dtmd_result_t result);

static int dtmd_helper_is_helper_list_all_removable_devices_common(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_all_removable_devices_generic(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_all_removable_devices_failed(dt_command_t *cmd);

static int dtmd_helper_is_helper_list_removable_device_common(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_removable_device_generic(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_removable_device_failed(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_removable_device_parameters_match(dt_command_t *cmd, const char *device_path);

static int dtmd_helper_is_helper_mount_common(dt_command_t *cmd);
static int dtmd_helper_is_helper_mount_generic(dt_command_t *cmd);
static int dtmd_helper_is_helper_mount_failed(dt_command_t *cmd);
static int dtmd_helper_is_helper_mount_parameters_match(dt_command_t *cmd, const char *path, const char *mount_options);

static int dtmd_helper_is_helper_unmount_common(dt_command_t *cmd);
static int dtmd_helper_is_helper_unmount_generic(dt_command_t *cmd);
static int dtmd_helper_is_helper_unmount_failed(dt_command_t *cmd);
static int dtmd_helper_is_helper_unmount_parameters_match(dt_command_t *cmd, const char *path);

static int dtmd_helper_is_helper_list_supported_filesystems_common(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_supported_filesystems_generic(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_supported_filesystems_failed(dt_command_t *cmd);

static int dtmd_helper_is_helper_list_supported_filesystem_options_common(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_supported_filesystem_options_generic(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_supported_filesystem_options_failed(dt_command_t *cmd);
static int dtmd_helper_is_helper_list_supported_filesystem_options_parameters_match(dt_command_t *cmd, const char *filesystem);

static int dtmd_helper_cmd_check_removable_device_common(const dt_command_t *cmd);
static int dtmd_helper_cmd_check_removable_device(const dt_command_t *cmd);
static int dtmd_helper_cmd_check_supported_filesystems(const dt_command_t *cmd);
static int dtmd_helper_cmd_check_supported_filesystem_options(const dt_command_t *cmd);

static void dtmd_helper_free_removable_device_recursive(dtmd_removable_media_t *device);
static void dtmd_helper_free_removable_device(dtmd_removable_media_t *device);
static void dtmd_helper_free_supported_filesystems(size_t supported_filesystems_count, const char **supported_filesystems_list);
static void dtmd_helper_free_supported_filesystem_options(size_t supported_filesystem_options_count, const char **supported_filesystem_options_list);

static dtmd_result_t dtmd_helper_capture_socket(dtmd_t *handle, int timeout, struct timespec *time_cur, struct timespec *time_end);
static dtmd_result_t dtmd_helper_read_data(dtmd_t *handle, int timeout, struct timespec *time_cur, struct timespec *time_end);

static int dtmd_helper_dprintf_list_all_removable_devices(dtmd_t *handle, void *args);
static int dtmd_helper_dprintf_list_removable_device(dtmd_t *handle, void *args);
static int dtmd_helper_dprintf_mount(dtmd_t *handle, void *args);
static int dtmd_helper_dprintf_unmount(dtmd_t *handle, void *args);
static int dtmd_helper_dprintf_list_supported_filesystems(dtmd_t *handle, void *args);
static int dtmd_helper_dprintf_list_supported_filesystem_options(dtmd_t *handle, void *args);

typedef int (*dtmd_helper_dprintf_func_t)(dtmd_t *handle, void *args);
typedef dtmd_helper_result_t (*dtmd_helper_process_func_t)(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);
typedef int (*dtmd_helper_exit_func_t)(dtmd_t *handle, void *state);
typedef void (*dtmd_helper_exit_clear_func_t)(void *state);

dtmd_result_t dtmd_helper_generic_process(dtmd_t *handle, int timeout, void *params, void *state, dtmd_helper_dprintf_func_t dprintf_func, dtmd_helper_process_func_t process_func, dtmd_helper_exit_func_t exit_func, dtmd_helper_exit_clear_func_t exit_clear_func);

static dtmd_helper_result_t dtmd_helper_process_list_all_removable_devices(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);
static dtmd_helper_result_t dtmd_helper_process_list_removable_device(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);
static dtmd_helper_result_t dtmd_helper_process_mount(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);
static dtmd_helper_result_t dtmd_helper_process_unmount(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);
static dtmd_helper_result_t dtmd_helper_process_list_supported_filesystems(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);
static dtmd_helper_result_t dtmd_helper_process_list_supported_filesystem_options(dtmd_t *handle, dt_command_t *cmd, void *params, void *state);

static int dtmd_helper_exit_list_all_removable_devices(dtmd_t *handle, void *state);
static void dtmd_helper_exit_clear_list_all_removable_devices(void *state);

static int dtmd_helper_exit_list_removable_device(dtmd_t *handle, void *state);
static void dtmd_helper_exit_clear_list_removable_device(void *state);

static int dtmd_helper_exit_list_supported_filesystems(dtmd_t *handle, void *state);
static void dtmd_helper_exit_clear_list_supported_filesystems(void *state);

static int dtmd_helper_exit_list_supported_filesystem_options(dtmd_t *handle, void *state);
static void dtmd_helper_exit_clear_list_supported_filesystem_options(void *state);

static void dtmd_helper_free_string_array(size_t count, const char **data);
static int dtmd_helper_validate_string_array(size_t count, const char **data);

dtmd_t* dtmd_init(dtmd_callback_t callback, void *arg, dtmd_result_t *result)
{
	dtmd_t *handle;
	struct sockaddr_un sockaddr;
	dtmd_result_t errorcode;
	/* char data = 0; */

	if (callback == NULL)
	{
		errorcode = dtmd_input_error;
		goto dtmd_init_error_1;
	}

	handle = (dtmd_t*) malloc(sizeof(dtmd_t));
	if (handle == NULL)
	{
		errorcode = dtmd_memory_error;
		goto dtmd_init_error_1;
	}

	handle->callback      = callback;
	handle->callback_arg  = arg;
	handle->result_state  = dtmd_ok;
	handle->library_state = dtmd_state_default;
	handle->buffer[0]     = 0;
	handle->cur_pos       = 0;
	handle->error_code    = dtmd_error_code_unknown;

	if (sem_init(&(handle->caller_socket), 0, 0) == -1)
	{
		errorcode = dtmd_internal_initialization_error;
		goto dtmd_init_error_2;
	}

	if (pipe(handle->feedback) == -1)
	{
		errorcode = dtmd_internal_initialization_error;
		goto dtmd_init_error_3;
	}

	if (pipe(handle->pipes) == -1)
	{
		errorcode = dtmd_internal_initialization_error;
		goto dtmd_init_error_4;
	}

	handle->socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (handle->socket_fd == -1)
	{
		errorcode = dtmd_internal_initialization_error;
		goto dtmd_init_error_5;
	}

	sockaddr.sun_family = AF_LOCAL;
	memset(sockaddr.sun_path, 0, sizeof(sockaddr.sun_path));
	strncpy(sockaddr.sun_path, dtmd_daemon_socket_addr, sizeof(sockaddr.sun_path) - 1);

	if (connect(handle->socket_fd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un)) == -1)
	{
		errorcode = dtmd_daemon_not_responding_error;
		goto dtmd_init_error_6;
	}

	if ((pthread_create(&(handle->worker), NULL, &dtmd_worker_function, handle)) != 0)
	{
		errorcode = dtmd_internal_initialization_error;
		goto dtmd_init_error_6;
	}

	if (result != NULL)
	{
		*result = dtmd_ok;
	}

	return handle;
/*
dtmd_init_error_7:
	write(handle->pipes[1], &data, sizeof(char));
	pthread_join(handle->worker, NULL);
*/
dtmd_init_error_6:
	shutdown(handle->socket_fd, SHUT_RDWR);
	close(handle->socket_fd);

dtmd_init_error_5:
	close(handle->pipes[0]);
	close(handle->pipes[1]);

dtmd_init_error_4:
	close(handle->feedback[0]);
	close(handle->feedback[1]);

dtmd_init_error_3:
	sem_destroy(&(handle->caller_socket));

dtmd_init_error_2:
	free(handle);

dtmd_init_error_1:
	if (result != NULL)
	{
		*result = errorcode;
	}

	return NULL;
}

void dtmd_deinit(dtmd_t *handle)
{
	char data = 0;

	if (handle != NULL)
	{
		write(handle->pipes[1], &data, sizeof(char));
		pthread_join(handle->worker, NULL);

		shutdown(handle->socket_fd, SHUT_RDWR);
		close(handle->socket_fd);
		close(handle->pipes[0]);
		close(handle->pipes[1]);
		close(handle->feedback[0]);
		close(handle->feedback[1]);
		sem_destroy(&(handle->caller_socket));
		free(handle);
	}
}

static void* dtmd_worker_function(void *arg)
{
	dtmd_t *handle;
	struct pollfd fds[2];
	int rc;
	dt_command_t *cmd;
	char data;
	dtmd_result_t res;
	char *eol;

	handle = (dtmd_t*) arg;

	fds[0].fd = handle->pipes[0];
	fds[1].fd = handle->socket_fd;

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			if (!dt_validate_command(handle->buffer))
			{
				goto dtmd_worker_function_error;
			}

			cmd = dt_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				goto dtmd_worker_function_error;
			}

			res = dtmd_helper_handle_cmd(handle, cmd);
			dt_free_command(cmd);

			if (res != dtmd_ok)
			{
				goto dtmd_worker_function_error;
			}
		}

		fds[0].events  = POLLIN;
		fds[0].revents = 0;
		fds[1].events  = POLLIN;
		fds[1].revents = 0;

		rc = poll(fds, 2, -1);

		if ((rc == -1)
			|| (fds[0].revents & POLLERR)
			|| (fds[0].revents & POLLHUP)
			|| (fds[0].revents & POLLNVAL)
			|| (fds[1].revents & POLLERR)
			|| (fds[1].revents & POLLHUP)
			|| (fds[1].revents & POLLNVAL))
		{
			goto dtmd_worker_function_error;
		}

		if (fds[0].revents & POLLIN)
		{
			rc = read(handle->pipes[0], &data, sizeof(char));

			if (rc == 1)
			{
				if (data == 1)
				{
					// release ownership of socket and wait for return
					data = 1;
					write(handle->feedback[1], &data, sizeof(char));

					sem_wait(&(handle->caller_socket));
				}
				else
				{
					goto dtmd_worker_function_exit;
				}
			}
			else
			{
				goto dtmd_worker_function_error;
			}
		}
		else if (fds[1].revents & POLLIN)
		{
			rc = read(handle->socket_fd, &(handle->buffer[handle->cur_pos]), dtmd_command_max_length - handle->cur_pos);
			if (rc <= 0)
			{
				goto dtmd_worker_function_error;
			}

			handle->cur_pos += rc;
			handle->buffer[handle->cur_pos] = 0;
		}
	}

dtmd_worker_function_error:
	// Signal about error
	handle->callback(handle, handle->callback_arg, NULL);

dtmd_worker_function_exit:
	// Signal about exit
	data = 0;
	write(handle->feedback[1], &data, sizeof(char));

	pthread_exit(0);
}

dtmd_result_t dtmd_list_all_removable_devices(dtmd_t *handle, int timeout, dtmd_removable_media_t **result_list)
{
	dtmd_result_t res;

	dtmd_helper_state_list_all_removable_devices_t state;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (result_list == NULL)
	{
		return dtmd_input_error;
	}

	state.got_started = 0;
	state.result = NULL;

	res = dtmd_helper_generic_process(handle,
		timeout,
		NULL,
		&state,
		&dtmd_helper_dprintf_list_all_removable_devices,
		&dtmd_helper_process_list_all_removable_devices,
		&dtmd_helper_exit_list_all_removable_devices,
		&dtmd_helper_exit_clear_list_all_removable_devices);

	*result_list = state.result;

	return res;
}

dtmd_result_t dtmd_list_removable_device(dtmd_t *handle, int timeout, const char *device_path, dtmd_removable_media_t **result_list)
{
	dtmd_result_t res;

	dtmd_helper_params_list_removable_device_t params;
	dtmd_helper_state_list_removable_device_t state;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((device_path == NULL) || (*device_path == 0) || (result_list == NULL))
	{
		return dtmd_input_error;
	}

	params.device_path = device_path;

	state.got_started = 0;
	state.result = NULL;

	res = dtmd_helper_generic_process(handle,
		timeout,
		&params,
		&state,
		&dtmd_helper_dprintf_list_removable_device,
		&dtmd_helper_process_list_removable_device,
		&dtmd_helper_exit_list_removable_device,
		&dtmd_helper_exit_clear_list_removable_device);

	*result_list = state.result;

	return res;
}

dtmd_result_t dtmd_mount(dtmd_t *handle, int timeout, const char *path, const char *mount_options)
{
	dtmd_helper_params_mount_t params;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((path == NULL) || (*path == 0))
	{
		return dtmd_input_error;
	}

	params.path = path;
	params.mount_options = mount_options;

	return dtmd_helper_generic_process(handle,
		timeout,
		&params,
		NULL,
		&dtmd_helper_dprintf_mount,
		&dtmd_helper_process_mount,
		NULL,
		NULL);
}

dtmd_result_t dtmd_unmount(dtmd_t *handle, int timeout, const char *path)
{
	dtmd_helper_params_unmount_t params;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((path == NULL) || (*path == 0))
	{
		return dtmd_input_error;
	}

	params.path = path;

	return dtmd_helper_generic_process(handle,
		timeout,
		&params,
		NULL,
		&dtmd_helper_dprintf_unmount,
		&dtmd_helper_process_unmount,
		NULL,
		NULL);
}

dtmd_result_t dtmd_list_supported_filesystems(dtmd_t *handle, int timeout, size_t *supported_filesystems_count, const char ***supported_filesystems_list)
{
	dtmd_result_t res;

	dtmd_helper_state_list_supported_filesystems_t state;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((supported_filesystems_count == NULL) || (supported_filesystems_list == NULL))
	{
		return dtmd_input_error;
	}

	state.got_started = 0;
	state.got_result = 0;
	state.result_list = NULL;
	state.result_count = 0;

	res = dtmd_helper_generic_process(handle,
		timeout,
		NULL,
		&state,
		&dtmd_helper_dprintf_list_supported_filesystems,
		&dtmd_helper_process_list_supported_filesystems,
		&dtmd_helper_exit_list_supported_filesystems,
		&dtmd_helper_exit_clear_list_supported_filesystems);

	*supported_filesystems_count = state.result_count;
	*supported_filesystems_list  = state.result_list;

	return res;
}

dtmd_result_t dtmd_list_supported_filesystem_options(dtmd_t *handle, int timeout, const char *filesystem, size_t *supported_filesystem_options_count, const char ***supported_filesystem_options_list)
{
	dtmd_result_t res;

	dtmd_helper_params_list_supported_filesystem_options_t params;
	dtmd_helper_state_list_supported_filesystem_options_t state;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((supported_filesystem_options_count == NULL) || (supported_filesystem_options_list == NULL))
	{
		return dtmd_input_error;
	}

	params.filesystem = filesystem;

	state.got_started = 0;
	state.got_result = 0;
	state.result_list = NULL;
	state.result_count = 0;

	res = dtmd_helper_generic_process(handle,
		timeout,
		&params,
		&state,
		&dtmd_helper_dprintf_list_supported_filesystem_options,
		&dtmd_helper_process_list_supported_filesystem_options,
		&dtmd_helper_exit_list_supported_filesystem_options,
		&dtmd_helper_exit_clear_list_supported_filesystem_options);

	*supported_filesystem_options_count = state.result_count;
	*supported_filesystem_options_list  = state.result_list;

	return res;
}

static int dtmd_helper_fill_data(char **where, char **from, dtmd_internal_fill_type_t internal_fill_type)
{
	switch (internal_fill_type)
	{
	case dtmd_internal_fill_copy:
		if (*from != NULL)
		{
			*where = strdup(*from);
			if (*where == NULL)
			{
				return 0;
			}
		}
		else
		{
			*where = NULL;
		}

		return 1;
		break;

	case dtmd_internal_fill_move:
		*where = *from;
		*from = NULL;
		return 1;
		break;

	case dtmd_internal_fill_link:
		*where = *from;
		return 1;
		break;

	default:
		return dtmd_input_error;
		break;
	}

	return 0;
}

static dtmd_result_t dtmd_fill_removable_device_from_notification_implementation(dtmd_t *handle, dt_command_t *cmd, dtmd_internal_fill_type_t internal_fill_type, dtmd_removable_media_t **result)
{
	dtmd_removable_media_t *constructed_media = NULL;

	constructed_media = (dtmd_removable_media_t*) malloc(sizeof(dtmd_removable_media_t));
	if (constructed_media == NULL)
	{
		handle->result_state = dtmd_memory_error;
		goto dtmd_fill_removable_device_from_notification_implementation_error_1;
	}

	constructed_media->private_data = NULL;

	switch (internal_fill_type)
	{
	case dtmd_internal_fill_link:
		constructed_media->private_data = (void*) (dtmd_removable_media_internal_state_fields_are_linked);
		break;

	case dtmd_internal_fill_copy:
	case dtmd_internal_fill_move:
	default:
		break;
	}

	constructed_media->path         = NULL;
	constructed_media->type         = dtmd_removable_media_type_unknown_or_persistent;
	constructed_media->subtype      = dtmd_removable_media_subtype_unknown_or_persistent;
	constructed_media->state        = dtmd_removable_media_state_unknown;
	constructed_media->fstype       = NULL;
	constructed_media->label        = NULL;
	constructed_media->mnt_point    = NULL;
	constructed_media->mnt_opts     = NULL;
	constructed_media->parent       = NULL;
	constructed_media->first_child  = NULL;
	constructed_media->last_child   = NULL;
	constructed_media->next_node    = NULL;
	constructed_media->prev_node    = NULL;

	if (!dtmd_helper_fill_data(&(constructed_media->path), &(cmd->args[1]), internal_fill_type))
	{
		handle->result_state = dtmd_memory_error;
		goto dtmd_fill_removable_device_from_notification_implementation_error_2;
	}

	constructed_media->type         = dtmd_string_to_device_type(cmd->args[2]);

	switch (constructed_media->type)
	{
	case dtmd_removable_media_type_device_partition:
		if (!dtmd_helper_fill_data(&(constructed_media->fstype), &(cmd->args[3]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}

		if (!dtmd_helper_fill_data(&(constructed_media->label), &(cmd->args[4]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}

		if (!dtmd_helper_fill_data(&(constructed_media->mnt_point), &(cmd->args[5]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}

		if (!dtmd_helper_fill_data(&(constructed_media->mnt_opts), &(cmd->args[6]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}
		break;

	case dtmd_removable_media_type_stateless_device:
		constructed_media->subtype   = dtmd_string_to_device_subtype(cmd->args[3]);
		break;

	case dtmd_removable_media_type_stateful_device:
		constructed_media->subtype   = dtmd_string_to_device_subtype(cmd->args[3]);
		constructed_media->state     = dtmd_string_to_device_state(cmd->args[4]);

		if (!dtmd_helper_fill_data(&(constructed_media->fstype), &(cmd->args[5]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}

		if (!dtmd_helper_fill_data(&(constructed_media->label), &(cmd->args[6]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}

		if (!dtmd_helper_fill_data(&(constructed_media->mnt_point), &(cmd->args[7]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}

		if (!dtmd_helper_fill_data(&(constructed_media->mnt_opts), &(cmd->args[8]), internal_fill_type))
		{
			handle->result_state = dtmd_memory_error;
			goto dtmd_fill_removable_device_from_notification_implementation_error_2;
		}
		break;

	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		break;
	}

	*result = constructed_media;

	return dtmd_ok;

dtmd_fill_removable_device_from_notification_implementation_error_2:
	dtmd_helper_free_removable_device_recursive(constructed_media);

dtmd_fill_removable_device_from_notification_implementation_error_1:
	return handle->result_state;
}

dtmd_result_t dtmd_fill_removable_device_from_notification(dtmd_t *handle, const dt_command_t *cmd, dtmd_fill_type_t fill_type, dtmd_removable_media_t **result)
{
	dtmd_internal_fill_type_t internal_fill_type;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((cmd == NULL) || (result == NULL))
	{
		return dtmd_input_error;
	}

	switch (fill_type)
	{
	case dtmd_fill_copy:
		internal_fill_type = dtmd_internal_fill_copy;
		break;

	case dtmd_fill_link:
		internal_fill_type = dtmd_internal_fill_link;
		break;

	default:
		return dtmd_input_error;
		break;
	}

	if (!(dtmd_is_notification_valid_removable_device(handle, cmd)))
	{
		return dtmd_input_error;
	}

	/* it's safe to do a cast here since allowed fill types a read-only ones */
	return dtmd_fill_removable_device_from_notification_implementation(handle, (dt_command_t*) cmd, internal_fill_type, result);
}

int dtmd_is_state_invalid(dtmd_t *handle)
{
	if (handle == NULL)
	{
		return 1;
	}

	return dtmd_helper_is_state_invalid(handle->result_state);
}

int dtmd_is_notification_valid_removable_device(dtmd_t *handle, const dt_command_t *cmd)
{
	if (handle == NULL)
	{
		return 0;
	}

	return ((cmd->cmd != NULL)
		&& ((strcmp(cmd->cmd, dtmd_notification_removable_device_added) == 0)
			|| (strcmp(cmd->cmd, dtmd_notification_removable_device_changed) == 0))
		&& (dtmd_helper_cmd_check_removable_device_common(cmd)));
}

dtmd_error_code_t dtmd_get_code_of_command_fail(dtmd_t *handle)
{
	if (handle == NULL)
	{
		return dtmd_error_code_unknown;
	}

	return handle->error_code;
}

void dtmd_free_removable_devices(dtmd_t *handle, dtmd_removable_media_t *devices_list)
{
	if ((handle == NULL) || (devices_list == NULL))
	{
		return;
	}

	dtmd_helper_free_removable_device(devices_list);
}

void dtmd_free_supported_filesystems_list(dtmd_t *handle, size_t supported_filesystems_count, const char **supported_filesystems_list)
{
	if ((handle == NULL) || (supported_filesystems_list == NULL))
	{
		return;
	}

	dtmd_helper_free_supported_filesystems(supported_filesystems_count, supported_filesystems_list);
}

void dtmd_free_supported_filesystem_options_list(dtmd_t *handle, size_t supported_filesystem_options_count, const char **supported_filesystem_options_list)
{
	if ((handle == NULL) || (supported_filesystem_options_list == NULL))
	{
		return;
	}

	dtmd_helper_free_supported_filesystem_options(supported_filesystem_options_count, supported_filesystem_options_list);
}

static dtmd_result_t dtmd_helper_handle_cmd(dtmd_t *handle, dt_command_t *cmd)
{
	switch (handle->library_state)
	{
	case dtmd_state_default:
		if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
			&& ((dtmd_helper_is_helper_list_all_removable_devices_failed(cmd))
				|| (dtmd_helper_is_helper_list_removable_device_failed(cmd))
				|| (dtmd_helper_is_helper_mount_failed(cmd))
				|| (dtmd_helper_is_helper_unmount_failed(cmd))
				|| (dtmd_helper_is_helper_list_supported_filesystems_failed(cmd))
				|| (dtmd_helper_is_helper_list_supported_filesystem_options_failed(cmd))))
		{
			return dtmd_ok;
		}

		if ((strcmp(cmd->cmd, dtmd_response_succeeded) == 0)
			&& ((dtmd_helper_is_helper_mount_generic(cmd))
				|| (dtmd_helper_is_helper_unmount_generic(cmd))))
		{
			return dtmd_ok;
		}

		if (strcmp(cmd->cmd, dtmd_response_started) == 0)
		{
			if (dtmd_helper_is_helper_list_all_removable_devices_generic(cmd))
			{
				handle->library_state = dtmd_state_in_list_all_removable_devices;
				return dtmd_ok;
			}
			else if (dtmd_helper_is_helper_list_removable_device_generic(cmd))
			{
				handle->library_state = dtmd_state_in_list_removable_device;
				return dtmd_ok;
			}
			else if (dtmd_helper_is_helper_list_supported_filesystems_generic(cmd))
			{
				handle->library_state = dtmd_state_in_list_supported_filesystems;
				return dtmd_ok;
			}
			else if (dtmd_helper_is_helper_list_supported_filesystem_options_generic(cmd))
			{
				handle->library_state = dtmd_state_in_list_supported_filesystem_options;
				return dtmd_ok;
			}
		}

		return dtmd_helper_handle_callback_cmd(handle, cmd);
		break;

	case dtmd_state_in_list_all_removable_devices:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_all_removable_devices_generic(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_removable_device(cmd))
		{
			return dtmd_ok;
		}
		break;

	case dtmd_state_in_list_removable_device:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_removable_device_generic(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_removable_device(cmd))
		{
			return dtmd_ok;
		}
		break;

	case dtmd_state_in_list_supported_filesystems:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_supported_filesystems_generic(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_supported_filesystems(cmd))
		{
			return dtmd_ok;
		}
		break;

	case dtmd_state_in_list_supported_filesystem_options:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_supported_filesystem_options_generic(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_supported_filesystem_options(cmd))
		{
			return dtmd_ok;
		}
		break;
	}

	return dtmd_input_error;
}

static dtmd_result_t dtmd_helper_handle_callback_cmd(dtmd_t *handle, dt_command_t *cmd)
{
	if (   ((strcmp(cmd->cmd, dtmd_notification_removable_device_added)   == 0) && (dtmd_helper_cmd_check_removable_device_common(cmd)))
		|| ((strcmp(cmd->cmd, dtmd_notification_removable_device_removed) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_removable_device_changed) == 0) && (dtmd_helper_cmd_check_removable_device_common(cmd)))
		|| ((strcmp(cmd->cmd, dtmd_notification_removable_device_mounted) == 0) && (cmd->args_count == 3) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_removable_device_unmounted) == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL)))
	{
		handle->callback(handle, handle->callback_arg, cmd);
		return dtmd_ok;
	}
	else
	{
		return dtmd_input_error;
	}
}

static dtmd_result_t dtmd_helper_wait_for_input(int handle, int timeout)
{
	struct pollfd fd;
	int rc;

	fd.fd      = handle;
	fd.events  = POLLIN;
	fd.revents = 0;

	rc = poll(&fd, 1, timeout);

	switch (rc)
	{
	case 0:
		return dtmd_timeout;

	case -1:
		return dtmd_time_error;

	default:
		if (fd.revents & POLLIN)
		{
			return dtmd_ok;
		}
		else
		{
			return dtmd_invalid_state;
		}
	}
}

static int dtmd_helper_is_state_invalid(dtmd_result_t result)
{
	switch (result)
	{
		case dtmd_ok:
		case dtmd_timeout:
		case dt_command_failed:
			return 0;
		default:
			return 1;
	}
}

static int dtmd_helper_is_helper_list_all_removable_devices_common(dt_command_t *cmd)
{
	return (cmd->args[0] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_all_removable_devices) == 0);
}

static int dtmd_helper_is_helper_list_all_removable_devices_generic(dt_command_t *cmd)
{
	return (cmd->args_count == 1) && dtmd_helper_is_helper_list_all_removable_devices_common(cmd);
}

static int dtmd_helper_is_helper_list_all_removable_devices_failed(dt_command_t *cmd)
{
	return (cmd->args_count == 2) && (cmd->args[1] != NULL) && dtmd_helper_is_helper_list_all_removable_devices_common(cmd);
}

static int dtmd_helper_is_helper_list_removable_device_common(dt_command_t *cmd)
{
	return (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_removable_device) == 0);
}

static int dtmd_helper_is_helper_list_removable_device_generic(dt_command_t *cmd)
{
	return (cmd->args_count == 2) && dtmd_helper_is_helper_list_removable_device_common(cmd);
}

static int dtmd_helper_is_helper_list_removable_device_failed(dt_command_t *cmd)
{
	return (cmd->args_count == 3) && (cmd->args[2] != NULL) && dtmd_helper_is_helper_list_removable_device_common(cmd);
}

static int dtmd_helper_is_helper_list_removable_device_parameters_match(dt_command_t *cmd, const char *device_path)
{
	return (strcmp(cmd->args[1], device_path) == 0);
}

static int dtmd_helper_is_helper_mount_common(dt_command_t *cmd)
{
	return (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_mount) == 0);
}

static int dtmd_helper_is_helper_mount_generic(dt_command_t *cmd)
{
	return (cmd->args_count == 3) && dtmd_helper_is_helper_mount_common(cmd);
}

static int dtmd_helper_is_helper_mount_failed(dt_command_t *cmd)
{
	return (cmd->args_count == 4) && (cmd->args[3] != NULL) && dtmd_helper_is_helper_mount_common(cmd);
}

static int dtmd_helper_is_helper_mount_parameters_match(dt_command_t *cmd, const char *path, const char *mount_options)
{
	return (strcmp(cmd->args[1], path) == 0)
		&& (((cmd->args[2] != NULL) && (mount_options != NULL) && (strcmp(cmd->args[2], mount_options) == 0))
			|| ((cmd->args[2] == NULL) && (mount_options == NULL)));
}

static int dtmd_helper_is_helper_unmount_common(dt_command_t *cmd)
{
	return (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_unmount) == 0);
}

static int dtmd_helper_is_helper_unmount_generic(dt_command_t *cmd)
{
	return (cmd->args_count == 2) && dtmd_helper_is_helper_unmount_common(cmd);
}

static int dtmd_helper_is_helper_unmount_failed(dt_command_t *cmd)
{
	return (cmd->args_count == 3) && (cmd->args[2] != NULL) && dtmd_helper_is_helper_unmount_common(cmd);
}

static int dtmd_helper_is_helper_unmount_parameters_match(dt_command_t *cmd, const char *path)
{
	return (strcmp(cmd->args[1], path) == 0);
}

static int dtmd_helper_is_helper_list_supported_filesystems_common(dt_command_t *cmd)
{
	return (cmd->args[0] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_supported_filesystems) == 0);
}

static int dtmd_helper_is_helper_list_supported_filesystems_generic(dt_command_t *cmd)
{
	return (cmd->args_count == 1) && dtmd_helper_is_helper_list_supported_filesystems_common(cmd);
}

static int dtmd_helper_is_helper_list_supported_filesystems_failed(dt_command_t *cmd)
{
	return (cmd->args_count == 2) && (cmd->args[1] != NULL) && dtmd_helper_is_helper_list_supported_filesystems_common(cmd);
}

static int dtmd_helper_is_helper_list_supported_filesystem_options_common(dt_command_t *cmd)
{
	return (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_supported_filesystem_options) == 0);
}

static int dtmd_helper_is_helper_list_supported_filesystem_options_generic(dt_command_t *cmd)
{
	return (cmd->args_count == 2) && dtmd_helper_is_helper_list_supported_filesystem_options_common(cmd);
}

static int dtmd_helper_is_helper_list_supported_filesystem_options_failed(dt_command_t *cmd)
{
	return (cmd->args_count == 3) && (cmd->args[2] != NULL) && dtmd_helper_is_helper_list_supported_filesystem_options_common(cmd);
}

static int dtmd_helper_is_helper_list_supported_filesystem_options_parameters_match(dt_command_t *cmd, const char *filesystem)
{
	return (strcmp(cmd->args[1], filesystem) == 0);
}

static int dtmd_helper_cmd_check_removable_device_common(const dt_command_t *cmd)
{
	dtmd_removable_media_type_t type;
	dtmd_removable_media_subtype_t subtype;
	dtmd_removable_media_state_t state;

	if ((cmd->args_count >= 3) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
	{
		type = dtmd_string_to_device_type(cmd->args[2]);

		switch (type)
		{
		case dtmd_removable_media_type_device_partition:
			if (cmd->args_count != 7)
			{
				break;
			}

			return 1;
			break;

		case dtmd_removable_media_type_stateless_device:
			if ((cmd->args_count != 4) || (cmd->args[3] == NULL))
			{
				break;
			}

			subtype = dtmd_string_to_device_subtype(cmd->args[3]);
			if (subtype == dtmd_removable_media_subtype_unknown_or_persistent)
			{
				break;
			}

			return 1;
			break;

		case dtmd_removable_media_type_stateful_device:
			if ((cmd->args_count != 9) || (cmd->args[3] == NULL) || (cmd->args[4] == NULL))
			{
				break;
			}

			subtype = dtmd_string_to_device_subtype(cmd->args[3]);
			if (subtype == dtmd_removable_media_subtype_unknown_or_persistent)
			{
				break;
			}

			state = dtmd_string_to_device_state(cmd->args[4]);
			if (state == dtmd_removable_media_state_unknown)
			{
				break;
			}

			return 1;
			break;

		case dtmd_removable_media_type_unknown_or_persistent:
		default:
			break;
		}
	}

	return 0;
}

static int dtmd_helper_cmd_check_removable_device(const dt_command_t *cmd)
{
	return ((strcmp(cmd->cmd, dtmd_response_argument_removable_device) == 0) && (dtmd_helper_cmd_check_removable_device_common(cmd)));
}

static int dtmd_helper_cmd_check_supported_filesystems(const dt_command_t *cmd)
{
	return ((strcmp(cmd->cmd, dtmd_response_argument_supported_filesystems_lists) == 0) && (dtmd_helper_validate_string_array(cmd->args_count, (const char **) cmd->args)));
}

static int dtmd_helper_cmd_check_supported_filesystem_options(const dt_command_t *cmd)
{
	return ((strcmp(cmd->cmd, dtmd_response_argument_supported_filesystem_options_lists) == 0) && (dtmd_helper_validate_string_array(cmd->args_count, (const char **) cmd->args)));
}

static void dtmd_helper_free_removable_device_recursive(dtmd_removable_media_t *device)
{
	dtmd_removable_media_t *cur;
	dtmd_removable_media_t *next;

	// first unlink
	if (device->prev_node != NULL)
	{
		device->prev_node->next_node = device->next_node;
	}

	if (device->next_node != NULL)
	{
		device->next_node->prev_node = device->prev_node;
	}

	// then recursively free children
	next = device->first_child;

	while (next != NULL)
	{
		cur = next;
		next = cur->next_node;
		dtmd_helper_free_removable_device_recursive(cur);
	}

	// and free node itself
	if (!(((size_t) device->private_data) & dtmd_removable_media_internal_state_fields_are_linked))
	{
		if (device->path != NULL)
		{
			free(device->path);
		}

		if (device->fstype != NULL)
		{
			free(device->fstype);
		}

		if (device->label != NULL)
		{
			free(device->label);
		}

		if (device->mnt_point != NULL)
		{
			free(device->mnt_point);
		}

		if (device->mnt_opts != NULL)
		{
			free(device->mnt_opts);
		}
	}

	free(device);
}

static void dtmd_helper_free_removable_device(dtmd_removable_media_t *device)
{
	dtmd_removable_media_t *cur;
	dtmd_removable_media_t *next;

	next = device;

	while (next != NULL)
	{
		cur = next;
		next = cur->next_node;
		dtmd_helper_free_removable_device_recursive(cur);
	}
}

static void dtmd_helper_free_supported_filesystems(size_t supported_filesystems_count, const char **supported_filesystems_list)
{
	dtmd_helper_free_string_array(supported_filesystems_count, supported_filesystems_list);
}

static void dtmd_helper_free_supported_filesystem_options(size_t supported_filesystem_options_count, const char **supported_filesystem_options_list)
{
	dtmd_helper_free_string_array(supported_filesystem_options_count, supported_filesystem_options_list);
}

static dtmd_result_t dtmd_helper_capture_socket(dtmd_t *handle, int timeout, struct timespec *time_cur, struct timespec *time_end)
{
	char data = 1;
	dtmd_result_t res;
	int rc;

	write(handle->pipes[1], &data, sizeof(char));

	if (timeout >= 0)
	{
		if (clock_gettime(CLOCK_MONOTONIC, time_cur) == -1)
		{
			return dtmd_time_error;
		}

		time_end->tv_sec  = time_cur->tv_sec  + timeout / 1000;
		time_end->tv_nsec = time_cur->tv_nsec + (timeout % 1000) * 1000000;

		rc = (time_end->tv_sec - time_cur->tv_sec) * 1000 + (time_end->tv_nsec - time_cur->tv_nsec) / 1000000;
		if (rc < 0)
		{
			rc = 0;
		}

		res = dtmd_helper_wait_for_input(handle->feedback[0], rc);

		if (res != dtmd_ok)
		{
			return res;
		}
	}

	read(handle->feedback[0], &data, sizeof(char));

	if (data == 0)
	{
		return dtmd_invalid_state;
	}

	return dtmd_ok;
}

static dtmd_result_t dtmd_helper_read_data(dtmd_t *handle, int timeout, struct timespec *time_cur, struct timespec *time_end)
{
	dtmd_result_t res;
	int rc;

	if (handle->cur_pos == dtmd_command_max_length)
	{
		return dtmd_invalid_state;
	}

	if (timeout >= 0)
	{
		if (clock_gettime(CLOCK_MONOTONIC, time_cur) == -1)
		{
			return dtmd_time_error;
		}

		rc = (time_end->tv_sec - time_cur->tv_sec) * 1000 + (time_end->tv_nsec - time_cur->tv_nsec) / 1000000;
		if (rc < 0)
		{
			rc = 0;
		}

		res = dtmd_helper_wait_for_input(handle->socket_fd, rc);

		if (res != dtmd_ok)
		{
			return res;
		}
	}

	rc = read(handle->socket_fd, &(handle->buffer[handle->cur_pos]), dtmd_command_max_length - handle->cur_pos);
	if (rc <= 0)
	{
		return dtmd_io_error;
	}

	handle->cur_pos += rc;
	handle->buffer[handle->cur_pos] = 0;

	return dtmd_ok;
}

static int dtmd_helper_dprintf_list_all_removable_devices(dtmd_t *handle, void *args)
{
	return dprintf(handle->socket_fd, dtmd_command_list_all_removable_devices "()\n");
}

inline static int dtmd_helper_dprintf_list_removable_device_implementation(dtmd_t *handle, dtmd_helper_params_list_removable_device_t *args)
{
	return dprintf(handle->socket_fd, dtmd_command_list_removable_device "(\"%s\")\n", args->device_path);
}

static int dtmd_helper_dprintf_list_removable_device(dtmd_t *handle, void *args)
{
	return dtmd_helper_dprintf_list_removable_device_implementation(handle, (dtmd_helper_params_list_removable_device_t*) args);
}

inline static int dtmd_helper_dprintf_mount_implementation(dtmd_t *handle, dtmd_helper_params_mount_t *args)
{
	return dprintf(handle->socket_fd, dtmd_command_mount "(\"%s\", %s%s%s)\n", args->path,
		(args->mount_options != NULL) ? ("\"") : (""),
		(args->mount_options != NULL) ? (args->mount_options) : ("nil"),
		(args->mount_options != NULL) ? ("\"") : (""));
}

static int dtmd_helper_dprintf_mount(dtmd_t *handle, void *args)
{
	return dtmd_helper_dprintf_mount_implementation(handle, (dtmd_helper_params_mount_t*) args);
}

inline static int dtmd_helper_dprintf_unmount_implementation(dtmd_t *handle, dtmd_helper_params_unmount_t *args)
{
	return dprintf(handle->socket_fd, dtmd_command_unmount "(\"%s\")\n", args->path);
}

static int dtmd_helper_dprintf_unmount(dtmd_t *handle, void *args)
{
	return dtmd_helper_dprintf_unmount_implementation(handle, (dtmd_helper_params_unmount_t*) args);
}

static int dtmd_helper_dprintf_list_supported_filesystems(dtmd_t *handle, void *args)
{
	return dprintf(handle->socket_fd, dtmd_command_list_supported_filesystems "()\n");
}

inline static int dtmd_helper_dprintf_list_supported_filesystem_options_implementation(dtmd_t *handle, dtmd_helper_params_list_supported_filesystem_options_t *args)
{
	return dprintf(handle->socket_fd, dtmd_command_list_supported_filesystem_options "(\"%s\")\n", args->filesystem);
}

static int dtmd_helper_dprintf_list_supported_filesystem_options(dtmd_t *handle, void *args)
{
	return dtmd_helper_dprintf_list_supported_filesystem_options_implementation(handle, (dtmd_helper_params_list_supported_filesystem_options_t*) args);
}

dtmd_result_t dtmd_helper_generic_process(dtmd_t *handle, int timeout, void *params, void *state, dtmd_helper_dprintf_func_t dprintf_func, dtmd_helper_process_func_t process_func, dtmd_helper_exit_func_t exit_func, dtmd_helper_exit_clear_func_t exit_clear_func)
{
	char data = 0;
	dt_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;
	dtmd_helper_result_t result_code;

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_helper_generic_process_error;
		}
		else
		{
			goto dtmd_helper_generic_process_exit;
		}
	}

	if (dprintf_func(handle, params) < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_helper_generic_process_error;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			if (!dt_validate_command(handle->buffer))
			{
				goto dtmd_helper_generic_process_error;
			}

			cmd = dt_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_helper_generic_process_error;
			}

			result_code = process_func(handle, cmd, params, state);
			dt_free_command(cmd);

			switch (result_code)
			{
			case dtmd_helper_result_exit:
				goto dtmd_helper_generic_process_exit;

			case dtmd_helper_result_error:
				goto dtmd_helper_generic_process_error;
			}
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_helper_generic_process_error;
			}
			else
			{
				goto dtmd_helper_generic_process_exit;
			}
		}
	}

dtmd_helper_generic_process_exit:
	if (state != NULL)
	{
		if (!exit_func(handle, state))
		{
			goto dtmd_helper_generic_process_error;
		}
	}

	goto dtmd_helper_generic_process_finish;

dtmd_helper_generic_process_error:
	if (state != NULL)
	{
		exit_clear_func(state);
	}

	write(handle->pipes[1], &data, sizeof(char));

dtmd_helper_generic_process_finish:
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

inline static dtmd_helper_result_t dtmd_helper_process_list_all_removable_devices_implementation(dtmd_t *handle, dt_command_t *cmd, void *params, dtmd_helper_state_list_all_removable_devices_t *state)
{
	dtmd_result_t res;
	int is_parent_path = 0;
	dtmd_removable_media_t *media_ptr = NULL;
	dtmd_removable_media_t *constructed_media = NULL;

	if (state->got_started)
	{
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
			&& (dtmd_helper_is_helper_list_all_removable_devices_generic(cmd)))
		{
			handle->result_state = dtmd_ok;
			handle->library_state = dtmd_state_default;
			return dtmd_helper_result_exit;
		}

		if (dtmd_helper_cmd_check_removable_device(cmd))
		{
			if (strcmp(cmd->args[0], dtmd_root_device_path) == 0)
			{
				is_parent_path = 1;
			}

			if (!is_parent_path)
			{
				media_ptr = dtmd_find_media(cmd->args[0], state->result);
				if (media_ptr == NULL)
				{
					handle->result_state = dtmd_invalid_state;
					return dtmd_helper_result_error;
				}
			}

			res = dtmd_fill_removable_device_from_notification_implementation(handle, cmd, dtmd_internal_fill_move, &constructed_media);
			if (res != dtmd_ok)
			{
				handle->result_state = res;
				return dtmd_helper_result_error;
			}

			if (is_parent_path)
			{
				if (state->result != NULL)
				{
					media_ptr = state->result;

					while (media_ptr->next_node != NULL)
					{
						media_ptr = media_ptr->next_node;
					}

					constructed_media->prev_node = media_ptr;
					media_ptr->next_node = constructed_media;
				}
				else
				{
					state->result = constructed_media;
				}
			}
			else
			{
				constructed_media->parent = media_ptr;

				if (media_ptr->last_child != NULL)
				{
					media_ptr->last_child->next_node = constructed_media;
					constructed_media->prev_node = media_ptr->last_child;
					media_ptr->last_child = constructed_media;
				}
				else
				{
					media_ptr->first_child = media_ptr->last_child = constructed_media;
				}
			}
		}
		else
		{
			handle->result_state = dtmd_invalid_state;
			return dtmd_helper_result_error;
		}
	}
	else
	{
		if (handle->library_state == dtmd_state_default)
		{
			if ((strcmp(cmd->cmd, dtmd_response_started) == 0)
				&& (dtmd_helper_is_helper_list_all_removable_devices_generic(cmd)))
			{
				state->got_started = 1;
				handle->library_state = dtmd_state_in_list_all_removable_devices;
				return dtmd_helper_result_ok;
			}
			else if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
				&& (dtmd_helper_is_helper_list_all_removable_devices_failed(cmd)))
			{
				handle->result_state = dt_command_failed;
				handle->error_code = dtmd_string_to_error_code(cmd->args[cmd->args_count - 1]);
				handle->library_state = dtmd_state_default;
				return dtmd_helper_result_exit;
			}
		}

		res = dtmd_helper_handle_cmd(handle, cmd);
		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				return dtmd_helper_result_error;
			}
			else
			{
				return dtmd_helper_result_exit;
			}
		}
	}

	return dtmd_helper_result_ok;
}

static dtmd_helper_result_t dtmd_helper_process_list_all_removable_devices(dtmd_t *handle, dt_command_t *cmd, void *params, void *state)
{
	return dtmd_helper_process_list_all_removable_devices_implementation(handle, cmd, params, (dtmd_helper_state_list_all_removable_devices_t*) state);
}

inline static dtmd_helper_result_t dtmd_helper_process_list_removable_device_implementation(dtmd_t *handle, dt_command_t *cmd, dtmd_helper_params_list_removable_device_t *params, dtmd_helper_state_list_removable_device_t *state)
{
	dtmd_result_t res;
	int is_parent_path = 0;
	dtmd_removable_media_t *media_ptr = NULL;
	dtmd_removable_media_t *constructed_media = NULL;

	if (state->got_started)
	{
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
			&& (dtmd_helper_is_helper_list_removable_device_generic(cmd))
			&& (dtmd_helper_is_helper_list_removable_device_parameters_match(cmd, params->device_path)))
		{
			handle->result_state = dtmd_ok;
			handle->library_state = dtmd_state_default;
			return dtmd_helper_result_exit;
		}

		if (dtmd_helper_cmd_check_removable_device(cmd))
		{
			if ((strcmp(cmd->args[0], dtmd_root_device_path) == 0) || (strcmp(cmd->args[1], params->device_path) == 0))
			{
				is_parent_path = 1;
			}

			if (!is_parent_path)
			{
				media_ptr = dtmd_find_media(cmd->args[0], state->result);
				if (media_ptr == NULL)
				{
					handle->result_state = dtmd_invalid_state;
					return dtmd_helper_result_error;
				}
			}

			res = dtmd_fill_removable_device_from_notification_implementation(handle, cmd, dtmd_internal_fill_move, &constructed_media);
			if (res != dtmd_ok)
			{
				handle->result_state = res;
				return dtmd_helper_result_error;
			}

			if (is_parent_path)
			{
				if (state->result != NULL)
				{
					media_ptr = state->result;

					while (media_ptr->next_node != NULL)
					{
						media_ptr = media_ptr->next_node;
					}

					constructed_media->prev_node = media_ptr;
					media_ptr->next_node = constructed_media;
				}
				else
				{
					state->result = constructed_media;
				}
			}
			else
			{
				constructed_media->parent = media_ptr;

				if (media_ptr->last_child != NULL)
				{
					media_ptr->last_child->next_node = constructed_media;
					constructed_media->prev_node = media_ptr->last_child;
					media_ptr->last_child = constructed_media;
				}
				else
				{
					media_ptr->first_child = media_ptr->last_child = constructed_media;
				}
			}
		}
		else
		{
			handle->result_state = dtmd_invalid_state;
			return dtmd_helper_result_error;
		}
	}
	else
	{
		if (handle->library_state == dtmd_state_default)
		{
			if ((strcmp(cmd->cmd, dtmd_response_started) == 0)
				&& (dtmd_helper_is_helper_list_removable_device_generic(cmd))
				&& (dtmd_helper_is_helper_list_removable_device_parameters_match(cmd, params->device_path)))
			{
				state->got_started = 1;
				handle->library_state = dtmd_state_in_list_removable_device;
				return dtmd_helper_result_ok;
			}
			else if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
				&& (dtmd_helper_is_helper_list_removable_device_failed(cmd))
				&& (dtmd_helper_is_helper_list_removable_device_parameters_match(cmd, params->device_path)))
			{
				handle->result_state = dt_command_failed;
				handle->error_code = dtmd_string_to_error_code(cmd->args[cmd->args_count - 1]);
				handle->library_state = dtmd_state_default;
				return dtmd_helper_result_exit;
			}
		}

		res = dtmd_helper_handle_cmd(handle, cmd);
		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				return dtmd_helper_result_error;
			}
			else
			{
				return dtmd_helper_result_exit;
			}
		}
	}

	return dtmd_helper_result_ok;
}

static dtmd_helper_result_t dtmd_helper_process_list_removable_device(dtmd_t *handle, dt_command_t *cmd, void *params, void *state)
{
	return dtmd_helper_process_list_removable_device_implementation(handle, cmd, (dtmd_helper_params_list_removable_device_t*) params, (dtmd_helper_state_list_removable_device_t*) state);
}

inline static dtmd_helper_result_t dtmd_helper_process_mount_implementation(dtmd_t *handle, dt_command_t *cmd, dtmd_helper_params_mount_t *params, void *state)
{
	dtmd_result_t res;

	if (handle->library_state == dtmd_state_default)
	{
		if ((strcmp(cmd->cmd, dtmd_response_succeeded) == 0)
			&& (dtmd_helper_is_helper_mount_generic(cmd))
			&& (dtmd_helper_is_helper_mount_parameters_match(cmd, params->path, params->mount_options)))
		{
			handle->result_state = dtmd_ok;
			return dtmd_helper_result_exit;
		}
		else if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
			&& (dtmd_helper_is_helper_mount_failed(cmd))
			&& (dtmd_helper_is_helper_mount_parameters_match(cmd, params->path, params->mount_options)))
		{
			handle->result_state = dt_command_failed;
			handle->error_code = dtmd_string_to_error_code(cmd->args[cmd->args_count - 1]);
			return dtmd_helper_result_exit;
		}
	}

	res = dtmd_helper_handle_cmd(handle, cmd);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			return dtmd_helper_result_error;
		}
		else
		{
			return dtmd_helper_result_exit;
		}
	}

	return dtmd_helper_result_ok;
}

static dtmd_helper_result_t dtmd_helper_process_mount(dtmd_t *handle, dt_command_t *cmd, void *params, void *state)
{
	return dtmd_helper_process_mount_implementation(handle, cmd, (dtmd_helper_params_mount_t*) params, state);
}

inline static dtmd_helper_result_t dtmd_helper_process_unmount_implementation(dtmd_t *handle, dt_command_t *cmd, dtmd_helper_params_unmount_t *params, void *state)
{
	dtmd_result_t res;

	if (handle->library_state == dtmd_state_default)
	{
		if ((strcmp(cmd->cmd, dtmd_response_succeeded) == 0)
			&& (dtmd_helper_is_helper_unmount_generic(cmd))
			&& (dtmd_helper_is_helper_unmount_parameters_match(cmd, params->path)))
		{
			handle->result_state = dtmd_ok;
			return dtmd_helper_result_exit;
		}
		else if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
			&& (dtmd_helper_is_helper_unmount_failed(cmd))
			&& (dtmd_helper_is_helper_unmount_parameters_match(cmd, params->path)))
		{
			handle->result_state = dt_command_failed;
			handle->error_code = dtmd_string_to_error_code(cmd->args[cmd->args_count - 1]);
			return dtmd_helper_result_exit;
		}
	}

	res = dtmd_helper_handle_cmd(handle, cmd);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			return dtmd_helper_result_error;
		}
		else
		{
			return dtmd_helper_result_exit;
		}
	}

	return dtmd_helper_result_ok;
}

static dtmd_helper_result_t dtmd_helper_process_unmount(dtmd_t *handle, dt_command_t *cmd, void *params, void *state)
{
	return dtmd_helper_process_unmount_implementation(handle, cmd, (dtmd_helper_params_unmount_t*) params, state);
}

inline static dtmd_helper_result_t dtmd_helper_process_list_supported_filesystems_implementation(dtmd_t *handle, dt_command_t *cmd, void *params, dtmd_helper_state_list_supported_filesystems_t *state)
{
	dtmd_result_t res;

	if (state->got_started)
	{
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
			&& (dtmd_helper_is_helper_list_supported_filesystems_generic(cmd)))
		{
			handle->result_state = dtmd_ok;
			handle->library_state = dtmd_state_default;
			return dtmd_helper_result_exit;
		}

		if ((dtmd_helper_cmd_check_supported_filesystems(cmd))
			&& (state->got_result == 0))
		{
			state->got_result = 1;

			state->result_count = cmd->args_count;
			state->result_list  = (const char**) cmd->args;

			cmd->args_count = 0;
			cmd->args       = NULL;
		}
		else
		{
			handle->result_state = dtmd_invalid_state;
			return dtmd_helper_result_error;
		}
	}
	else
	{
		if (handle->library_state == dtmd_state_default)
		{
			if ((strcmp(cmd->cmd, dtmd_response_started) == 0)
				&& (dtmd_helper_is_helper_list_supported_filesystems_generic(cmd)))
			{
				state->got_started = 1;
				handle->library_state = dtmd_state_in_list_supported_filesystems;
				return dtmd_helper_result_ok;
			}
			else if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
				&& (dtmd_helper_is_helper_list_supported_filesystems_failed(cmd)))
			{
				handle->result_state = dt_command_failed;
				handle->error_code = dtmd_string_to_error_code(cmd->args[cmd->args_count - 1]);
				handle->library_state = dtmd_state_default;
				return dtmd_helper_result_exit;
			}
		}

		res = dtmd_helper_handle_cmd(handle, cmd);
		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				return dtmd_helper_result_error;
			}
			else
			{
				return dtmd_helper_result_exit;
			}
		}
	}

	return dtmd_helper_result_ok;
}

static dtmd_helper_result_t dtmd_helper_process_list_supported_filesystems(dtmd_t *handle, dt_command_t *cmd, void *params, void *state)
{
	return dtmd_helper_process_list_supported_filesystems_implementation(handle, cmd, params, (dtmd_helper_state_list_supported_filesystems_t*) state);
}

inline static dtmd_helper_result_t dtmd_helper_process_list_supported_filesystem_options_implementation(dtmd_t *handle, dt_command_t *cmd, dtmd_helper_params_list_supported_filesystem_options_t *params, dtmd_helper_state_list_supported_filesystem_options_t *state)
{
	dtmd_result_t res;

	if (state->got_started)
	{
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
			&& (dtmd_helper_is_helper_list_supported_filesystem_options_generic(cmd))
			&& (dtmd_helper_is_helper_list_supported_filesystem_options_parameters_match(cmd, params->filesystem)))
		{
			handle->result_state = dtmd_ok;
			handle->library_state = dtmd_state_default;
			return dtmd_helper_result_exit;
		}

		if ((dtmd_helper_cmd_check_supported_filesystem_options(cmd))
			&& (state->got_result == 0))
		{
			state->got_result = 1;

			state->result_count = cmd->args_count;
			state->result_list  = (const char**) cmd->args;

			cmd->args_count = 0;
			cmd->args       = NULL;
		}
		else
		{
			handle->result_state = dtmd_invalid_state;
			return dtmd_helper_result_error;
		}
	}
	else
	{
		if (handle->library_state == dtmd_state_default)
		{
			if ((strcmp(cmd->cmd, dtmd_response_started) == 0)
				&& (dtmd_helper_is_helper_list_supported_filesystem_options_generic(cmd))
				&& (dtmd_helper_is_helper_list_supported_filesystem_options_parameters_match(cmd, params->filesystem)))
			{
				state->got_started = 1;
				handle->library_state = dtmd_state_in_list_supported_filesystem_options;
				return dtmd_helper_result_ok;
			}
			else if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
				&& (dtmd_helper_is_helper_list_supported_filesystem_options_failed(cmd))
				&& (dtmd_helper_is_helper_list_supported_filesystem_options_parameters_match(cmd, params->filesystem)))
			{
				handle->result_state = dt_command_failed;
				handle->error_code = dtmd_string_to_error_code(cmd->args[cmd->args_count - 1]);
				handle->library_state = dtmd_state_default;
				return dtmd_helper_result_exit;
			}
		}

		res = dtmd_helper_handle_cmd(handle, cmd);
		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				return dtmd_helper_result_error;
			}
			else
			{
				return dtmd_helper_result_exit;
			}
		}
	}

	return dtmd_helper_result_ok;
}

static dtmd_helper_result_t dtmd_helper_process_list_supported_filesystem_options(dtmd_t *handle, dt_command_t *cmd, void *params, void *state)
{
	return dtmd_helper_process_list_supported_filesystem_options_implementation(handle, cmd, (dtmd_helper_params_list_supported_filesystem_options_t*) params, (dtmd_helper_state_list_supported_filesystem_options_t*) state);
}

inline static int dtmd_helper_exit_list_all_removable_devices_implementation(dtmd_t *handle, dtmd_helper_state_list_all_removable_devices_t *state)
{
	if (handle->result_state == dtmd_ok)
	{
		if (!state->got_started)
		{
			handle->result_state = dtmd_invalid_state;
			return 0;
		}
	}
	else
	{
		dtmd_helper_exit_clear_list_all_removable_devices(state);
	}

	return 1;
}

static int dtmd_helper_exit_list_all_removable_devices(dtmd_t *handle, void *state)
{
	return dtmd_helper_exit_list_all_removable_devices_implementation(handle, (dtmd_helper_state_list_all_removable_devices_t*) state);
}

inline static void dtmd_helper_exit_clear_list_all_removable_devices_implementation(dtmd_helper_state_list_all_removable_devices_t *state)
{
	if (state->result != NULL)
	{
		dtmd_helper_free_removable_device(state->result);
		state->result = NULL;
	}
}

static void dtmd_helper_exit_clear_list_all_removable_devices(void *state)
{
	return dtmd_helper_exit_clear_list_all_removable_devices_implementation((dtmd_helper_state_list_all_removable_devices_t*) state);
}

inline static int dtmd_helper_exit_list_removable_device_implementation(dtmd_t *handle, dtmd_helper_state_list_removable_device_t *state)
{
	if (handle->result_state == dtmd_ok)
	{
		if ((!state->got_started)
			|| (state->result == NULL)
			|| (state->result->next_node != NULL))
		{
			handle->result_state = dtmd_invalid_state;
			return 0;
		}
	}
	else
	{
		dtmd_helper_exit_clear_list_removable_device(state);
	}

	return 1;
}

static int dtmd_helper_exit_list_removable_device(dtmd_t *handle, void *state)
{
	return dtmd_helper_exit_list_removable_device_implementation(handle, (dtmd_helper_state_list_removable_device_t*) state);
}

inline static void dtmd_helper_exit_clear_list_removable_device_implementation(dtmd_helper_state_list_removable_device_t *state)
{
	if (state->result != NULL)
	{
		dtmd_helper_free_removable_device(state->result);
		state->result = NULL;
	}
}

static void dtmd_helper_exit_clear_list_removable_device(void *state)
{
	return dtmd_helper_exit_clear_list_removable_device_implementation((dtmd_helper_state_list_removable_device_t*) state);
}

inline static int dtmd_helper_exit_list_supported_filesystems_implementation(dtmd_t *handle, dtmd_helper_state_list_supported_filesystems_t *state)
{
	if (handle->result_state == dtmd_ok)
	{
		if (!state->got_result)
		{
			handle->result_state = dtmd_invalid_state;
			return 0;
		}
	}
	else
	{
		dtmd_helper_exit_clear_list_supported_filesystems(state);
	}

	return 1;
}

static int dtmd_helper_exit_list_supported_filesystems(dtmd_t *handle, void *state)
{
	return dtmd_helper_exit_list_supported_filesystems_implementation(handle, (dtmd_helper_state_list_supported_filesystems_t*) state);
}

inline static void dtmd_helper_exit_clear_list_supported_filesystems_implementation(dtmd_helper_state_list_supported_filesystems_t *state)
{
	if (state->result_list != NULL)
	{
		dtmd_helper_free_supported_filesystems(state->result_count, state->result_list);
	}

	state->result_list  = NULL;
	state->result_count = 0;
}

static void dtmd_helper_exit_clear_list_supported_filesystems(void *state)
{
	dtmd_helper_exit_clear_list_supported_filesystems_implementation((dtmd_helper_state_list_supported_filesystems_t*) state);
}

inline static int dtmd_helper_exit_list_supported_filesystem_options_implementation(dtmd_t *handle, dtmd_helper_state_list_supported_filesystem_options_t *state)
{
	if (handle->result_state == dtmd_ok)
	{
		if (!state->got_result)
		{
			handle->result_state = dtmd_invalid_state;
			return 0;
		}
	}
	else
	{
		dtmd_helper_exit_clear_list_supported_filesystem_options(state);
	}

	return 1;
}

static int dtmd_helper_exit_list_supported_filesystem_options(dtmd_t *handle, void *state)
{
	return dtmd_helper_exit_list_supported_filesystem_options_implementation(handle, (dtmd_helper_state_list_supported_filesystem_options_t*) state);
}

inline static void dtmd_helper_exit_clear_list_supported_filesystem_options_implementation(dtmd_helper_state_list_supported_filesystem_options_t *state)
{
	if (state->result_list != NULL)
	{
		dtmd_helper_free_supported_filesystem_options(state->result_count, state->result_list);
	}

	state->result_list  = NULL;
	state->result_count = 0;
}

static void dtmd_helper_exit_clear_list_supported_filesystem_options(void *state)
{
	dtmd_helper_exit_clear_list_supported_filesystem_options_implementation((dtmd_helper_state_list_supported_filesystem_options_t*) state);
}

static void dtmd_helper_free_string_array(size_t count, const char **data)
{
	size_t i;

	for (i = 0; i < count; ++i)
	{
		if (data[i] != NULL)
		{
			free((char*)data[i]);
		}
	}

	free(data);
}

static int dtmd_helper_validate_string_array(size_t count, const char **data)
{
	size_t i;

	for (i = 0; i < count; ++i)
	{
		if (data[i] == NULL)
		{
			return 0;
		}
	}

	return 1;
}
