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
#include <dirent.h>
#include <sys/stat.h>

#include <dtmd.h>
#include "daemon/dtmd-internal.h"
#include "daemon/lists.h"
#include "daemon/actions.h"
#include "daemon/mnt_funcs.h"
#include "daemon/system_module.h"
#include "daemon/config_file.h"

#define dtmd_daemon_lock "/var/lock/dtmd.lock"

#define DTMD_INVERTED_SOCKET_MASK 0

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

void remove_empty_dirs(const char *dirname)
{
	DIR *dir;
	struct dirent *d;
	char *fulldirname = NULL;
	int fulldirname_len = 0;
	int fulldirname_maxlen = 0;
	void *tmp;

	dir = opendir(dirname);
	if (dir == NULL)
	{
		return;
	}

	while ((d = readdir(dir)) != NULL)
	{
		if ((d->d_type == DT_DIR)
			&& (strcmp(d->d_name, ".") != 0)
			&& (strcmp(d->d_name, "..") != 0))
		{
			fulldirname_len = strlen(dirname) + strlen(d->d_name) + 1;

			if (fulldirname_len > fulldirname_maxlen)
			{
				fulldirname_maxlen = fulldirname_len;
				tmp = realloc(fulldirname, fulldirname_maxlen + 1);
				if (tmp == NULL)
				{
					goto remove_empty_dirs_error_1;
				}

				fulldirname = (char*) tmp;
			}

			strcpy(fulldirname, dirname);
			strcat(fulldirname, "/");
			strcat(fulldirname, d->d_name);

			/* next line will remove only empty and not mounted directories */
			rmdir(fulldirname);
		}
	}

remove_empty_dirs_error_1:
	if (fulldirname != NULL)
	{
		free(fulldirname);
	}

	closedir(dir);
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

	dtmd_device_system_t *dtmd_dev_system;
	dtmd_device_enumeration_t *dtmd_dev_enum;
	dtmd_device_monitor_t *dtmd_dev_mon;
	dtmd_info_t *dtmd_dev_device;
	dtmd_device_action_type_t dtmd_dev_action;

	int monfd;
	int mountfd;
	void *tmp;
	char *tmp_str;
#define pollfds_count_default 3
	unsigned int pollfds_count = pollfds_count_default;
	dtmd_command_t *cmd;

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

	lockfd = open(dtmd_daemon_lock, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (lockfd == -1)
	{
		fprintf(stderr, "Error obtaining lock file\n");
		result = -1;
		goto exit_2;
	}

	unlink(sockaddr.sun_path);
	def_mask = umask(DTMD_INVERTED_SOCKET_MASK);
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
	if (write(lockfd, buffer, sizeof(buffer) - 1) != sizeof(buffer) - 1)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error writing process pid to lockfile\n");
		}

		result = -1;
		goto exit_4;
	}

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

	dtmd_dev_system = device_system_init();
	if (dtmd_dev_system == NULL)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error opening device system\n");
		}

		result = -1;
		goto exit_4;
	}

	dtmd_dev_mon = device_system_start_monitoring(dtmd_dev_system);
	if (dtmd_dev_mon == NULL)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error opening device monitor\n");
		}

		result = -1;
		goto exit_5;
	}

	monfd = device_system_get_monitor_fd(dtmd_dev_mon);
	if (monfd < 0)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error getting device monitor file descriptor\n");
		}

		result = -1;
		goto exit_6;
	}

	mountfd = open(dtmd_internal_mounts_file, O_RDONLY);
	if (mountfd < 0)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error opening mounts file descriptor\n");
		}

		result = -1;
		goto exit_6;
	}

	dtmd_dev_enum = device_system_enumerate_devices(dtmd_dev_system);
	if (dtmd_dev_enum == NULL)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error querying device enumeration\n");
		}

		result = -1;
		goto exit_6;
	}

	read_config();

	while ((rc = device_system_next_enumerated_device(dtmd_dev_enum, &dtmd_dev_device)) > 0)
	{
		switch (dtmd_dev_device->type)
		{
		case dtmd_info_device:
			if ((dtmd_dev_device->path != NULL)
				&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent))
			{
				if (add_media_block(dtmd_dev_device->path, dtmd_dev_device->media_type) < 0)
				{
					device_system_free_enumerated_device(dtmd_dev_enum, dtmd_dev_device);
					device_system_finish_enumerate_devices(dtmd_dev_enum);
					result = -1;
					goto exit_7;
				}
			}
			break;

		case dtmd_info_partition:
			if ((dtmd_dev_device->path != NULL)
				&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent)
				&& (dtmd_dev_device->fstype != NULL)
				&& (dtmd_dev_device->path_parent != NULL))
			{
				if (add_media_partition(dtmd_dev_device->path_parent,
					dtmd_dev_device->media_type,
					dtmd_dev_device->path,
					dtmd_dev_device->fstype,
					dtmd_dev_device->label) < 0)
				{
					device_system_free_enumerated_device(dtmd_dev_enum, dtmd_dev_device);
					device_system_finish_enumerate_devices(dtmd_dev_enum);
					result = -1;
					goto exit_7;
				}
			}
			break;

		case dtmd_info_stateful_device:
			if ((dtmd_dev_device->path != NULL)
				&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent)
				&& (dtmd_dev_device->state != dtmd_removable_media_state_unknown))
			{
				if (add_stateful_media(dtmd_dev_device->path,
					dtmd_dev_device->media_type,
					dtmd_dev_device->state,
					dtmd_dev_device->fstype,
					dtmd_dev_device->label) < 0)
				{
					device_system_free_enumerated_device(dtmd_dev_enum, dtmd_dev_device);
					device_system_finish_enumerate_devices(dtmd_dev_enum);
					result = -1;
					goto exit_7;
				}
			}
			break;
		}

		device_system_free_enumerated_device(dtmd_dev_enum, dtmd_dev_device);
	}

	device_system_finish_enumerate_devices(dtmd_dev_enum);

	if (rc < 0)
	{
		result = -1;
		goto exit_7;
	}

	if (check_mount_changes() < 0)
	{
		result = -1;
		goto exit_7;
	}

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

			continue;
		}

		if ((pollfds[1].revents & POLLHUP) || (pollfds[1].revents & POLLERR) || (pollfds[1].revents & POLLNVAL))
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

		if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR) || (pollfds[0].revents & POLLNVAL))
		{
			result = -1;
			goto exit_8;
		}
		else if (pollfds[0].revents & POLLIN)
		{
			rc = device_system_monitor_get_device(dtmd_dev_mon, &dtmd_dev_device, &dtmd_dev_action);
			if (rc > 0)
			{
				rc = 0;

				switch (dtmd_dev_action)
				{
				case dtmd_device_action_add:
				case dtmd_device_action_online:
					switch (dtmd_dev_device->type)
					{
					case dtmd_info_device:
						if ((dtmd_dev_device->path != NULL)
							&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent))
						{
							rc = add_media_block(dtmd_dev_device->path, dtmd_dev_device->media_type);
						}
						break;

					case dtmd_info_partition:
						if ((dtmd_dev_device->path != NULL)
							&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent)
							&& (dtmd_dev_device->fstype != NULL)
							&& (dtmd_dev_device->path_parent != NULL))
						{
							rc = add_media_partition(dtmd_dev_device->path_parent,
								dtmd_dev_device->media_type,
								dtmd_dev_device->path,
								dtmd_dev_device->fstype,
								dtmd_dev_device->label);
						}
						break;

					case dtmd_info_stateful_device:
						if ((dtmd_dev_device->path != NULL)
							&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent)
							&& (dtmd_dev_device->state != dtmd_removable_media_state_unknown))
						{
							rc = add_stateful_media(dtmd_dev_device->path,
								dtmd_dev_device->media_type,
								dtmd_dev_device->state,
								dtmd_dev_device->fstype,
								dtmd_dev_device->label);
						}
						break;
					}
					break;

				case dtmd_device_action_remove:
				case dtmd_device_action_offline:
					switch (dtmd_dev_device->type)
					{
					case dtmd_info_device:
						if (dtmd_dev_device->path != NULL)
						{
							rc = remove_media_block(dtmd_dev_device->path);
						}
						break;

					case dtmd_info_partition:
						if (dtmd_dev_device->path != NULL)
						{
							rc = remove_media_partition(NULL, dtmd_dev_device->path);
						}
						break;

					case dtmd_info_stateful_device:
						if (dtmd_dev_device->path != NULL)
						{
							rc = remove_stateful_media(dtmd_dev_device->path);
						}
						break;
					}
					break;

				case dtmd_device_action_change:
					switch (dtmd_dev_device->type)
					{
					case dtmd_info_stateful_device:
						if ((dtmd_dev_device->path != NULL)
							&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent)
							&& (dtmd_dev_device->state != dtmd_removable_media_state_unknown))
						{
							rc = change_stateful_media(dtmd_dev_device->path,
								dtmd_dev_device->media_type,
								dtmd_dev_device->state,
								dtmd_dev_device->fstype,
								dtmd_dev_device->label);
						}
						break;
					}
					break;
				}

				device_system_monitor_free_device(dtmd_dev_mon, dtmd_dev_device);

				if (rc < 0)
				{
					result = -1;
					goto exit_8;
				}
			}
			else if (rc < 0)
			{
				result = -1;
				goto exit_8;
			}
		}

		if ((pollfds[2].revents & POLLHUP) || (pollfds[2].revents & POLLNVAL))
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
			if ((pollfds[i].revents & POLLHUP) || (pollfds[i].revents & POLLERR) || (pollfds[i].revents & POLLNVAL))
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

				rc = read(clients[j]->clientfd, &(clients[j]->buf[clients[j]->buf_used]), dtmd_command_max_length - clients[j]->buf_used);
				if ((rc <= 0) && (errno != EINTR))
				{
					remove_client(pollfds[i].fd);
					continue;
				}

				clients[j]->buf_used += rc;
				clients[j]->buf[clients[j]->buf_used] = 0;
				result = 1;

				while ((tmp_str = strchr(clients[j]->buf, '\n')) != NULL)
				{
					rc = dtmd_validate_command(clients[j]->buf);
					if (!rc)
					{
						remove_client(pollfds[i].fd);
						result = 0;
						break;
					}

					cmd = dtmd_parse_command(clients[j]->buf);
					if (cmd == NULL)
					{
						result = -1;
						goto exit_8;
					}

					rc = invoke_command(j, cmd);
					dtmd_free_command(cmd);
					if (rc < 0)
					{
						result = -1;
						goto exit_8;
					}

					clients[j]->buf_used -= (tmp_str + 1 - clients[j]->buf);
					memmove(clients[j]->buf, tmp_str+1, clients[j]->buf_used + 1);
				}

				if ((result) && (clients[j]->buf_used == dtmd_command_max_length))
				{
					remove_client(pollfds[i].fd);
					continue;
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
	// TODO: unmount all media on exit?
	remove_empty_dirs(dtmd_internal_mount_dir);
	unlink(dtmd_internal_mtab_temporary);

	free(pollfds);

exit_7:
	// first remove clients, because remove_all_* produces notifications
	remove_all_clients();
	remove_all_media();
	remove_all_stateful_media();
	close(mountfd);
	free_config();

exit_6:
	device_system_stop_monitoring(dtmd_dev_mon);

exit_5:
	device_system_deinit(dtmd_dev_system);

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
