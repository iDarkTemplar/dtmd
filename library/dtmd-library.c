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

typedef enum dtmd_library_state
{
	dtmd_state_default,
	dtmd_state_in_enum_all,
	dtmd_state_in_list_device,
	dtmd_state_in_list_partition,
	dtmd_state_in_list_stateful_device
} dtmd_library_state_t;

struct dtmd_library
{
	dtmd_callback_t callback;
	void *callback_arg;
	pthread_t worker;
	int pipes[2];
	int feedback[2];
	int socket_fd;
	dtmd_result_t result_state;
	dtmd_library_state_t library_state;

	sem_t caller_socket;

	unsigned int cur_pos;
	char buffer[dtmd_command_max_length + 1];
};

static void* dtmd_worker_function(void *arg);

static dtmd_result_t dtmd_helper_handle_cmd(dtmd_t *handle, dtmd_command_t *cmd);
static dtmd_result_t dtmd_helper_handle_callback_cmd(dtmd_t *handle, dtmd_command_t *cmd);
static dtmd_result_t dtmd_helper_wait_for_input(int handle, int timeout);
static int dtmd_helper_is_state_invalid(dtmd_result_t result);

static int dtmd_helper_is_helper_enum_all(dtmd_command_t *cmd);
static int dtmd_helper_is_helper_list_device(dtmd_command_t *cmd);
static int dtmd_helper_is_helper_list_partition(dtmd_command_t *cmd);
static int dtmd_helper_is_helper_list_stateful_device(dtmd_command_t *cmd);
static int dtmd_helper_is_helper_mount(dtmd_command_t *cmd);
static int dtmd_helper_is_helper_unmount(dtmd_command_t *cmd);

static int dtmd_helper_cmd_check_device(dtmd_command_t *cmd);
static int dtmd_helper_cmd_check_partition(dtmd_command_t *cmd);
static int dtmd_helper_cmd_check_stateful_device(dtmd_command_t *cmd);

static void dtmd_helper_free_device(dtmd_device_t *device);
static void dtmd_helper_free_partition(dtmd_partition_t *partition);
static void dtmd_helper_free_stateful_device(dtmd_stateful_device_t *stateful_device);

static int dtmd_helper_string_to_int(const char *string, unsigned int *number);

static int dtmd_helper_validate_device(dtmd_device_t *device);
static int dtmd_helper_validate_partition(dtmd_partition_t *partition);
static int dtmd_helper_validate_stateful_device(dtmd_stateful_device_t *stateful_device);

static dtmd_result_t dtmd_helper_capture_socket(dtmd_t *handle, int timeout, struct timespec *time_cur, struct timespec *time_end);
static dtmd_result_t dtmd_helper_read_data(dtmd_t *handle, int timeout, struct timespec *time_cur, struct timespec *time_end);

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
	strncat(sockaddr.sun_path, dtmd_daemon_socket_addr, sizeof(sockaddr.sun_path) - 1);

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
	dtmd_command_t *cmd;
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
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				goto dtmd_worker_function_error;
			}

			res = dtmd_helper_handle_cmd(handle, cmd);
			dtmd_free_command(cmd);

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

		if (fds[1].revents & POLLIN)
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
	handle->callback(handle->callback_arg, NULL);

dtmd_worker_function_exit:
	// Signal about exit
	data = 0;
	write(handle->feedback[1], &data, sizeof(char));

	pthread_exit(0);
}

