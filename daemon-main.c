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

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <errno.h>

#include "dtmd.h"

#define dtmd_daemon_lock "/var/lock/dtmd.lock"

int continue_working = 1;

struct removable_media
{
	char *path;
};

struct removable_media **media = NULL;
unsigned int media_count = 0;

int *clients = NULL;
unsigned int clients_count = 0;

int add_media(const char *path)
{
	unsigned int i;
	struct removable_media *cur_media;
	struct removable_media **tmp;

	for (i = 0; i < media_count; ++i)
	{
		if (strcmp(media[i]->path, path) == 0)
		{
			return 1;
		}
	}

	cur_media = (struct removable_media*) malloc(sizeof(struct removable_media));
	if (cur_media == NULL)
	{
		return 0;
	}

	cur_media->path = strdup(path);
	if (cur_media->path == NULL)
	{
		free(cur_media);
		return 0;
	}

	tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*)*(media_count+1));
	if (tmp == NULL)
	{
		free(cur_media->path);
		free(cur_media);
		return 0;
	}

	++media_count;
	media = tmp;
	media[media_count-1] = cur_media;

	return 1;
}

int remove_media(const char *path)
{
	unsigned int i;
	unsigned int j;
	struct removable_media **tmp;

	for (i = 0; i < media_count; ++i)
	{
		if (strcmp(media[i]->path, path) == 0)
		{
			free(media[i]->path);
			free(media[i]);
			--media_count;

			if (media_count > 0)
			{
				for (j = i+1; j < media_count+1; ++j)
				{
					media[j-1] = media[j];
				}

				media[media_count] = NULL;

				tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*)*media_count);
				if (tmp == NULL)
				{
					return 0;
				}

				media = tmp;
			}
			else
			{
				free(media);
				media = NULL;
			}

			return 1;
		}
	}

	return 1;
}

void remove_all_media(void)
{
	unsigned int i;

	if (media != NULL)
	{
		for (i = 0; i < media_count; ++i)
		{
			if (media[i] != NULL)
			{
				free(media[i]->path);
				free(media[i]);
			}
		}

		free(media);

		media_count = 0;
		media = NULL;
	}
}

int add_client(int client)
{
	unsigned int i;
	int *tmp;

	for (i = 0; i < clients_count; ++i)
	{
		if (clients[i] == client)
		{
			return 1;
		}
	}

	tmp = (int*) realloc(clients, sizeof(int)*(clients_count+1));
	if (tmp == NULL)
	{
		shutdown(client, SHUT_RDWR);
		close(client);
		return 0;
	}

	++clients_count;
	clients = tmp;
	clients[clients_count-1] = client;

	return 1;
}

int remove_client(int client)
{
	unsigned int i;
	unsigned int j;
	int *tmp;

	for (i = 0; i < clients_count; ++i)
	{
		if (clients[i] == client)
		{
			shutdown(clients[i], SHUT_RDWR);
			close(clients[i]);
			--clients_count;

			if (clients_count > 0)
			{
				for (j = i+1; j < clients_count+1; ++j)
				{
					clients[j-1] = clients[j];
				}

				clients[clients_count] = -1;

				tmp = (int*) realloc(clients, sizeof(int)*media_count);
				if (tmp == NULL)
				{
					return 0;
				}

				clients = tmp;
			}
			else
			{
				free(clients);
				clients = NULL;
			}

			return 1;
		}
	}

	return 1;
}

void remove_all_clients(void)
{
	unsigned int i;

	if (clients != NULL)
	{
		for (i = 0; i < media_count; ++i)
		{
			if (clients[i] != -1)
			{
				shutdown(clients[i], SHUT_RDWR);
				close(clients[i]);
			}
		}

		free(clients);

		clients_count = 0;
		clients = NULL;
	}
}

void signal_handler(int signum)
{
	switch (signum)
	{
	case SIGTERM:
		continue_working = 0;
		break;
	}
}

int setSigHandlers(void)
{
	struct sigaction action;

	action.sa_handler = signal_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	if (sigaction(SIGTERM, &action, NULL) < 0)
	{
		return -1;
	}

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		return -1;
	}

	return 0;
}

int redirectStdio(void)
{
	int fd1;
	int fd2;

	fd1 = open("/dev/null", O_RDONLY);
	if (fd1 == -1)
	{
		return -1;
	}

	fd2 = open("/dev/null", O_WRONLY);
	if (fd2 == -1)
	{
		close(fd1);

		return -1;
	}

	if (dup2(fd1, STDIN_FILENO) == -1)
	{
		close(fd1);
		close(fd2);

		return -1;
	}

	if (dup2(fd2, STDOUT_FILENO) == -1)
	{
		close(fd1);
		close(fd2);

		return -1;
	}

	if (dup2(fd2, STDERR_FILENO) == -1)
	{
		close(fd1);
		close(fd2);

		return -1;
	}

	close(fd1);
	close(fd2);

	return 0;
}

void check_lock_file(void)
{
	FILE *lockpidfile;
	int lockpid;

	lockpidfile = fopen(dtmd_daemon_lock, "r");
	if (lockpidfile != NULL)
	{
		if (fscanf(lockpidfile, "%10d\n", &lockpid) == 1)
		{
			if (kill(lockpid, 0) == -1)
			{
				fclose(lockpidfile);
				lockpidfile = NULL;
				unlink(dtmd_daemon_lock);
			}
		}

		if (lockpidfile != NULL)
		{
			fclose(lockpidfile);
		}
	}
}

