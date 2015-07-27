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

void client_callback(void *arg, const dt_command_t *cmd)
{
	if (arg == (void*)1)
	{
		if (cmd != NULL)
		{
			if ((strcmp(cmd->cmd, dtmd_notification_add_disk) == 0) && (cmd->args_count == 2)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Disk added\nPath: %s\nType: %s\n", cmd->args[0], cmd->args[1]);
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_remove_disk) == 0) && (cmd->args_count == 1)
				&& (cmd->args[0] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Disk removed\nPath: %s\n", cmd->args[0]);
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_disk_changed) == 0) && (cmd->args_count == 2)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Disk changed\nPath: %s\nType: %s\n", cmd->args[0], cmd->args[1]);
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_add_partition) == 0) && (cmd->args_count == 4)
				&& (cmd->args[0] != NULL) && (cmd->args[3] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition added\nParent device: %s\nPath: %s\n", cmd->args[3], cmd->args[0]);

				if (cmd->args[1] != NULL)
				{
					fprintf(stdout, "Filesystem type: %s\n", cmd->args[1]);
				}

				if (cmd->args[2] != NULL)
				{
					fprintf(stdout, "Label: %s\n", cmd->args[2]);
				}
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_remove_partition) == 0) && (cmd->args_count == 1)
				&& (cmd->args[0] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition removed\nPath: %s\n", cmd->args[0]);
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_partition_changed) == 0) && (cmd->args_count == 4)
				&& (cmd->args[0] != NULL) && (cmd->args[3] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition changed\nParent device: %s\nPath: %s\n", cmd->args[3], cmd->args[0]);

				if (cmd->args[1] != NULL)
				{
					fprintf(stdout, "Filesystem type: %s\n", cmd->args[1]);
				}

				if (cmd->args[2] != NULL)
				{
					fprintf(stdout, "Label: %s\n", cmd->args[2]);
				}
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_add_stateful_device) == 0) && (cmd->args_count == 5)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Stateful device added\nPath: %s\nType: %s\nState: %s\n", cmd->args[0], cmd->args[1], cmd->args[2]);

				if (cmd->args[3] != NULL)
				{
					fprintf(stdout, "Filesystem type: %s\n", cmd->args[3]);
				}

				if (cmd->args[4] != NULL)
				{
					fprintf(stdout, "Label: %s\n", cmd->args[4]);
				}
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_remove_stateful_device) == 0) && (cmd->args_count == 1)
				&& (cmd->args[0] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Stateful device removed\nPath: %s\n", cmd->args[0]);
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_stateful_device_changed) == 0) && (cmd->args_count == 5)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Stateful device changed\nPath: %s\nType: %s\nState: %s\n", cmd->args[0], cmd->args[1], cmd->args[2]);

				if (cmd->args[3] != NULL)
				{
					fprintf(stdout, "Filesystem type: %s\n", cmd->args[3]);
				}

				if (cmd->args[4] != NULL)
				{
					fprintf(stdout, "Label: %s\n", cmd->args[4]);
				}
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_mount) == 0) && (cmd->args_count == 3)
				&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
			{
				print_first(first);
				fprintf(stdout, "Partition mounted\nPath: %s\nMount point: %s\nMount options: %s\n", cmd->args[0], cmd->args[1], cmd->args[2]);
			}
			else if ((strcmp(cmd->cmd, dtmd_notification_unmount) == 0) && (cmd->args_count == 2)
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
		"\t\tlist_stateful_device path\n"
		"\t\tmount device [ mount_options ]\n"
		"\t\tunmount device\n"
		"\t\tls_fs\n"
		"\t\tls_fs_opts\n"
		"\t\tmonitor\n", app);
}

void client_print_partition(const dtmd_partition_t *partition)
{
	fprintf(stdout, "Path: %s\n", partition->path);

	if (partition->fstype != NULL)
	{
		fprintf(stdout, "Filesystem type: %s\n", partition->fstype);
	}

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
}

void client_print_device(const dtmd_device_t *device)
{
	size_t i;

	fprintf(stdout, "Path: %s\n", device->path);
	fprintf(stdout, "Type: %s\n", dtmd_device_type_to_string(device->type));
	fprintf(stdout, "Partitions: %zu\n\n", device->partitions_count);

	for (i = 0; i < device->partitions_count; ++i)
	{
		if (i != 0)
		{
			fprintf(stdout, "\n");
		}

		fprintf(stdout, "Partition %zu:\n", i);
		client_print_partition(device->partition[i]);
	}
}