dtmd_result_t dtmd_enum_devices(dtmd_t *handle, int timeout, unsigned int *device_count, dtmd_device_t ***result, unsigned int *stateful_device_count, dtmd_stateful_device_t ***result_stateful)
{
	char data = 1;
	dtmd_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;
	int got_started = 0;
	unsigned int result_count = 0;
	unsigned int result_stateful_count = 0;
	dtmd_device_t **result_devices = NULL;
	dtmd_stateful_device_t **result_devices_stateful = NULL;
	unsigned int device;
	unsigned int partition;
	int got_devices_count = 0;
	int got_stateful_devices_count = 0;
	unsigned int expected_devices_count;
	unsigned int expected_stateful_devices_count;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	if ((device_count == NULL) || (result == NULL) || (stateful_device_count == NULL) || (result_stateful == NULL))
	{
		return dtmd_input_error;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_enum_all_error_1;
		}
		else
		{
			goto dtmd_enum_all_exit_1;
		}
	}

	if (dprintf(handle->socket_fd, dtmd_command_enum_all "()\n") < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_enum_all_error_1;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_enum_all_error_1;
			}

			if (got_started)
			{
				if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
					&& (dtmd_helper_is_helper_enum_all(cmd)))
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_ok;
					handle->library_state = dtmd_state_default;
					goto dtmd_enum_all_exit_1;
				}

				if ((strcmp(cmd->cmd, dtmd_response_argument_devices) == 0)
					&& (cmd->args_count == 1)
					&& (cmd->args[0] != NULL)
					&& (got_devices_count == 0))
				{
					got_devices_count = 1;

					if (dtmd_helper_string_to_int(cmd->args[0], &expected_devices_count) == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_enum_all_error_1;
					}

					result_devices = (dtmd_device_t**) malloc(sizeof(dtmd_device_t*)*expected_devices_count);
					if (result_devices == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_enum_all_error_1;
					}

					for (result_count = 0; result_count < expected_devices_count; ++result_count)
					{
						result_devices[result_count] = NULL;
					}

					result_count = 0;
				}
				else if ((strcmp(cmd->cmd, dtmd_response_argument_device) == 0)
					&& (cmd->args_count == 3)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[2] != NULL))
				{
					if (result_count > 0)
					{
						if (!dtmd_helper_validate_device(result_devices[result_count-1]))
						{
							dtmd_free_command(cmd);
							handle->result_state = dtmd_invalid_state;
							goto dtmd_enum_all_error_1;
						}
					}

					++result_count;

					if (result_count > expected_devices_count)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_input_error;
						goto dtmd_enum_all_error_1;
					}

					result_devices[result_count-1] = (dtmd_device_t*) malloc(sizeof(dtmd_device_t));
					if (result_devices[result_count-1] == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_enum_all_error_1;
					}

					result_devices[result_count-1]->path = NULL;
					result_devices[result_count-1]->partition = NULL;
					result_devices[result_count-1]->partitions_count = 0;

					result_devices[result_count-1]->type = dtmd_string_to_device_type(cmd->args[1]);
					if (result_devices[result_count-1]->type == dtmd_removable_media_unknown_or_persistent)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_enum_all_error_1;
					}

					result_devices[result_count-1]->path = cmd->args[0];
					cmd->args[0] = NULL;

					if (!dtmd_helper_string_to_int(cmd->args[2], &(result_devices[result_count-1]->partitions_count)))
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_enum_all_error_1;
					}

					if (result_devices[result_count-1]->partitions_count > 0)
					{
						result_devices[result_count-1]->partition = (dtmd_partition_t**) malloc(sizeof(dtmd_partition_t*)*(result_devices[result_count-1]->partitions_count));
						if (result_devices[result_count-1]->partition == NULL)
						{
							dtmd_free_command(cmd);
							handle->result_state = dtmd_memory_error;
							goto dtmd_enum_all_error_1;
						}

						for (partition = 0; partition < result_devices[result_count-1]->partitions_count; ++partition)
						{
							result_devices[result_count-1]->partition[partition] = NULL;
						}
					}
				}
				else if ((strcmp(cmd->cmd, dtmd_response_argument_partition) == 0)
					&& (cmd->args_count == 6)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[3] != NULL))
				{
					if (result_count == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_enum_all_error_1;
					}

					if (strcmp(cmd->args[3], result_devices[result_count-1]->path) != 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_enum_all_error_1;
					}

					for (partition = 0; partition < result_devices[result_count-1]->partitions_count; ++partition)
					{
						if (result_devices[result_count-1]->partition[partition] == NULL)
						{
							break;
						}
					}

					if (partition >= result_devices[result_count-1]->partitions_count)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_enum_all_error_1;
					}

					result_devices[result_count-1]->partition[partition] = (dtmd_partition_t*) malloc(sizeof(dtmd_partition_t));
					if (result_devices[result_count-1]->partition[partition] == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_enum_all_error_1;
					}

					result_devices[result_count-1]->partition[partition]->path      = cmd->args[0];
					result_devices[result_count-1]->partition[partition]->fstype    = cmd->args[1];
					result_devices[result_count-1]->partition[partition]->label     = cmd->args[2];
					result_devices[result_count-1]->partition[partition]->mnt_point = cmd->args[4];
					result_devices[result_count-1]->partition[partition]->mnt_opts  = cmd->args[5];

					cmd->args[0] = NULL;
					cmd->args[1] = NULL;
					cmd->args[2] = NULL;
					cmd->args[4] = NULL;
					cmd->args[5] = NULL;
				}
				else if ((strcmp(cmd->cmd, dtmd_response_argument_stateful_devices) == 0)
					&& (cmd->args_count == 1)
					&& (cmd->args[0] != NULL)
					&& (got_stateful_devices_count == 0))
				{
					got_stateful_devices_count = 1;

					if (dtmd_helper_string_to_int(cmd->args[0], &expected_stateful_devices_count) == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_enum_all_error_1;
					}

					result_devices_stateful = (dtmd_stateful_device_t**) malloc(sizeof(dtmd_stateful_device_t*)*expected_stateful_devices_count);
					if (result_devices_stateful == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_enum_all_error_1;
					}

					for (result_stateful_count = 0; result_stateful_count < expected_stateful_devices_count; ++result_stateful_count)
					{
						result_devices_stateful[result_stateful_count] = NULL;
					}

					result_stateful_count = 0;
				}
				else if ((strcmp(cmd->cmd, dtmd_response_argument_stateful_device) == 0)
					&& (cmd->args_count == 7)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[2] != NULL))
				{
					++result_stateful_count;

					if (result_stateful_count > expected_stateful_devices_count)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_input_error;
						goto dtmd_enum_all_error_1;
					}

					result_devices_stateful[result_stateful_count-1] = (dtmd_stateful_device_t*) malloc(sizeof(dtmd_stateful_device_t));
					if (result_devices_stateful[result_stateful_count-1] == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_enum_all_error_1;
					}

					result_devices_stateful[result_stateful_count-1]->path      = NULL;
					result_devices_stateful[result_stateful_count-1]->fstype    = NULL;
					result_devices_stateful[result_stateful_count-1]->label     = NULL;
					result_devices_stateful[result_stateful_count-1]->mnt_point = NULL;
					result_devices_stateful[result_stateful_count-1]->mnt_opts  = NULL;

					result_devices_stateful[result_stateful_count-1]->type = dtmd_string_to_device_type(cmd->args[1]);
					if (result_devices_stateful[result_stateful_count-1]->type == dtmd_removable_media_unknown_or_persistent)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_enum_all_error_1;
					}

					result_devices_stateful[result_stateful_count-1]->state = dtmd_string_to_device_state(cmd->args[2]);
					if (result_devices_stateful[result_stateful_count-1]->state == dtmd_removable_media_state_unknown)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_enum_all_error_1;
					}

					result_devices_stateful[result_stateful_count-1]->path      = cmd->args[0];
					result_devices_stateful[result_stateful_count-1]->fstype    = cmd->args[3];
					result_devices_stateful[result_stateful_count-1]->label     = cmd->args[4];
					result_devices_stateful[result_stateful_count-1]->mnt_point = cmd->args[5];
					result_devices_stateful[result_stateful_count-1]->mnt_opts  = cmd->args[6];

					cmd->args[0] = NULL;
					cmd->args[3] = NULL;
					cmd->args[4] = NULL;
					cmd->args[5] = NULL;
					cmd->args[6] = NULL;

					if (!dtmd_helper_validate_stateful_device(result_devices_stateful[result_stateful_count-1]))
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_enum_all_error_1;
					}
				}
				else
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_invalid_state;
					goto dtmd_enum_all_error_1;
				}
			}
			else
			{
				if ((handle->library_state == dtmd_state_default)
					&& (dtmd_helper_is_helper_enum_all(cmd)))
				{
					if (strcmp(cmd->cmd, dtmd_response_started) == 0)
					{
						got_started = 1;
						handle->library_state = dtmd_state_in_enum_all;
					}
					else if (strcmp(cmd->cmd, dtmd_response_failed) == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_command_failed;
						handle->library_state = dtmd_state_default;
						goto dtmd_enum_all_exit_1;
					}
				}
				else
				{
					res = dtmd_helper_handle_cmd(handle, cmd);

					if (res != dtmd_ok)
					{
						dtmd_free_command(cmd);
						handle->result_state = res;

						if (dtmd_helper_is_state_invalid(res))
						{
							goto dtmd_enum_all_error_1;
						}
						else
						{
							goto dtmd_enum_all_exit_1;
						}
					}
				}
			}

			dtmd_free_command(cmd);
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_enum_all_error_1;
			}
			else
			{
				goto dtmd_enum_all_exit_1;
			}
		}
	}

