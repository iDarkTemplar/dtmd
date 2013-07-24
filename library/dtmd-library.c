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

#include "library/dtmd-library.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>

#include "dtmd.h"

struct dtmd_library
{
	dtmd_callback callback;
	void *callback_arg;
	pthread_t worker;
	int pipes[2];
	int socket_fd;
	int worker_fd;
};

static void* dtmd_worker_function(void *arg);

dtmd_t* dtmd_init(dtmd_callback callback, void *arg)
{
	dtmd_t *handle;
	struct sockaddr_un sockaddr;

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

	if (pipe(handle->pipes) == -1)
	{
		goto dtmd_init_error_2;
	}

	handle->socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (handle->socket_fd == -1)
	{
		goto dtmd_init_error_3;
	}

	handle->worker_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (handle->worker_fd == -1)
	{
		goto dtmd_init_error_4;
	}

	sockaddr.sun_family = AF_LOCAL;
	memset(sockaddr.sun_path, 0, sizeof(sockaddr.sun_path));
	strncat(sockaddr.sun_path, dtmd_daemon_socket_addr, sizeof(sockaddr.sun_path) - 1);

	if (connect(handle->socket_fd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un)) == -1)
	{
		goto dtmd_init_error_5;
	}

	if (connect(handle->worker_fd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un)) == -1)
	{
		goto dtmd_init_error_5;
	}

	if ((pthread_create(&(handle->worker), NULL, &dtmd_worker_function, handle)) != 0)
	{
		goto dtmd_init_error_5;
	}

	return handle;
/*
dtmd_init_error_6:
	write(handle->pipes[1], "", sizeof(""));
	pthread_join(handle->worker, NULL);
*/
dtmd_init_error_5:
	shutdown(handle->worker_fd, SHUT_RDWR);
	close(handle->worker_fd);

dtmd_init_error_4:
	shutdown(handle->socket_fd, SHUT_RDWR);
	close(handle->socket_fd);

dtmd_init_error_3:
	close(handle->pipes[0]);
	close(handle->pipes[1]);

dtmd_init_error_2:
	free(handle);

dtmd_init_error_1:
	return NULL;
}

void dtmd_deinit(dtmd_t *handle)
{
	if (handle == NULL)
	{
		return;
	}

	write(handle->pipes[1], "", sizeof(""));
	pthread_join(handle->worker, NULL);

	shutdown(handle->worker_fd, SHUT_RDWR);
	close(handle->worker_fd);
	shutdown(handle->socket_fd, SHUT_RDWR);
	close(handle->socket_fd);
	close(handle->pipes[0]);
	close(handle->pipes[1]);
	free(handle);
}

static void* dtmd_worker_function(void *arg)
{
	dtmd_t *handle;

	handle = (dtmd_t*) arg;

	pthread_exit(0);
}

int dtmd_enum_devices(dtmd_t *handle)
{
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	// TODO: implement
	return dtmd_ok;
}

int dtmd_list_device(dtmd_t *handle, const char *device_path)
{
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((device_path == NULL) || (strlen(device_path) == 0))
	{
		return dtmd_input_error;
	}

	// TODO: implement
	return dtmd_ok;
}

int dtmd_mount(dtmd_t *handle, const char *path, const char *mount_point, const char *mount_options)
{
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((path == NULL) || (mount_point == NULL)
		|| (strlen(path) == 0) || (strlen(mount_point) == 0)
		|| ((mount_options != NULL) && (strlen(mount_options) == 0)))
	{
		return dtmd_input_error;
	}

	// TODO: implement
	return dtmd_ok;
}

int dtmd_unmount(dtmd_t *handle, const char *path, const char *mount_point)
{
	if (handle == NULL)
	{
		return dtmd_library_not_initialized;
	}

	if ((path == NULL) || (mount_point == NULL)
		|| (strlen(path) == 0) || (strlen(mount_point) == 0))
	{
		return dtmd_input_error;
	}

	// TODO: implement
	return dtmd_ok;
}
