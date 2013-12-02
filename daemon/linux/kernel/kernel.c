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

#include "daemon/system_module.h"
#include "daemon/lists.h"

#include <blkid.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#if OS == Linux
#define __USE_GNU
#endif /* OS == Linux */

#include <net/if.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#define block_devices_dir "/sys/block"
#define block_mmc_devices_dir "/sys/bus/mmc/devices"
#define block_usb_devices_dir "/sys/bus/usb/devices"

#define filename_dev "dev"
#define filename_removable "removable"
#define filename_device_type "device/type"
#define removable_correct_value 1
#define devices_dir "/dev"
#define block_sys_dir "/sys"
#define block_dir_name "block"

#define scsi_type_direct_access "0"
#define scsi_type_cd_dvd "5"
#define scsi_type_sd_card "SD"

#define NETLINK_STRING_ACTION "ACTION="
#define NETLINK_STRING_SUBSYSTEM "SUBSYSTEM="
#define NETLINK_STRING_DEVNAME "DEVNAME="
#define NETLINK_STRING_DEVTYPE "DEVTYPE="
#define NETLINK_STRING_DEVPATH "DEVPATH="

#define NETLINK_STRING_ACTION_ADD "add"
#define NETLINK_STRING_ACTION_ONLINE "online"
#define NETLINK_STRING_ACTION_REMOVE "remove"
#define NETLINK_STRING_ACTION_OFFLINE "offline"
#define NETLINK_STRING_ACTION_CHANGE "change"
#define NETLINK_STRING_SUBSYSTEM_BLOCK "block"
#define NETLINK_STRING_DEVTYPE_DISK "disk"
#define NETLINK_STRING_DEVTYPE_PARTITION "partition"

#define NETLINK_GROUP_KERNEL 1

// TODO: build hierarchy of devices and store it. Use second thread to receive netlink events.

struct dtmd_device_enumeration
{
	dtmd_device_system_t *system;

	DIR *dir_pointer;
	DIR *dir_pointer_partitions;

	struct dirent *dirent_device;
	struct dirent *dirent_device_partition;

	DIR *dir_pointer_usb;
	DIR *dir_pointer_usb_device;
	DIR *dir_pointer_usb_host;
	DIR *dir_pointer_usb_target;
	DIR *dir_pointer_usb_target_device;
	DIR *dir_pointer_usb_target_device_blocks;

	struct dirent *dirent_usb;
	struct dirent *dirent_usb_device;
	struct dirent *dirent_usb_host;
	struct dirent *dirent_usb_target;
	struct dirent *dirent_usb_target_device;
	struct dirent *dirent_usb_target_device_blocks;

	DIR *dir_pointer_mmc;
	DIR *dir_pointer_mmc_device;
	DIR *dir_pointer_mmc_device_blocks;

	struct dirent *dirent_mmc;
	struct dirent *dirent_mmc_device;
	struct dirent *dirent_mmc_device_blocks;
};

struct dtmd_device_monitor
{
	dtmd_device_system_t *system;
	int fd;
};

struct dtmd_device_system
{
	uint16_t enumeration_count;
	dtmd_device_enumeration_t **enumerations;

	uint16_t monitor_count;
	dtmd_device_monitor_t **monitors;
};

static int read_int_from_file(const char *filename)
{
	FILE *file;
	int value = 0;
	int read_val;
	int not_first = 0;

	file = fopen(filename, "r");
	if (file == NULL)
	{
		goto read_int_from_file_error_1;
	}

	while ((read_val = fgetc(file)) != EOF)
	{
		if ((read_val < '0')
			|| (read_val > '9'))
		{
			if (not_first)
			{
				break;
			}
			else
			{
				goto read_int_from_file_error_2;
			}
		}

		not_first = 1;
		value = (value * 10) + (read_val - '0');
	}

	fclose(file);

	return value;

read_int_from_file_error_2:
	fclose(file);

read_int_from_file_error_1:
	return -1;
}

static char* read_string_from_file(const char *filename)
{
	FILE *file;
	char *result;
	int result_len = 0;
	int read_val;
	void *tmp;

	file = fopen(filename, "r");
	if (file == NULL)
	{
		goto read_string_from_file_error_1;
	}

	result = (char*) malloc((result_len+1) * sizeof(char));
	if (result == NULL)
	{
		goto read_string_from_file_error_2;
	}

	while ((read_val = fgetc(file)) != EOF)
	{
		if (!isprint(read_val))
		{
			break;
		}

		tmp = realloc(result, result_len + 2);
		if (tmp == NULL)
		{
			goto read_string_from_file_error_3;
		}

		result = (char*) tmp;
		result[result_len++] = read_val;
	}

	fclose(file);
	result[result_len] = 0;

	return result;

read_string_from_file_error_3:
	free(result);

read_string_from_file_error_2:
	fclose(file);

read_string_from_file_error_1:
	return NULL;
}

static dtmd_removable_media_type_t device_type_from_string(const char *string)
{
	if (strcmp(string, scsi_type_direct_access) == 0)
	{
		return dtmd_removable_media_removable_disk;
	}
	else if (strcmp(string, scsi_type_sd_card) == 0)
	{
		return dtmd_removable_media_sd_card;
	}
	else if (strcmp(string, scsi_type_cd_dvd) == 0)
	{
		return dtmd_removable_media_cdrom;
	}
	else
	{
		return dtmd_removable_media_unknown_or_persistent;
	}
}

static int helper_blkid_read_data_from_partition(const char *partition_name, const char **fstype, const char **label)
{
	blkid_probe pr;

	const char *local_fstype = NULL;
	const char *local_label  = NULL;

	pr = blkid_new_probe_from_filename(partition_name);
	if (pr == NULL)
	{
		*fstype = NULL;
		*label  = NULL;
		return 0;
	}

	blkid_probe_enable_superblocks(pr, 1);
	blkid_do_fullprobe(pr);

	blkid_probe_lookup_value(pr, "TYPE", &local_fstype, NULL);
	blkid_probe_lookup_value(pr, "LABEL", &local_label, NULL);

	if (local_fstype != NULL)
	{
		local_fstype = strdup(local_fstype);
		if (local_fstype == NULL)
		{
			goto helper_blkid_read_data_from_partition_error_1;
		}
	}

	if (local_label != NULL)
	{
		local_label = strdup(local_label);
		if (local_label == NULL)
		{
			goto helper_blkid_read_data_from_partition_error_2;
		}
	}

	blkid_free_probe(pr);

	*fstype = local_fstype;
	*label  = local_label;

	return 1;

helper_blkid_read_data_from_partition_error_2:
	if (local_fstype != NULL)
	{
		free((char*) local_fstype);
	}

helper_blkid_read_data_from_partition_error_1:
	blkid_free_probe(pr);

	*fstype = NULL;
	*label  = NULL;

	return -1;
}