dtmd_enum_all_exit_1:
	if (handle->result_state == dtmd_ok)
	{
		if ((!got_devices_count)
			|| (!got_stateful_devices_count)
			|| (result_count != expected_devices_count)
			|| (result_stateful_count != expected_stateful_devices_count))
		{
			handle->result_state = dtmd_invalid_state;
		}

		if ((result_devices != NULL)
			&& ((result_devices[result_count-1] == NULL)
				|| (!dtmd_helper_validate_device(result_devices[result_count-1]))))
		{
			handle->result_state = dtmd_invalid_state;
			goto dtmd_enum_all_error_1;
		}

		if ((result_devices_stateful != NULL)
			&& (result_devices_stateful[result_stateful_count-1] == NULL))
		{
			handle->result_state = dtmd_invalid_state;
			goto dtmd_enum_all_error_1;
		}

		*result                = result_devices;
		*device_count          = result_count;
		*result_stateful       = result_devices_stateful;
		*stateful_device_count = result_stateful_count;
	}
	else
	{
		if (result_devices != NULL)
		{
			for (device = 0; device < result_count; ++device)
			{
				if (result_devices[device] != NULL)
				{
					dtmd_helper_free_device(result_devices[device]);
				}
			}

			free(result_devices);
		}

		if (result_devices_stateful != NULL)
		{
			for (device = 0; device < result_stateful_count; ++device)
			{
				if (result_devices_stateful[device] != NULL)
				{
					dtmd_helper_free_stateful_device(result_devices_stateful[device]);
				}
			}

			free(result_devices_stateful);
		}

		*result                = NULL;
		*device_count          = 0;
		*result_stateful       = NULL;
		*stateful_device_count = 0;
	}

	sem_post(&(handle->caller_socket));

	return handle->result_state;

dtmd_enum_all_error_1:
	if (result_devices != NULL)
	{
		for (device = 0; device < result_count; ++device)
		{
			if (result_devices[device] != NULL)
			{
				dtmd_helper_free_device(result_devices[device]);
			}
		}

		free(result_devices);
	}

	if (result_devices_stateful != NULL)
	{
		for (device = 0; device < result_stateful_count; ++device)
		{
			if (result_devices_stateful[device] != NULL)
			{
				dtmd_helper_free_stateful_device(result_devices_stateful[device]);
			}
		}

		free(result_devices_stateful);
	}

	data = 0;
	write(handle->pipes[1], &data, sizeof(char));
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

