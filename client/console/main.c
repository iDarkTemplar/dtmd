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

void client_callback(void *arg, const dtmd_command_t *cmd)
{
	if (arg == (void*)1)
	{
		//
	}
}

void printUsage(char *app)
{
	fprintf(stderr, "USAGE: %s [ enumerate | mount device mount_point [ mount_options ]\n"
	"\t| unmount device mount_point | monitor\n", app);
}

int client_enumerate(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	unsigned int count, i, j;
	dtmd_device_t **devices;

	lib = dtmd_init(&client_callback, (void*)0);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library\n");
		return -1;
	}

	result = dtmd_enum_devices(lib, -1, &count, &devices);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't enumerate devices, error code %d\n", result);
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Found %u devices\n", count);

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "Device %u:\n", i);
		fprintf(stdout, "Path: %s\n", devices[i]->path);
		fprintf(stdout, "Type: %s\n", dtmd_device_type_to_string(devices[i]->type));
		fprintf(stdout, "Partitions: %u\n\n", devices[i]->partitions_count);

		for (j = 0; j < devices[i]->partitions_count; ++j)
		{
			fprintf(stdout, "Partition %u:\n", j);
			fprintf(stdout, "Path: %s\n", devices[i]->partition[j]->path);
			fprintf(stdout, "Filesystem type: %s\n", devices[i]->partition[j]->type);

			if (devices[i]->partition[j]->label != NULL)
			{
				fprintf(stdout, "Label: %s\n", devices[i]->partition[j]->label);
			}

			if (devices[i]->partition[j]->mnt_point != NULL)
			{
				fprintf(stdout, "Mount point: %s\n", devices[i]->partition[j]->mnt_point);
			}

			if (devices[i]->partition[j]->mnt_opts != NULL)
			{
				fprintf(stdout, "Mount options: %s\n", devices[i]->partition[j]->mnt_opts);
			}
		}
	}

	dtmd_free_devices_array(lib, count, devices);
	dtmd_deinit(lib);

	return 0;
}

int client_mount(char *device, char *mount_point, char *mount_opts)
{
	dtmd_t *lib;
	dtmd_result_t result;
	int func_result = 0;

	lib = dtmd_init(&client_callback, (void*)0);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library\n");
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

	lib = dtmd_init(&client_callback, (void*)0);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library\n");
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
	return 0;
}

int main(int argc, char **argv)
{
	if ((argc == 2) && (strcmp(argv[1], "enumerate") == 0))
	{
		return client_enumerate();
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
