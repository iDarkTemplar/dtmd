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
#include <sys/wait.h>

#include <dtmd.h>
#include "daemon/dtmd-internal.h"
#include "daemon/lists.h"
#include "daemon/actions.h"
#include "daemon/mnt_funcs.h"
#include "daemon/system_module.h"
#include "daemon/config_file.h"
#include "daemon/filesystem_mnt.h"
#include "daemon/log.h"
#include "daemon/return_codes.h"

#define dtmd_daemon_lock "/var/run/dtmd.pid"

#if (defined OS_FreeBSD)
#include <sys/event.h>
#include <sys/mount.h>
#error use EVFILT_FS
#error use VQ_MOUNT / VQ_UNMOUNT
#endif /* (defined OS_FreeBSD) */

static volatile unsigned char continue_working  = 1;
static unsigned char check_config_only = 0;

void print_usage(char *name)
{
	fprintf(stderr, "USAGE: %s [options]\n"
		"where options are one or more of following:\n"
		"\t-n\n"
		"\t--no-daemon\t- do not daemonize\n"
		"\t-c\n"
		"\t--check-config\t- check config file and quit\n",
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
		return result_fatal_error;
	}

	if (!daemonize)
	{
		if (sigaction(SIGINT, &action, NULL) < 0)
		{
			return result_fatal_error;
		}
	}

	action.sa_handler = SIG_IGN;

	if (sigaction(SIGPIPE, &action, NULL) < 0)
	{
		return result_fatal_error;
	}

	return result_success;
}

int unblockAllSignals(void)
{
	sigset_t set;

	if (sigfillset(&set) < 0)
	{
		return result_fatal_error;
	}

	if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
	{
		return result_fatal_error;
	}

	return result_success;
}