dtmd_result_t dtmd_list_device(dtmd_t *handle, int timeout, const char *device_path, dtmd_device_t **result)
{
	char data = 1;
	dtmd_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;
	int got_started = 0;
	dtmd_device_t *result_device = NULL;
	unsigned int partition;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	if ((device_path == NULL) || (*device_path == 0) || (result == NULL))
	{
		return dtmd_input_error;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_list_device_error_1;
		}
		else
		{
			goto dtmd_list_device_exit_1;
		}
	}

	if (dprintf(handle->socket_fd, dtmd_command_list_device "(\"%s\")\n", device_path) < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_list_device_error_1;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_list_device_error_1;
			}

			if (got_started)
			{
				if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
					&& (dtmd_helper_is_helper_list_device(cmd))
					&& (strcmp(cmd->args[1], device_path) == 0))
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_ok;
					handle->library_state = dtmd_state_default;
					goto dtmd_list_device_exit_1;
				}

				if ((strcmp(cmd->cmd, dtmd_response_argument_device) == 0)
					&& (cmd->args_count == 3)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[2] != NULL)
					&& (result_device == NULL))
				{
					result_device = (dtmd_device_t*) malloc(sizeof(dtmd_device_t));
					if (result_device == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_list_device_error_1;
					}

					result_device->path = NULL;
					result_device->partition = NULL;
					result_device->partitions_count = 0;

					result_device->type = dtmd_string_to_device_type(cmd->args[1]);
					if (result_device->type == dtmd_removable_media_unknown_or_persistent)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_list_device_error_1;
					}

					result_device->path = cmd->args[0];
					cmd->args[0] = NULL;

					if (!dtmd_helper_string_to_int(cmd->args[2], &(result_device->partitions_count)))
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_list_device_error_1;
					}

					if (result_device->partitions_count > 0)
					{
						result_device->partition = (dtmd_partition_t**) malloc(sizeof(dtmd_partition_t*)*result_device->partitions_count);
						if (result_device->partition == NULL)
						{
							dtmd_free_command(cmd);
							handle->result_state = dtmd_memory_error;
							goto dtmd_list_device_error_1;
						}

						for (partition = 0; partition < result_device->partitions_count; ++partition)
						{
							result_device->partition[partition] = NULL;
						}
					}
				}
				else if ((strcmp(cmd->cmd, dtmd_response_argument_partition) == 0)
					&& (cmd->args_count == 6)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[3] != NULL))
				{
					if (result_device == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_list_device_error_1;
					}

					if (strcmp(cmd->args[3], result_device->path) != 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_list_device_error_1;
					}

					for (partition = 0; partition < result_device->partitions_count; ++partition)
					{
						if (result_device->partition[partition] == NULL)
						{
							break;
						}
					}

					if (partition >= result_device->partitions_count)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_list_device_error_1;
					}

					result_device->partition[partition] = (dtmd_partition_t*) malloc(sizeof(dtmd_partition_t));
					if (result_device->partition[partition] == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_list_device_error_1;
					}

					result_device->partition[partition]->path      = cmd->args[0];
					result_device->partition[partition]->fstype    = cmd->args[1];
					result_device->partition[partition]->label     = cmd->args[2];
					result_device->partition[partition]->mnt_point = cmd->args[4];
					result_device->partition[partition]->mnt_opts  = cmd->args[5];

					cmd->args[0] = NULL;
					cmd->args[1] = NULL;
					cmd->args[2] = NULL;
					cmd->args[4] = NULL;
					cmd->args[5] = NULL;
				}
				else
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_invalid_state;
					goto dtmd_list_device_error_1;
				}
			}
			else
			{
				if ((handle->library_state == dtmd_state_default)
					&& (dtmd_helper_is_helper_list_device(cmd))
					&& (strcmp(cmd->args[1], device_path) == 0))
				{
					if (strcmp(cmd->cmd, dtmd_response_started) == 0)
					{
						got_started = 1;
						handle->library_state = dtmd_state_in_list_device;
					}
					else if (strcmp(cmd->cmd, dtmd_response_failed) == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_command_failed;
						handle->library_state = dtmd_state_default;
						goto dtmd_list_device_exit_1;
					}
				}
				else
				{
					res = dtmd_helper_handle_cmd(handle, cmd);

					if (res != dtmd_ok)
					{
						dtmd_free_command(cmd);
						handle->result_state = res;

						if (dtmd_helper_is_state_invalid(res))
						{
							goto dtmd_list_device_error_1;
						}
						else
						{
							goto dtmd_list_device_exit_1;
						}
					}
				}
			}

			dtmd_free_command(cmd);
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_list_device_error_1;
			}
			else
			{
				goto dtmd_list_device_exit_1;
			}
		}
	}

dtmd_list_device_exit_1:
	if (handle->result_state == dtmd_ok)
	{
		if ((result_device != NULL) && (!dtmd_helper_validate_device(result_device)))
		{
			handle->result_state = dtmd_invalid_state;
			goto dtmd_list_device_error_1;
		}

		*result = result_device;
	}
	else
	{
		if (result_device != NULL)
		{
			dtmd_helper_free_device(result_device);
		}

		*result = NULL;
	}

	sem_post(&(handle->caller_socket));

	return handle->result_state;

dtmd_list_device_error_1:
	if (result_device != NULL)
	{
		dtmd_helper_free_device(result_device);
	}

	data = 0;
	write(handle->pipes[1], &data, sizeof(char));
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

dtmd_result_t dtmd_list_partition(dtmd_t *handle, int timeout, const char *partition_path, dtmd_partition_t **result)
{
	char data = 1;
	dtmd_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;
	int got_started = 0;
	dtmd_partition_t *result_partition = NULL;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	if ((partition_path == NULL) || (*partition_path == 0) || (result == NULL))
	{
		return dtmd_input_error;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_list_partition_error_1;
		}
		else
		{
			goto dtmd_list_partition_exit_1;
		}
	}

	if (dprintf(handle->socket_fd, dtmd_command_list_partition "(\"%s\")\n", partition_path) < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_list_partition_error_1;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_list_partition_error_1;
			}

			if (got_started)
			{
				if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
					&& (dtmd_helper_is_helper_list_partition(cmd))
					&& (strcmp(cmd->args[1], partition_path) == 0))
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_ok;
					handle->library_state = dtmd_state_default;
					goto dtmd_list_partition_exit_1;
				}

				if ((strcmp(cmd->cmd, dtmd_response_argument_partition) == 0)
					&& (cmd->args_count == 6)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[3] != NULL)
					&& (result_partition == NULL))
				{
					if (strcmp(cmd->args[0], partition_path) != 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_list_partition_error_1;
					}

					result_partition = (dtmd_partition_t*) malloc(sizeof(dtmd_partition_t));
					if (result_partition == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_list_partition_error_1;
					}

					result_partition->path      = cmd->args[0];
					result_partition->fstype    = cmd->args[1];
					result_partition->label     = cmd->args[2];
					result_partition->mnt_point = cmd->args[4];
					result_partition->mnt_opts  = cmd->args[5];

					cmd->args[0] = NULL;
					cmd->args[1] = NULL;
					cmd->args[2] = NULL;
					cmd->args[4] = NULL;
					cmd->args[5] = NULL;
				}
				else
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_invalid_state;
					goto dtmd_list_partition_error_1;
				}
			}
			else
			{
				if ((handle->library_state == dtmd_state_default)
					&& (dtmd_helper_is_helper_list_partition(cmd))
					&& (strcmp(cmd->args[1], partition_path) == 0))
				{
					if (strcmp(cmd->cmd, dtmd_response_started) == 0)
					{
						got_started = 1;
						handle->library_state = dtmd_state_in_list_partition;
					}
					else if (strcmp(cmd->cmd, dtmd_response_failed) == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_command_failed;
						handle->library_state = dtmd_state_default;
						goto dtmd_list_partition_exit_1;
					}
				}
				else
				{
					res = dtmd_helper_handle_cmd(handle, cmd);

					if (res != dtmd_ok)
					{
						dtmd_free_command(cmd);
						handle->result_state = res;

						if (dtmd_helper_is_state_invalid(res))
						{
							goto dtmd_list_partition_error_1;
						}
						else
						{
							goto dtmd_list_partition_exit_1;
						}
					}
				}
			}

			dtmd_free_command(cmd);
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_list_partition_error_1;
			}
			else
			{
				goto dtmd_list_partition_exit_1;
			}
		}
	}