static void device_system_free_device(dtmd_info_t *device)
{
	if (device->path != NULL)
	{
		free((char*) device->path);
	}

	if (device->fstype != NULL)
	{
		free((char*) device->fstype);
	}

	if (device->label != NULL)
	{
		free((char*) device->label);
	}

	if (device->path_parent != NULL)
	{
		free((char*) device->path_parent);
	}

	free(device);
}

static int open_netlink_socket(void)
{
	struct sockaddr_nl local;
	int fd;
	pid_t pid;

	pid = (pthread_self() << 16) | getpid();

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd == -1)
	{
		return -1;
	}

	int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) != 0)
	{
		close(fd);
		return -1;
	}

	memset(&local, 0, sizeof(local));	/* fill-in local address information */
	local.nl_family = AF_NETLINK;
	local.nl_pid    = pid;
	local.nl_groups = NETLINK_KOBJECT_UEVENT;

	if (bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0)
	{
		close(fd);
		return -1;
	}

	return fd;
}

static int helper_read_partition(const char *name, const char *device_name, const char *device_name_parent, dtmd_info_t **device)
{
	char *device_type;
	struct stat stat_entry;
	char *start_string;

	dtmd_info_t *device_info;
	dtmd_removable_media_type_t media_type;

	start_string = (char *) name + strlen(name);

	strcpy(start_string, device_name);
	strcat(start_string, "/");
	strcat(start_string, filename_dev);

	if (stat(name, &stat_entry) != 0)
	{
		goto helper_read_partition_exit_1;
	}

	strcpy(start_string, filename_device_type);
	device_type = read_string_from_file(name);
	if (device_type == NULL)
	{
		goto helper_read_partition_exit_1;
	}

	media_type = device_type_from_string(device_type);
	free(device_type);

	device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
	if (device_info == NULL)
	{
		goto helper_read_partition_error_1;
	}

	device_info->path = (char*) malloc(strlen(devices_dir) + strlen(device_name) + 2);
	if (device_info->path == NULL)
	{
		goto helper_read_partition_error_2;
	}

	device_info->path_parent = (char*) malloc(strlen(devices_dir) + strlen(device_name_parent) + 2);
	if (device_info->path_parent == NULL)
	{
		goto helper_read_partition_error_3;
	}

	device_info->type = dtmd_info_partition;

	strcpy((char*) device_info->path, devices_dir);
	strcat((char*) device_info->path, "/");
	strcat((char*) device_info->path, device_name);

	strcpy((char*) device_info->path_parent, devices_dir);
	strcat((char*) device_info->path_parent, "/");
	strcat((char*) device_info->path_parent, device_name_parent);

	device_info->media_type   = media_type;
	device_info->state        = dtmd_removable_media_state_unknown;
	device_info->private_data = NULL;

	if ((helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label)) != 1)
	    || (device_info->fstype == NULL))
	{
		if (device_info->fstype != NULL)
		{
			free((char*) device_info->fstype);
		}

		if (device_info->label != NULL)
		{
			free((char*) device_info->label);
		}

		goto helper_read_partition_error_4;
	}

	*device = device_info;
	return 1;

helper_read_partition_exit_1:
	return 0;

helper_read_partition_error_4:
	free((char*) device_info->path_parent);

helper_read_partition_error_3:
	free((char*) device_info->path);

helper_read_partition_error_2:
	free(device_info);

helper_read_partition_error_1:
	return -1;
}

static int helper_read_device(const char *name, const char *device_name, int check_removable, dtmd_info_t **device)
{
	char *device_type;
	struct stat stat_entry;
	char *start_string;

	dtmd_info_t *device_info;
	dtmd_removable_media_type_t media_type;

	start_string = (char *) name + strlen(name);

	strcpy(start_string, filename_dev);

	if (stat(name, &stat_entry) != 0)
	{
		goto helper_read_device_exit_1;
	}

	if (check_removable)
	{
		strcpy(start_string, filename_removable);

		if (read_int_from_file(name) != removable_correct_value)
		{
			goto helper_read_device_exit_1;
		}
	}

	strcpy(start_string, filename_device_type);
	device_type = read_string_from_file(name);
	if (device_type == NULL)
	{
		goto helper_read_device_exit_1;
	}

	media_type = device_type_from_string(device_type);
	free(device_type);

	if (media_type != dtmd_removable_media_unknown_or_persistent)
	{
		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			goto helper_read_device_error_1;
		}

		device_info->path = (char*) malloc(strlen(devices_dir) + strlen(device_name) + 2);
		if (device_info->path == NULL)
		{
			goto helper_read_device_error_2;
		}

		strcpy((char*) device_info->path, devices_dir);
		strcat((char*) device_info->path, "/");
		strcat((char*) device_info->path, device_name);

		device_info->media_type   = media_type;
		device_info->path_parent  = NULL;
		device_info->private_data = NULL;

		switch (media_type)
		{
		case dtmd_removable_media_removable_disk:
		case dtmd_removable_media_sd_card:
			device_info->type   = dtmd_info_device;
			device_info->fstype = NULL;
			device_info->label  = NULL;
			device_info->state  = dtmd_removable_media_state_unknown;
			*device             = device_info;
			return 1;

		case dtmd_removable_media_cdrom:
			device_info->type = dtmd_info_stateful_device;

			switch (helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label)))
			{
			case 0:
				device_info->state = dtmd_removable_media_state_empty;
				break;

			case 1:
				if (device_info->fstype == NULL)
				{
					device_info->state = dtmd_removable_media_state_clear;
				}
				else
				{
					device_info->state = dtmd_removable_media_state_ok;
				}
				break;

			case -1:
				goto helper_read_device_error_3;
			}

			*device = device_info;
			return 2;
		}
	}

helper_read_device_exit_1:
	return 0;

helper_read_device_error_3:
	free((char*) device_info->path);

helper_read_device_error_2:
	free(device_info);

helper_read_device_error_1:
	return -1;
}

dtmd_device_system_t* device_system_init(void)
{
	dtmd_device_system_t *device_system;

	device_system = (dtmd_device_system_t*) malloc(sizeof(dtmd_device_system_t));
	if (device_system == NULL)
	{
		goto device_system_init_error_1;
	}

	device_system->enumeration_count = 0;
	device_system->enumerations      = NULL;
	device_system->monitor_count     = 0;
	device_system->monitors          = NULL;

	return device_system;

device_system_init_error_1:
	return NULL;
}

