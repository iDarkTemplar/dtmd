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
#include "actions.h"
#include "mnt_funcs.h"

#ifdef SUBSYSTEM_LINUX_UDEV
#include "linux/udev/udev.h"
#endif

#define dtmd_daemon_lock "/var/lock/dtmd.lock"
#define default_read_size 4096
#define maximum_read_size default_read_size * 10

/* TODO:
	partitions = enumerate partitions + parent

	listen to partitions and check if they have removable parent?

	removable media = removable parent + partitions
		type = SUBSYSTEM / DEVTYPE
		removable media = block/disk + ATTRS{removable}=="1"
		partition = block/partition + removable media parent

	ATTR{events}=="media_change eject_request"

	ID_FS_LABEL=BEAST_MODE
	ID_FS_LABEL_ENC=BEAST\x20MODE
	ID_FS_TYPE=vfat

	LABEL_ENC decoding

	monitor:
		device add/remove
		partition mount/unmount
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

static unsigned char continue_working = 1;
static unsigned char daemonize        = 1;

void print_usage(char *name)
{
	fprintf(stderr, "USAGE: %s [options]\n"
		"where options are one or more of following:\n"
		"\t-n\n"
		"\t--no-daemon\t- do not daemonize\n",
		name);
}

void signal_handler(int signum)
{
	switch (signum)
	{
	case SIGTERM:
	case SIGINT:
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

	if (!daemonize)
	{
		if (sigaction(SIGINT, &action, NULL) < 0)
		{
			return -1;
		}
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
	unsigned int j;

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
	const char *fstype;
	const char *label;
	unsigned char media_type;
	int monfd;
	int mountfd;
	void *tmp;
	unsigned char *tmp_str;
#define pollfds_count_default 3
	unsigned int pollfds_count = pollfds_count_default;

	struct pollfd *pollfds = NULL;

	for (rc = 1; rc < argc; ++rc)
	{
		if ((strcmp(argv[rc],"-n") == 0) || (strcmp(argv[rc],"--no-daemon") == 0))
		{
			daemonize = 0;
		}
		else
		{
			print_usage(argv[0]);
			return -1;
		}
	}

	socketfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (socketfd == -1)
	{
		fprintf(stderr, "Error opening socket\n");
		result = -1;
		goto exit_1;
	}

	sockaddr.sun_family = AF_LOCAL;
	memset(sockaddr.sun_path, 0, sizeof(sockaddr.sun_path));
	strncat(sockaddr.sun_path, dtmd_daemon_socket_addr, sizeof(sockaddr.sun_path) - 1);

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

	if (daemonize)
	{
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
	}

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
		if (!daemonize)
		{
			fprintf(stderr, "Error setting signal handlers\n");
		}

		result = -1;
		goto exit_4;
	}

	umask(0);

	if (daemonize)
	{
		if (setsid() == -1)
		{
			result = -1;
			goto exit_4;
		}
	}

	if (chdir("/") == -1)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error changing directory to /\n");
		}

		result = -1;
		goto exit_4;
	}

	if (daemonize)
	{
		if (redirectStdio() != 0)
		{
			result = -1;
			goto exit_4;
		}
	}

	if (listen(socketfd, backlog) == -1)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error listening socket\n");
		}

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

	mountfd = open("/proc/self/mounts", O_RDONLY);
	if (mountfd == -1)
	{
		result = -1;
		goto exit_6;
	}

	if (udev_enumerate_add_match_subsystem(enumerate, "block") < 0)
	{
		udev_enumerate_unref(enumerate);
		result = -1;
		goto exit_mountfd;
	}

	if (udev_enumerate_scan_devices(enumerate) < 0)
	{
		udev_enumerate_unref(enumerate);
		result = -1;
		goto exit_mountfd;
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
			media_type = get_device_type(dev);

			if (media_type > 0)
			{
				if (add_media_block(path, media_type) < 0)
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
				fstype      = udev_device_get_property_value(dev, "ID_FS_TYPE");
				label       = udev_device_get_property_value(dev, "ID_FS_LABEL_ENC");
				media_type  = get_device_type(dev_parent);

				if ((media_type > 0) && (fstype != NULL))
				{
					if (add_media_partition(path_parent, media_type, path, fstype, label) < 0)
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

	pollfds = (struct pollfd*) malloc(sizeof(struct pollfd)*pollfds_count);
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

		pollfds[1].fd = socketfd;
		pollfds[1].events = POLLIN;
		pollfds[1].revents = 0;

		pollfds[2].fd = mountfd;
		pollfds[2].events = POLLERR;
		pollfds[2].revents = 0;

		for (i = 0; i < clients_count; ++i)
		{
			pollfds[i + pollfds_count_default].fd      = clients[i]->clientfd;
			pollfds[i + pollfds_count_default].events  = POLLIN;
			pollfds[i + pollfds_count_default].revents = 0;
		}

		rc = poll(pollfds, pollfds_count, -1);
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
		else if (pollfds[0].revents & POLLIN)
		{
			dev = udev_monitor_receive_device(mon);
			if (dev != NULL)
			{
				path = udev_device_get_devnode(dev);
				action = udev_device_get_action(dev);
				devtype = udev_device_get_devtype(dev);

				if ((action != NULL) && (path != NULL))
				{
					if ((strcmp(action, "add") == 0) || (strcmp(action, "online") == 0))
					{
						rc = 0;

						if (strcmp(devtype, "disk") == 0)
						{
							media_type = get_device_type(dev);

							if (media_type > 0)
							{
								rc = add_media_block(path, media_type);
							}
						}
						else if (strcmp(devtype, "partition") == 0)
						{
							dev_parent = udev_device_get_parent_with_subsystem_devtype(dev, "block", "disk");
							if (dev_parent != NULL)
							{
								path_parent = udev_device_get_devnode(dev_parent);
								fstype      = udev_device_get_property_value(dev, "ID_FS_TYPE");
								label       = udev_device_get_property_value(dev, "ID_FS_LABEL_ENC");
								media_type  = get_device_type(dev_parent);

								if ((media_type > 0) && (fstype != NULL))
								{
									rc = add_media_partition(path_parent, media_type, path, fstype, label);
								}
							}
						}

						if (rc < 0)
						{
							udev_device_unref(dev);
							result = -1;
							goto exit_8;
						}
						else if (rc > 0)
						{
							// TODO: notify all clients
						}
					}
					else if ((strcmp(action, "remove") == 0) || (strcmp(action, "offline") == 0))
					{
						rc = 0;

						if (strcmp(devtype, "disk") == 0)
						{
							rc = remove_media_block(path);
						}
						else if (strcmp(devtype, "partition") == 0)
						{
							rc = remove_media_partition(NULL, path);
						}

						if (rc < 0)
						{
							udev_device_unref(dev);
							result = -1;
							goto exit_8;
						}
						else if (rc > 0)
						{
							// TODO: notify all clients
						}
					}
					else if (strcmp(action, "change") == 0)
					{
						// TODO: ignore?
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

		if ((pollfds[1].revents & POLLHUP) || (pollfds[1].revents & POLLERR))
		{
			result = -1;
			goto exit_8;
		}
		else if (pollfds[1].revents & POLLIN)
		{
			rc = accept(socketfd, NULL, NULL);
			if (rc < 0)
			{
				result = -1;
				goto exit_8;
			}

			rc = add_client(rc);
			if (rc < 0)
			{
				result = -1;
				goto exit_8;
			}
		}

		if (pollfds[2].revents & POLLHUP)
		{
			result = -1;
			goto exit_8;
		}
		else if (pollfds[2].revents & POLLERR)
		{
			if (check_mount_changes() < 0)
			{
				result = -1;
				goto exit_8;
			}
		}

		for (i = pollfds_count_default; i < pollfds_count; ++i)
		{
			if ((pollfds[i].revents & POLLHUP) || (pollfds[i].revents & POLLERR))
			{
				remove_client(pollfds[i].fd);
			}
			else if (pollfds[i].revents & POLLIN)
			{
				for (j = 0; j < clients_count; ++j)
				{
					if (clients[j]->clientfd == pollfds[i].fd)
					{
						break;
					}
				}

				if (j == clients_count)
				{
					result = -1;
					goto exit_8;
				}

				if (clients[j]->buf_size < clients[j]->buf_used + default_read_size)
				{
					if (clients[j]->buf_used + default_read_size >= maximum_read_size)
					{
						remove_client(pollfds[i].fd);
						continue;
					}

					tmp = realloc(clients[j]->buf, clients[j]->buf_used + default_read_size + 1);
					if (tmp == NULL)
					{
						result = -1;
						goto exit_8;
					}

					clients[j]->buf = (unsigned char*) tmp;
					clients[j]->buf_size = clients[j]->buf_used + default_read_size;
				}

				rc = read(clients[j]->clientfd, &(clients[j]->buf[clients[j]->buf_used]), default_read_size);
				if ((rc <= 0) && (errno != EINTR))
				{
					remove_client(pollfds[i].fd);
					continue;
				}

				clients[j]->buf[clients[j]->buf_used + rc] = 0;
				clients[j]->buf_used += rc;

				while ((tmp_str = (unsigned char*) strchr((const char*) clients[j]->buf, '\n')) != NULL)
				{
					rc = parse_command(j);
					if (rc < 0)
					{
						remove_client(pollfds[i].fd);
						break;
					}

					clients[j]->buf_used -= (tmp_str + 1 - clients[j]->buf);
					memmove(clients[j]->buf, tmp_str+1, clients[j]->buf_used);
				}
			}
		}

		if (pollfds_count != pollfds_count_default + clients_count)
		{
			pollfds_count = pollfds_count_default + clients_count;
			tmp = realloc(pollfds, sizeof(struct pollfd)*pollfds_count);
			if (tmp == NULL)
			{
				result = -1;
				goto exit_8;
			}

			pollfds = (struct pollfd*) tmp;
		}
	}

exit_8:
	free(pollfds);

exit_7:
	remove_all_media();
	remove_all_clients();

exit_mountfd:
	close(mountfd);

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