dtmd_list_partition_exit_1:
	if (handle->result_state == dtmd_ok)
	{
		if ((result_partition != NULL) && (!dtmd_helper_validate_partition(result_partition)))
		{
			handle->result_state = dtmd_invalid_state;
			goto dtmd_list_partition_error_1;
		}

		*result = result_partition;
	}
	else
	{
		if (result_partition != NULL)
		{
			dtmd_helper_free_partition(result_partition);
		}

		*result = NULL;
	}

	sem_post(&(handle->caller_socket));

	return handle->result_state;

dtmd_list_partition_error_1:
	if (result_partition != NULL)
	{
		dtmd_helper_free_partition(result_partition);
	}

	data = 0;
	write(handle->pipes[1], &data, sizeof(char));
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

dtmd_result_t dtmd_list_stateful_device(dtmd_t *handle, int timeout, const char *device_path, dtmd_stateful_device_t **result)
{
	char data = 1;
	dtmd_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;
	int got_started = 0;
	dtmd_stateful_device_t *result_stateful_device = NULL;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	if ((device_path == NULL) || (*device_path == 0) || (result == NULL))
	{
		return dtmd_input_error;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_list_stateful_device_error_1;
		}
		else
		{
			goto dtmd_list_stateful_device_exit_1;
		}
	}

	if (dprintf(handle->socket_fd, dtmd_command_list_stateful_device "(\"%s\")\n", device_path) < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_list_stateful_device_error_1;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_list_stateful_device_error_1;
			}

			if (got_started)
			{
				if ((strcmp(cmd->cmd, dtmd_response_finished) == 0)
					&& (dtmd_helper_is_helper_list_stateful_device(cmd))
					&& (strcmp(cmd->args[1], device_path) == 0))
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_ok;
					handle->library_state = dtmd_state_default;
					goto dtmd_list_stateful_device_exit_1;
				}

				if ((strcmp(cmd->cmd, dtmd_response_argument_stateful_device) == 0)
					&& (cmd->args_count == 7)
					&& (cmd->args[0] != NULL)
					&& (cmd->args[1] != NULL)
					&& (cmd->args[2] != NULL)
					&& (result_stateful_device == NULL))
				{
					if (strcmp(cmd->args[0], device_path) != 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_io_error;
						goto dtmd_list_stateful_device_error_1;
					}

					result_stateful_device = (dtmd_stateful_device_t*) malloc(sizeof(dtmd_stateful_device_t));
					if (result_stateful_device == NULL)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_memory_error;
						goto dtmd_list_stateful_device_error_1;
					}

					result_stateful_device->path      = NULL;
					result_stateful_device->fstype    = NULL;
					result_stateful_device->label     = NULL;
					result_stateful_device->mnt_point = NULL;
					result_stateful_device->mnt_opts  = NULL;

					result_stateful_device->type = dtmd_string_to_device_type(cmd->args[1]);
					if (result_stateful_device->type == dtmd_removable_media_unknown_or_persistent)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_list_stateful_device_error_1;
					}

					result_stateful_device->state = dtmd_string_to_device_state(cmd->args[2]);
					if (result_stateful_device->state == dtmd_removable_media_state_unknown)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_invalid_state;
						goto dtmd_list_stateful_device_error_1;
					}

					result_stateful_device->path      = cmd->args[0];
					result_stateful_device->fstype    = cmd->args[3];
					result_stateful_device->label     = cmd->args[4];
					result_stateful_device->mnt_point = cmd->args[5];
					result_stateful_device->mnt_opts  = cmd->args[6];

					cmd->args[0] = NULL;
					cmd->args[3] = NULL;
					cmd->args[4] = NULL;
					cmd->args[5] = NULL;
					cmd->args[6] = NULL;
				}
				else
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_invalid_state;
					goto dtmd_list_stateful_device_error_1;
				}
			}
			else
			{
				if ((handle->library_state == dtmd_state_default)
					&& (dtmd_helper_is_helper_list_stateful_device(cmd))
					&& (strcmp(cmd->args[1], device_path) == 0))
				{
					if (strcmp(cmd->cmd, dtmd_response_started) == 0)
					{
						got_started = 1;
						handle->library_state = dtmd_state_in_list_stateful_device;
					}
					else if (strcmp(cmd->cmd, dtmd_response_failed) == 0)
					{
						dtmd_free_command(cmd);
						handle->result_state = dtmd_command_failed;
						handle->library_state = dtmd_state_default;
						goto dtmd_list_stateful_device_exit_1;
					}
				}
				else
				{
					res = dtmd_helper_handle_cmd(handle, cmd);

					if (res != dtmd_ok)
					{
						dtmd_free_command(cmd);
						handle->result_state = res;

						if (dtmd_helper_is_state_invalid(res))
						{
							goto dtmd_list_stateful_device_error_1;
						}
						else
						{
							goto dtmd_list_stateful_device_exit_1;
						}
					}
				}
			}

			dtmd_free_command(cmd);
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_list_stateful_device_error_1;
			}
			else
			{
				goto dtmd_list_stateful_device_exit_1;
			}
		}
	}