static void helper_free_enumeration(dtmd_device_enumeration_t *enumeration)
{
	if (enumeration->dir_pointer != NULL)
	{
		closedir(enumeration->dir_pointer);
	}

	if (enumeration->dir_pointer_partitions != NULL)
	{
		closedir(enumeration->dir_pointer_partitions);
	}

	if (enumeration->dir_pointer_usb != NULL)
	{
		closedir(enumeration->dir_pointer_usb);
	}

	if (enumeration->dir_pointer_usb_device != NULL)
	{
		closedir(enumeration->dir_pointer_usb_device);
	}

	if (enumeration->dir_pointer_usb_host != NULL)
	{
		closedir(enumeration->dir_pointer_usb_host);
	}

	if (enumeration->dir_pointer_usb_target != NULL)
	{
		closedir(enumeration->dir_pointer_usb_target);
	}

	if (enumeration->dir_pointer_usb_target_device != NULL)
	{
		closedir(enumeration->dir_pointer_usb_target_device);
	}

	if (enumeration->dir_pointer_usb_target_device_blocks != NULL)
	{
		closedir(enumeration->dir_pointer_usb_target_device_blocks);
	}

	if (enumeration->dir_pointer_mmc != NULL)
	{
		closedir(enumeration->dir_pointer_mmc);
	}

	if (enumeration->dir_pointer_mmc_device != NULL)
	{
		closedir(enumeration->dir_pointer_mmc_device);
	}

	if (enumeration->dir_pointer_mmc_device_blocks != NULL)
	{
		closedir(enumeration->dir_pointer_mmc_device_blocks);
	}

	free(enumeration);
}

static void helper_free_monitor(dtmd_device_monitor_t *monitor)
{
	if (monitor->fd >= 0)
	{
		close(monitor->fd);
	}

	free(monitor);
}

void device_system_deinit(dtmd_device_system_t *system)
{
	uint32_t i;

	if (system != NULL)
	{
		if (system->enumerations != NULL)
		{
			for (i = 0; i < (uint32_t) system->enumeration_count; ++i)
			{
				if (system->enumerations[i] != NULL)
				{
					helper_free_enumeration(system->enumerations[i]);
				}
			}

			free(system->enumerations);
		}

		if (system->monitors != NULL)
		{
			for (i = 0; i < (uint32_t) system->monitor_count; ++i)
			{
				if (system->monitors[i] != NULL)
				{
					helper_free_monitor(system->monitors[i]);
				}
			}

			free(system->monitors);
		}

		free(system);
	}
}

dtmd_device_enumeration_t* device_system_enumerate_devices(dtmd_device_system_t *system)
{
	DIR *dir_pointer;
	DIR *dir_pointer_usb;
	DIR *dir_pointer_mmc;
	dtmd_device_enumeration_t *enumeration;
	void **tmp;

	if (system == NULL)
	{
		goto device_system_enumerate_devices_error_1;
	}

	dir_pointer = opendir(block_devices_dir);
	if (dir_pointer == NULL)
	{
		goto device_system_enumerate_devices_error_1;
	}

	dir_pointer_mmc = opendir(block_mmc_devices_dir);
	if (dir_pointer_mmc == NULL)
	{
		goto device_system_enumerate_devices_error_2;
	}

	dir_pointer_usb = opendir(block_usb_devices_dir);
	if (dir_pointer_usb == NULL)
	{
		goto device_system_enumerate_devices_error_3;
	}

	enumeration = (dtmd_device_enumeration_t*) malloc(sizeof(dtmd_device_enumeration_t));
	if (enumeration == NULL)
	{
		goto device_system_enumerate_devices_error_4;
	}

	tmp = realloc(system->enumerations, (system->enumeration_count + 1) * sizeof(dtmd_device_enumeration_t*));
	if (tmp == NULL)
	{
		goto device_system_enumerate_devices_error_5;
	}

	system->enumerations = (dtmd_device_enumeration_t**) tmp;
	system->enumerations[system->enumeration_count] = enumeration;
	++(system->enumeration_count);

	enumeration->system                               = system;

	enumeration->dir_pointer                          = dir_pointer;
	enumeration->dir_pointer_partitions               = NULL;

	enumeration->dir_pointer_usb                      = dir_pointer_usb;
	enumeration->dir_pointer_usb_device               = NULL;
	enumeration->dir_pointer_usb_host                 = NULL;
	enumeration->dir_pointer_usb_target               = NULL;
	enumeration->dir_pointer_usb_target_device        = NULL;
	enumeration->dir_pointer_usb_target_device_blocks = NULL;

	enumeration->dir_pointer_mmc                      = dir_pointer_mmc;
	enumeration->dir_pointer_mmc_device               = NULL;
	enumeration->dir_pointer_mmc_device_blocks        = NULL;

	return enumeration;

device_system_enumerate_devices_error_5:
	free(enumeration);

device_system_enumerate_devices_error_4:
	closedir(dir_pointer_usb);

device_system_enumerate_devices_error_3:
	closedir(dir_pointer_mmc);

device_system_enumerate_devices_error_2:
	closedir(dir_pointer);

device_system_enumerate_devices_error_1:
	return NULL;
}

void device_system_finish_enumerate_devices(dtmd_device_enumeration_t *enumeration)
{
	void **tmp;
	uint32_t i;

	if (enumeration != NULL)
	{
		if (enumeration->system != NULL)
		{
			for (i = 0; i < (uint32_t) enumeration->system->enumeration_count; ++i)
			{
				if (enumeration->system->enumerations[i] == enumeration)
				{
					if (i != (uint32_t)(enumeration->system->enumeration_count - 1))
					{
						enumeration->system->enumerations[i] = enumeration->system->enumerations[enumeration->system->enumeration_count - 1];
					}

					if (enumeration->system->enumeration_count > 1)
					{
						tmp = realloc(enumeration->system->enumerations, (enumeration->system->enumeration_count - 1) * sizeof(dtmd_device_enumeration_t*));
						if (tmp != NULL)
						{
							enumeration->system->enumerations = (dtmd_device_enumeration_t**) tmp;
						}
					}
					else
					{
						free(enumeration->system->enumerations);
						enumeration->system->enumerations = NULL;
					}

					--(enumeration->system->enumeration_count);
					break;
				}
			}
		}

		helper_free_enumeration(enumeration);
	}
}

