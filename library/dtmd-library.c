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
	pthread_mutex_t callback_mutex;
	int pipes[2];
	int socket_fd;
};

static void* dtmd_worker_function(void *arg);

dtmd_t* dtmd_init(void)
{
	dtmd_t *handle;
	struct sockaddr_un sockaddr;

	handle = (dtmd_t*) malloc(sizeof(dtmd_t));
	if (handle == NULL)
	{
		goto dtmd_init_error_1;
	}

	if (pthread_mutex_init(&(handle->callback_mutex), NULL) != 0)
	{
		goto dtmd_init_error_2;
	}

	if (pipe(handle->pipes) == -1)
	{
		goto dtmd_init_error_3;
	}

	handle->socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (handle->socket_fd == -1)
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
	close(handle->socket_fd);

dtmd_init_error_4:
	close(handle->pipes[0]);
	close(handle->pipes[1]);

dtmd_init_error_3:
	pthread_mutex_destroy(&(handle->callback_mutex));

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

	close(handle->socket_fd);
	close(handle->pipes[0]);
	close(handle->pipes[1]);
	pthread_mutex_destroy(&(handle->callback_mutex));
	free(handle);
}

void dtmd_set_callback(dtmd_t *handle, dtmd_callback callback, void *arg)
{
	if (handle == NULL)
	{
		return;
	}

	pthread_mutex_lock(&(handle->callback_mutex));
	handle->callback     = callback;
	handle->callback_arg = arg;
	pthread_mutex_unlock(&(handle->callback_mutex));
}

static void* dtmd_worker_function(void *arg)
{
	dtmd_t *handle;

	handle = (dtmd_t*) arg;

	pthread_exit(0);
}