dtmd_list_stateful_device_exit_1:
	if (handle->result_state == dtmd_ok)
	{
		if ((result_stateful_device != NULL) && (!dtmd_helper_validate_stateful_device(result_stateful_device)))
		{
			handle->result_state = dtmd_invalid_state;
			goto dtmd_list_stateful_device_error_1;
		}

		*result = result_stateful_device;
	}
	else
	{
		if (result_stateful_device != NULL)
		{
			dtmd_helper_free_stateful_device(result_stateful_device);
		}

		*result = NULL;
	}

	sem_post(&(handle->caller_socket));

	return handle->result_state;

dtmd_list_stateful_device_error_1:
	if (result_stateful_device != NULL)
	{
		dtmd_helper_free_stateful_device(result_stateful_device);
	}

	data = 0;
	write(handle->pipes[1], &data, sizeof(char));
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

dtmd_result_t dtmd_mount(dtmd_t *handle, int timeout, const char *path, const char *mount_options)
{
	char data = 1;
	dtmd_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	if ((path == NULL) || (*path == 0))
	{
		return dtmd_input_error;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_mount_error_1;
		}
		else
		{
			goto dtmd_mount_exit_1;
		}
	}

	if (dprintf(handle->socket_fd, dtmd_command_mount "(\"%s\", %s%s%s)\n", path,
		(mount_options != NULL) ? ("\"") : (""),
		(mount_options != NULL) ? (mount_options) : ("nil"),
		(mount_options != NULL) ? ("\"") : ("")
		) < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_mount_error_1;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_mount_error_1;
			}

			if ((handle->library_state == dtmd_state_default)
				&& (dtmd_helper_is_helper_mount(cmd))
				&& (strcmp(cmd->args[1], path) == 0)
				&& (((cmd->args[2] != NULL) && (strcmp(cmd->args[2], mount_options) == 0))
					|| ((cmd->args[2] == NULL) && (cmd->args[2] == NULL))))
			{
				if (strcmp(cmd->cmd, dtmd_response_succeeded) == 0)
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_ok;
					goto dtmd_mount_exit_1;
				}
				else if (strcmp(cmd->cmd, dtmd_response_failed) == 0)
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_command_failed;
					goto dtmd_mount_exit_1;
				}
			}
			else
			{
				res = dtmd_helper_handle_cmd(handle, cmd);

				if (res != dtmd_ok)
				{
					dtmd_free_command(cmd);
					handle->result_state = res;

					if (dtmd_helper_is_state_invalid(res))
					{
						goto dtmd_mount_error_1;
					}
					else
					{
						goto dtmd_mount_exit_1;
					}
				}
			}

			dtmd_free_command(cmd);
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_mount_error_1;
			}
			else
			{
				goto dtmd_mount_exit_1;
			}
		}
	}

dtmd_mount_exit_1:
	sem_post(&(handle->caller_socket));
	return handle->result_state;

dtmd_mount_error_1:
	data = 0;
	write(handle->pipes[1], &data, sizeof(char));
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

dtmd_result_t dtmd_unmount(dtmd_t *handle, int timeout, const char *path)
{
	char data = 1;
	dtmd_command_t *cmd;
	dtmd_result_t res;
	struct timespec time_cur, time_end;
	char *eol;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (dtmd_is_state_invalid(handle))
	{
		return dtmd_invalid_state;
	}

	if ((path == NULL) || (*path == 0))
	{
		return dtmd_input_error;
	}

	res = dtmd_helper_capture_socket(handle, timeout, &time_cur, &time_end);
	if (res != dtmd_ok)
	{
		handle->result_state = res;

		if (dtmd_helper_is_state_invalid(res))
		{
			goto dtmd_unmount_error_1;
		}
		else
		{
			goto dtmd_unmount_exit_1;
		}
	}

	if (dprintf(handle->socket_fd, dtmd_command_unmount "(\"%s\")\n", path) < 0)
	{
		handle->result_state = dtmd_io_error;
		goto dtmd_unmount_error_1;
	}

	for (;;)
	{
		while ((eol = strchr(handle->buffer, '\n')) != NULL)
		{
			cmd = dtmd_parse_command(handle->buffer);

			handle->cur_pos -= (eol + 1 - handle->buffer);
			memmove(handle->buffer, eol+1, handle->cur_pos + 1);

			if (cmd == NULL)
			{
				handle->result_state = dtmd_invalid_state;
				goto dtmd_unmount_error_1;
			}

			if ((handle->library_state == dtmd_state_default)
			    && (dtmd_helper_is_helper_unmount(cmd))
			    && (strcmp(cmd->args[1], path) == 0))
			{
				if (strcmp(cmd->cmd, dtmd_response_succeeded) == 0)
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_ok;
					goto dtmd_unmount_exit_1;
				}
				else if (strcmp(cmd->cmd, dtmd_response_failed) == 0)
				{
					dtmd_free_command(cmd);
					handle->result_state = dtmd_command_failed;
					goto dtmd_unmount_exit_1;
				}
			}
			else
			{
				res = dtmd_helper_handle_cmd(handle, cmd);

				if (res != dtmd_ok)
				{
					dtmd_free_command(cmd);
					handle->result_state = res;

					if (dtmd_helper_is_state_invalid(res))
					{
						goto dtmd_unmount_error_1;
					}
					else
					{
						goto dtmd_unmount_exit_1;
					}
				}
			}

			dtmd_free_command(cmd);
		}

		res = dtmd_helper_read_data(handle, timeout, &time_cur, &time_end);

		if (res != dtmd_ok)
		{
			handle->result_state = res;

			if (dtmd_helper_is_state_invalid(res))
			{
				goto dtmd_unmount_error_1;
			}
			else
			{
				goto dtmd_unmount_exit_1;
			}
		}
	}

