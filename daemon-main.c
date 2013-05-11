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
#include "lists.h"

#define dtmd_daemon_lock "/var/lock/dtmd.lock"

/* TODO:
	partitions = enumerate partitions + parent

	listen to partitions and check if they have removable parent?

	removable media = removable parent + partitions
		type = SUBSYSTEM / DEVTYPE
		removable media = block/disk + ATTRS{removable}=="1"
		partition = block/partition + removable media parent
*/

/* TODO: global
daemon:
	1) Daemonize
	2a) enable monitoring
	2) enum all devices
	3) wait for events or commands
	4) on exit try to unmount everything mounted?

	client: make library
*/

int continue_working = 1;

int send_notification(const char *media, const char *action)
{
	return 0;
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
	struct udev_device *dev_parent;
	const char *path;
	const char *path_parent;
	const char *action;
	const char *devtype;
	const char *sysattr;
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
		close(socketfd);
		close(lockfd);
		goto exit_1;
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

	if (udev_monitor_filter_add_match_subsystem_devtype(mon, "block", "disk") < 0)
	{
		result = -1;
		goto exit_6;
	}

	if (udev_monitor_enable_receiving(mon) < 0)
	{
		result = -1;
		goto exit_6;
	}

	monfd = udev_monitor_get_fd(mon);

	enumerate = udev_enumerate_new(udev);

	if (udev_enumerate_add_match_subsystem(enumerate, "block") < 0)
	{
		udev_enumerate_unref(enumerate);
		result = -1;
		goto exit_6;
	}

	if (udev_enumerate_scan_devices(enumerate) < 0)
	{
		udev_enumerate_unref(enumerate);
		result = -1;
		goto exit_6;
	}

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices)
	{
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);
		if (dev == NULL)
		{
			udev_enumerate_unref(enumerate);
			result = -1;
			goto exit_7;
		}

		path = udev_device_get_devnode(dev);
		if (path == NULL)
		{
			udev_device_unref(dev);
			udev_enumerate_unref(enumerate);
			result = -1;
			goto exit_7;
		}

		devtype = udev_device_get_devtype(dev);
		if (strcmp(devtype, "disk") == 0)
		{
			sysattr = udev_device_get_sysattr_value(dev, "removable");
			if ((sysattr != NULL) && (strcmp(sysattr, "1") == 0))
			{
				if (add_media_block(path) < 0)
				{
					udev_device_unref(dev);
					udev_enumerate_unref(enumerate);
					result = -1;
					goto exit_7;
				}
			}
		}
		else if (strcmp(devtype, "partition") == 0)
		{
			dev_parent = udev_device_get_parent_with_subsystem_devtype(dev, "block", "disk");
			if (dev_parent != NULL)
			{
				path_parent = udev_device_get_devnode(dev_parent);
				sysattr = udev_device_get_sysattr_value(dev_parent, "removable");
				if ((path_parent != NULL) && (sysattr != NULL) && (strcmp(sysattr, "1") == 0))
				{
					if (add_media_partition(path_parent, path) < 0)
					{
						udev_device_unref(dev);
						udev_enumerate_unref(enumerate);
						result = -1;
						goto exit_7;
					}
				}
			}
		}

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);

	pollfds = (struct pollfd*) malloc(sizeof(struct pollfd));
	if (pollfds == NULL)
	{
		result = -1;
		goto exit_7;
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
				goto exit_8;
			}
		}

		if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR))
		{
			result = -1;
			goto exit_8;
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
					if ((strcmp(action, "add") == 0) || (strcmp(action, "online") == 0))
					{
						rc = add_media_block(path);
						if (rc < 0)
						{
							udev_device_unref(dev);
							result = -1;
							goto exit_8;
						}
						else if (rc > 0)
						{
						}
					}
					else if ((strcmp(action, "remove") == 0) || (strcmp(action, "offline") == 0))
					{
						rc = remove_media_block(path);
						if (rc < 0)
						{
							udev_device_unref(dev);
							result = -1;
							goto exit_8;
						}
						else if (rc > 0)
						{
						}
					}
					else if (strcmp(action, "change") == 0)
					{
					}
				}

				udev_device_unref(dev);
			}
			else
			{
				result = -1;
				goto exit_8;
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

exit_8:
	free(pollfds);

exit_7:
	remove_all_media();

exit_6:
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
