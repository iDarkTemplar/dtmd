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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

volatile int run = 1;

void client_sighandler(int signum)
{
	run = 0;
}

int first = 1;

#define print_first(var) \
	if (var) \
	{ \
		var = 0; \
	} \
	else \
	{ \
		fprintf(stdout, "\n"); \
	}

void client_callback(void *arg, const dtmd_command_t *cmd)
{
	if (arg == (void*)1)
	{
		if (cmd != NULL)
		{
			if ((strcmp(cmd->cmd, "add_disk") == 0) && (cmd->args_count == 2)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Disk added\nPath: %s\nType: %s\n", cmd->args[0], cmd->args[1]);
			}
			else if ((strcmp(cmd->cmd, "remove_disk") == 0) && (cmd->args_count == 1)
				&& (cmd->args[0] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Disk removed\nPath: %s\n", cmd->args[0]);
			}
			else if ((strcmp(cmd->cmd, "add_partition") == 0) && (cmd->args_count == 4)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[3] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition added\nParent device: %s\nPath: %s\nFilesystem type: %s\n", cmd->args[3], cmd->args[0], cmd->args[1]);

				if (cmd->args[2] != NULL)
				{
					fprintf(stdout, "Label: %s\n", cmd->args[2]);
				}
			}
			else if ((strcmp(cmd->cmd, "remove_partition") == 0) && (cmd->args_count == 1)
				&& (cmd->args[0] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition removed\nPath: %s\n", cmd->args[0]);
			}
			else if ((strcmp(cmd->cmd, "mount") == 0) && (cmd->args_count == 3)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition mounted\nPath: %s\nMount point: %s\nMount options: %s\n", cmd->args[0], cmd->args[1], cmd->args[2]);
			}
			else if ((strcmp(cmd->cmd, "unmount") == 0) && (cmd->args_count == 2)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition unmounted\nPath: %s\nMount point: %s\n", cmd->args[0], cmd->args[1]);
			}
		}
		else
		{
			fprintf(stdout, "Got exit signal from daemon\n");
			run = 0;
		}
	}
}

void printUsage(char *app)
{
	fprintf(stderr, "USAGE: %s command\n"
		"\twhere command is one of following:\n"
		"\t\tenumerate\n"
		"\t\tlist_device path\n"
		"\t\tlist_partition path\n"
		"\t\tmount device mount_point [ mount_options ]\n"
		"\t\tunmount device mount_point\n"
		"\t\tmonitor\n", app);
}

void client_print_partition(const dtmd_partition_t *partition)
{
	fprintf(stdout, "Path: %s\n", partition->path);
	fprintf(stdout, "Filesystem type: %s\n", partition->type);

	if (partition->label != NULL)
	{
		fprintf(stdout, "Label: %s\n", partition->label);
	}

	if (partition->mnt_point != NULL)
	{
		fprintf(stdout, "Mount point: %s\n", partition->mnt_point);
	}

	if (partition->mnt_opts != NULL)
	{
		fprintf(stdout, "Mount options: %s\n", partition->mnt_opts);
	}

	fprintf(stdout, "\n");
}

void client_print_device(const dtmd_device_t *device)
{
	unsigned int i;

	fprintf(stdout, "Path: %s\n", device->path);
	fprintf(stdout, "Type: %s\n", dtmd_device_type_to_string(device->type));
	fprintf(stdout, "Partitions: %u\n\n", device->partitions_count);

	for (i = 0; i < device->partitions_count; ++i)
	{
		fprintf(stdout, "Partition %u:\n", i);
		client_print_partition(device->partition[i]);
	}
}

int client_enumerate(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	unsigned int count, i;
	dtmd_device_t **devices;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_enum_devices(lib, -1, &count, &devices);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't enumerate devices, error code %d\n", result);
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Found %u devices\n\n", count);

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "Device %u:\n", i);
		client_print_device(devices[i]);
	}

	dtmd_free_devices_array(lib, count, devices);
	dtmd_deinit(lib);

	return 0;
}

int client_list_device(const char *path)
{
	dtmd_t *lib;
	dtmd_result_t result;
	dtmd_device_t *device;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_device(lib, -1, path, &device);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list device, error code %d\n", result);
		dtmd_deinit(lib);
		return -1;
	}

	if (device != NULL)
	{
		client_print_device(device);
		dtmd_free_device(lib, device);
	}
	else
	{
		fprintf(stdout, "Didn't find device with specified path!\n");
	}

	dtmd_deinit(lib);

	return 0;
}

int client_list_partition(const char *path)
{
	dtmd_t *lib;
	dtmd_result_t result;
	dtmd_partition_t *partition;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_partition(lib, -1, path, &partition);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list partition, error code %d\n", result);
		dtmd_deinit(lib);
		return -1;
	}

	if (partition != NULL)
	{
		client_print_partition(partition);
		dtmd_free_partition(lib, partition);
	}
	else
	{
		fprintf(stdout, "Didn't find partition with specified path!\n");
	}

	dtmd_deinit(lib);

	return 0;
}

int client_mount(char *device, char *mount_point, char *mount_opts)
{
	dtmd_t *lib;
	dtmd_result_t result;
	int func_result = 0;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_mount(lib, -1, device, mount_point, mount_opts);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Mount successful!\n");
	}
	else
	{
		fprintf(stdout, "Mount failed, error code %d\n", result);
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}

int client_unmount(char *device, char *mount_point)
{
	dtmd_t *lib;
	dtmd_result_t result;
	int func_result = 0;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_unmount(lib, -1, device, mount_point);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Unmount successful!\n");
	}
	else
	{
		fprintf(stdout, "Unmount failed, error code %d\n", result);
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}

int client_monitor(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	struct sigaction act;

	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = &client_sighandler;

	if ((sigaction(SIGINT, &act, NULL) != 0) || (sigaction(SIGTERM, &act, NULL) != 0))
	{
		fprintf(stderr, "Couldn't set signal handler\n");
		return -1;
	}

	lib = dtmd_init(&client_callback, (void*)1, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	while (run)
	{
		sleep(1);
	}

	dtmd_deinit(lib);

	return 0;
}

int main(int argc, char **argv)
{
	if ((argc == 2) && (strcmp(argv[1], "enumerate") == 0))
	{
		return client_enumerate();
	}
	else if ((argc == 3) && (strcmp(argv[1], "list_device") == 0))
	{
		return client_list_device(argv[2]);
	}
	else if ((argc == 3) && (strcmp(argv[1], "list_partition") == 0))
	{
		return client_list_partition(argv[2]);
	}
	else if (((argc == 4) || (argc == 5)) && (strcmp(argv[1], "mount") == 0))
	{
		return client_mount(argv[2], argv[3], (argc == 5) ? (argv[4]) : (NULL));
	}
	else if ((argc == 4) && (strcmp(argv[1], "unmount") == 0))
	{
		return client_unmount(argv[2], argv[3]);
	}
	else if ((argc == 2) && (strcmp(argv[1], "monitor") == 0))
	{
		return client_monitor();
	}
	else
	{
		printUsage(argv[0]);
		return -1;
	}
}