dtmd_unmount_exit_1:
	sem_post(&(handle->caller_socket));
	return handle->result_state;

dtmd_unmount_error_1:
	data = 0;
	write(handle->pipes[1], &data, sizeof(char));
	sem_post(&(handle->caller_socket));
	return handle->result_state;
}

int dtmd_is_state_invalid(dtmd_t *handle)
{
	if (handle == NULL)
	{
		return 1;
	}

	return dtmd_helper_is_state_invalid(handle->result_state);
}

void dtmd_free_devices_array(dtmd_t *handle, unsigned int device_count, dtmd_device_t **devices)
{
	unsigned int i;

	if ((handle == NULL) || (devices == NULL))
	{
		return;
	}

	for (i = 0; i < device_count; ++i)
	{
		if (devices[i] != NULL)
		{
			dtmd_helper_free_device(devices[i]);
		}
	}

	free(devices);
}

void dtmd_free_device(dtmd_t *handle, dtmd_device_t *device)
{
	if ((handle == NULL) || (device == NULL))
	{
		return;
	}

	dtmd_helper_free_device(device);
}

void dtmd_free_partition(dtmd_t *handle, dtmd_partition_t *partition)
{
	if ((handle == NULL) || (partition == NULL))
	{
		return;
	}

	dtmd_helper_free_partition(partition);
}

void dtmd_free_stateful_devices_array(dtmd_t *handle, unsigned int stateful_device_count, dtmd_stateful_device_t **stateful_devices)
{
	unsigned int i;

	if ((handle == NULL) || (stateful_devices == NULL))
	{
		return;
	}

	for (i = 0; i < stateful_device_count; ++i)
	{
		if (stateful_devices[i] != NULL)
		{
			dtmd_helper_free_stateful_device(stateful_devices[i]);
		}
	}

	free(stateful_devices);
}

void dtmd_free_stateful_device(dtmd_t *handle, dtmd_stateful_device_t *stateful_device)
{
	if ((handle == NULL) || (stateful_device == NULL))
	{
		return;
	}

	dtmd_helper_free_stateful_device(stateful_device);
}

static dtmd_result_t dtmd_helper_handle_cmd(dtmd_t *handle, dtmd_command_t *cmd)
{
	switch (handle->library_state)
	{
	case dtmd_state_default:
		if ((strcmp(cmd->cmd, dtmd_response_failed) == 0)
			&& ((dtmd_helper_is_helper_enum_all(cmd))
				|| (dtmd_helper_is_helper_list_device(cmd))
				|| (dtmd_helper_is_helper_list_partition(cmd)
				|| (dtmd_helper_is_helper_list_stateful_device(cmd)))
				|| (dtmd_helper_is_helper_mount(cmd))
				|| (dtmd_helper_is_helper_unmount(cmd))))
		{
			return dtmd_ok;
		}

		if ((strcmp(cmd->cmd, dtmd_response_succeeded) == 0)
			&& ((dtmd_helper_is_helper_mount(cmd))
				|| (dtmd_helper_is_helper_unmount(cmd))))
		{
			return dtmd_ok;
		}

		if (strcmp(cmd->cmd, dtmd_response_started) == 0)
		{
			if (dtmd_helper_is_helper_enum_all(cmd))
			{
				handle->library_state = dtmd_state_in_enum_all;
				return dtmd_ok;
			}
			else if (dtmd_helper_is_helper_list_device(cmd))
			{
				handle->library_state = dtmd_state_in_list_device;
				return dtmd_ok;
			}
			else if (dtmd_helper_is_helper_list_partition(cmd))
			{
				handle->library_state = dtmd_state_in_list_partition;
				return dtmd_ok;
			}
			else if (dtmd_helper_is_helper_list_stateful_device(cmd))
			{
				handle->library_state = dtmd_state_in_list_stateful_device;
				return dtmd_ok;
			}
		}

		return dtmd_helper_handle_callback_cmd(handle, cmd);
		break;

	case dtmd_state_in_enum_all:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_enum_all(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_device(cmd) || dtmd_helper_cmd_check_partition(cmd) || dtmd_helper_cmd_check_stateful_device(cmd))
		{
			return dtmd_ok;
		}
		break;

	case dtmd_state_in_list_device:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_device(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_device(cmd) || dtmd_helper_cmd_check_partition(cmd))
		{
			return dtmd_ok;
		}
		break;

	case dtmd_state_in_list_partition:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_partition(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_partition(cmd))
		{
			return dtmd_ok;
		}
		break;

	case dtmd_state_in_list_stateful_device:
		if ((strcmp(cmd->cmd, dtmd_response_finished) == 0) && (dtmd_helper_is_helper_list_stateful_device(cmd)))
		{
			handle->library_state = dtmd_state_default;
			return dtmd_ok;
		}

		if (dtmd_helper_cmd_check_stateful_device(cmd))
		{
			return dtmd_ok;
		}
		break;
	}

	return dtmd_input_error;
}

