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

#include "dtmd-library.h"

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

#include "dtmd.h"

struct dtmd_library
{
	dtmd_callback callback;
	void *callback_arg;
	pthread_t worker;
	int pipes[2];
	int feedback[2];
	int socket_fd;
	int state;

	sem_t caller_socket;
};

static void* dtmd_worker_function(void *arg);

dtmd_t* dtmd_init(dtmd_callback callback, void *arg)
{
	dtmd_t *handle;
	struct sockaddr_un sockaddr;
	/* unsigned char data = 0; */

	if (callback == NULL)
	{
		goto dtmd_init_error_1;
	}

	handle = (dtmd_t*) malloc(sizeof(dtmd_t));
	if (handle == NULL)
	{
		goto dtmd_init_error_1;
	}

	handle->callback     = callback;
	handle->callback_arg = arg;

	handle->state = 0;

	if (sem_init(&(handle->caller_socket), 0, 0) == -1)
	{
		goto dtmd_init_error_2;
	}

	if (pipe(handle->feedback) == -1)
	{
		goto dtmd_init_error_3;
	}

	if (pipe(handle->pipes) == -1)
	{
		goto dtmd_init_error_4;
	}

	handle->socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (handle->socket_fd == -1)
	{
		goto dtmd_init_error_5;
	}

	sockaddr.sun_family = AF_LOCAL;
	memset(sockaddr.sun_path, 0, sizeof(sockaddr.sun_path));
	strncat(sockaddr.sun_path, dtmd_daemon_socket_addr, sizeof(sockaddr.sun_path) - 1);

	if (connect(handle->socket_fd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un)) == -1)
	{
		goto dtmd_init_error_6;
	}

	if ((pthread_create(&(handle->worker), NULL, &dtmd_worker_function, handle)) != 0)
	{
		goto dtmd_init_error_6;
	}

	return handle;
/*
dtmd_init_error_7:
	write(handle->pipes[1], &data, sizeof(unsigned char));
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
	return NULL;
}

void dtmd_deinit(dtmd_t *handle)
{
	unsigned char data = 0;

	if (handle != NULL)
	{
		write(handle->pipes[1], &data, sizeof(unsigned char));
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
	struct dtmd_command *cmd;
	unsigned char data;

	handle = (dtmd_t*) arg;

	fds[0].fd = handle->pipes[0];
	fds[1].fd = handle->socket_fd;

	for (;;)
	{
		fds[0].events  = POLLIN;
		fds[0].revents = 0;
		fds[1].events  = POLLOUT;
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
			handle->callback(handle->callback_arg, NULL);
			break;
		}

		if (fds[0].revents & POLLIN)
		{
			rc = read(handle->pipes[0], &data, sizeof(unsigned char));

			if ((rc == 1) || (data == 1))
			{
				// release ownership of socket and wait for return
				data = 1;
				write(handle->feedback[1], &data, sizeof(unsigned char));

				sem_wait(&(handle->caller_socket));
			}
			else
			{
				// error
				break;
			}
		}

		if (fds[1].revents & POLLIN)
		{
			// TODO: process notifications
		}
	}

	data = 0;
	write(handle->feedback[1], &data, sizeof(unsigned char));

	pthread_exit(0);
}

int dtmd_enum_devices(dtmd_t *handle)
{
	// TODO: rewrite
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (handle->state)
	{
		return dtmd_invalid_state;
	}

	if (dprintf(handle->socket_fd, "enum_all()\n") < 0)
	{
		handle->state = 1;
		return dtmd_io_error;
	}

	return dtmd_ok;
}

int dtmd_list_device(dtmd_t *handle, const char *device_path)
{
	unsigned char data = 1;

	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (handle->state)
	{
		return dtmd_invalid_state;
	}

	if ((device_path == NULL) || (strlen(device_path) == 0))
	{
		return dtmd_input_error;
	}

	write(handle->pipes[1], &data, sizeof(unsigned char));
	read(handle->feedback[0], &data, sizeof(unsigned char));

	if (data == 0)
	{
		write(handle->pipes[1], &data, sizeof(unsigned char));
		sem_post(&(handle->caller_socket));
		handle->state = 1;
		return dtmd_invalid_state;
	}

	if (dprintf(handle->socket_fd, "list_device(\"%s\")\n", device_path) < 0)
	{
		data = 0;
		write(handle->pipes[1], &data, sizeof(unsigned char));
		sem_post(&(handle->caller_socket));
		handle->state = 1;
		return dtmd_io_error;
	}

	// TODO: read listed devices

	sem_post(&(handle->caller_socket));

	return dtmd_ok;
}

int dtmd_mount(dtmd_t *handle, const char *path, const char *mount_point, const char *mount_options)
{
	// TODO: rewrite
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (handle->state)
	{
		return dtmd_invalid_state;
	}

	if ((path == NULL) || (mount_point == NULL)
		|| (strlen(path) == 0) || (strlen(mount_point) == 0)
		|| ((mount_options != NULL) && (strlen(mount_options) == 0)))
	{
		return dtmd_input_error;
	}

	if (mount_options != NULL)
	{
		if (dprintf(handle->socket_fd, "mount(\"%s\", \"%s\", \"%s\")\n", path, mount_point, mount_options) < 0)
		{
			handle->state = 1;
			return dtmd_io_error;
		}
	}
	else
	{
		if (dprintf(handle->socket_fd, "mount(\"%s\", \"%s\", nil)\n", path, mount_point) < 0)
		{
			handle->state = 1;
			return dtmd_io_error;
		}
	}

	return dtmd_ok;
}

int dtmd_unmount(dtmd_t *handle, const char *path, const char *mount_point)
{
	// TODO: rewrite
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if (handle->state)
	{
		return dtmd_invalid_state;
	}

	if ((path == NULL) || (mount_point == NULL)
		|| (strlen(path) == 0) || (strlen(mount_point) == 0))
	{
		return dtmd_input_error;
	}

	if (dprintf(handle->socket_fd, "unmount(\"%s\", \"%s\")\n", path, mount_point) < 0)
	{
		handle->state = 1;
		return dtmd_io_error;
	}

	return dtmd_ok;
}
