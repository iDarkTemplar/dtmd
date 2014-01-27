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
#include <poll.h>

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

#define IFLIST_REPLY_BUFFER 8192

struct dtmd_device_enumeration
{
	dtmd_device_system_t *system;

	dtmd_info_t **devices;
	uint32_t devices_count;
	uint32_t current_device;
};

typedef struct dtmd_monitor_item
{
	dtmd_info_t *item;
	dtmd_device_action_type_t action;

	struct dtmd_monitor_item *next;
} dtmd_monitor_item_t;

struct dtmd_device_monitor
{
	dtmd_device_system_t *system;

	int data_pipe[2];

	dtmd_monitor_item_t *first;
	dtmd_monitor_item_t *last;
};

typedef struct dtmd_device_internal
{
	dtmd_info_t *device;

	dtmd_info_t **partitions;
	uint32_t partitions_count;
} dtmd_device_internal_t;

struct dtmd_device_system
{
	int events_fd;
	int worker_control_pipe[2];
	pthread_mutex_t control_mutex;
	pthread_t worker_thread;

	dtmd_device_internal_t **devices;
	uint32_t devices_count;

	dtmd_info_t **stateful_devices;
	uint32_t stateful_devices_count;

	uint16_t enumeration_count;
	dtmd_device_enumeration_t **enumerations;

	uint16_t monitor_count;
	dtmd_device_monitor_t **monitors;
};

typedef struct dtmd_info_private
{
	dtmd_device_system_t *system;
	uint32_t counter;
} dtmd_info_private_t;

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
	uint32_t counter = 0;

	if (device->private_data != NULL)
	{
		pthread_mutex_lock(&(((dtmd_info_private_t*)device->private_data)->system->control_mutex));

		counter = --(((dtmd_info_private_t*)device->private_data)->counter);

		pthread_mutex_unlock(&(((dtmd_info_private_t*)device->private_data)->system->control_mutex));
	}

	if (counter == 0)
	{
		if (device->private_data != NULL)
		{
			free(device->private_data);
		}

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

static int helper_read_partition(dtmd_device_system_t *device_system, const char *name, const char *device_name, const char *device_name_parent, dtmd_info_t **device)
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

	device_info->private_data = malloc(sizeof(dtmd_info_private_t));
	if (device_info->private_data == NULL)
	{
		goto helper_read_partition_error_2;
	}

	((dtmd_info_private_t*) device_info->private_data)->system  = device_system;
	((dtmd_info_private_t*) device_info->private_data)->counter = 1;

	device_info->path = (char*) malloc(strlen(devices_dir) + strlen(device_name) + 2);
	if (device_info->path == NULL)
	{
		goto helper_read_partition_error_3;
	}

	device_info->path_parent = (char*) malloc(strlen(devices_dir) + strlen(device_name_parent) + 2);
	if (device_info->path_parent == NULL)
	{
		goto helper_read_partition_error_4;
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

		goto helper_read_partition_error_5;
	}

	*device = device_info;
	return 1;

helper_read_partition_exit_1:
	return 0;

helper_read_partition_error_5:
	free((char*) device_info->path_parent);

helper_read_partition_error_4:
	free((char*) device_info->path);

helper_read_partition_error_3:
	free(device_info->private_data);

helper_read_partition_error_2:
	free(device_info);

helper_read_partition_error_1:
	return -1;
}

static int helper_read_device(dtmd_device_system_t *device_system, const char *name, const char *device_name, int check_removable, dtmd_info_t **device)
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

		device_info->private_data = malloc(sizeof(dtmd_info_private_t));
		if (device_info->private_data == NULL)
		{
			goto helper_read_device_error_2;
		}

		((dtmd_info_private_t*) device_info->private_data)->system  = device_system;
		((dtmd_info_private_t*) device_info->private_data)->counter = 1;

		device_info->path = (char*) malloc(strlen(devices_dir) + strlen(device_name) + 2);
		if (device_info->path == NULL)
		{
			goto helper_read_device_error_3;
		}

		strcpy((char*) device_info->path, devices_dir);
		strcat((char*) device_info->path, "/");
		strcat((char*) device_info->path, device_name);

		device_info->media_type   = media_type;
		device_info->path_parent  = NULL;

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
				goto helper_read_device_error_4;
			}

			*device = device_info;
			return 2;
		}
	}

helper_read_device_exit_1:
	return 0;

helper_read_device_error_4:
	free((char*) device_info->path);

helper_read_device_error_3:
	free(device_info->private_data);

helper_read_device_error_2:
	free(device_info);

helper_read_device_error_1:
	return -1;
}

static int device_system_init_add_stateless_device(dtmd_device_system_t *device_system, dtmd_info_t *device)
{
	uint32_t index;
	dtmd_device_internal_t *internal_device;
	void *tmp;

	for (index = 0; index < device_system->devices_count; ++index)
	{
		if (strcmp(device_system->devices[index]->device->path, device->path) == 0)
		{
			goto device_system_init_add_stateless_device_exit_1;
		}
	}

	internal_device = malloc(sizeof(dtmd_device_internal_t));
	if (internal_device == NULL)
	{
		goto device_system_init_add_stateless_device_error_1;
	}

	tmp = realloc(device_system->devices, (device_system->devices_count + 1) * sizeof(dtmd_device_internal_t*));
	if (tmp == NULL)
	{
		goto device_system_init_add_stateless_device_error_2;
	}

	device_system->devices = (dtmd_device_internal_t**) tmp;

	internal_device->device           = device;
	internal_device->partitions       = NULL;
	internal_device->partitions_count = 0;

	device_system->devices[device_system->devices_count] = internal_device;
	++(device_system->devices_count);

	return 1;

device_system_init_add_stateless_device_exit_1:
	device_system_free_device(device);

	return 0;

device_system_init_add_stateless_device_error_2:
	free(internal_device);

device_system_init_add_stateless_device_error_1:
	device_system_free_device(device);

	return -1;
}

static int device_system_init_add_stateful_device(dtmd_device_system_t *device_system, dtmd_info_t *device)
{
	uint32_t index;
	void *tmp;

	for (index = 0; index < device_system->stateful_devices_count; ++index)
	{
		if (strcmp(device_system->stateful_devices[index]->path, device->path) == 0)
		{
			goto device_system_init_add_stateful_device_exit_1;
		}
	}

	tmp = realloc(device_system->stateful_devices, (device_system->stateful_devices_count + 1) * sizeof(dtmd_info_t*));
	if (tmp == NULL)
	{
		goto device_system_init_add_stateful_device_error_1;
	}

	device_system->stateful_devices = (dtmd_info_t**) tmp;

	device_system->stateful_devices[device_system->stateful_devices_count] = device;
	++(device_system->stateful_devices_count);

	return 1;

device_system_init_add_stateful_device_exit_1:
	device_system_free_device(device);

	return 0;

device_system_init_add_stateful_device_error_1:
	device_system_free_device(device);

	return -1;
}