void client_print_stateful_device(const dtmd_stateful_device_t *stateful_device)
{
	fprintf(stdout, "Path: %s\n", stateful_device->path);
	fprintf(stdout, "Type: %s\n", dtmd_device_type_to_string(stateful_device->type));
	fprintf(stdout, "State: %s\n", dtmd_device_state_to_string(stateful_device->state));

	if (stateful_device->fstype != NULL)
	{
		fprintf(stdout, "Filesystem type: %s\n", stateful_device->fstype);
	}

	if (stateful_device->label != NULL)
	{
		fprintf(stdout, "Label: %s\n", stateful_device->label);
	}

	if (stateful_device->mnt_point != NULL)
	{
		fprintf(stdout, "Mount point: %s\n", stateful_device->mnt_point);
	}

	if (stateful_device->mnt_opts != NULL)
	{
		fprintf(stdout, "Mount options: %s\n", stateful_device->mnt_opts);
	}
}

int client_enumerate(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	size_t count, count_stateful, i;
	dtmd_device_t **devices;
	dtmd_stateful_device_t **stateful_devices;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_enum_devices(lib, -1, &count, &devices, &count_stateful, &stateful_devices);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't enumerate devices, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Found %zu devices\n\n", count + count_stateful);

	for (i = 0; i < count_stateful; ++i)
	{
		fprintf(stdout, "Device %zu:\n", i);
		client_print_stateful_device(stateful_devices[i]);
		fprintf(stdout, "\n");
	}

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "Device %zu:\n", i + count_stateful);
		client_print_device(devices[i]);
		fprintf(stdout, "\n");
	}

	dtmd_free_stateful_devices_array(lib, count_stateful, stateful_devices);
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
		fprintf(stderr, "Couldn't list device, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
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
		fprintf(stderr, "Couldn't list partition, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
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

int client_list_stateful_device(const char *path)
{
	dtmd_t *lib;
	dtmd_result_t result;
	dtmd_stateful_device_t *stateful_device;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_stateful_device(lib, -1, path, &stateful_device);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list stateful device, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	if (stateful_device != NULL)
	{
		client_print_stateful_device(stateful_device);
		dtmd_free_stateful_device(lib, stateful_device);
	}
	else
	{
		fprintf(stdout, "Didn't find stateful device with specified path!\n");
	}

	dtmd_deinit(lib);

	return 0;
}

int client_mount(char *device, char *mount_opts)
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

	result = dtmd_mount(lib, -1, device, mount_opts);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Mount successful!\n");
	}
	else
	{
		fprintf(stdout, "Mount failed, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}

int client_unmount(char *device)
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

	result = dtmd_unmount(lib, -1, device);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Unmount successful!\n");
	}
	else
	{
		fprintf(stdout, "Unmount failed, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}

int client_list_supported_filesystems(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	size_t count, i;
	const char **filesystems;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_supported_filesystems(lib, -1, &count, &filesystems);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list supported filesystems, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Got %zu supported filesystems:\n", count);

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "\t%s\n", filesystems[i]);
	}

	dtmd_free_supported_filesystems_list(lib, count, filesystems);
	dtmd_deinit(lib);

	return 0;
}

int client_list_supported_filesystem_options(const char *filesystem)
{
	dtmd_t *lib;
	dtmd_result_t result;
	size_t count, i;
	const char **options_list;

	lib = dtmd_init(&client_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_supported_filesystem_options(lib, -1,filesystem, &count, &options_list);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list supported filesystems, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Got %zu supported filesystem options for %s:\n", count, filesystem);

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "\t%s\n", options_list[i]);
	}

	dtmd_free_supported_filesystem_options_list(lib, count, options_list);
	dtmd_deinit(lib);

	return 0;
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
	else if ((argc == 3) && (strcmp(argv[1], "list_stateful_device") == 0))
	{
		return client_list_stateful_device(argv[2]);
	}
	else if (((argc == 3) || (argc == 4)) && (strcmp(argv[1], "mount") == 0))
	{
		return client_mount(argv[2], (argc == 4) ? (argv[3]) : (NULL));
	}
	else if ((argc == 3) && (strcmp(argv[1], "unmount") == 0))
	{
		return client_unmount(argv[2]);
	}
	else if ((argc == 2) && (strcmp(argv[1], "ls_fs") == 0))
	{
		return client_list_supported_filesystems();
	}
	else if ((argc == 3) && (strcmp(argv[1], "ls_fs_opts") == 0))
	{
		return client_list_supported_filesystem_options(argv[2]);
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