int redirectStdio(void)
{
	int fd1;
	int fd2;

	fd1 = open("/dev/null", O_RDONLY);
	if (fd1 == -1)
	{
		return result_fatal_error;
	}

	fd2 = open("/dev/null", O_WRONLY);
	if (fd2 == -1)
	{
		close(fd1);

		return result_fatal_error;
	}

	if (dup2(fd1, STDIN_FILENO) == -1)
	{
		close(fd1);
		close(fd2);

		return result_fatal_error;
	}

	if (dup2(fd2, STDOUT_FILENO) == -1)
	{
		close(fd1);
		close(fd2);

		return result_fatal_error;
	}

	if (dup2(fd2, STDERR_FILENO) == -1)
	{
		close(fd1);
		close(fd2);

		return result_fatal_error;
	}

	close(fd1);
	close(fd2);

	return result_success;
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

int create_mount_dir_recursive(char *directory)
{
	int length;
	int result = result_success;
	char *delim;
	struct stat dirstat;

	length = strlen(directory);
	if (length == 0)
	{
		return result_success;
	}

	if (stat(directory, &dirstat) == 0)
	{
		if (S_ISDIR(dirstat.st_mode))
		{
			return result_success;
		}
		else
		{
			return result_fatal_error;
		}
	}

	delim = strrchr(directory, '/');

	if (delim != NULL)
	{
		*delim = 0;
		result = create_mount_dir_recursive(directory);
		*delim = '/';
	}

	if (result == result_success)
	{
		result = mkdir(directory, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		if (result >= 0)
		{
			result = result_success;
		}
		else
		{
			result = result_fatal_error;
		}
	}

	return result;
}

int create_mount_dir(const char *directory)
{
	char *dir;
	int result;

	dir = strdup(directory);
	if (dir == NULL)
	{
		return result_fatal_error;
	}

	result = create_mount_dir_recursive(dir);
	free(dir);

	return result;
}

int main(int argc, char **argv)
{
	int result = 0;
	int socketfd;
	struct sockaddr_un sockaddr;
	int lockfd;
	int rc;
	uid_t process_uid;
	pid_t child = -1;
	char buffer[12];
	const int backlog = 4;
	unsigned int i;
	unsigned int j;
	struct stat st;
	int daemonpipe[2] = { -1, -1 };
	unsigned char daemondata;
	int successfully_initialized = 0;

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
		else if ((strcmp(argv[rc],"-c") == 0) || (strcmp(argv[rc],"--check-config") == 0))
		{
			check_config_only = 1;
		}
		else
		{
			print_usage(argv[0]);
			return -1;
		}
	}

	rc = read_config();
	if (check_config_only == 1)
	{
		free_config();

		switch (rc)
		{
		case read_config_return_ok:
			printf("Config file is correct\n");
			return 0;

		case read_config_return_no_file:
			printf("Config file is missing: defaults are assumed\n");
			return 0;

		default:
			printf("Config file is incorrect, error on line %d\n", rc);
			return -1;
		}
	}
	else
	{
		switch (rc)
		{
		case read_config_return_ok:
		case read_config_return_no_file:
			break;

		default:
			fprintf(stderr, "Config file is incorrect, error on line %d\n", rc);
			result = -1;
			goto exit_1;
		}
	}

	process_uid = geteuid();
	if (process_uid != 0)
	{
		fprintf(stderr, "Error: process UID is not root, UID is %u\n", process_uid);
		result = -1;
		goto exit_1;
	}

	umask(0);

	if (chdir("/") == -1)
	{
		fprintf(stderr, "Error changing directory to /\n");
		result = -1;
		goto exit_1;
	}

	if (create_mount_dir_on_startup)
	{
		rc = create_mount_dir((mount_dir != NULL) ? mount_dir : dtmd_internal_mount_dir);
		if (is_result_failure(rc))
		{
			fprintf(stderr, "Error: could not create mount directory\n");
			result = -1;
			goto exit_1;
		}
	}
	else
	{
		if ((stat((mount_dir != NULL) ? mount_dir : dtmd_internal_mount_dir, &st) != 0)
			|| (!S_ISDIR(st.st_mode)))
		{
			fprintf(stderr, "Error: mount directory does not exist or is not a directory\n");
			result = -1;
			goto exit_1;
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
	rc = bind(socketfd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un));
	if (rc == -1)
	{
		fprintf(stderr, "Error binding socket\n");
		result = -1;
		goto exit_3;
	}

	if (daemonize)
	{
		if (pipe(daemonpipe) != 0)
		{
			fprintf(stderr, "Error creating pipes\n");
			result = -1;
			goto exit_4;
		}

		child = fork();
		if (child == -1)
		{
			fprintf(stderr, "Error forking\n");

			close(daemonpipe[0]);
			close(daemonpipe[1]);
			daemonpipe[0] = -1;
			daemonpipe[1] = -1;

			result = -1;
			goto exit_4;
		}
		else if (child != 0)
		{
			// parent - exit
			close(socketfd);
			close(lockfd);

			close(daemonpipe[1]);
			daemonpipe[1] = -1;

			waitpid(child, &rc, 0);

			if ((read(daemonpipe[0], &daemondata, sizeof(unsigned char)) != sizeof(unsigned char))
				|| (daemondata != 1))
			{
				fprintf(stderr, "Failed starting daemon\n");
				result = -1;
			}

			close(daemonpipe[0]);
			daemonpipe[0] = -1;

			goto exit_1;
		}

		// child - daemon
		close(daemonpipe[0]);
		daemonpipe[0] = -1;
	}

	if (daemonize)
	{
		if (setsid() == -1)
		{
			result = -1;
			goto exit_4_pipe;
		}

		child = fork();
		if (child == -1)
		{
			result = -1;
			goto exit_4_pipe;
		}
		else if (child != 0)
		{
			// parent - exit
			close(socketfd);
			close(lockfd);
			close(daemonpipe[1]);
			daemonpipe[1] = -1;
			result = 0;
			goto exit_1;
		}

		// child - daemon
	}

	if (is_result_failure(unblockAllSignals()))
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error unblocking signal handlers\n");
		}

		result = -1;
		goto exit_4_pipe;
	}

	if (is_result_failure(setSigHandlers()))
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error setting signal handlers\n");
		}

		result = -1;
		goto exit_4_pipe;
	}

	snprintf(buffer, sizeof(buffer), "%10d\n", getpid());
	if (write(lockfd, buffer, sizeof(buffer) - 1) != sizeof(buffer) - 1)
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error writing process pid to lockfile\n");
		}

		result = -1;
		goto exit_4_pipe;
	}

#if (defined _POSIX_SYNCHRONIZED_IO) && (_POSIX_SYNCHRONIZED_IO > 0)
	if (fdatasync(lockfd) != 0)