static int device_system_init_fill_devices(dtmd_device_system_t *device_system)
{
	DIR *dir_pointer = NULL;
	DIR *dir_pointer_partitions = NULL;

	struct dirent *dirent_device = NULL;
	struct dirent *dirent_device_partition = NULL;

	DIR *dir_pointer_usb = NULL;
	DIR *dir_pointer_usb_device = NULL;
	DIR *dir_pointer_usb_host = NULL;
	DIR *dir_pointer_usb_target = NULL;
	DIR *dir_pointer_usb_target_device = NULL;
	DIR *dir_pointer_usb_target_device_blocks = NULL;

	struct dirent *dirent_usb = NULL;
	struct dirent *dirent_usb_device = NULL;
	struct dirent *dirent_usb_host = NULL;
	struct dirent *dirent_usb_target = NULL;
	struct dirent *dirent_usb_target_device = NULL;
	struct dirent *dirent_usb_target_device_blocks = NULL;

	DIR *dir_pointer_mmc = NULL;
	DIR *dir_pointer_mmc_device = NULL;
	DIR *dir_pointer_mmc_device_blocks = NULL;

	struct dirent *dirent_mmc = NULL;
	struct dirent *dirent_mmc_device = NULL;
	struct dirent *dirent_mmc_device_blocks = NULL;

	dtmd_info_t *device;
	void *tmp;

	char file_name[PATH_MAX + 1];
	size_t len_core;
	size_t len_base;
	size_t len_ext;
	size_t len_dev_base;
	struct stat statbuf;

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

	dir_pointer = opendir(block_devices_dir);
	if (dir_pointer == NULL)
	{
		goto device_system_init_fill_devices_error_plain_1;
	}

	len_base = strlen(block_devices_dir);

	while ((dirent_device = readdir(dir_pointer)) != NULL)
	{
		if ((strcmp(dirent_device->d_name, ".") == 0)
			|| (strcmp(dirent_device->d_name, "..") == 0))
		{
			continue;
		}

		len_core = strlen(dirent_device->d_name) + 2;

		if (len_core + len_base + len_ext > PATH_MAX)
		{
			goto device_system_init_fill_devices_error_plain_2;
		}

		strcpy(file_name, block_devices_dir);
		strcat(file_name, "/");
		strcat(file_name, dirent_device->d_name);

		if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
		{
			continue;
		}

		strcat(file_name, "/");

		switch (helper_read_device(device_system, file_name, dirent_device->d_name, 1, &device))
		{
		case 1: // device
			switch (device_system_init_add_stateless_device(device_system, device))
			{
			case 1: // ok
				file_name[len_base + len_core - 1] = 0;
				dir_pointer_partitions = opendir(file_name);
				if (dir_pointer_partitions == NULL)
				{
					goto device_system_init_fill_devices_error_plain_2;
				}
				break;
			/*
			case 0: // ok
				break;
			*/
			case -1: // error
				goto device_system_init_fill_devices_error_plain_2;
				//break;
			}
			break;

		case 2: // stateful_device
			switch (device_system_init_add_stateful_device(device_system, device))
			{
			/*
			case 1: // ok
				break;
			*/
			/*
			case 0: //ok
				break;
			*/
			case -1: // error
				goto device_system_init_fill_devices_error_plain_2;
				//break;
			}
			break;
		/*
		case 0:
			break;
		*/
		case -1:
			goto device_system_init_fill_devices_error_plain_2;
			//break;
		}

		if (dir_pointer_partitions != NULL)
		{
			len_dev_base = strlen(dirent_device->d_name) + 1;

			while ((dirent_device_partition = readdir(dir_pointer_partitions)) != NULL)
			{
				if ((strcmp(dirent_device_partition->d_name, ".") == 0)
					|| (strcmp(dirent_device_partition->d_name, "..") == 0))
				{
					continue;
				}

				len_core = strlen(dirent_device_partition->d_name) + 2;

				if (len_core + len_base + len_dev_base + len_ext > PATH_MAX)
				{
					goto device_system_init_fill_devices_error_plain_3;
				}

				strcpy(file_name, block_devices_dir);
				strcat(file_name, "/");
				strcat(file_name, dirent_device->d_name);
				strcat(file_name, "/");

				switch (helper_read_partition(device_system,
					file_name,
					dirent_device_partition->d_name,
					dirent_device->d_name,
					&device))
				{
				case 1:
					tmp = realloc(device_system->devices[device_system->devices_count-1]->partitions,
						(device_system->devices[device_system->devices_count-1]->partitions_count + 1) * sizeof(dtmd_info_t*));
					if (tmp == NULL)
					{
						device_system_free_device(device);
						goto device_system_init_fill_devices_error_plain_3;
					}

					device_system->devices[device_system->devices_count-1]->partitions = (dtmd_info_t**) tmp;

					device_system->devices[device_system->devices_count-1]->partitions[device_system->devices[device_system->devices_count-1]->partitions_count] = device;
					++(device_system->devices[device_system->devices_count-1]->partitions_count);
					break;
				/*
				case 0:
					break;
				*/
				case -1:
					goto device_system_init_fill_devices_error_plain_3;
					//break;
				}
			}

			closedir(dir_pointer_partitions);
			dir_pointer_partitions = NULL;
		}
	}

	closedir(dir_pointer);
	dir_pointer = NULL;

	dir_pointer_usb = opendir(block_usb_devices_dir);
	if (dir_pointer_usb == NULL)
	{
		goto device_system_init_fill_devices_error_usb_1;
	}

	len_base = strlen(block_usb_devices_dir);

	while ((dirent_usb = readdir(dir_pointer_usb)) != NULL)
	{
		if ((strcmp(dirent_usb->d_name, ".") == 0)
			|| (strcmp(dirent_usb->d_name, "..") == 0))
		{
			continue;
		}

		len_core = strlen(dirent_usb->d_name) + 2;

		if (len_core + len_base + len_ext > PATH_MAX)
		{
			goto device_system_init_fill_devices_error_usb_2;
		}

		strcpy(file_name, block_usb_devices_dir);
		strcat(file_name, "/");
		strcat(file_name, dirent_usb->d_name);

		if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
		{
			continue;
		}

		dir_pointer_usb_device = opendir(file_name);
		if (dir_pointer_usb_device == NULL)
		{
			goto device_system_init_fill_devices_error_usb_2;
		}

		while ((dirent_usb_device = readdir(dir_pointer_usb_device)) != NULL)
		{
			if ((strcmp(dirent_usb_device->d_name, ".") == 0)
				|| (strcmp(dirent_usb_device->d_name, "..") == 0))
			{
				continue;
			}

			if (strncmp(dirent_usb_device->d_name, "host", strlen("host")) != 0)
			{
				continue;
			}

			len_core = strlen(dirent_usb_device->d_name) + strlen(dirent_usb->d_name) + 3;

			if (len_core + len_base + len_ext > PATH_MAX)
			{
				goto device_system_init_fill_devices_error_usb_3;
			}

			strcpy(file_name, block_usb_devices_dir);
			strcat(file_name, "/");
			strcat(file_name, dirent_usb->d_name);
			strcat(file_name, "/");
			strcat(file_name, dirent_usb_device->d_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			dir_pointer_usb_host = opendir(file_name);
			if (dir_pointer_usb_host == NULL)
			{
				goto device_system_init_fill_devices_error_usb_3;
			}

			while ((dirent_usb_host = readdir(dir_pointer_usb_host)) != NULL)
			{
				if ((strcmp(dirent_usb_host->d_name, ".") == 0)
					|| (strcmp(dirent_usb_host->d_name, "..") == 0))
				{
					continue;
				}

				if (strncmp(dirent_usb_host->d_name, "target", strlen("target")) != 0)
				{
					continue;
				}

				len_core = strlen(dirent_usb_host->d_name) + strlen(dirent_usb_device->d_name) + strlen(dirent_usb->d_name) + 4;

				if (len_core + len_base + len_ext > PATH_MAX)
				{
					goto device_system_init_fill_devices_error_usb_4;
				}

				strcpy(file_name, block_usb_devices_dir);
				strcat(file_name, "/");
				strcat(file_name, dirent_usb->d_name);
				strcat(file_name, "/");
				strcat(file_name, dirent_usb_device->d_name);
				strcat(file_name, "/");
				strcat(file_name, dirent_usb_host->d_name);

				if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
				{
					continue;
				}

				dir_pointer_usb_target = opendir(file_name);
				if (dir_pointer_usb_target == NULL)
				{
					goto device_system_init_fill_devices_error_usb_4;
				}

				while ((dirent_usb_target = readdir(dir_pointer_usb_target)) != NULL)
				{
					if ((strcmp(dirent_usb_target->d_name, ".") == 0)
						|| (strcmp(dirent_usb_target->d_name, "..") == 0))
					{
						continue;
					}

					len_core = strlen(block_dir_name) + strlen(dirent_usb_target->d_name) + strlen(dirent_usb_host->d_name)
						+ strlen(dirent_usb_device->d_name) + strlen(dirent_usb->d_name) + 6;

					if (len_core + len_base + len_ext > PATH_MAX)
					{
						goto device_system_init_fill_devices_error_usb_5;
					}

					strcpy(file_name, block_usb_devices_dir);
					strcat(file_name, "/");
					strcat(file_name, dirent_usb->d_name);
					strcat(file_name, "/");
					strcat(file_name, dirent_usb_device->d_name);
					strcat(file_name, "/");
					strcat(file_name, dirent_usb_host->d_name);
					strcat(file_name, "/");
					strcat(file_name, dirent_usb_target->d_name);
					strcat(file_name, "/");
					strcat(file_name, block_dir_name);

					if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
					{
						continue;
					}

					dir_pointer_usb_target_device = opendir(file_name);
					if (dir_pointer_usb_target_device == NULL)
					{
						goto device_system_init_fill_devices_error_usb_5;
					}

					while ((dirent_usb_target_device = readdir(dir_pointer_usb_target_device)) != NULL)
					{
						if ((strcmp(dirent_usb_target_device->d_name, ".") == 0)
							|| (strcmp(dirent_usb_target_device->d_name, "..") == 0))
						{
							continue;
						}

						len_core = strlen(dirent_usb_target_device->d_name) + strlen(block_dir_name)
							+ strlen(dirent_usb_target->d_name) + strlen(dirent_usb_host->d_name)
							+ strlen(dirent_usb_device->d_name) + strlen(dirent_usb->d_name) + 8;

						if (len_core + len_base + len_ext > PATH_MAX)
						{
							goto device_system_init_fill_devices_error_usb_6;
						}

						strcpy(file_name, block_usb_devices_dir);
						strcat(file_name, "/");
						strcat(file_name, dirent_usb->d_name);
						strcat(file_name, "/");
						strcat(file_name, dirent_usb_device->d_name);
						strcat(file_name, "/");
						strcat(file_name, dirent_usb_host->d_name);
						strcat(file_name, "/");
						strcat(file_name, dirent_usb_target->d_name);
						strcat(file_name, "/");
						strcat(file_name, block_dir_name);
						strcat(file_name, "/");
						strcat(file_name, dirent_usb_target_device->d_name);

						if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
						{
							continue;
						}

						strcat(file_name, "/");

						switch (helper_read_device(device_system, file_name, dirent_usb_target_device->d_name, 0, &device))
						{
						case 1: // device
							switch (device_system_init_add_stateless_device(device_system, device))
							{
							case 1: // ok
								file_name[len_base + len_core - 1] = 0;
								dir_pointer_usb_target_device_blocks = opendir(file_name);
								if (dir_pointer_usb_target_device_blocks == NULL)
								{
									goto device_system_init_fill_devices_error_usb_6;
								}
								break;
							/*
							case 0: // ok
								break;
							*/
							case -1: // error
								goto device_system_init_fill_devices_error_usb_6;
								//break;
							}
							break;

						case 2: // stateful_device
							switch (device_system_init_add_stateful_device(device_system, device))
							{
							/*
							case 1: // ok
								break;
							*/
							/*
							case 0: //ok
								break;
							*/
							case -1: // error
								goto device_system_init_fill_devices_error_usb_6;
								//break;
							}
							break;
						/*
						case 0:
							break;
						*/
						case -1:
							goto device_system_init_fill_devices_error_plain_2;
							//break;
						}

						if (dir_pointer_usb_target_device_blocks != NULL)
						{
							len_dev_base = strlen(dirent_usb_target_device->d_name) + strlen(block_dir_name)
								+ strlen(dirent_usb_target->d_name) + strlen(dirent_usb_host->d_name)
								+ strlen(dirent_usb_device->d_name) + strlen(dirent_usb->d_name) + 7;

							while ((dirent_usb_target_device_blocks = readdir(dir_pointer_usb_target_device_blocks)) != NULL)
							{
								if ((strcmp(dirent_usb_target_device_blocks->d_name, ".") == 0)
									|| (strcmp(dirent_usb_target_device_blocks->d_name, "..") == 0))
								{
									continue;
								}

								len_core = strlen(dirent_usb_target_device_blocks->d_name) + 2;

								if (len_core + len_base + len_dev_base + len_ext > PATH_MAX)
								{
									goto device_system_init_fill_devices_error_usb_7;
								}

								strcpy(file_name, block_usb_devices_dir);
								strcat(file_name, "/");
								strcat(file_name, dirent_usb->d_name);
								strcat(file_name, "/");
								strcat(file_name, dirent_usb_device->d_name);
								strcat(file_name, "/");
								strcat(file_name, dirent_usb_host->d_name);
								strcat(file_name, "/");
								strcat(file_name, dirent_usb_target->d_name);
								strcat(file_name, "/");
								strcat(file_name, block_dir_name);
								strcat(file_name, "/");
								strcat(file_name, dirent_usb_target_device->d_name);
								strcat(file_name, "/");

								switch (helper_read_partition(device_system,
									file_name,
									dirent_usb_target_device_blocks->d_name,
									dirent_usb_target_device->d_name,
									&device))
								{
								case 1:
									tmp = realloc(device_system->devices[device_system->devices_count-1]->partitions,
										(device_system->devices[device_system->devices_count-1]->partitions_count + 1) * sizeof(dtmd_info_t*));
									if (tmp == NULL)
									{
										device_system_free_device(device);
										goto device_system_init_fill_devices_error_usb_7;
									}

									device_system->devices[device_system->devices_count-1]->partitions = (dtmd_info_t**) tmp;

									device_system->devices[device_system->devices_count-1]->partitions[device_system->devices[device_system->devices_count-1]->partitions_count] = device;
									++(device_system->devices[device_system->devices_count-1]->partitions_count);
									break;
								/*
								case 0:
									break;
								*/
								case -1:
									goto device_system_init_fill_devices_error_usb_7;
									//break;
								}
							}

							closedir(dir_pointer_usb_target_device_blocks);
							dir_pointer_usb_target_device_blocks = NULL;
						}
					}

					closedir(dir_pointer_usb_target_device);
					dir_pointer_usb_target_device = NULL;
				}

				closedir(dir_pointer_usb_target);
				dir_pointer_usb_target = NULL;
			}

			closedir(dir_pointer_usb_host);
			dir_pointer_usb_host = NULL;
		}

		closedir(dir_pointer_usb_device);
		dir_pointer_usb_device = NULL;
	}

	closedir(dir_pointer_usb);
	dir_pointer_usb = NULL;

	dir_pointer_mmc = opendir(block_mmc_devices_dir);
	if (dir_pointer_mmc == NULL)
	{
		goto device_system_init_fill_devices_error_mmc_1;
	}

	len_base = strlen(block_mmc_devices_dir);

	while ((dirent_mmc = readdir(dir_pointer_mmc)) != NULL)
	{
		if ((strcmp(dirent_mmc->d_name, ".") == 0)
			|| (strcmp(dirent_mmc->d_name, "..") == 0))
		{
			continue;
		}

		len_core = strlen(block_dir_name) + strlen(dirent_mmc->d_name) + 3;

		if (len_core + len_base + len_ext > PATH_MAX)
		{
			goto device_system_init_fill_devices_error_mmc_2;
		}

		strcpy(file_name, block_mmc_devices_dir);
		strcat(file_name, "/");
		strcat(file_name, dirent_mmc->d_name);
		strcat(file_name, "/");
		strcat(file_name, block_dir_name);

		if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
		{
			continue;
		}

		dir_pointer_mmc_device = opendir(file_name);
		if (dir_pointer_mmc_device == NULL)
		{
			goto device_system_init_fill_devices_error_mmc_2;
		}

		while ((dirent_mmc_device = readdir(dir_pointer_mmc_device)) != NULL)
		{
			if ((strcmp(dirent_mmc_device->d_name, ".") == 0)
				|| (strcmp(dirent_mmc_device->d_name, "..") == 0))
			{
				continue;
			}

			len_core = strlen(dirent_mmc_device->d_name)
				+ strlen(block_dir_name) + strlen(dirent_mmc->d_name) + 5;

			if (len_core + len_base + len_ext > PATH_MAX)
			{
				goto device_system_init_fill_devices_error_mmc_3;
			}

			strcpy(file_name, block_mmc_devices_dir);
			strcat(file_name, "/");
			strcat(file_name, dirent_mmc->d_name);
			strcat(file_name, "/");
			strcat(file_name, block_dir_name);
			strcat(file_name, "/");
			strcat(file_name, dirent_mmc_device->d_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			strcat(file_name, "/");

			switch (helper_read_device(device_system, file_name, dirent_mmc_device->d_name, 0, &device))
			{
			case 1: // device
				switch (device_system_init_add_stateless_device(device_system, device))
				{
				case 1: // ok
					file_name[len_base + len_core - 1] = 0;
					dir_pointer_mmc_device_blocks = opendir(file_name);
					if (dir_pointer_mmc_device_blocks == NULL)
					{
						goto device_system_init_fill_devices_error_mmc_3;
					}
					break;
				/*
				case 0: // ok
					break;
				*/
				case -1: // error
					goto device_system_init_fill_devices_error_mmc_3;
					//break;
				}
				break;

			case 2: // stateful_device
				switch (device_system_init_add_stateful_device(device_system, device))
				{
				/*
				case 1: // ok
					break;
				*/
				/*
				case 0: //ok
					break;
				*/
				case -1: // error
					goto device_system_init_fill_devices_error_mmc_3;
					//break;
				}
				break;
			/*
			case 0:
				break;
			*/
			case -1:
				goto device_system_init_fill_devices_error_mmc_3;
				//break;
			}

			if (dir_pointer_mmc_device_blocks != NULL)
			{
				len_dev_base = strlen(dirent_mmc_device->d_name)
					+ strlen(block_dir_name) + strlen(dirent_mmc->d_name) + 4;

				while ((dirent_mmc_device_blocks = readdir(dir_pointer_mmc_device_blocks)) != NULL)
				{
					if ((strcmp(dirent_mmc_device_blocks->d_name, ".") == 0)
						|| (strcmp(dirent_mmc_device_blocks->d_name, "..") == 0))
					{
						continue;
					}

					len_core = strlen(dirent_mmc_device_blocks->d_name) + 2;

					if (len_core + len_base + len_dev_base + len_ext > PATH_MAX)
					{
						goto device_system_init_fill_devices_error_mmc_4;
					}

					strcpy(file_name, block_mmc_devices_dir);
					strcat(file_name, "/");
					strcat(file_name, dirent_mmc->d_name);
					strcat(file_name, "/");
					strcat(file_name, block_dir_name);
					strcat(file_name, "/");
					strcat(file_name, dirent_mmc_device->d_name);
					strcat(file_name, "/");

					switch (helper_read_partition(device_system,
						file_name,
						dirent_mmc_device_blocks->d_name,
						dirent_mmc_device->d_name,
						&device))
					{
					case 1:
						tmp = realloc(device_system->devices[device_system->devices_count-1]->partitions,
							(device_system->devices[device_system->devices_count-1]->partitions_count + 1) * sizeof(dtmd_info_t*));
						if (tmp == NULL)
						{
							device_system_free_device(device);
							goto device_system_init_fill_devices_error_mmc_4;
						}

						device_system->devices[device_system->devices_count-1]->partitions = (dtmd_info_t**) tmp;

						device_system->devices[device_system->devices_count-1]->partitions[device_system->devices[device_system->devices_count-1]->partitions_count] = device;
						++(device_system->devices[device_system->devices_count-1]->partitions_count);
						break;
					/*
					case 0:
						break;
					*/
					case -1:
						goto device_system_init_fill_devices_error_mmc_4;
						//break;
					}
				}

				closedir(dir_pointer_mmc_device_blocks);
				dir_pointer_mmc_device_blocks = NULL;
			}
		}

		closedir(dir_pointer_mmc_device);
		dir_pointer_mmc_device = NULL;
	}

	closedir(dir_pointer_mmc);
	dir_pointer_mmc = NULL;

	return 0;

device_system_init_fill_devices_error_mmc_4:
	closedir(dir_pointer_mmc_device_blocks);

device_system_init_fill_devices_error_mmc_3:
	closedir(dir_pointer_mmc_device);

device_system_init_fill_devices_error_mmc_2:
	closedir(dir_pointer_mmc);

	goto device_system_init_fill_devices_error_mmc_2;

device_system_init_fill_devices_error_usb_7:
	closedir(dir_pointer_usb_target_device_blocks);

device_system_init_fill_devices_error_usb_6:
	closedir(dir_pointer_usb_target_device);

device_system_init_fill_devices_error_usb_5:
	closedir(dir_pointer_usb_target);

device_system_init_fill_devices_error_usb_4:
	closedir(dir_pointer_usb_host);

device_system_init_fill_devices_error_usb_3:
	closedir(dir_pointer_usb_device);

device_system_init_fill_devices_error_usb_2:
	closedir(dir_pointer_usb);

	goto device_system_init_fill_devices_error_usb_1;

device_system_init_fill_devices_error_plain_3:
	closedir(dir_pointer_partitions);

device_system_init_fill_devices_error_plain_2:
	closedir(dir_pointer);

device_system_init_fill_devices_error_mmc_1:
device_system_init_fill_devices_error_usb_1:
device_system_init_fill_devices_error_plain_1:
	return -1;
}

static void device_system_free_all_devices(dtmd_device_system_t *device_system)
{
	uint32_t i, j;

	if (device_system->devices != NULL)
	{
		for (i = 0; i < device_system->devices_count; ++i)
		{
			if (device_system->devices[i] != NULL)
			{
				if (device_system->devices[i]->device != NULL)
				{
					device_system_free_device(device_system->devices[i]->device);
				}

				if (device_system->devices[i]->partitions != NULL)
				{
					for (j = 0; j < device_system->devices[i]->partitions_count; ++j)
					{
						if (device_system->devices[i]->partitions[j] != NULL)
						{
							device_system_free_device(device_system->devices[i]->partitions[j]);
						}
					}

					free(device_system->devices[i]->partitions);
				}

				free(device_system->devices[i]);
			}
		}

		free(device_system->devices);
	}

	if (device_system->stateful_devices != NULL)
	{
		for (i = 0; i < device_system->stateful_devices_count; ++i)
		{
			if (device_system->stateful_devices[i] != NULL)
			{
				device_system_free_device(device_system->stateful_devices[i]);
			}
		}

		free(device_system->stateful_devices);
	}
}

static dtmd_info_t* device_system_copy_device(dtmd_info_t *device)
{
	pthread_mutex_lock(&(((dtmd_info_private_t*)device->private_data)->system->control_mutex));

	++(((dtmd_info_private_t*)device->private_data)->counter);

	pthread_mutex_unlock(&(((dtmd_info_private_t*)device->private_data)->system->control_mutex));

	return device;
}

static void device_system_free_monitor_item(dtmd_monitor_item_t *item)
{
	if (item->item != NULL)
	{
		device_system_free_device(item->item);
	}

	free(item);
}

static int device_system_monitor_receive_device(int fd, dtmd_info_t **device, dtmd_device_action_type_t *action)
{
	struct sockaddr_nl kernel;
	struct iovec io;
	char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
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

	len = recvmsg(fd, &rtnl_reply, 0);
	if (len > 0)
	{
		if ((kernel.nl_family != AF_NETLINK)
			|| (kernel.nl_pid != 0)
			|| (kernel.nl_groups != NETLINK_GROUP_KERNEL))
		{
			goto device_system_monitor_receive_device_exit_1;
		}

		cmsg = CMSG_FIRSTHDR(&rtnl_reply);
		if ((cmsg == NULL)|| (cmsg->cmsg_type != SCM_CREDENTIALS))
		{
			goto device_system_monitor_receive_device_exit_1;
		}

		cred = (struct ucred*) CMSG_DATA(cmsg);

		if ((cred->pid != 0)
			|| (cred->uid != 0)
			|| (cred->gid != 0))
		{
			goto device_system_monitor_receive_device_exit_1;
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
			goto device_system_monitor_receive_device_exit_1;
		}

		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			goto device_system_monitor_receive_device_error_1;
		}

		switch (action_type)
		{
		case dtmd_device_action_add:
		case dtmd_device_action_online:
		case dtmd_device_action_change:
			if (strlen(block_sys_dir) + strlen(devpath) + strlen(filename_device_type) + 4 > PATH_MAX)
			{
				goto device_system_monitor_receive_device_error_2;
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
				goto device_system_monitor_receive_device_error_2;
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
				device_info->type = dtmd_info_unknown;
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
			goto device_system_monitor_receive_device_error_2;
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
					goto device_system_monitor_receive_device_error_3;
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
					goto device_system_monitor_receive_device_error_3;
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
				goto device_system_monitor_receive_device_error_3;
			}

			*last_delim = 0;
			last_delim = strrchr(devpath, '/');
			if (last_delim == NULL)
			{
				goto device_system_monitor_receive_device_error_3;
			}

			device_info->path_parent = (char*) malloc(strlen(devices_dir) + strlen(last_delim) + 1);
			if (device_info->path_parent == NULL)
			{
				goto device_system_monitor_receive_device_error_3;
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
		goto device_system_monitor_receive_device_error_1;
	}

device_system_monitor_receive_device_exit_1:
	return 0;

device_system_monitor_receive_device_error_3:
	if (device_info->fstype != NULL)
	{
		free((char*) device_info->fstype);
	}

	if (device_info->label != NULL)
	{
		free((char*) device_info->label);
	}

	free((char*) device_info->path);

device_system_monitor_receive_device_error_2:
	free(device_info);

device_system_monitor_receive_device_error_1:
	return -1;
}

static int device_system_monitor_add_item(dtmd_device_monitor_t *monitor, dtmd_info_t *device, dtmd_device_action_type_t action)
{
	char data = 1;
	dtmd_monitor_item_t *monitor_item;

	monitor_item = (dtmd_monitor_item_t*) malloc(sizeof(dtmd_monitor_item_t));
	if (monitor_item == NULL)
	{
		goto device_system_monitor_add_item_error_1;
	}

	monitor_item->item = device_system_copy_device(device);
	if (monitor_item->item == NULL)
	{
		goto device_system_monitor_add_item_error_2;
	}

	monitor_item->action = action;
	monitor_item->next   = NULL;

	if (monitor->last != NULL)
	{
		monitor->last->next = monitor_item;
		monitor->last = monitor->last->next;
	}
	else
	{
		monitor->first = monitor_item;
		monitor->last = monitor->first;
	}

	write(monitor->data_pipe[1], &data, 1);

	return 1;

device_system_monitor_add_item_error_2:
	free(monitor_item);

device_system_monitor_add_item_error_1:
	return -1;
}

static void* device_system_worker_function(void *arg)
{
	dtmd_device_system_t *device_system;
	struct pollfd fds[2];
	char data;
	int rc;
	dtmd_info_t *device;
	dtmd_device_action_type_t action;
	dtmd_info_type_t found_device_type;
	uint32_t device_index, partition_index, parent_index, monitor_index;
	int found_parent;
	void *tmp;
	dtmd_device_internal_t *device_item;

	device_system = (dtmd_device_system_t*) arg;

	fds[0].fd = device_system->worker_control_pipe[0];
	fds[1].fd = device_system->events_fd;

	for (;;)
	{
		fds[0].events  = POLLIN;
		fds[0].revents = 0;
		fds[1].events  = POLLIN;
		fds[1].revents = 0;

		rc = poll(fds, 2, -1);

		if ((rc == -1)
			|| (fds[0].revents & POLLERR)
			|| (fds[0].revents & POLLHUP)
			|| (fds[0].revents & POLLNVAL)
			|| (fds[1].revents & POLLERR)
			|| (fds[1].revents & POLLHUP)
			|| (fds[1].revents & POLLNVAL))
		{
			goto device_system_worker_function_error_1;
		}

		if (fds[0].revents & POLLIN)
		{
			rc = read(device_system->worker_control_pipe[0], &data, sizeof(char));

			if (rc == 1)
			{
				goto device_system_worker_function_exit;
			}
			else
			{
				goto device_system_worker_function_error_1;
			}
		}

		if (fds[1].revents & POLLIN)
		{
			rc = device_system_monitor_receive_device(fds[1].fd, &device, &action);
			switch (rc)
			{
			case 1:
				switch (action)
				{
				case dtmd_device_action_add:
				case dtmd_device_action_online:
				case dtmd_device_action_remove:
				case dtmd_device_action_offline:
				case dtmd_device_action_change:
					device->private_data = malloc(sizeof(dtmd_info_private_t));
					if (device->private_data == NULL)
					{
						goto device_system_worker_function_error_2;
					}

					((dtmd_info_private_t*) device->private_data)->system  = device_system;
					((dtmd_info_private_t*) device->private_data)->counter = 1;

					if (pthread_mutex_lock(&(device_system->control_mutex)) != 0)
					{
						goto device_system_worker_function_error_2;
					}

					found_device_type = dtmd_info_unknown;
					found_parent = 0;

					for (device_index = 0; device_index < device_system->devices_count; ++device_index)
					{
						if ((device_system->devices[device_index] != NULL)
							&& (device_system->devices[device_index]->device != NULL))
						{
							if (strcmp(device_system->devices[device_index]->device->path, device->path) == 0)
							{
								found_device_type = dtmd_info_device;
								break;
							}

							if ((device->type == dtmd_info_partition)
								&& (strcmp(device_system->devices[device_index]->device->path, device->path_parent) == 0))
							{
								parent_index = device_index;
								found_parent = 1;
							}

							if (device_system->devices[device_index]->partitions != NULL)
							{
								for (partition_index = 0; partition_index < device_system->devices[device_index]->partitions_count; ++partition_index)
								{
									if ((device_system->devices[device_index]->partitions[partition_index] != NULL)
										&& (strcmp(device_system->devices[device_index]->partitions[partition_index]->path, device->path) == 0))
									{
										found_device_type = dtmd_info_partition;
										break;
									}
								}
							}
						}
					}

					if ((found_device_type == dtmd_info_unknown)
						&& (device->type != dtmd_info_partition))
					{
						for (device_index = 0; device_index < device_system->stateful_devices_count; ++device_index)
						{
							if ((device_system->stateful_devices[device_index] != NULL)
								&& (strcmp(device_system->stateful_devices[device_index]->path, device->path) == 0))
							{
								found_device_type = dtmd_info_stateful_device;
								break;
							}
						}
					}

					switch (action)
					{
					case dtmd_device_action_add:
					case dtmd_device_action_online:
						if (found_device_type == dtmd_info_unknown)
						{
							if ((device->type != dtmd_info_partition)
								|| (found_parent))
							{
								for (monitor_index = 0; monitor_index < device_system->monitor_count; ++monitor_index)
								{
									if (device_system_monitor_add_item(device_system->monitors[monitor_index], device, action) < 0)
									{
										goto device_system_worker_function_error_3;
									}
								}

								switch (device->type)
								{
								case dtmd_info_device:
									device_item = (dtmd_device_internal_t*) malloc(sizeof(dtmd_device_internal_t));
									if (device_item == NULL)
									{
										goto device_system_worker_function_error_3;
									}

									device_item->device           = device;
									device_item->partitions       = NULL;
									device_item->partitions_count = 0;

									tmp = realloc(device_system->devices, (device_system->devices_count + 1) * sizeof(dtmd_device_internal_t*));
									if (tmp == NULL)
									{
										goto device_system_worker_function_error_4;
									}

									device_system->devices = (dtmd_device_internal_t**) tmp;
									device_system->devices[device_system->devices_count] = device_item;
									++(device_system->devices_count);
									break;

								case dtmd_info_partition:
									tmp = realloc(device_system->devices[parent_index]->partitions, (device_system->devices[parent_index]->partitions_count + 1) * sizeof(dtmd_info_t*));
									if (tmp == NULL)
									{
										goto device_system_worker_function_error_3;
									}

									device_system->devices[parent_index]->partitions = (dtmd_info_t**) tmp;
									device_system->devices[parent_index]->partitions[device_system->devices[parent_index]->partitions_count] = device;
									++(device_system->devices[parent_index]->partitions_count);
									break;

								case dtmd_info_stateful_device:
									tmp = realloc(device_system->stateful_devices, (device_system->stateful_devices_count + 1) * sizeof(dtmd_info_t*));
									if (tmp == NULL)
									{
										goto device_system_worker_function_error_3;
									}

									device_system->stateful_devices = (dtmd_info_t**) tmp;
									device_system->stateful_devices[device_system->stateful_devices_count] = device;
									++(device_system->stateful_devices_count);
									break;
								}
							}
						}
						break;

					case dtmd_device_action_remove:
					case dtmd_device_action_offline:
						if (found_device_type != dtmd_info_unknown)
						{
							for (monitor_index = 0; monitor_index < device_system->monitor_count; ++monitor_index)
							{
								switch (found_device_type)
								{
								case dtmd_info_device:
									for (parent_index = 0; parent_index < device_system->devices[device_index]->partitions_count; ++parent_index)
									{
										if (device_system_monitor_add_item(device_system->monitors[monitor_index], device_system->devices[device_index]->partitions[parent_index], action) < 0)
										{
											goto device_system_worker_function_error_3;
										}
									}

									if (device_system_monitor_add_item(device_system->monitors[monitor_index], device_system->devices[device_index]->device, action) < 0)
									{
										goto device_system_worker_function_error_3;
									}
									break;

								case dtmd_info_partition:
									if (device_system_monitor_add_item(device_system->monitors[monitor_index], device_system->devices[parent_index]->partitions[partition_index], action) < 0)
									{
										goto device_system_worker_function_error_3;
									}
									break;

								case dtmd_info_stateful_device:
									if (device_system_monitor_add_item(device_system->monitors[monitor_index], device_system->stateful_devices[device_index], action) < 0)
									{
										goto device_system_worker_function_error_3;
									}
									break;
								}
							}

							switch (found_device_type)
							{
							case dtmd_info_device:
								for (parent_index = 0; parent_index < device_system->devices[device_index]->partitions_count; ++parent_index)
								{
									if (device_system->devices[device_index]->partitions[parent_index] != NULL)
									{
										device_system_free_device(device_system->devices[device_index]->partitions[parent_index]);
									}
								}

								device_system_free_device(device_system->devices[device_index]->device);
								free(device_system->devices[device_index]);

								--(device_system->devices_count);

								if (device_system->devices_count > 0)
								{
									device_system->devices[device_index] = device_system->devices[device_system->devices_count];

									tmp = realloc(device_system->devices, device_system->devices_count * sizeof(dtmd_device_internal_t*));
									if (tmp != NULL)
									{
										device_system->devices = (dtmd_device_internal_t**) tmp;
									}
									else
									{
										device_system->devices[device_system->devices_count + 1] = NULL;
									}
								}
								else
								{
									free(device_system->devices);
									device_system->devices = NULL;
								}
								break;

							case dtmd_info_partition:
								device_system_free_device(device_system->devices[parent_index]->partitions[partition_index]);

								--(device_system->devices[parent_index]->partitions_count);

								if (device_system->devices[parent_index]->partitions_count > 0)
								{
									device_system->devices[parent_index]->partitions[partition_index] = device_system->devices[parent_index]->partitions[device_system->devices[parent_index]->partitions_count + 1];

									tmp = realloc(device_system->devices[parent_index]->partitions, device_system->devices[parent_index]->partitions_count * sizeof(dtmd_info_t*));
									if (tmp != NULL)
									{
										device_system->devices[parent_index]->partitions = (dtmd_info_t**) tmp;
									}
									else
									{
										device_system->devices[parent_index]->partitions[device_system->devices[parent_index]->partitions_count + 1] = NULL;
									}
								}
								else
								{
									free(device_system->devices[parent_index]->partitions);
									device_system->devices[parent_index]->partitions = NULL;
								}
								break;

							case dtmd_info_stateful_device:
								device_system_free_device(device_system->stateful_devices[device_index]);

								--(device_system->stateful_devices_count);

								if (device_system->stateful_devices_count > 0)
								{
									device_system->stateful_devices[device_index] = device_system->stateful_devices[device_system->stateful_devices_count + 1];

									tmp = realloc(device_system->stateful_devices, device_system->stateful_devices_count * sizeof(dtmd_info_t*));
									if (tmp != NULL)
									{
										device_system->stateful_devices = (dtmd_info_t**) tmp;
									}
									else
									{
										device_system->stateful_devices[device_system->stateful_devices_count + 1] = NULL;
									}
								}
								else
								{
									free(device_system->stateful_devices);
									device_system->stateful_devices = NULL;
								}
								break;
							}
						}

						device_system_free_device(device);
						break;

					case dtmd_device_action_change:
						if ((found_device_type != dtmd_info_unknown) && (device->type == found_device_type))
						{
							for (monitor_index = 0; monitor_index < device_system->monitor_count; ++monitor_index)
							{
								if (device_system_monitor_add_item(device_system->monitors[monitor_index], device, action) < 0)
								{
									goto device_system_worker_function_error_3;
								}
							}

							switch (device->type)
							{
							case dtmd_info_device:
								device_system_free_device(device_system->devices[device_index]->device);
								device_system->devices[device_index]->device = device;
								break;

							case dtmd_info_partition:
								device_system_free_device(device_system->devices[parent_index]->partitions[partition_index]);
								device_system->devices[parent_index]->partitions[partition_index] = device;
								break;

							case dtmd_info_stateful_device:
								device_system_free_device(device_system->stateful_devices[device_index]);
								device_system->stateful_devices[device_index] = device;
								break;
							}
						}
						break;

					case dtmd_device_action_unknown:
					default:
						device_system_free_device(device);
						break;
					}

					pthread_mutex_unlock(&(device_system->control_mutex));
					break;

				case dtmd_device_action_unknown:
				default:
					device_system_free_device(device);
					break;
				}
				break;

			/*
			case 0:
				break;
			*/

			case -1:
				goto device_system_worker_function_error_1;
				/* break; */
			}
		}
	}

device_system_worker_function_exit:
	pthread_mutex_lock(&(device_system->control_mutex));

	data = 2;

	for (parent_index = 0; parent_index < device_system->monitor_count; ++parent_index)
	{
		write(device_system->monitors[parent_index]->data_pipe[1], &data, 1);
	}

	pthread_mutex_unlock(&(device_system->control_mutex));

	goto device_system_worker_function_terminate;

device_system_worker_function_error_4:
	free(device_item);

device_system_worker_function_error_3:
	device_system_free_device(device);

	goto device_system_worker_function_error_1_locked;

device_system_worker_function_error_2:
	device_system_free_device(device);

device_system_worker_function_error_1:
	pthread_mutex_lock(&(device_system->control_mutex));

device_system_worker_function_error_1_locked:
	data = 0;

	for (parent_index = 0; parent_index < device_system->monitor_count; ++parent_index)
	{
		write(device_system->monitors[parent_index]->data_pipe[1], &data, 1);
	}

	pthread_mutex_unlock(&(device_system->control_mutex));

device_system_worker_function_terminate:
	pthread_exit(0);
}

dtmd_device_system_t* device_system_init(void)
{
	dtmd_device_system_t *device_system;
	pthread_mutexattr_t mutex_attr;
	/* char data = 0; */

	device_system = (dtmd_device_system_t*) malloc(sizeof(dtmd_device_system_t));
	if (device_system == NULL)
	{
		goto device_system_init_error_1;
	}

	device_system->devices                = NULL;
	device_system->devices_count          = 0;
	device_system->stateful_devices       = NULL;
	device_system->stateful_devices_count = 0;
	device_system->enumerations           = NULL;
	device_system->enumeration_count      = 0;
	device_system->monitors               = NULL;
	device_system->monitor_count          = 0;

	device_system->events_fd = open_netlink_socket();
	if (device_system->events_fd < 0)
	{
		goto device_system_init_error_2;
	}

	if (pthread_mutexattr_init(&mutex_attr) != 0)
	{
		goto device_system_init_error_3;
	}

	if (pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE) != 0)
	{
		goto device_system_init_error_4;
	}

	if (pthread_mutex_init(&(device_system->control_mutex), &mutex_attr) != 0)
	{
		goto device_system_init_error_4;
	}

	if (device_system_init_fill_devices(device_system) < 0)
	{
		goto device_system_init_error_5;
	}

	if (pipe(device_system->worker_control_pipe) < 0)
	{
		goto device_system_init_error_5;
	}

	if ((pthread_create(&(device_system->worker_thread), NULL, &device_system_worker_function, device_system)) != 0)
	{
		goto device_system_init_error_6;
	}

	pthread_mutexattr_destroy(&mutex_attr);

	return device_system;

/*
device_system_init_error_7:
	write(device_system->worker_control_pipe[1], &data, sizeof(char));
	pthread_join(device_system->worker_thread, NULL);
*/

device_system_init_error_6:
	close(device_system->worker_control_pipe[0]);
	close(device_system->worker_control_pipe[1]);

device_system_init_error_5:
	pthread_mutex_destroy(&(device_system->control_mutex));

device_system_init_error_4:
	pthread_mutexattr_destroy(&mutex_attr);

device_system_init_error_3:
	device_system_free_all_devices(device_system);
	close(device_system->events_fd);

device_system_init_error_2:
	free(device_system);

device_system_init_error_1:
	return NULL;
}

static void helper_free_enumeration(dtmd_device_enumeration_t *enumeration)
{
	uint32_t i;

	if (enumeration->devices_count > 0)
	{
		for (i = 0; i < enumeration->devices_count; ++i)
		{
			device_system_free_device(enumeration->devices[i]);
		}

		free(enumeration->devices);
	}

	free(enumeration);
}

static void helper_free_monitor(dtmd_device_monitor_t *monitor)
{
	dtmd_monitor_item_t *item, *delete_item;

	item = monitor->first;

	while (item != NULL)
	{
		delete_item = item;
		item = item->next;

		device_system_free_monitor_item(delete_item);
	}

	close(monitor->data_pipe[0]);
	close(monitor->data_pipe[1]);

	free(monitor);
}

void device_system_deinit(dtmd_device_system_t *system)
{
	uint32_t i;
	char data = 0;

	if (system != NULL)
	{
		write(system->worker_control_pipe[1], &data, sizeof(char));
		pthread_join(system->worker_thread, NULL);

		close(system->events_fd);

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

		close(system->worker_control_pipe[0]);
		close(system->worker_control_pipe[1]);

		device_system_free_all_devices(system);

		pthread_mutex_destroy(&(system->control_mutex));

		free(system);
	}
}

dtmd_device_enumeration_t* device_system_enumerate_devices(dtmd_device_system_t *system)
{
	dtmd_device_enumeration_t *enumeration;
	void **tmp;
	uint32_t devices_count;
	uint32_t i, j, k;

	if (system == NULL)
	{
		goto device_system_enumerate_devices_error_1;
	}

	if (pthread_mutex_lock(&(system->control_mutex)) != 0)
	{
		goto device_system_enumerate_devices_error_1;
	}

	devices_count = system->devices_count + system->stateful_devices_count;

	for (i = 0; i < system->devices_count; ++i)
	{
		devices_count += system->devices[i]->partitions_count;
	}

	enumeration = (dtmd_device_enumeration_t*) malloc(sizeof(dtmd_device_enumeration_t));
	if (enumeration == NULL)
	{
		goto device_system_enumerate_devices_error_2;
	}

	enumeration->system         = system;
	enumeration->devices_count  = devices_count;
	enumeration->current_device = 0;
	k = 0;

	if (enumeration->devices_count > 0)
	{
		enumeration->devices = (dtmd_info_t**) malloc(devices_count * sizeof(dtmd_info_t*));
		if (enumeration->devices == NULL)
		{
			goto device_system_enumerate_devices_error_3;
		}

		for (i = 0 ; i < system->devices_count; ++i)
		{
			enumeration->devices[k] = device_system_copy_device(system->devices[i]->device);
			if (enumeration->devices[k] == NULL)
			{
				goto device_system_enumerate_devices_error_4;
			}

			++k;

			for (j = 0; j < system->devices[i]->partitions_count; ++j)
			{
				enumeration->devices[k] = device_system_copy_device(system->devices[i]->partitions[j]);
				if (enumeration->devices[k] == NULL)
				{
					goto device_system_enumerate_devices_error_4;
				}

				++k;
			}
		}

		for (i = 0; i < system->stateful_devices_count; ++i)
		{
			enumeration->devices[k] = device_system_copy_device(system->stateful_devices[i]);
			if (enumeration->devices[k] == NULL)
			{
				goto device_system_enumerate_devices_error_4;
			}

			++k;
		}
	}
	else
	{
		enumeration->devices = NULL;
	}

	tmp = realloc(system->enumerations, (system->enumeration_count + 1) * sizeof(dtmd_device_enumeration_t*));
	if (tmp == NULL)
	{
		goto device_system_enumerate_devices_error_4;
	}

	system->enumerations = (dtmd_device_enumeration_t**) tmp;
	system->enumerations[system->enumeration_count] = enumeration;
	++(system->enumeration_count);

	pthread_mutex_unlock(&(system->control_mutex));

	return enumeration;

device_system_enumerate_devices_error_4:
	if (enumeration->devices_count > 0)
	{
		for (i = 0; i < k; ++i)
		{
			device_system_free_device(enumeration->devices[i]);
		}

		free(enumeration->devices);
	}

device_system_enumerate_devices_error_3:
	free(enumeration);

device_system_enumerate_devices_error_2:
	pthread_mutex_unlock(&(system->control_mutex));

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
					if (enumeration->system->enumeration_count > 1)
					{
						if (i != (uint32_t)(enumeration->system->enumeration_count - 1))
						{
							enumeration->system->enumerations[i] = enumeration->system->enumerations[enumeration->system->enumeration_count - 1];
						}

						tmp = realloc(enumeration->system->enumerations, (enumeration->system->enumeration_count - 1) * sizeof(dtmd_device_enumeration_t*));
						if (tmp != NULL)
						{
							enumeration->system->enumerations = (dtmd_device_enumeration_t**) tmp;
						}
						else
						{
							enumeration->system->enumerations[enumeration->system->enumeration_count - 1] = NULL;
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
	if ((enumeration == NULL)
		|| (device == NULL))
	{
		return -1;
	}

	if (enumeration->current_device < enumeration->devices_count)
	{
		*device = enumeration->devices[enumeration->current_device];
		++(enumeration->current_device);
		return 1;
	}
	else
	{
		*device = NULL;
		return 0;
	}
}

void device_system_free_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device)
{
	// NOTE: do nothing, it's freed on enumeration free
}

dtmd_device_monitor_t* device_system_start_monitoring(dtmd_device_system_t *system)
{
	dtmd_device_monitor_t *monitor;
	void **tmp;

	if (system == NULL)
	{
		goto device_system_start_monitoring_error_1;
	}

	monitor = (dtmd_device_monitor_t*) malloc(sizeof(dtmd_device_monitor_t));
	if (monitor == NULL)
	{
		goto device_system_start_monitoring_error_1;
	}

	monitor->system = system;

	monitor->first = NULL;
	monitor->last  = NULL;

	if (pipe(monitor->data_pipe) < 0)
	{
		goto device_system_start_monitoring_error_2;
	}

	if (pthread_mutex_lock(&(system->control_mutex)) != 0)
	{
		goto device_system_start_monitoring_error_3;
	}

	tmp = realloc(system->monitors, (system->monitor_count + 1) * sizeof(dtmd_device_monitor_t*));
	if (tmp == NULL)
	{
		goto device_system_start_monitoring_error_4;
	}

	system->monitors = (dtmd_device_monitor_t**) tmp;
	system->monitors[system->monitor_count] = monitor;
	++(system->monitor_count);

	pthread_mutex_unlock(&(system->control_mutex));

	return monitor;

device_system_start_monitoring_error_4:
	pthread_mutex_unlock(&(system->control_mutex));

device_system_start_monitoring_error_3:
	close(monitor->data_pipe[0]);
	close(monitor->data_pipe[1]);

device_system_start_monitoring_error_2:
	free(monitor);

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
			pthread_mutex_lock(&(monitor->system->control_mutex));

			for (i = 0; i < (uint32_t) monitor->system->monitor_count; ++i)
			{
				if (monitor->system->monitors[i] == monitor)
				{
					if (monitor->system->monitor_count > 1)
					{
						if (i != (uint32_t)(monitor->system->monitor_count - 1))
						{
							monitor->system->monitors[i] = monitor->system->monitors[monitor->system->monitor_count - 1];
						}

						tmp = realloc(monitor->system->monitors, (monitor->system->monitor_count - 1) * sizeof(dtmd_device_monitor_t*));
						if (tmp != NULL)
						{
							monitor->system->monitors = (dtmd_device_monitor_t**) tmp;
						}
						else
						{
							monitor->system->monitors[monitor->system->monitor_count - 1] = NULL;
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

			pthread_mutex_unlock(&(monitor->system->control_mutex));
		}

		helper_free_monitor(monitor);
	}
}

int device_system_get_monitor_fd(dtmd_device_monitor_t *monitor)
{
	if (monitor != NULL)
	{
		return monitor->data_pipe[0];
	}
	else
	{
		return -1;
	}
}

int device_system_monitor_get_device(dtmd_device_monitor_t *monitor, dtmd_info_t **device, dtmd_device_action_type_t *action)
{
	char data;
	int rc;
	dtmd_monitor_item_t *delete_item;

	if ((monitor == NULL)
		|| (device == NULL)
		|| (action == NULL))
	{
		goto device_system_monitor_get_device_error_1;
	}

	if (pthread_mutex_lock(&(monitor->system->control_mutex)) != 0)
	{
		goto device_system_monitor_get_device_error_1;
	}

	rc = read(monitor->data_pipe[0], &data, 1);
	if (rc != 1)
	{
		goto device_system_monitor_get_device_error_2;
	}

	switch (data)
	{
	case 0: // error
		/*goto device_system_monitor_get_device_error_2; */
		break;

	case 1: // data
		if (monitor->first != NULL)
		{
			delete_item = monitor->first;

			if (monitor->first != monitor->last)
			{
				monitor->first = monitor->first->next;
			}
			else
			{
				monitor->first = NULL;
				monitor->last  = NULL;
			}

			*device = delete_item->item;
			*action = delete_item->action;
			free(delete_item);

			pthread_mutex_unlock(&(monitor->system->control_mutex));

			return 1;
		}

		// NOTE: passthrough

	case 2: // exit
		*device = NULL;
		*action = dtmd_device_action_unknown;
		pthread_mutex_unlock(&(monitor->system->control_mutex));

		return 0;
	}

device_system_monitor_get_device_error_2:
	pthread_mutex_unlock(&(monitor->system->control_mutex));

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