int main(int argc, char **argv)
{
	int result = 0;
	int socketfd;
	struct sockaddr_un sockaddr;
	int lockfd;
	int rc;
	mode_t def_mask;
	pid_t child = -1;
	char buffer[12];
	const int backlog = 4;
	unsigned int i;

	struct udev *udev;
	struct udev_monitor *mon;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	const char *path;
	const char *action;
	int monfd;

	struct pollfd *pollfds = NULL;

	socketfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (socketfd == -1)
	{
		fprintf(stderr, "Error opening socket\n");
		result = -1;
		goto exit_1;
	}

	sockaddr.sun_family = AF_LOCAL;
	memset(sockaddr.sun_path, 0, sizeof(sockaddr.sun_path));
	strncat(sockaddr.sun_path, dtmd_daemon_socket_addr, sizeof(sockaddr.sun_path));

	check_lock_file();

	lockfd = open(dtmd_daemon_lock, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (lockfd == -1)
	{
		fprintf(stderr, "Error obtaining lock file\n");
		result = -1;
		goto exit_2;
	}

	unlink(sockaddr.sun_path);
	def_mask = umask(S_IRWXG | S_IRWXO);
	rc = bind(socketfd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un));
	umask(def_mask);

	if (rc == -1)
	{
		fprintf(stderr, "Error binding socket\n");
		result = -1;
		goto exit_3;
	}

	child = fork();
	if (child == -1)
	{
		fprintf(stderr, "Error forking\n");
		result = -1;
		goto exit_4;
	}
	else if (child != 0)
	{
		// parent - exit
		goto exit_4;
	}

	// child - daemon
	snprintf(buffer, sizeof(buffer), "%10d\n", getpid());
	write(lockfd, buffer, sizeof(buffer) - 1);
#ifdef _POSIX_SYNCHRONIZED_IO
	fdatasync(lockfd);
#else
	fsync(lockfd);
#endif

	rc = setSigHandlers();
	if (rc == -1)
	{
		result = -1;
		goto exit_4;
	}

	umask(0);
	if (setsid() == -1)
	{
		result = -1;
		goto exit_4;
	}

	if (chdir("/") == -1)
	{
		result = -1;
		goto exit_4;
	}

	if (redirectStdio() != 0)
	{
		result = -1;
		goto exit_4;
	}

	if (listen(socketfd, backlog) == -1)
	{
		result = -1;
		goto exit_4;
	}

	udev = udev_new();
	if (udev == NULL)
	{
		result = -1;
		goto exit_4;
	}

	mon = udev_monitor_new_from_netlink(udev, "udev");
	if (mon == NULL)
	{
		result = -1;
		goto exit_5;
	}

	udev_monitor_filter_add_match_subsystem_devtype(mon, "block", "disk");
	udev_monitor_enable_receiving(mon);
	monfd = udev_monitor_get_fd(mon);

	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_add_match_sysattr(enumerate, "removable", "1");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices)
	{
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);
		if (dev == NULL)
		{
			result = -1;
			goto exit_6;
		}

		path = udev_device_get_devnode(dev);
		if (path == NULL)
		{
			udev_device_unref(dev);
			result = -1;
			goto exit_6;
		}

		if (!add_media(path))
		{
			udev_device_unref(dev);
			result = -1;
			goto exit_6;
		}

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);

	pollfds = (struct pollfd*) malloc(sizeof(struct pollfd));
	if (pollfds == NULL)
	{
		result = -1;
		goto exit_6;
	}

	while (continue_working)
	{
		pollfds[0].fd = monfd;
		pollfds[0].events = POLLIN;
		pollfds[0].revents = 0;

		for (i = 0; i < clients_count; ++i)
		{
			pollfds[i+1].fd = clients[i];
			pollfds[i+1].events = POLLIN;
			pollfds[i+1].revents = 0;
		}

		rc = poll(pollfds, 1 + clients_count, -1);
		if (rc == -1)
		{
			if (errno != EINTR)
			{
				result = -1;
				goto exit_7;
			}
		}

		if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR))
		{
			result = -1;
			goto exit_7;
		}

		if (pollfds[0].revents & POLLIN)
		{
			dev = udev_monitor_receive_device(mon);
			if (dev != NULL)
			{
				path = udev_device_get_devnode(dev);
				action = udev_device_get_action(dev);

				if ((action != NULL) && (path != NULL))
				{
					if (strcmp(action, "add") == 0)
					{

					}
					else if (strcmp(action, "remove") == 0)
					{

					}
					else if (strcmp(action, "change") == 0)
					{

					}
					else if (strcmp(action, "online") == 0)
					{

					}
					else if (strcmp(action, "offline") == 0)
					{

					}
				}

				udev_device_unref(dev);
			}
			else
			{
				result = -1;
				goto exit_7;
			}
		}

		for (i = 0; i < clients_count; ++i)
		{
			if ((pollfds[i+1].revents & POLLHUP) || (pollfds[i+1].revents & POLLERR))
			{

			}

			if (pollfds[i+1].revents & POLLIN)
			{

			}
		}
	}

exit_7:
	free(pollfds);

exit_6:
	remove_all_media();
	udev_monitor_unref(mon);

exit_5:
	udev_unref(udev);

exit_4:
	close(socketfd);
	unlink(sockaddr.sun_path);

	close(lockfd);
	unlink(dtmd_daemon_lock);
	goto exit_1;

exit_3:
	close(lockfd);
	unlink(dtmd_daemon_lock);

exit_2:
	close(socketfd);

exit_1:
	return result;
}