#else
	if (fsync(lockfd) != 0)
#endif
	{
		if (!daemonize)
		{
			fprintf(stderr, "Error synchronizing lockfile\n");
		}

		result = -1;
		goto exit_4_pipe;
	}

	if (daemonize)
	{
		if (is_result_failure(redirectStdio()))
		{
			result = -1;
			goto exit_4_pipe;
		}
	}

	if (!daemonize)
	{
		// use stderr instead if it is available
		use_syslog = 0;
	}

#ifndef DISABLE_SYSLOG
	if (use_syslog)
	{
		openlog("dtmd", LOG_PID, LOG_DAEMON);
	}
#endif /* DISABLE_SYSLOG */

	if (listen(socketfd, backlog) == -1)
	{
		WRITE_LOG(LOG_ERR, "Error listening socket");
		result = -1;
		goto exit_4_log;
	}

	dtmd_dev_system = device_system_init();
	if (dtmd_dev_system == NULL)
	{
		result = -1;
		goto exit_4_log;
	}

	dtmd_dev_mon = device_system_start_monitoring(dtmd_dev_system);
	if (dtmd_dev_mon == NULL)
	{
		result = -1;
		goto exit_5;
	}

	monfd = device_system_get_monitor_fd(dtmd_dev_mon);
	if (monfd < 0)
	{
		result = -1;
		goto exit_6;
	}

	mountfd = open(dtmd_internal_mounts_file, O_RDONLY);
	if (mountfd < 0)
	{
		WRITE_LOG(LOG_ERR, "Error opening mounts file descriptor");
		result = -1;
		goto exit_6;
	}

	dtmd_dev_enum = device_system_enumerate_devices(dtmd_dev_system);
	if (dtmd_dev_enum == NULL)
	{
		result = -1;
		goto exit_6;
	}

	while (is_result_successful(rc = device_system_next_enumerated_device(dtmd_dev_enum, &dtmd_dev_device)))
	{
		switch (dtmd_dev_device->type)
		{
		case dtmd_info_device:
			if ((dtmd_dev_device->path != NULL)
				&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent))
			{
				if (is_result_fatal_error(add_media_block(dtmd_dev_device->path, dtmd_dev_device->media_type)))
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
				&& (dtmd_dev_device->path_parent != NULL))
			{
				if (is_result_fatal_error(add_media_partition(dtmd_dev_device->path_parent,
					dtmd_dev_device->media_type,
					dtmd_dev_device->path,
					dtmd_dev_device->fstype,
					dtmd_dev_device->label)))
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
				if (is_result_fatal_error(add_stateful_media(dtmd_dev_device->path,
					dtmd_dev_device->media_type,
					dtmd_dev_device->state,
					dtmd_dev_device->fstype,
					dtmd_dev_device->label)))
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

	if (is_result_fatal_error(rc))
	{
		result = -1;
		goto exit_7;
	}

	if (is_result_fatal_error(check_mount_changes()))
	{
		result = -1;
		goto exit_7;
	}

	pollfds = (struct pollfd*) malloc(sizeof(struct pollfd)*pollfds_count);
	if (pollfds == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = -1;
		goto exit_7;
	}

	if (daemonpipe[1] != -1)
	{
		// successful initialization
		daemondata = 1;
		write(daemonpipe[1], &daemondata, sizeof(unsigned char));
		close(daemonpipe[1]);
		daemonpipe[1] = -1;
	}

	successfully_initialized = 1;

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
				WRITE_LOG_ARGS(LOG_ERR, "Poll failed, errno %d", errno);
				result = -1;
				goto exit_8;
			}

			continue;
		}

		if ((pollfds[1].revents & POLLHUP) || (pollfds[1].revents & POLLERR) || (pollfds[1].revents & POLLNVAL))
		{
			WRITE_LOG(LOG_ERR, "Invalid poll result on client socket");
			result = -1;
			goto exit_8;
		}
		else if (pollfds[1].revents & POLLIN)
		{
			rc = accept(socketfd, NULL, NULL);
			if (rc < 0)
			{
				WRITE_LOG_ARGS(LOG_ERR, "Accepting client failed, errno %d", errno);
				result = -1;
				goto exit_8;
			}

			rc = add_client(rc);
			if (is_result_fatal_error(rc))
			{
				result = -1;
				goto exit_8;
			}
		}

		if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR) || (pollfds[0].revents & POLLNVAL))
		{
			WRITE_LOG(LOG_ERR, "Invalid poll result on device monitoring socket");
			result = -1;
			goto exit_8;
		}
		else if (pollfds[0].revents & POLLIN)
		{
			rc = device_system_monitor_get_device(dtmd_dev_mon, &dtmd_dev_device, &dtmd_dev_action);
			if (is_result_successful(rc))
			{
				rc = result_fail;

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
					case dtmd_info_device:
						if ((dtmd_dev_device->path != NULL)
							&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent))
						{
							rc = change_media_block(dtmd_dev_device->path, dtmd_dev_device->media_type);
						}
						break;

					case dtmd_info_partition:
						if ((dtmd_dev_device->path != NULL)
							&& (dtmd_dev_device->media_type != dtmd_removable_media_unknown_or_persistent)
							&& (dtmd_dev_device->path_parent != NULL))
						{
							rc = change_media_partition(dtmd_dev_device->path_parent,
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
			}

			if (is_result_fatal_error(rc))
			{
				result = -1;
				goto exit_8;
			}
		}

		if ((pollfds[2].revents & POLLHUP) || (pollfds[2].revents & POLLNVAL))
		{
			WRITE_LOG(LOG_ERR, "Invalid poll result on device monitoring socket");
			result = -1;
			goto exit_8;
		}
		else if (pollfds[2].revents & POLLERR)
		{
			if (is_result_fatal_error(check_mount_changes()))
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
					WRITE_LOG(LOG_ERR, "BUG: could not find non-existent client");
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

				while ((tmp_str = strchr(clients[j]->buf, '\n')) != NULL)
				{
					rc = dtmd_validate_command(clients[j]->buf);
					if (!rc)
					{
						goto exit_remove_client;
					}

					cmd = dtmd_parse_command(clients[j]->buf);
					if (cmd == NULL)
					{
						WRITE_LOG(LOG_ERR, "Memory allocation failure");
						result = -1;
						goto exit_8;
					}

					rc = invoke_command(j, cmd);
					dtmd_free_command(cmd);

					switch (rc)
					{
					case result_bug:
					case result_fatal_error:
						result = -1;
						goto exit_8;

					case result_client_error:
						goto exit_remove_client;

					case result_fail:
					case result_success:
					default:
						break;
					}

					clients[j]->buf_used -= (tmp_str + 1 - clients[j]->buf);
					memmove(clients[j]->buf, tmp_str+1, clients[j]->buf_used + 1);
				}

				if (clients[j]->buf_used == dtmd_command_max_length)
				{
exit_remove_client:
					remove_client(pollfds[i].fd);
				}
			}
		}

		if (pollfds_count != pollfds_count_default + clients_count)
		{
			pollfds_count = pollfds_count_default + clients_count;
			tmp = realloc(pollfds, sizeof(struct pollfd)*pollfds_count);
			if (tmp == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				result = -1;
				goto exit_8;
			}

			pollfds = (struct pollfd*) tmp;
		}
	}

exit_8:
#if (defined OS_Linux)
	unlink(dtmd_internal_mtab_temporary);
#endif /* (defined OS_Linux) */

	free(pollfds);

exit_7:
	// first remove clients, because remove_all_* produces notifications
	remove_all_clients();

	if (unmount_on_exit)
	{
		invoke_unmount_all(-1);
	}

	if (successfully_initialized && clear_mount_dir)
	{
		remove_empty_dirs((mount_dir != NULL) ? mount_dir : dtmd_internal_mount_dir);
	}

	remove_all_media();
	remove_all_stateful_media();
	close(mountfd);

exit_6:
	device_system_stop_monitoring(dtmd_dev_mon);

exit_5:
	device_system_deinit(dtmd_dev_system);

exit_4_log:
#ifndef DISABLE_SYSLOG
	if (use_syslog)
	{
		closelog();
	}
#endif /* DISABLE_SYSLOG */

exit_4_pipe:
	if (daemonpipe[1] != -1)
	{
		// failed initialization
		daemondata = 0;
		write(daemonpipe[1], &daemondata, sizeof(unsigned char));
		close(daemonpipe[1]);
		daemonpipe[1] = -1;
	}

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
	free_config();
	return result;
}