static dtmd_result_t dtmd_helper_handle_callback_cmd(dtmd_t *handle, dtmd_command_t *cmd)
{
	if (   ((strcmp(cmd->cmd, dtmd_notification_add_disk)                == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_remove_disk)             == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_add_partition)           == 0) && (cmd->args_count == 4) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[3] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_remove_partition)        == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_add_stateful_device)     == 0) && (cmd->args_count == 5) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_remove_stateful_device)  == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_stateful_device_changed) == 0) && (cmd->args_count == 5) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_mount)                   == 0) && (cmd->args_count == 3) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
		|| ((strcmp(cmd->cmd, dtmd_notification_unmount)                 == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL)))
	{
		handle->callback(handle->callback_arg, cmd);
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
	fd.revents = POLLOUT;

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
		case dtmd_command_failed:
			return 0;
		default:
			return 1;
	}
}

static int dtmd_helper_is_helper_enum_all(dtmd_command_t *cmd)
{
	return (cmd->args_count == 1) && (cmd->args[0] != NULL) && (strcmp(cmd->args[0], dtmd_command_enum_all) == 0);
}

static int dtmd_helper_is_helper_list_device(dtmd_command_t *cmd)
{
	return (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_device) == 0);
}

static int dtmd_helper_is_helper_list_partition(dtmd_command_t *cmd)
{
	return (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_partition) == 0);
}

static int dtmd_helper_is_helper_list_stateful_device(dtmd_command_t *cmd)
{
	return (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_list_stateful_device) == 0);
}

static int dtmd_helper_is_helper_mount(dtmd_command_t *cmd)
{
	return (cmd->args_count == 3) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_mount) == 0);
}

static int dtmd_helper_is_helper_unmount(dtmd_command_t *cmd)
{
	return (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (strcmp(cmd->args[0], dtmd_command_unmount) == 0);
}

static int dtmd_helper_cmd_check_device(dtmd_command_t *cmd)
{
	return (strcmp(cmd->cmd, dtmd_response_argument_device) == 0) && (cmd->args_count == 3) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL);
}

static int dtmd_helper_cmd_check_partition(dtmd_command_t *cmd)
{
	return (strcmp(cmd->cmd, dtmd_response_argument_partition) == 0) && (cmd->args_count == 6) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[3] != NULL);
}

static int dtmd_helper_cmd_check_stateful_device(dtmd_command_t *cmd)
{
	return (strcmp(cmd->cmd, dtmd_response_argument_stateful_device) == 0) && (cmd->args_count == 7) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL);
}

static void dtmd_helper_free_device(dtmd_device_t *device)
{
	unsigned int i;

	if (device->path != NULL)
	{
		free(device->path);
	}

	if ((device->partitions_count > 0) && (device->partition != NULL))
	{
		for (i = 0; i < device->partitions_count; ++i)
		{
			if (device->partition[i] != NULL)
			{
				dtmd_helper_free_partition(device->partition[i]);
			}
		}

		free(device->partition);
	}

	free(device);
}

static void dtmd_helper_free_partition(dtmd_partition_t *partition)
{
	if (partition->path != NULL)
	{
		free(partition->path);
	}

	if (partition->fstype != NULL)
	{
		free(partition->fstype);
	}

	if (partition->label != NULL)
	{
		free(partition->label);
	}

	if (partition->mnt_point != NULL)
	{
		free(partition->mnt_point);
	}

	if (partition->mnt_opts != NULL)
	{
		free(partition->mnt_opts);
	}

	free(partition);
}

static void dtmd_helper_free_stateful_device(dtmd_stateful_device_t *stateful_device)
{
	if (stateful_device->fstype != NULL)
	{
		free(stateful_device->fstype);
	}

	if (stateful_device->label != NULL)
	{
		free(stateful_device->label);
	}

	if (stateful_device->mnt_point != NULL)
	{
		free(stateful_device->mnt_point);
	}

	if (stateful_device->mnt_opts != NULL)
	{
		free(stateful_device->mnt_opts);
	}

	if (stateful_device->path != NULL)
	{
		free(stateful_device->path);
	}

	free(stateful_device);
}

static int dtmd_helper_string_to_int(const char *string, unsigned int *number)
{
	unsigned int result = 0;

	if (string[0] == 0)
	{
		return 0;
	}

	do
	{
		if ((*string >= '0') && (*string <= '9'))
		{
			result = (result * 10) + (*string - '0');
		}
		else
		{
			return 0;
		}

		++string;
	} while (*string != 0);

	*number = result;

	return 1;
}

static int dtmd_helper_validate_device(dtmd_device_t *device)
{
	unsigned int i;

	if ((device->path == NULL) || (device->type == dtmd_removable_media_unknown_or_persistent))
	{
		return 0;
	}

	if (device->partitions_count > 0)
	{
		if (device->partition == NULL)
		{
			return 0;
		}

		for (i = 0; i < device->partitions_count; ++i)
		{
			if ((device->partition[i] == NULL) || (!dtmd_helper_validate_partition(device->partition[i])))
			{
				return 0;
			}
		}
	}
	else if (device->partition != NULL)
	{
		return 0;
	}

	return 1;
}

static int dtmd_helper_validate_partition(dtmd_partition_t *partition)
{
	if ((partition->path == NULL) || (partition->fstype == NULL))
	{
		return 0;
	}

	return 1;
}

static int dtmd_helper_validate_stateful_device(dtmd_stateful_device_t *stateful_device)
{
	if ((stateful_device->path == NULL)
		|| (stateful_device->type == dtmd_removable_media_unknown_or_persistent)
		|| (stateful_device->state == dtmd_removable_media_state_unknown)
		|| ((stateful_device->state == dtmd_removable_media_state_ok)
			&& (stateful_device->fstype == NULL))
		|| ((stateful_device->state != dtmd_removable_media_state_ok)
			&& (stateful_device->fstype != NULL)))
	{
		return 0;
	}

	return 1;
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