int device_system_next_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t **device)
{
	char file_name[PATH_MAX + 1];
	size_t len_core;
	size_t len_base;
	size_t len_ext;
	size_t len_dev_base;
	struct stat statbuf;

	if ((enumeration == NULL)
		|| (device == NULL))
	{
		goto device_system_next_enumerated_device_error_1;
	}

	len_ext       = strlen(filename_dev);

	len_base      = strlen(filename_removable);
	if (len_base > len_ext)
	{
		len_ext = len_base;
	}

	len_base      = strlen(filename_device_type);
	if (len_base > len_ext)
	{
		len_ext = len_base;
	}

	if (enumeration->dir_pointer != NULL)
	{
		len_base = strlen(block_devices_dir);

		if (enumeration->dir_pointer_partitions != NULL)
		{
			len_dev_base = strlen(enumeration->dirent_device->d_name) + 1;

			while ((enumeration->dirent_device_partition = readdir(enumeration->dir_pointer_partitions)) != NULL)
			{
				if ((strcmp(enumeration->dirent_device_partition->d_name, ".") == 0)
				    || (strcmp(enumeration->dirent_device_partition->d_name, "..") == 0))
				{
					continue;
				}

				len_core = strlen(enumeration->dirent_device_partition->d_name) + 2;

				if (len_core + len_base + len_dev_base + len_ext > PATH_MAX)
				{
					goto device_system_next_enumerated_device_error_1;
				}

				strcpy(file_name, block_devices_dir);
				strcat(file_name, "/");
				strcat(file_name, enumeration->dirent_device->d_name);
				strcat(file_name, "/");

				switch (helper_read_partition(file_name,
					enumeration->dirent_device_partition->d_name,
					enumeration->dirent_device->d_name,
					device))
				{
				case 1:
					return 1;
				/*
				case 0:
					break;
				*/
				case -1:
					goto device_system_next_enumerated_device_error_1;
					//break;
				}
			}

			closedir(enumeration->dir_pointer_partitions);
			enumeration->dir_pointer_partitions = NULL;
		}

		while ((enumeration->dirent_device = readdir(enumeration->dir_pointer)) != NULL)
		{
			if ((strcmp(enumeration->dirent_device->d_name, ".") == 0)
			    || (strcmp(enumeration->dirent_device->d_name, "..") == 0))
			{
				continue;
			}

			len_core = strlen(enumeration->dirent_device->d_name) + 2;

			if (len_core + len_base + len_ext > PATH_MAX)
			{
				goto device_system_next_enumerated_device_error_1;
			}

			strcpy(file_name, block_devices_dir);
			strcat(file_name, "/");
			strcat(file_name, enumeration->dirent_device->d_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			strcat(file_name, "/");

			switch (helper_read_device(file_name, enumeration->dirent_device->d_name, 1, device))
			{
			case 1: // device
				file_name[len_base + len_core - 1] = 0;
				enumeration->dir_pointer_partitions = opendir(file_name);
				if (enumeration->dir_pointer_partitions == NULL)
				{
					device_system_free_device(*device);
					*device = NULL;
					goto device_system_next_enumerated_device_error_1;
				}
				return 1;

			case 2: // stateful_device
				return 1;
			/*
			case 0:
				break;
			*/
			case -1:
				goto device_system_next_enumerated_device_error_1;
				//break;
			}
		}

		closedir(enumeration->dir_pointer);
		enumeration->dir_pointer = NULL;
	}

	if (enumeration->dir_pointer_usb != NULL)
	{
		len_base = strlen(block_usb_devices_dir);

		if (enumeration->dir_pointer_usb_device != NULL)
		{
			if (enumeration->dir_pointer_usb_host != NULL)
			{
				if (enumeration->dir_pointer_usb_target != NULL)
				{
					if (enumeration->dir_pointer_usb_target_device != NULL)
					{
						if (enumeration->dir_pointer_usb_target_device_blocks != NULL)
						{
							len_dev_base = strlen(enumeration->dirent_usb_target_device->d_name) + strlen(block_dir_name)
								+ strlen(enumeration->dirent_usb_target->d_name) + strlen(enumeration->dirent_usb_host->d_name)
								+ strlen(enumeration->dirent_usb_device->d_name) + strlen(enumeration->dirent_usb->d_name) + 7;

							while ((enumeration->dirent_usb_target_device_blocks = readdir(enumeration->dir_pointer_usb_target_device_blocks)) != NULL)
							{
								if ((strcmp(enumeration->dirent_usb_target_device_blocks->d_name, ".") == 0)
									|| (strcmp(enumeration->dirent_usb_target_device_blocks->d_name, "..") == 0))
								{
									continue;
								}

								len_core = strlen(enumeration->dirent_usb_target_device_blocks->d_name) + 2;

								if (len_core + len_base + len_dev_base + len_ext > PATH_MAX)
								{
									goto device_system_next_enumerated_device_error_1;
								}

								strcpy(file_name, block_usb_devices_dir);
								strcat(file_name, "/");
								strcat(file_name, enumeration->dirent_usb->d_name);
								strcat(file_name, "/");
								strcat(file_name, enumeration->dirent_usb_device->d_name);
								strcat(file_name, "/");
								strcat(file_name, enumeration->dirent_usb_host->d_name);
								strcat(file_name, "/");
								strcat(file_name, enumeration->dirent_usb_target->d_name);
								strcat(file_name, "/");
								strcat(file_name, block_dir_name);
								strcat(file_name, "/");
								strcat(file_name, enumeration->dirent_usb_target_device->d_name);
								strcat(file_name, "/");

								switch (helper_read_partition(file_name,
									enumeration->dirent_usb_target_device_blocks->d_name,
									enumeration->dirent_usb_target_device->d_name,
									device))
								{
								case 1:
									return 1;
								/*
								case 0:
									break;
								*/
								case -1:
									goto device_system_next_enumerated_device_error_1;
									//break;
								}
							}

							closedir(enumeration->dir_pointer_usb_target_device_blocks);
							enumeration->dir_pointer_usb_target_device_blocks = NULL;
						}

device_system_next_enumerated_device_usb_step_5:

						while ((enumeration->dirent_usb_target_device = readdir(enumeration->dir_pointer_usb_target_device)) != NULL)
						{
							if ((strcmp(enumeration->dirent_usb_target_device->d_name, ".") == 0)
								|| (strcmp(enumeration->dirent_usb_target_device->d_name, "..") == 0))
							{
								continue;
							}

							len_core = strlen(enumeration->dirent_usb_target_device->d_name) + strlen(block_dir_name)
								+ strlen(enumeration->dirent_usb_target->d_name) + strlen(enumeration->dirent_usb_host->d_name)
								+ strlen(enumeration->dirent_usb_device->d_name) + strlen(enumeration->dirent_usb->d_name) + 8;

							if (len_core + len_base + len_ext > PATH_MAX)
							{
								goto device_system_next_enumerated_device_error_1;
							}

							strcpy(file_name, block_usb_devices_dir);
							strcat(file_name, "/");
							strcat(file_name, enumeration->dirent_usb->d_name);
							strcat(file_name, "/");
							strcat(file_name, enumeration->dirent_usb_device->d_name);
							strcat(file_name, "/");
							strcat(file_name, enumeration->dirent_usb_host->d_name);
							strcat(file_name, "/");
							strcat(file_name, enumeration->dirent_usb_target->d_name);
							strcat(file_name, "/");
							strcat(file_name, block_dir_name);
							strcat(file_name, "/");
							strcat(file_name, enumeration->dirent_usb_target_device->d_name);

							if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
							{
								continue;
							}

							strcat(file_name, "/");

							switch (helper_read_device(file_name, enumeration->dirent_usb_target_device->d_name, 0, device))
							{
							case 1: // device
								file_name[len_base + len_core - 1] = 0;
								enumeration->dir_pointer_usb_target_device_blocks = opendir(file_name);
								if (enumeration->dir_pointer_usb_target_device_blocks == NULL)
								{
									device_system_free_device(*device);
									*device = NULL;
									goto device_system_next_enumerated_device_error_1;
								}
								return 1;

							case 2: // stateful_device
								return 1;
							/*
							case 0:
								break;
							*/
							case -1:
								goto device_system_next_enumerated_device_error_1;
								//break;
							}
						}

						closedir(enumeration->dir_pointer_usb_target_device);
						enumeration->dir_pointer_usb_target_device = NULL;
					}

device_system_next_enumerated_device_usb_step_4:

					while ((enumeration->dirent_usb_target = readdir(enumeration->dir_pointer_usb_target)) != NULL)
					{
						if ((strcmp(enumeration->dirent_usb_target->d_name, ".") == 0)
							|| (strcmp(enumeration->dirent_usb_target->d_name, "..") == 0))
						{
							continue;
						}

						len_core = strlen(block_dir_name) + strlen(enumeration->dirent_usb_target->d_name) + strlen(enumeration->dirent_usb_host->d_name)
							+ strlen(enumeration->dirent_usb_device->d_name) + strlen(enumeration->dirent_usb->d_name) + 6;

						if (len_core + len_base + len_ext > PATH_MAX)
						{
							goto device_system_next_enumerated_device_error_1;
						}

						strcpy(file_name, block_usb_devices_dir);
						strcat(file_name, "/");
						strcat(file_name, enumeration->dirent_usb->d_name);
						strcat(file_name, "/");
						strcat(file_name, enumeration->dirent_usb_device->d_name);
						strcat(file_name, "/");
						strcat(file_name, enumeration->dirent_usb_host->d_name);
						strcat(file_name, "/");
						strcat(file_name, enumeration->dirent_usb_target->d_name);
						strcat(file_name, "/");
						strcat(file_name, block_dir_name);

						if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
						{
							continue;
						}

						enumeration->dir_pointer_usb_target_device = opendir(file_name);
						if (enumeration->dir_pointer_usb_target_device == NULL)
						{
							goto device_system_next_enumerated_device_error_1;
						}

						goto device_system_next_enumerated_device_usb_step_5;
					}

					closedir(enumeration->dir_pointer_usb_target);
					enumeration->dir_pointer_usb_target = NULL;
				}

device_system_next_enumerated_device_usb_step_3:

				while ((enumeration->dirent_usb_host = readdir(enumeration->dir_pointer_usb_host)) != NULL)
				{
					if ((strcmp(enumeration->dirent_usb_host->d_name, ".") == 0)
						|| (strcmp(enumeration->dirent_usb_host->d_name, "..") == 0))
					{
						continue;
					}

					if (strncmp(enumeration->dirent_usb_host->d_name, "target", strlen("target")) != 0)
					{
						continue;
					}

					len_core = strlen(enumeration->dirent_usb_host->d_name) + strlen(enumeration->dirent_usb_device->d_name) + strlen(enumeration->dirent_usb->d_name) + 4;

					if (len_core + len_base + len_ext > PATH_MAX)
					{
						goto device_system_next_enumerated_device_error_1;
					}

					strcpy(file_name, block_usb_devices_dir);
					strcat(file_name, "/");
					strcat(file_name, enumeration->dirent_usb->d_name);
					strcat(file_name, "/");
					strcat(file_name, enumeration->dirent_usb_device->d_name);
					strcat(file_name, "/");
					strcat(file_name, enumeration->dirent_usb_host->d_name);

					if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
					{
						continue;
					}

					enumeration->dir_pointer_usb_target = opendir(file_name);
					if (enumeration->dir_pointer_usb_target == NULL)
					{
						goto device_system_next_enumerated_device_error_1;
					}

					goto device_system_next_enumerated_device_usb_step_4;
				}

				closedir(enumeration->dir_pointer_usb_host);
				enumeration->dir_pointer_usb_host = NULL;
			}

device_system_next_enumerated_device_usb_step_2:

			while ((enumeration->dirent_usb_device = readdir(enumeration->dir_pointer_usb_device)) != NULL)
			{
				if ((strcmp(enumeration->dirent_usb_device->d_name, ".") == 0)
					|| (strcmp(enumeration->dirent_usb_device->d_name, "..") == 0))
				{
					continue;
				}

				if (strncmp(enumeration->dirent_usb_device->d_name, "host", strlen("host")) != 0)
				{
					continue;
				}

				len_core = strlen(enumeration->dirent_usb_device->d_name) + strlen(enumeration->dirent_usb->d_name) + 3;

				if (len_core + len_base + len_ext > PATH_MAX)
				{
					goto device_system_next_enumerated_device_error_1;
				}

				strcpy(file_name, block_usb_devices_dir);
				strcat(file_name, "/");
				strcat(file_name, enumeration->dirent_usb->d_name);
				strcat(file_name, "/");
				strcat(file_name, enumeration->dirent_usb_device->d_name);

				if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
				{
					continue;
				}

				enumeration->dir_pointer_usb_host = opendir(file_name);
				if (enumeration->dir_pointer_usb_host == NULL)
				{
					goto device_system_next_enumerated_device_error_1;
				}

				goto device_system_next_enumerated_device_usb_step_3;
			}

			closedir(enumeration->dir_pointer_usb_device);
			enumeration->dir_pointer_usb_device = NULL;
		}

		while ((enumeration->dirent_usb = readdir(enumeration->dir_pointer_usb)) != NULL)
		{
			if ((strcmp(enumeration->dirent_usb->d_name, ".") == 0)
				|| (strcmp(enumeration->dirent_usb->d_name, "..") == 0))
			{
				continue;
			}

			len_core = strlen(enumeration->dirent_usb->d_name) + 2;

			if (len_core + len_base + len_ext > PATH_MAX)
			{
				goto device_system_next_enumerated_device_error_1;
			}

			strcpy(file_name, block_usb_devices_dir);
			strcat(file_name, "/");
			strcat(file_name, enumeration->dirent_usb->d_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			enumeration->dir_pointer_usb_device = opendir(file_name);
			if (enumeration->dir_pointer_usb_device == NULL)
			{
				goto device_system_next_enumerated_device_error_1;
			}

			goto device_system_next_enumerated_device_usb_step_2;
		}

		closedir(enumeration->dir_pointer_usb);
		enumeration->dir_pointer_usb = NULL;
	}

	if (enumeration->dir_pointer_mmc != NULL)
	{
		len_base = strlen(block_mmc_devices_dir);

		if (enumeration->dir_pointer_mmc_device != NULL)
		{
			if (enumeration->dir_pointer_mmc_device_blocks != NULL)
			{
				len_dev_base = strlen(enumeration->dirent_mmc_device->d_name)
					+ strlen(block_dir_name) + strlen(enumeration->dirent_mmc->d_name) + 4;

				while ((enumeration->dirent_mmc_device_blocks = readdir(enumeration->dir_pointer_mmc_device_blocks)) != NULL)
				{
					if ((strcmp(enumeration->dirent_mmc_device_blocks->d_name, ".") == 0)
						|| (strcmp(enumeration->dirent_mmc_device_blocks->d_name, "..") == 0))
					{
						continue;
					}

					len_core = strlen(enumeration->dirent_mmc_device_blocks->d_name) + 2;

					if (len_core + len_base + len_dev_base + len_ext > PATH_MAX)
					{
						goto device_system_next_enumerated_device_error_1;
					}

					strcpy(file_name, block_mmc_devices_dir);
					strcat(file_name, "/");
					strcat(file_name, enumeration->dirent_mmc->d_name);
					strcat(file_name, "/");
					strcat(file_name, block_dir_name);
					strcat(file_name, "/");
					strcat(file_name, enumeration->dirent_mmc_device->d_name);
					strcat(file_name, "/");

					switch (helper_read_partition(file_name,
						enumeration->dirent_mmc_device_blocks->d_name,
						enumeration->dirent_mmc_device->d_name,
						device))
					{
					case 1:
						return 1;
					/*
					case 0:
						break;
					*/
					case -1:
						goto device_system_next_enumerated_device_error_1;
						//break;
					}
				}

				closedir(enumeration->dir_pointer_mmc_device_blocks);
				enumeration->dir_pointer_mmc_device_blocks = NULL;
			}

device_system_next_enumerated_device_mmc_step_2:
			while ((enumeration->dirent_mmc_device = readdir(enumeration->dir_pointer_mmc_device)) != NULL)
			{
				if ((strcmp(enumeration->dirent_mmc_device->d_name, ".") == 0)
					|| (strcmp(enumeration->dirent_mmc_device->d_name, "..") == 0))
				{
					continue;
				}

				len_core = strlen(enumeration->dirent_mmc_device->d_name)
					+ strlen(block_dir_name) + strlen(enumeration->dirent_mmc->d_name) + 5;

				if (len_core + len_base + len_ext > PATH_MAX)
				{
					goto device_system_next_enumerated_device_error_1;
				}

				strcpy(file_name, block_mmc_devices_dir);
				strcat(file_name, "/");
				strcat(file_name, enumeration->dirent_mmc->d_name);
				strcat(file_name, "/");
				strcat(file_name, block_dir_name);
				strcat(file_name, "/");
				strcat(file_name, enumeration->dirent_mmc_device->d_name);

				if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
				{
					continue;
				}

				strcat(file_name, "/");

				switch (helper_read_device(file_name, enumeration->dirent_mmc_device->d_name, 0, device))
				{
				case 1: // device
					file_name[len_base + len_core - 1] = 0;
					enumeration->dir_pointer_mmc_device_blocks = opendir(file_name);
					if (enumeration->dir_pointer_mmc_device_blocks == NULL)
					{
						device_system_free_device(*device);
						*device = NULL;
						goto device_system_next_enumerated_device_error_1;
					}
					return 1;

				case 2: // stateful_device
					return 1;
				/*
				case 0:
					break;
				*/
				case -1:
					goto device_system_next_enumerated_device_error_1;
					//break;
				}
			}

			closedir(enumeration->dir_pointer_mmc_device);
			enumeration->dir_pointer_mmc_device = NULL;
		}

		while ((enumeration->dirent_mmc = readdir(enumeration->dir_pointer_mmc)) != NULL)
		{
			if ((strcmp(enumeration->dirent_mmc->d_name, ".") == 0)
				|| (strcmp(enumeration->dirent_mmc->d_name, "..") == 0))
			{
				continue;
			}

			len_core = strlen(block_dir_name) + strlen(enumeration->dirent_mmc->d_name) + 3;

			if (len_core + len_base + len_ext > PATH_MAX)
			{
				goto device_system_next_enumerated_device_error_1;
			}

			strcpy(file_name, block_mmc_devices_dir);
			strcat(file_name, "/");
			strcat(file_name, enumeration->dirent_mmc->d_name);
			strcat(file_name, "/");
			strcat(file_name, block_dir_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			enumeration->dir_pointer_mmc_device = opendir(file_name);
			if (enumeration->dir_pointer_mmc_device == NULL)
			{
				goto device_system_next_enumerated_device_error_1;
			}

			goto device_system_next_enumerated_device_mmc_step_2;
		}

		closedir(enumeration->dir_pointer_mmc);
		enumeration->dir_pointer_mmc = NULL;
	}

	*device = NULL;
	return 0;

device_system_next_enumerated_device_error_1:
	return -1;
}

void device_system_free_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device)
{
	if ((enumeration != NULL)
		&& (device != NULL))
	{
		device_system_free_device(device);
	}
}

dtmd_device_monitor_t* device_system_start_monitoring(dtmd_device_system_t *system)
{
	int fd;
	dtmd_device_monitor_t *monitor;
	void **tmp;

	if (system == NULL)
	{
		goto device_system_start_monitoring_error_1;
	}

	fd = open_netlink_socket();
	if (fd < 0)
	{
		goto device_system_start_monitoring_error_1;
	}

	monitor = (dtmd_device_monitor_t*) malloc(sizeof(dtmd_device_monitor_t));
	if (monitor == NULL)
	{
		goto device_system_start_monitoring_error_2;
	}

	tmp = realloc(system->monitors, (system->monitor_count + 1) * sizeof(dtmd_device_monitor_t*));
	if (tmp == NULL)
	{
		goto device_system_start_monitoring_error_3;
	}

	system->monitors = (dtmd_device_monitor_t**) tmp;
	system->monitors[system->monitor_count] = monitor;
	++(system->monitor_count);

	monitor->system = system;
	monitor->fd     = fd;

	return monitor;

device_system_start_monitoring_error_3:
	free(monitor);

device_system_start_monitoring_error_2:
	close(fd);

device_system_start_monitoring_error_1:
	return NULL;
}

void device_system_stop_monitoring(dtmd_device_monitor_t *monitor)
{
	void **tmp;
	uint32_t i;

	if (monitor != NULL)
	{
		if (monitor->system != NULL)
		{
			for (i = 0; i < (uint32_t) monitor->system->monitor_count; ++i)
			{
				if (monitor->system->monitors[i] == monitor)
				{
					if (i != (uint32_t)(monitor->system->monitor_count - 1))
					{
						monitor->system->monitors[i] = monitor->system->monitors[monitor->system->monitor_count - 1];
					}

					if (monitor->system->monitor_count > 1)
					{
						tmp = realloc(monitor->system->monitors, (monitor->system->monitor_count - 1) * sizeof(dtmd_device_monitor_t*));
						if (tmp != NULL)
						{
							monitor->system->monitors = (dtmd_device_monitor_t**) tmp;
						}
					}
					else
					{
						free(monitor->system->monitors);
						monitor->system->monitors = NULL;
					}

					--(monitor->system->monitor_count);
					break;
				}
			}
		}

		helper_free_monitor(monitor);
	}
}

int device_system_get_monitor_fd(dtmd_device_monitor_t *monitor)
{
	if (monitor != NULL)
	{
		return monitor->fd;
	}
	else
	{
		return -1;
	}
}

int device_system_monitor_get_device(dtmd_device_monitor_t *monitor, dtmd_info_t **device, dtmd_device_action_type_t *action)
{
	struct sockaddr_nl kernel;
	struct iovec io;
	char cred_msg[CMSG_SPACE(sizeof(struct ucred))];

	const int IFLIST_REPLY_BUFFER = 8192;
	char reply[IFLIST_REPLY_BUFFER];

	ssize_t len;

	struct msghdr rtnl_reply;
	struct iovec io_reply;

	ssize_t pos;
	struct ucred *cred;
	struct cmsghdr *cmsg;

	dtmd_device_action_type_t action_type = dtmd_device_action_unknown;
	char *devtype = NULL;
	char *devname = NULL;
	char *subsystem = NULL;
	char *devpath = NULL;
	dtmd_info_t *device_info;
	char file_name[PATH_MAX + 1];
	char *device_type;

	char *last_delim;

	if ((monitor == NULL)
		|| (device == NULL)
		|| (action == NULL))
	{
		goto device_system_monitor_get_device_error_1;
	}

	memset(&kernel, 0, sizeof(kernel));
	kernel.nl_family = AF_NETLINK;

	memset(&io_reply, 0, sizeof(io_reply));
	memset(&rtnl_reply, 0, sizeof(rtnl_reply));

	io.iov_base = reply;
	io.iov_len = IFLIST_REPLY_BUFFER;
	rtnl_reply.msg_iov = &io;
	rtnl_reply.msg_iovlen = 1;
	rtnl_reply.msg_name = &kernel;
	rtnl_reply.msg_namelen = sizeof(kernel);
	rtnl_reply.msg_control = &cred_msg;
	rtnl_reply.msg_controllen = sizeof(cred_msg);

	len = recvmsg(monitor->fd, &rtnl_reply, 0);
	if (len > 0)
	{
		if ((kernel.nl_family != AF_NETLINK)
			|| (kernel.nl_pid != 0)
			|| (kernel.nl_groups != NETLINK_GROUP_KERNEL))
		{
			goto device_system_monitor_get_device_exit_1;
		}

		cmsg = CMSG_FIRSTHDR(&rtnl_reply);
		if ((cmsg == NULL)|| (cmsg->cmsg_type != SCM_CREDENTIALS))
		{
			goto device_system_monitor_get_device_exit_1;
		}

		cred = (struct ucred*) CMSG_DATA(cmsg);

		if ((cred->pid != 0)
			|| (cred->uid != 0)
			|| (cred->gid != 0))
		{
			goto device_system_monitor_get_device_exit_1;
		}

		pos = 0;

		while (pos < len)
		{
			if (strncmp(&(reply[pos]), NETLINK_STRING_ACTION, strlen(NETLINK_STRING_ACTION)) == 0)
			{
				if (strcmp(&(reply[pos + strlen(NETLINK_STRING_ACTION)]), NETLINK_STRING_ACTION_ADD) == 0)
				{
					action_type = dtmd_device_action_add;
				}
				else if (strcmp(&(reply[pos + strlen(NETLINK_STRING_ACTION)]), NETLINK_STRING_ACTION_ONLINE) == 0)
				{
					action_type = dtmd_device_action_online;
				}
				else if (strcmp(&(reply[pos + strlen(NETLINK_STRING_ACTION)]), NETLINK_STRING_ACTION_REMOVE) == 0)
				{
					action_type = dtmd_device_action_remove;
				}
				else if (strcmp(&(reply[pos + strlen(NETLINK_STRING_ACTION)]), NETLINK_STRING_ACTION_OFFLINE) == 0)
				{
					action_type = dtmd_device_action_offline;
				}
				else if (strcmp(&(reply[pos + strlen(NETLINK_STRING_ACTION)]), NETLINK_STRING_ACTION_CHANGE) == 0)
				{
					action_type = dtmd_device_action_change;
				}
			}
			else if (strncmp(&(reply[pos]), NETLINK_STRING_SUBSYSTEM, strlen(NETLINK_STRING_SUBSYSTEM)) == 0)
			{
				subsystem = &(reply[pos + strlen(NETLINK_STRING_SUBSYSTEM)]);
			}
			else if (strncmp(&(reply[pos]), NETLINK_STRING_DEVNAME, strlen(NETLINK_STRING_DEVNAME)) == 0)
			{
				devname = &(reply[pos + strlen(NETLINK_STRING_DEVNAME)]);
			}
			else if (strncmp(&(reply[pos]), NETLINK_STRING_DEVTYPE, strlen(NETLINK_STRING_DEVTYPE)) == 0)
			{
				devtype = &(reply[pos + strlen(NETLINK_STRING_DEVTYPE)]);
			}
			else if (strncmp(&(reply[pos]), NETLINK_STRING_DEVPATH, strlen(NETLINK_STRING_DEVPATH)) == 0)
			{
				devpath = &(reply[pos + strlen(NETLINK_STRING_DEVPATH)]);
			}

			pos += strlen(&(reply[pos])) + 1;
		}

		if ((action_type == dtmd_device_action_unknown)
			|| (devtype == NULL)
			|| (devname == NULL)
			|| (subsystem == NULL)
			|| (devpath == NULL)
			|| (strcmp(subsystem, NETLINK_STRING_SUBSYSTEM_BLOCK) != 0)
			|| ((strcmp(devtype, NETLINK_STRING_DEVTYPE_DISK) != 0)
				&& (strcmp(devtype, NETLINK_STRING_DEVTYPE_PARTITION) != 0)))
		{
			goto device_system_monitor_get_device_exit_1;
		}

		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			goto device_system_monitor_get_device_error_1;
		}

		switch (action_type)
		{
		case dtmd_device_action_add:
		case dtmd_device_action_online:
		case dtmd_device_action_change:
			if (strlen(block_sys_dir) + strlen(devpath) + strlen(filename_device_type) + 4 > PATH_MAX)
			{
				goto device_system_monitor_get_device_error_2;
			}

			strcpy(file_name, block_sys_dir);
			strcat(file_name, devpath);
			strcat(file_name, "/");
			strcat(file_name, filename_device_type);
			device_type = read_string_from_file(file_name);

			if (device_type == NULL)
			{
				strcpy(&(file_name[strlen(block_sys_dir) + strlen(devpath)]), "/../");
				strcat(file_name, filename_device_type);
				device_type = read_string_from_file(file_name);
			}

			if (device_type == NULL)
			{
				goto device_system_monitor_get_device_error_2;
			}

			device_info->media_type = device_type_from_string(device_type);
			free(device_type);

			if (strcmp(devtype, NETLINK_STRING_DEVTYPE_DISK) == 0)
			{
				switch (device_info->media_type)
				{
				case dtmd_removable_media_removable_disk:
				case dtmd_removable_media_sd_card:
					device_info->type = dtmd_info_device;
					break;

				case dtmd_removable_media_cdrom:
					device_info->type = dtmd_info_stateful_device;
					break;

				default:
					device_info->type = dtmd_info_unknown;
					break;
				}
			}
			else
			{
				device_info->type = dtmd_info_partition;
			}
			break;

		case dtmd_device_action_remove:
		case dtmd_device_action_offline:
			if (strcmp(devtype, NETLINK_STRING_DEVTYPE_DISK) == 0)
			{
				// TODO: on remove differ between device and stateful_device
				device_info->type = dtmd_info_device;
			}
			else
			{
				device_info->type = dtmd_info_partition;
			}
			device_info->media_type = dtmd_removable_media_unknown_or_persistent;
			break;
		}

		device_info->path = (char*) malloc(strlen(devices_dir) + strlen(devname) + 2);
		if (device_info->path == NULL)
		{
			goto device_system_monitor_get_device_error_2;
		}

		strcpy((char*) device_info->path, devices_dir);
		strcat((char*) device_info->path, "/");
		strcat((char*) device_info->path, devname);

		switch (action_type)
		{
		case dtmd_device_action_add:
		case dtmd_device_action_online:
		case dtmd_device_action_change:
			switch (device_info->type)
			{
			case dtmd_info_partition:
				device_info->state = dtmd_removable_media_state_unknown;

				if ((helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label)) != 1)
				    || (device_info->fstype == NULL))
				{
					goto device_system_monitor_get_device_error_3;
				}
				break;

			case dtmd_info_stateful_device:
				switch (helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label)))
				{
				case 0:
					device_info->state = dtmd_removable_media_state_empty;
					break;

				case 1:
					if (device_info->fstype == NULL)
					{
						device_info->state = dtmd_removable_media_state_clear;
					}
					else
					{
						device_info->state = dtmd_removable_media_state_ok;
					}
					break;

				case -1:
					goto device_system_monitor_get_device_error_3;
				}
				break;

			default:
				device_info->fstype = NULL;
				device_info->label  = NULL;
				device_info->state  = dtmd_removable_media_state_unknown;
				break;
			}
			break;

		default:
			device_info->fstype = NULL;
			device_info->label  = NULL;
			device_info->state  = dtmd_removable_media_state_unknown;
			break;
		}

		switch (device_info->type)
		{
		case dtmd_info_partition:
			last_delim = strrchr(devpath, '/');
			if (last_delim == NULL)
			{
				goto device_system_monitor_get_device_error_3;
			}

			*last_delim = 0;
			last_delim = strrchr(devpath, '/');
			if (last_delim == NULL)
			{
				goto device_system_monitor_get_device_error_3;
			}

			device_info->path_parent = (char*) malloc(strlen(devices_dir) + strlen(last_delim) + 1);
			if (device_info->path_parent == NULL)
			{
				goto device_system_monitor_get_device_error_3;
			}

			strcpy((char*) device_info->path_parent, devices_dir);
			strcat((char*) device_info->path_parent, last_delim);
			break;

		default:
			device_info->path_parent = NULL;
			break;
		}

		device_info->private_data = NULL;

		*device = device_info;
		*action = action_type;
		return 1;
	}
	else if (len < 0)
	{
		goto device_system_monitor_get_device_error_1;
	}

device_system_monitor_get_device_exit_1:
	return 0;

device_system_monitor_get_device_error_3:
	if (device_info->fstype != NULL)
	{
		free((char*) device_info->fstype);
	}

	if (device_info->label != NULL)
	{
		free((char*) device_info->label);
	}

	free((char*) device_info->path);

device_system_monitor_get_device_error_2:
	free(device_info);

device_system_monitor_get_device_error_1:
	return -1;
}

void device_system_monitor_free_device(dtmd_device_monitor_t *monitor, dtmd_info_t *device)
{
	if ((monitor != NULL)
		&& (device != NULL))
	{
		device_system_free_device(device);
	}
}
