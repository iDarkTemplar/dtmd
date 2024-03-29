/*
 * Copyright (C) 2016-2020 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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
#include "daemon/log.h"
#include "daemon/return_codes.h"

#if (defined OS_Linux)
#include <blkid.h>
#endif /* (defined OS_Linux) */

#include <stdlib.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#if (defined OS_Linux)
#define __USE_GNU
#endif /* (defined OS_Linux) */

#include <net/if.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <poll.h>
#include <fcntl.h>

#if (defined OS_Linux)
#include <linux/netlink.h>

#define block_devices_dir "/sys/class/block"
#define block_mmc_devices_dir "/sys/bus/mmc/devices"
#define block_usb_devices_dir "/sys/bus/usb/devices"

#define filename_dev "dev"
#define filename_removable "removable"
#define filename_device_type "device/type"
#define removable_correct_value 1
#define block_sys_dir "/sys"
#define block_dir_name "block"

#define scsi_type_direct_access "0"
#define scsi_type_cd_dvd "5"
#define scsi_type_sd_card "SD"

#define file_read_buffer 4096

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
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
#include <fcntl.h>
#include <sys/un.h>

#include <cam/cam.h>
//#include <cam/cam_debug.h>
//#include <cam/cam_ccb.h>
//#include <cam/scsi/scsi_all.h>
//#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_pass.h>
//#include <cam/scsi/scsi_message.h>
//#include <cam/scsi/smp_all.h>
//#include <cam/ata/ata_all.h>
#include <camlib.h>

#include <libgeom.h>

#define GEOM_CLASS_PART "PART"
#define GEOM_CLASS_LABEL "LABEL"
#define GEOM_CLASS_PART_FSTYPE_PARAM "type"

#define STREAMPIPE "/var/run/devd.pipe"
#define SEQPACKETPIPE "/var/run/devd.seqpacket.pipe"

#define DEVD_SKIP_DEVICE "pass"

#define DEVD_NOTIFICATION_STRING "!"
#define DEVD_SYSTEM_STRING "system="
#define DEVD_SUBSYSTEM_STRING "subsystem="
#define DEVD_TYPE_STRING "type="
#define DEVD_CDEV_STRING "cdev="

#define DEVD_STRING_SYSTEM_DEVFS "DEVFS"
#define DEVD_STRING_SUBSYSTEM_CDEV "CDEV"
#define DEVD_STRING_TYPE_CREATE "CREATE"
#define DEVD_STRING_TYPE_DESTROY "DESTROY"
#define DEVD_STRING_TYPE_MEDIACHANGE "MEDIACHANGE"
#endif /* (defined OS_FreeBSD) */

#define devices_dir "/dev"

#define IFLIST_REPLY_BUFFER 8192

typedef struct dtmd_enumeration_item
{
	dtmd_info_t *item;

	struct dtmd_enumeration_item *next;
} dtmd_enumeration_item_t;

struct dtmd_device_enumeration
{
	dtmd_device_system_t *system;

	dtmd_enumeration_item_t *first;
	dtmd_enumeration_item_t *last;
	dtmd_enumeration_item_t *current;

	struct dtmd_device_enumeration *next;
	struct dtmd_device_enumeration *prev;
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

	struct dtmd_device_monitor *next;
	struct dtmd_device_monitor *prev;
};

struct dtmd_device_system
{
	int events_fd;
	int worker_control_pipe[2];
	pthread_mutex_t control_mutex;
	pthread_t worker_thread;

	dtmd_device_enumeration_t *first_enumeration;
	dtmd_device_enumeration_t *last_enumeration;

	dtmd_device_monitor_t *first_monitor;
	dtmd_device_monitor_t *last_monitor;
};

typedef struct dtmd_info_private
{
	dtmd_device_system_t *system;
	uint32_t counter;
} dtmd_info_private_t;

#if (defined OS_Linux)
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
				WRITE_LOG_ARGS(LOG_ERR, "File '%s' does not contain valid number", filename);
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

static int read_string_from_file(char **resultstr, const char *filename)
{
	FILE *file;
	int result_len = 0;
	int read_val;
	void *tmp;

#ifndef NDEBUG
	if (resultstr == NULL)
	{
		return result_bug;
	}
#endif /* NDEBUG */

	file = fopen(filename, "r");
	if (file == NULL)
	{
		return result_fail;
	}

	*resultstr = (char*) malloc((result_len+1) * sizeof(char));
	if (*resultstr == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto read_string_from_file_error_1;
	}

	while ((read_val = fgetc(file)) != EOF)
	{
		if (!isprint(read_val))
		{
			break;
		}

		tmp = realloc(*resultstr, result_len + 2);
		if (tmp == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto read_string_from_file_error_2;
		}

		*resultstr = (char*) tmp;
		(*resultstr)[result_len++] = read_val;
	}

	fclose(file);
	(*resultstr)[result_len] = 0;

	return result_success;

read_string_from_file_error_2:
	free(*resultstr);
	*resultstr = NULL;

read_string_from_file_error_1:
	fclose(file);

	return result_fatal_error;
}

static dtmd_removable_media_type_t device_subtype_from_string(const char *string)
{
	if (strcmp(string, scsi_type_direct_access) == 0)
	{
		return dtmd_removable_media_subtype_removable_disk;
	}
	else if (strcmp(string, scsi_type_sd_card) == 0)
	{
		return dtmd_removable_media_subtype_sd_card;
	}
	else if (strcmp(string, scsi_type_cd_dvd) == 0)
	{
		return dtmd_removable_media_subtype_cdrom;
	}
	else
	{
		return dtmd_removable_media_subtype_unknown_or_persistent;
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
#if 0
		WRITE_LOG_ARGS(LOG_WARNING, "Failed initializing blkid for device '%s'", partition_name);
#endif /* 0 */
		*fstype = NULL;
		*label  = NULL;
		return result_fail;
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
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto helper_blkid_read_data_from_partition_error_1;
		}
	}

	if (local_label != NULL)
	{
		local_label = strdup(local_label);
		if (local_label == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto helper_blkid_read_data_from_partition_error_2;
		}
	}

	blkid_free_probe(pr);

	*fstype = local_fstype;
	*label  = local_label;

	return result_success;

helper_blkid_read_data_from_partition_error_2:
	if (local_fstype != NULL)
	{
		free((char*) local_fstype);
	}

helper_blkid_read_data_from_partition_error_1:
	blkid_free_probe(pr);

	*fstype = NULL;
	*label  = NULL;

	return result_fatal_error;
}
#endif /* (defined OS_Linux) */

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

#if (defined OS_Linux)
		if (device->sysfs_path != NULL)
		{
			free((char*) device->sysfs_path);
		}
#endif /* (defined OS_Linux) */

		free(device);
	}
}

#if (defined OS_Linux)
static int open_netlink_socket(void)
{
	int fd;
	int on = 1;
	pid_t pid;
	struct sockaddr_nl local;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd == -1)
	{
		WRITE_LOG(LOG_ERR, "Failed opening netlink socket");
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) != 0)
	{
		WRITE_LOG(LOG_ERR, "Failed setting passcred option for netlink socket");
		close(fd);
		return -1;
	}

	pid = (pthread_self() << 16) | getpid();

	memset(&local, 0, sizeof(local));	/* fill-in local address information */
	local.nl_family = AF_NETLINK;
	local.nl_pid    = pid;
	local.nl_groups = NETLINK_KOBJECT_UEVENT;

	if (bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0)
	{
		WRITE_LOG(LOG_ERR, "Failed binding netlink socket");
		close(fd);
		return -1;
	}

	return fd;
}

static int file_check_line(const char *path, const char *line)
{
	char buffer[file_read_buffer + 1];
	size_t buffer_used = 0;
	int line_start = 1;
	ssize_t read_size;
	char *char_ptr;
	int fd;
	int result = result_fail;

	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		return result_fail;
	}

	read_size = read(fd, buffer + buffer_used, file_read_buffer - buffer_used);
	while (read_size > 0)
	{
		buffer_used += read_size;
		buffer[buffer_used] = 0;

		while ((char_ptr = strchr(buffer, '\n')) != NULL)
		{
			if (line_start)
			{
				if ((char_ptr - buffer == strlen(line)) && (strncmp(buffer, line, strlen(line)) == 0))
				{
					result = result_success;
					goto file_check_line_exit;
				}
			}

			buffer_used -= (char_ptr + 1 - buffer);
			memmove(buffer, char_ptr + 1, buffer_used + 1);
			line_start = 1;
		}

		if (buffer_used == file_read_buffer)
		{
			line_start = 0;
			buffer_used = 0;
		}

		read_size = read(fd, buffer + buffer_used, file_read_buffer - buffer_used);
	}

file_check_line_exit:
	close(fd);
	return result;
}

static char*  helper_get_usb_parent_syspath(const char *path)
{
	char file_name[PATH_MAX + 1];
	char link_buffer[PATH_MAX + 1];
	char *result;
	size_t curlen = 0;
	ssize_t link_buffer_size;
	struct stat stat_entry;
	int intresult;

	result = realpath(path, file_name);
	if (result == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed to resolve real path: %s", path);
		return NULL;
	}

	curlen = strlen(file_name);

	if (file_name[curlen - 1] != '/')
	{
		if (curlen + strlen("/") > PATH_MAX)
		{
			WRITE_LOG_ARGS(LOG_ERR, "Path is too long: %s", file_name);
			return NULL;
		}

		strcat(file_name, "/");
		++curlen;
	}

	if (curlen + strlen("subsystem") > PATH_MAX)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Path is too long: %s", file_name);
		return NULL;
	}

	while ((curlen > strlen(block_sys_dir "/")) && (strncmp(file_name, block_sys_dir "/", strlen(block_sys_dir "/")) == 0))
	{
		strcpy(file_name + curlen, "subsystem");

		intresult = lstat(file_name, &stat_entry);
		if (intresult == 0)
		{
			link_buffer_size = readlink(file_name, link_buffer, sizeof(link_buffer) - 1);
			if ((link_buffer_size > strlen("/usb")) && (strncmp(&link_buffer[link_buffer_size - strlen("/usb")], "/usb", strlen("/usb")) == 0))
			{
				strcpy(file_name + curlen, "uevent");

				intresult = file_check_line(file_name, "DEVTYPE=usb_device");
				if (is_result_successful(intresult))
				{
					file_name[curlen - 1] = 0;

					result = strdup(file_name);
					if (result == NULL)
					{
						WRITE_LOG(LOG_ERR, "Memory allocation failure");
					}

					return result;
				}
				else if (is_result_fatal_error(intresult))
				{
					return NULL;
				}
			}
		}

		file_name[curlen - 1] = 0;
		result = strrchr(file_name, '/');

		if (result == NULL)
		{
			break;
		}

		result[1] = 0;
		curlen = strlen(file_name);
	}

	return NULL;
}

static int helper_read_device(char *name, const char *device_name, int check_removable, dtmd_info_t **device)
{
	char *device_type;
	struct stat stat_entry;
	char *start_string;
	int result;

	dtmd_info_t *device_info;
	dtmd_removable_media_subtype_t media_subtype;

	start_string = name + strlen(name);

	strcpy(start_string, filename_dev);

	if (stat(name, &stat_entry) != 0)
	{
		result = result_fail;
		goto helper_read_device_error_1;
	}

	if (check_removable)
	{
		strcpy(start_string, filename_removable);

		if (read_int_from_file(name) != removable_correct_value)
		{
			result = result_fail;
			goto helper_read_device_error_1;
		}
	}

	strcpy(start_string, filename_device_type);
	result = read_string_from_file(&device_type, name);
	if (is_result_failure(result))
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed to get device type from file '%s'", name);
		goto helper_read_device_error_1;
	}

	media_subtype = device_subtype_from_string(device_type);
	free(device_type);

	if (media_subtype == dtmd_removable_media_subtype_unknown_or_persistent)
	{
		result = result_fail;
		goto helper_read_device_error_1;
	}

	device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
	if (device_info == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto helper_read_device_error_1;
	}

	device_info->private_data = NULL;

	device_info->path = (char*) malloc(strlen(devices_dir "/") + strlen(device_name) + 1);
	if (device_info->path == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto helper_read_device_error_2;
	}

	strcpy((char*) device_info->path, devices_dir "/");
	strcat((char*) device_info->path, device_name);

	device_info->media_subtype = media_subtype;
	device_info->path_parent   = strdup(dtmd_root_device_path);
	if (device_info->path_parent == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto helper_read_device_error_3;
	}

	switch (media_subtype)
	{
	case dtmd_removable_media_subtype_removable_disk:
	case dtmd_removable_media_subtype_sd_card:
		start_string[0] = 0;
		device_info->sysfs_path	= helper_get_usb_parent_syspath(name);

		device_info->media_type = dtmd_removable_media_type_stateless_device;
		device_info->fstype     = NULL;
		device_info->label      = NULL;
		device_info->state      = dtmd_removable_media_state_unknown;
		*device                 = device_info;
		return 1;

	case dtmd_removable_media_subtype_cdrom:
		device_info->media_type = dtmd_removable_media_type_stateful_device;
		device_info->sysfs_path = NULL;

		result = helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label));
		switch (result)
		{
		case result_fail:
			device_info->state = dtmd_removable_media_state_empty;
			break;

		case result_success:
			if (device_info->fstype == NULL)
			{
				device_info->state = dtmd_removable_media_state_clear;
			}
			else
			{
				device_info->state = dtmd_removable_media_state_ok;
			}
			break;

		default:
			goto helper_read_device_error_4;
		}

		*device = device_info;
		return 2;

	default:
		result = result_fail;
		break;
	}

helper_read_device_error_4:
	free((char*) device_info->path_parent);

helper_read_device_error_3:
	free((char*) device_info->path);

helper_read_device_error_2:
	free(device_info);

helper_read_device_error_1:
	return result;
}
#endif /* (defined OS_Linux) */

static int enumeration_add_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device)
{
	dtmd_enumeration_item_t *enumeration_item;
	int result;

	for (enumeration_item = enumeration->first; enumeration_item != NULL; enumeration_item = enumeration_item->next)
	{
		if (strcmp(enumeration_item->item->path, device->path) == 0)
		{
			result = result_fail;
			goto enumeration_add_device_error_1;
		}
	}

	enumeration_item = (dtmd_enumeration_item_t*) malloc(sizeof(dtmd_enumeration_item_t));
	if (enumeration_item == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto enumeration_add_device_error_1;
	}

	enumeration_item->item = device;
	enumeration_item->next = NULL;

	if (enumeration->first == NULL)
	{
		enumeration->first = enumeration_item;
		enumeration->current = enumeration->first;
	}

	if (enumeration->last != NULL)
	{
		enumeration->last->next = enumeration_item;
	}

	enumeration->last = enumeration_item;

	return result_success;

enumeration_add_device_error_1:
	device_system_free_device(device);

	return result;
}

#if (defined OS_Linux)
static int helper_read_device_partitions(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device, const char *device_name)
{
	blkid_probe pr;
	blkid_partlist ls;
	blkid_partition par;
	int nparts, i, index, string_len;
	char *string;
	struct stat stat_entry;
	dtmd_info_t *device_info;
	int result;

	pr = blkid_new_probe_from_filename(device->path);
	if (pr == NULL)
	{
		result = result_fail;
		goto helper_read_device_partitions_error_1;
	}

	ls = blkid_probe_get_partitions(pr);
	nparts = blkid_partlist_numof_partitions(ls);

	for (i = 0; i < nparts; ++i)
	{
		par = blkid_partlist_get_partition(ls, i);
		index = blkid_partition_get_partno(par);

		string_len = snprintf(NULL, 0, devices_dir "/%s%d", device_name, index);
		if (string_len < 0)
		{
			result = result_fatal_error;
			goto helper_read_device_partitions_error_2;
		}

		string = (char*) malloc(string_len+1);
		if (string == NULL)
		{
			result = result_fatal_error;
			goto helper_read_device_partitions_error_2;
		}

		result = snprintf(string, string_len + 1, devices_dir "/%s%d", device_name, index);
		if (result != string_len)
		{
			result = result_fatal_error;
			goto helper_read_device_partitions_error_3;
		}

		if (stat(string, &stat_entry) != 0)
		{
			result = result_fail;
			goto helper_read_device_partitions_error_3;
		}

		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto helper_read_device_partitions_error_3;
		}

		device_info->private_data = NULL;

		device_info->path = string;
		device_info->path_parent = strdup(device->path);
		if (device_info->path_parent == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto helper_read_device_partitions_error_4;
		}

		device_info->sysfs_path    = NULL;
		device_info->media_type    = dtmd_removable_media_type_device_partition;
		device_info->media_subtype = device->media_subtype;
		device_info->state         = dtmd_removable_media_state_unknown;

		result = helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label));
		if (is_result_fatal_error(result))
		{
			goto helper_read_device_partitions_error_5;
		}

		result = enumeration_add_device(enumeration, device_info);
		if (is_result_fatal_error(result))
		{
			goto helper_read_device_partitions_error_2;
		}
	}

	blkid_free_probe(pr);

	return result_success;

helper_read_device_partitions_error_5:
	if (device_info->fstype != NULL)
	{
		free((char*) device_info->fstype);
	}

	if (device_info->label != NULL)
	{
		free((char*) device_info->label);
	}

	free((char*) device_info->path_parent);

helper_read_device_partitions_error_4:
	free(device_info);

helper_read_device_partitions_error_3:
	free(string);

helper_read_device_partitions_error_2:
	blkid_free_probe(pr);

helper_read_device_partitions_error_1:
	return result;
}

static int device_system_run_device_enumeration(dtmd_device_enumeration_t *enumeration)
{
	DIR *dir_pointer = NULL;
	struct dirent *dirent_device = NULL;

	DIR *dir_pointer_usb = NULL;
	DIR *dir_pointer_usb_device = NULL;
	DIR *dir_pointer_usb_host = NULL;
	DIR *dir_pointer_usb_target = NULL;
	DIR *dir_pointer_usb_target_device = NULL;

	struct dirent *dirent_usb = NULL;
	struct dirent *dirent_usb_device = NULL;
	struct dirent *dirent_usb_host = NULL;
	struct dirent *dirent_usb_target = NULL;
	struct dirent *dirent_usb_target_device = NULL;

	DIR *dir_pointer_mmc = NULL;
	DIR *dir_pointer_mmc_device = NULL;

	struct dirent *dirent_mmc = NULL;
	struct dirent *dirent_mmc_device = NULL;

	dtmd_info_t *device;

	int result;

	char file_name[PATH_MAX + 1];
	size_t len_core;
	size_t len_base;
	size_t len_ext;
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
	if (dir_pointer != NULL)
	{
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
				//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
				continue;
			}

			strcpy(file_name, block_devices_dir "/");
			strcat(file_name, dirent_device->d_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			strcat(file_name, "/");

			result = helper_read_device(file_name, dirent_device->d_name, 1, &device);
			switch (result)
			{
			case 1: // device
				result = enumeration_add_device(enumeration, device);
				switch (result)
				{
				case result_success: // ok
					result = helper_read_device_partitions(enumeration, device, dirent_device->d_name);
					switch (result)
					{
					case result_success: // ok
						break;

					case result_fail: // ok
						break;

					default: // error
						goto device_system_run_device_enumeration_error_plain_1;
						//break;
					}
					break;

				case result_fail: // ok
					break;

				default: // error
					goto device_system_run_device_enumeration_error_plain_1;
					//break;
				}
				break;

			case 2: // stateful_device
				result = enumeration_add_device(enumeration, device);
				switch (result)
				{
				case result_success: // ok
					break;

				case result_fail: //ok
					break;

				default: // error
					goto device_system_run_device_enumeration_error_plain_1;
					//break;
				}
				break;

			case result_fail:
				break;

			default:
				goto device_system_run_device_enumeration_error_plain_1;
				//break;
			}
		}

		closedir(dir_pointer);
		dir_pointer = NULL;
	}
	else
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", block_devices_dir);
	}

	dir_pointer_usb = opendir(block_usb_devices_dir);
	if (dir_pointer_usb != NULL)
	{
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
				//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
				continue;
			}

			strcpy(file_name, block_usb_devices_dir "/");
			strcat(file_name, dirent_usb->d_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			dir_pointer_usb_device = opendir(file_name);
			if (dir_pointer_usb_device != NULL)
			{
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
						//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
						continue;
					}

					strcpy(file_name, block_usb_devices_dir "/");
					strcat(file_name, dirent_usb->d_name);
					strcat(file_name, "/");
					strcat(file_name, dirent_usb_device->d_name);

					if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
					{
						continue;
					}

					dir_pointer_usb_host = opendir(file_name);
					if (dir_pointer_usb_host != NULL)
					{
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
								//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
								continue;
							}

							strcpy(file_name, block_usb_devices_dir "/");
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
							if (dir_pointer_usb_target != NULL)
							{
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
										//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
										continue;
									}

									strcpy(file_name, block_usb_devices_dir "/");
									strcat(file_name, dirent_usb->d_name);
									strcat(file_name, "/");
									strcat(file_name, dirent_usb_device->d_name);
									strcat(file_name, "/");
									strcat(file_name, dirent_usb_host->d_name);
									strcat(file_name, "/");
									strcat(file_name, dirent_usb_target->d_name);
									strcat(file_name, "/" block_dir_name);

									if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
									{
										continue;
									}

									dir_pointer_usb_target_device = opendir(file_name);
									if (dir_pointer_usb_target_device != NULL)
									{
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
												//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
												continue;
											}

											strcpy(file_name, block_usb_devices_dir "/");
											strcat(file_name, dirent_usb->d_name);
											strcat(file_name, "/");
											strcat(file_name, dirent_usb_device->d_name);
											strcat(file_name, "/");
											strcat(file_name, dirent_usb_host->d_name);
											strcat(file_name, "/");
											strcat(file_name, dirent_usb_target->d_name);
											strcat(file_name, "/" block_dir_name "/");
											strcat(file_name, dirent_usb_target_device->d_name);

											if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
											{
												continue;
											}

											strcat(file_name, "/");

											result = helper_read_device(file_name, dirent_usb_target_device->d_name, 0, &device);
											switch (result)
											{
											case 1: // device
												result = enumeration_add_device(enumeration, device);
												switch (result)
												{
												case result_success: // ok
													result = helper_read_device_partitions(enumeration, device, dirent_usb_target_device->d_name);
													switch (result)
													{
													case result_success: // ok
														break;

													case result_fail: // ok
														break;

													default: // error
														goto device_system_run_device_enumeration_error_usb_2;
														//break;
													}
													break;

												case result_fail: // ok
													break;

												default: // error
													goto device_system_run_device_enumeration_error_usb_2;
													//break;
												}
												break;

											case 2: // stateful_device
												result = enumeration_add_device(enumeration, device);
												switch (result)
												{
												case result_success: // ok
													break;

												case result_fail: //ok
													break;

												default: // error
													goto device_system_run_device_enumeration_error_usb_2;
													//break;
												}
												break;

											case result_fail:
												break;

											default:
												goto device_system_run_device_enumeration_error_plain_1;
												//break;
											}
										}

										closedir(dir_pointer_usb_target_device);
										dir_pointer_usb_target_device = NULL;
									}
									else
									{
										WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", file_name);
									}
								}

								closedir(dir_pointer_usb_target);
								dir_pointer_usb_target = NULL;
							}
							else
							{
								WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", file_name);
							}
						}

						closedir(dir_pointer_usb_host);
						dir_pointer_usb_host = NULL;
					}
					else
					{
						WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", file_name);
					}
				}

				closedir(dir_pointer_usb_device);
				dir_pointer_usb_device = NULL;
			}
			else
			{
				WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", file_name);
			}
		}

		closedir(dir_pointer_usb);
		dir_pointer_usb = NULL;
	}
	else
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", block_usb_devices_dir);
	}

	dir_pointer_mmc = opendir(block_mmc_devices_dir);
	if (dir_pointer_mmc != NULL)
	{
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
				//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
				continue;
			}

			strcpy(file_name, block_mmc_devices_dir "/");
			strcat(file_name, dirent_mmc->d_name);
			strcat(file_name, "/" block_dir_name);

			if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
			{
				continue;
			}

			dir_pointer_mmc_device = opendir(file_name);
			if (dir_pointer_mmc_device != NULL)
			{
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
						//WRITE_LOG(LOG_WARNING, "Error: got too long file name");
						continue;
					}

					strcpy(file_name, block_mmc_devices_dir);
					strcat(file_name, "/");
					strcat(file_name, dirent_mmc->d_name);
					strcat(file_name, "/" block_dir_name "/");
					strcat(file_name, dirent_mmc_device->d_name);

					if ((stat(file_name, &statbuf) != 0) || (!S_ISDIR(statbuf.st_mode)))
					{
						continue;
					}

					strcat(file_name, "/");

					result = helper_read_device(file_name, dirent_mmc_device->d_name, 0, &device);
					switch (result)
					{
					case 1: // device
						result = enumeration_add_device(enumeration, device);
						switch (result)
						{
						case result_success: // ok
							result = helper_read_device_partitions(enumeration, device, dirent_mmc_device->d_name);
							switch (result)
							{
							case result_success: // ok
								break;

							case result_fail: // ok
								break;

							default: // error
								goto device_system_run_device_enumeration_error_mmc_2;
								//break;
							}
							break;

						case result_fail: // ok
							break;

						default: // error
							goto device_system_run_device_enumeration_error_mmc_2;
							//break;
						}
						break;

					case 2: // stateful_device
						result = enumeration_add_device(enumeration, device);
						switch (result)
						{
						case result_success: // ok
							break;

						case result_fail: //ok
							break;

						default: // error
							goto device_system_run_device_enumeration_error_mmc_2;
							//break;
						}
						break;

					case result_fail:
						break;

					default:
						goto device_system_run_device_enumeration_error_mmc_2;
						//break;
					}
				}

				closedir(dir_pointer_mmc_device);
				dir_pointer_mmc_device = NULL;
			}
			else
			{
				WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", file_name);
			}
		}

		closedir(dir_pointer_mmc);
		dir_pointer_mmc = NULL;
	}
	else
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to open directory '%s'", block_mmc_devices_dir);
	}

	return result_success;

device_system_run_device_enumeration_error_mmc_2:
	closedir(dir_pointer_mmc_device);
	closedir(dir_pointer_mmc);

	goto device_system_run_device_enumeration_error_mmc_1;

device_system_run_device_enumeration_error_usb_2:
	closedir(dir_pointer_usb_target_device);
	closedir(dir_pointer_usb_target);
	closedir(dir_pointer_usb_host);
	closedir(dir_pointer_usb_device);
	closedir(dir_pointer_usb);

	goto device_system_run_device_enumeration_error_usb_1;

device_system_run_device_enumeration_error_plain_1:
	closedir(dir_pointer);

device_system_run_device_enumeration_error_mmc_1:
device_system_run_device_enumeration_error_usb_1:
	return result;
}
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
static dtmd_removable_media_subtype_t helper_get_device_subtype_from_data(const struct ata_params *ident_data)
{
	switch (ident_data->config & ATA_ATAPI_TYPE_MASK)
	{
	case ATA_ATAPI_TYPE_CDROM:
		return dtmd_removable_media_subtype_cdrom;

	case ATA_ATAPI_TYPE_DIRECT:
		return dtmd_removable_media_subtype_removable_disk;

	default:
		// TODO: differ between different media type, removable disks and cards
		return dtmd_removable_media_subtype_unknown_or_persistent;
	}
}

static int helper_read_device(const struct device_match_result *dev_result, const char *device_name, dtmd_info_t **device)
{
	int result;
	dtmd_info_t *device_info;

	device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
	if (device_info == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto helper_read_device_error_1;
	}

	device_info->private_data = NULL;

	device_info->path = (char*) malloc(strlen(devices_dir "/")  + strlen(device_name) + 1);
	if (device_info->path == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto helper_read_device_error_2;
	}

	strcpy((char*) device_info->path, devices_dir "/");
	strcat((char*) device_info->path, device_name);

	device_info->media_subtype  = helper_get_device_subtype_from_data(&(dev_result->ident_data));
	device_info->path_parent = strdup(dtmd_root_device_path);
	if (device_info->path_parent == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto helper_read_device_error_3;
	}

	switch (device_info->media_subtype)
	{
	case dtmd_removable_media_subtype_removable_disk:
	case dtmd_removable_media_subtype_sd_card:
		device_info->media_type = dtmd_removable_media_type_stateless_device;
		device_info->fstype     = NULL;
		device_info->label      = NULL;
		device_info->state      = dtmd_removable_media_state_unknown;
		*device                 = device_info;
		return 1;

	case dtmd_removable_media_subtype_cdrom:
		device_info->media_type = dtmd_removable_media_type_stateful_device;

		/*
		result = helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label));
		switch (result)
		{
		case result_fail:
			device_info->state = dtmd_removable_media_state_empty;
			break;

		case result_success:
			if (device_info->fstype == NULL)
			{
				device_info->state = dtmd_removable_media_state_clear;
			}
			else
			{
				device_info->state = dtmd_removable_media_state_ok;
			}
			break;

		default:
			goto helper_read_device_error_3;
		}
		*/
		// TODO: right definition of CD-ROM state
		device_info->fstype = NULL;
		device_info->label  = NULL;
		device_info->state  = dtmd_removable_media_state_unknown;

		*device = device_info;
		return 2;

	default:
		result = result_fail;
		break;
	}

/*
helper_read_device_error_4:
*/
	free((char*) device_info->path_parent);

helper_read_device_error_3:
	free((char*) device_info->path);

helper_read_device_error_2:
	free(device_info);

helper_read_device_error_1:
	return result;
}

static struct gclass* find_class(struct gmesh *mesh, const char *name)
{
	struct gclass *classp;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class)
	{
		if (strcmp(classp->lg_name, name) == 0)
		{
			return (classp);
		}
	}

	return (NULL);
}

static struct ggeom* find_geom(struct gclass *classp, const char *name)
{
	struct ggeom *gp;

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom)
	{
		if (strcmp(gp->lg_name, name) == 0)
		{
			return (gp);
		}
	}

	return (NULL);
}

static struct gconfig* find_config_param(struct gprovider *pp, const char *name)
{
	struct gconfig *conf;

	LIST_FOREACH(conf, &pp->lg_config, lg_config)
	{
		if (strcmp(conf->lg_name, name) == 0)
		{
			return (conf);
		}
	}

	return (NULL);
}

static int helper_read_device_partitions(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device, const char *device_name)
{
	struct gmesh mesh;
	struct gclass *classp_part, *classp_label;
	struct ggeom *gp_part, *gp_label;
	struct gprovider *pp_part;
	struct gconfig *conf;
	int result;
	char *string;
	char *raw_label;
	struct stat stat_entry;
	dtmd_info_t *device_info;

	result = geom_gettree(&mesh);
	if (result != 0)
	{
		WRITE_LOG(LOG_WARNING, "Failed to get geom tree");
		result = result_fatal_error;
		goto helper_read_device_partitions_error_1;
	}

	classp_part = find_class(&mesh, GEOM_CLASS_PART);
	if (classp_part == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Geom class \"%s\" not found", GEOM_CLASS_PART);
		result = result_fatal_error;
		goto helper_read_device_partitions_error_2;
	}

	classp_label = find_class(&mesh, GEOM_CLASS_LABEL);
	if (classp_label == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Geom class \"%s\" not found", GEOM_CLASS_LABEL);
		result = result_fatal_error;
		goto helper_read_device_partitions_error_2;
	}

	gp_part = find_geom(classp_part, device_name);
	if (gp_part == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "No such geom found: %s", device_name);
		result = result_fatal_error;
		goto helper_read_device_partitions_error_2;
	}

	result = 0;
	LIST_FOREACH(pp_part, &gp_part->lg_provider, lg_provider)
	{
		++result;
	}

	LIST_FOREACH(pp_part, &gp_part->lg_provider, lg_provider)
	{
		string = (char*) malloc(strlen(devices_dir "/") + strlen(pp_part->lg_name) + 1);
		if (string == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto helper_read_device_partitions_error_2;
		}

		strcpy(string, devices_dir "/");
		strcat(string, pp_part->lg_name);

		if (stat(string, &stat_entry) != 0)
		{
			WRITE_LOG_ARGS(LOG_WARNING, "Device node doesn't exist: %s", string);
			result = result_fail;
			goto helper_read_device_partitions_error_3;
		}

		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto helper_read_device_partitions_error_3;
		}

		device_info->private_data = NULL;
		device_info->path = string;
		device_info->path_parent = strdup(device->path);
		if (device_info->path_parent == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto helper_read_device_partitions_error_4;
		}

		device_info->media_type    = dtmd_removable_media_type_device_partition;
		device_info->media_subtype = device->media_subtype;
		device_info->state         = dtmd_removable_media_state_unknown;

		device_info->fstype = NULL;
		device_info->label  = NULL;

		conf = find_config_param(pp_part, GEOM_CLASS_PART_FSTYPE_PARAM);
		if (conf != NULL)
		{
			// TODO: check that filesystem actually exists
			device_info->fstype = strdup(conf->lg_val);
			if (device_info->fstype == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				result = result_fatal_error;
				goto helper_read_device_partitions_error_5;
			}

			gp_label = find_geom(classp_label, pp_part->lg_name);
			if ((gp_label != NULL) && (! LIST_EMPTY(&gp_label->lg_provider)))
			{
				raw_label = strchr(LIST_FIRST(&gp_label->lg_provider)->lg_name, '/');
				if (raw_label == NULL)
				{
					raw_label = LIST_FIRST(&gp_label->lg_provider)->lg_name;
				}

				device_info->label = strdup(raw_label);
				if (device_info->label == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					result = result_fatal_error;
					goto helper_read_device_partitions_error_6;
				}
			}
		}

		result = enumeration_add_device(enumeration, device_info);
		if (is_result_fatal_error(result))
		{
			goto helper_read_device_partitions_error_2;
		}
	}

	geom_deletetree(&mesh);

	return result_success;

/*
helper_read_device_partitions_error_7:
	free((char*) device_info->label);
*/

helper_read_device_partitions_error_6:
	free((char*) device_info->fstype);

helper_read_device_partitions_error_5:
	free((char*) device_info->path_parent);

helper_read_device_partitions_error_4:
	free(device_info);

helper_read_device_partitions_error_3:
	free(string);

helper_read_device_partitions_error_2:
	geom_deletetree(&mesh);

helper_read_device_partitions_error_1:
	return result;
}

static int device_system_run_device_enumeration(dtmd_device_enumeration_t *enumeration)
{
	union ccb ccb;
	int bufsize, fd;
	uint32_t i;
	int result;
	int skip_device = 1;
	struct device_match_result *dev_result;
	struct periph_match_result *periph_result;
	dtmd_info_t *device;
	char *device_name = NULL;

	if ((fd = open(XPT_DEVICE, O_RDWR)) == -1)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to open device '%s'", XPT_DEVICE);
		result = result_fatal_error;
		goto device_system_run_device_enumeration_error_1;
	}

	bzero(&ccb, sizeof(union ccb));

	ccb.ccb_h.path_id    = CAM_XPT_PATH_ID;
	ccb.ccb_h.target_id  = CAM_TARGET_WILDCARD;
	ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;

	ccb.ccb_h.func_code = XPT_DEV_MATCH;
	bufsize = sizeof(struct dev_match_result) * 100;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = (struct dev_match_result*) malloc(bufsize);
	if (ccb.cdm.matches == NULL)
	{
		WRITE_LOG(LOG_WARNING, "Failed to allocate memory");
		result = result_fatal_error;
		goto device_system_run_device_enumeration_error_2;
	}

	ccb.cdm.num_matches = 0;
	// Fetch all nodes
	ccb.cdm.num_patterns = 0;
	ccb.cdm.pattern_buf_len = 0;

	/*
	 * We do the ioctl multiple times if necessary, in case there are
	 * more than 100 nodes in the EDT.
	 */
	do
	{
		if (ioctl(fd, CAMIOCOMMAND, &ccb) < 0)
		{
			WRITE_LOG(LOG_WARNING, "Failed to send CAMIOCOMMAND ioctl");
			result = result_fatal_error;
			break;
		}

		if ((ccb.ccb_h.status != CAM_REQ_CMP)
				|| ((ccb.cdm.status != CAM_DEV_MATCH_LAST)
					&& (ccb.cdm.status != CAM_DEV_MATCH_MORE)))
		{
			WRITE_LOG_ARGS(LOG_WARNING, "Got CAM error %x, CDM error %d\n", ccb.ccb_h.status, ccb.cdm.status);
			result = result_fatal_error;
			break;
		}

		dev_result = NULL;

		for (i = 0; i < ccb.cdm.num_matches; ++i)
		{
			switch (ccb.cdm.matches[i].type)
			{
			case DEV_MATCH_DEVICE:
				dev_result = &ccb.cdm.matches[i].result.device_result;

				if ((dev_result->flags & DEV_RESULT_UNCONFIGURED)
					|| (! SID_IS_REMOVABLE(&(dev_result->inq_data)))
					/* NOTE: currently skip cd-roms due to missing notifications on disk insertion */
					|| ((dev_result->ident_data.config & ATA_ATAPI_TYPE_MASK) != ATA_ATAPI_TYPE_DIRECT))
				{
					skip_device = 1;
					break;
				}
				else
				{
					skip_device = 0;
				}
				break;

			case DEV_MATCH_PERIPH:
				periph_result = &ccb.cdm.matches[i].result.periph_result;

				if ((skip_device != 0)
					|| (strcmp(periph_result->periph_name, "pass") == 0)
					|| (dev_result == NULL))
				{
					break;
				}

				result = snprintf(NULL, 0, "%s%d", periph_result->periph_name, periph_result->unit_number);
				if (result < 0)
				{
					WRITE_LOG(LOG_WARNING, "Length calculation error");
					result = result_fatal_error;
					goto device_system_run_device_enumeration_error_3;
				}

				device_name = (char*) malloc(result + 1);
				if (device_name == NULL)
				{
					WRITE_LOG(LOG_WARNING, "Memory allocation failure");
					result = result_fatal_error;
					goto device_system_run_device_enumeration_error_3;
				}

				result = snprintf(device_name, result + 1, "%s%d", periph_result->periph_name, periph_result->unit_number);
				if (result < 0)
				{
					WRITE_LOG(LOG_WARNING, "Name calculation error");
					result = result_fatal_error;
					goto device_system_run_device_enumeration_error_4;
				}

				result = helper_read_device(dev_result, device_name, &device);
				switch (result)
				{
				case 1: // device
					result = enumeration_add_device(enumeration, device);
					switch (result)
					{
					case result_success: // ok
						result = helper_read_device_partitions(enumeration, device, device_name);
						switch (result)
						{
						case result_success: // ok
							break;

						case result_fail: // ok
							break;

						default: // error
							goto device_system_run_device_enumeration_error_4;
							//break;
						}
						break;

					case result_fail: // ok
						break;

					default: // error
						goto device_system_run_device_enumeration_error_4;
						//break;
					}
					break;

				case 2: // stateful_device
					result = enumeration_add_device(enumeration, device);
					switch (result)
					{
					case result_success: // ok
						break;

					case result_fail: //ok
						break;

					default: // error
						goto device_system_run_device_enumeration_error_4;
						//break;
					}
					break;

				case result_fail:
					break;

				default:
					goto device_system_run_device_enumeration_error_4;
					//break;
				}

				free(device_name);
				device_name = NULL;

				skip_device = 1; // Use first good name of device, skip other.
				break;

			case DEV_MATCH_BUS:
			default:
				break;
			}
		}
	} while ((ccb.ccb_h.status == CAM_REQ_CMP) && (ccb.cdm.status == CAM_DEV_MATCH_MORE));

	result = result_success;

device_system_run_device_enumeration_error_4:
	if (device_name != NULL)
	{
		free(device_name);
	}

device_system_run_device_enumeration_error_3:
	free(ccb.cdm.matches);

device_system_run_device_enumeration_error_2:
	close(fd);

device_system_run_device_enumeration_error_1:
	return result;
}
#endif /* (defined OS_FreeBSD) */

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

#if (defined OS_Linux)
static int device_system_monitor_receive_device(int fd, dtmd_info_t **device, dtmd_device_action_type_t *action)
{
	struct sockaddr_nl kernel;
	struct iovec io;
	char cred_msg[CMSG_SPACE(sizeof(struct ucred))];
	char reply[IFLIST_REPLY_BUFFER];
	ssize_t len;
	struct msghdr rtnl_reply;

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
	int result;

	memset(&kernel, 0, sizeof(kernel));
	memset(&rtnl_reply, 0, sizeof(rtnl_reply));

	kernel.nl_family = AF_NETLINK;
	io.iov_base = reply;
	io.iov_len = IFLIST_REPLY_BUFFER - 1;
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
			result = result_fail;
			goto device_system_monitor_receive_device_exit_1;
		}

		cmsg = CMSG_FIRSTHDR(&rtnl_reply);
		if ((cmsg == NULL)|| (cmsg->cmsg_type != SCM_CREDENTIALS))
		{
			result = result_fail;
			goto device_system_monitor_receive_device_exit_1;
		}

		cred = (struct ucred*) CMSG_DATA(cmsg);

		if ((cred->pid != 0)
			|| (cred->uid != 0)
			|| (cred->gid != 0))
		{
			result = result_fail;
			goto device_system_monitor_receive_device_exit_1;
		}

		for (pos = 0; pos < len; pos += strlen(&(reply[pos])) + 1)
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
			result = result_fail;
			goto device_system_monitor_receive_device_exit_1;
		}

		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto device_system_monitor_receive_device_exit_1;
		}

		switch (action_type)
		{
		case dtmd_device_action_add:
		case dtmd_device_action_online:
		case dtmd_device_action_change:
			if (strlen(block_sys_dir) + strlen(devpath) + strlen(filename_device_type) + 4 > PATH_MAX)
			{
				WRITE_LOG(LOG_WARNING, "Error: got too long file name");
				result = result_fail;
				goto device_system_monitor_receive_device_exit_2;
			}

			strcpy(file_name, block_sys_dir);
			strcat(file_name, devpath);
			strcat(file_name, "/" filename_device_type);

			result = read_string_from_file(&device_type, file_name);
			if (is_result_failure(result))
			{
				strcpy(&(file_name[strlen(block_sys_dir) + strlen(devpath)]), "/../" filename_device_type);
				result = read_string_from_file(&device_type, file_name);
			}

			if (is_result_successful(result))
			{
				device_info->media_subtype = device_subtype_from_string(device_type);
				free(device_type);

				if (strcmp(devtype, NETLINK_STRING_DEVTYPE_DISK) == 0)
				{
					switch (device_info->media_subtype)
					{
					case dtmd_removable_media_subtype_removable_disk:
					case dtmd_removable_media_subtype_sd_card:
						device_info->media_type = dtmd_removable_media_type_stateless_device;
						break;

					case dtmd_removable_media_subtype_cdrom:
						device_info->media_type = dtmd_removable_media_type_stateful_device;
						break;

					default:
						device_info->media_type = dtmd_removable_media_type_unknown_or_persistent;
						break;
					}

					strcpy(file_name, block_sys_dir);
					strcat(file_name, devpath);
					device_info->sysfs_path = helper_get_usb_parent_syspath(file_name);
				}
				else
				{
					device_info->media_type = dtmd_removable_media_type_device_partition;
					device_info->sysfs_path = NULL;
				}
			}
			else
			{
				if (action_type == dtmd_device_action_change)
				{
					device_info->media_subtype = dtmd_removable_media_subtype_unknown_or_persistent;
					device_info->media_type = dtmd_removable_media_type_unknown_or_persistent;
					device_info->sysfs_path = NULL;
				}
				else
				{
					/* NOTE: disabled log here because it writes about devices which should be just ignored
					WRITE_LOG_ARGS(LOG_WARNING, "Failed to get device type from file '%s'", file_name);
					*/
					result = result_fail;
					goto device_system_monitor_receive_device_exit_2;
				}
			}
			break;

		case dtmd_device_action_remove:
		case dtmd_device_action_offline:
			if (strcmp(devtype, NETLINK_STRING_DEVTYPE_DISK) == 0)
			{
				device_info->media_type = dtmd_removable_media_type_unknown_or_persistent;
			}
			else
			{
				device_info->media_type = dtmd_removable_media_type_device_partition;
			}

			device_info->media_subtype = dtmd_removable_media_subtype_unknown_or_persistent;
			device_info->sysfs_path = NULL;
			break;
		}

		device_info->path = (char*) malloc(strlen(devices_dir) + strlen(devname) + 2);
		if (device_info->path == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto device_system_monitor_receive_device_exit_2;
		}

		strcpy((char*) device_info->path, devices_dir "/");
		strcat((char*) device_info->path, devname);

		switch (action_type)
		{
		case dtmd_device_action_add:
		case dtmd_device_action_online:
		case dtmd_device_action_change:
			switch (device_info->media_type)
			{
			case dtmd_removable_media_type_device_partition:
				device_info->state = dtmd_removable_media_state_unknown;

				result = helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label));
				if (is_result_fatal_error(result))
				{
					goto device_system_monitor_receive_device_exit_3;
				}
				break;

			case dtmd_removable_media_type_stateful_device:
				result = helper_blkid_read_data_from_partition(device_info->path, &(device_info->fstype), &(device_info->label));
				switch (result)
				{
				case result_fail:
					device_info->state = dtmd_removable_media_state_empty;
					break;

				case result_success:
					if (device_info->fstype == NULL)
					{
						device_info->state = dtmd_removable_media_state_clear;
					}
					else
					{
						device_info->state = dtmd_removable_media_state_ok;
					}
					break;

				default:
					goto device_system_monitor_receive_device_exit_3;
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

		switch (device_info->media_type)
		{
		case dtmd_removable_media_type_device_partition:
			last_delim = strrchr(devpath, '/');
			if (last_delim == NULL)
			{
				WRITE_LOG(LOG_ERR, "Invalid device path");
				result = result_fail;
				goto device_system_monitor_receive_device_exit_3;
			}

			*last_delim = 0;
			last_delim = strrchr(devpath, '/');
			if (last_delim == NULL)
			{
				WRITE_LOG(LOG_ERR, "Invalid device path");
				result = result_fail;
				goto device_system_monitor_receive_device_exit_3;
			}

			device_info->path_parent = (char*) malloc(strlen(devices_dir) + strlen(last_delim) + 1);
			if (device_info->path_parent == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				result = result_fatal_error;
				goto device_system_monitor_receive_device_exit_3;
			}

			strcpy((char*) device_info->path_parent, devices_dir);
			strcat((char*) device_info->path_parent, last_delim);
			break;

		default:
			device_info->path_parent = strdup(dtmd_root_device_path);
			if (device_info->path_parent == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				result = result_fatal_error;
				goto device_system_monitor_receive_device_exit_3;
			}
			break;
		}

		device_info->private_data = NULL;

		*device = device_info;
		*action = action_type;
		return result_success;
	}
	else if (len < 0)
	{
		WRITE_LOG(LOG_ERR, "Failed to receive data from netlink socket");
		result = result_fatal_error;
		goto device_system_monitor_receive_device_exit_1;
	}

	return result_fail;

/*
device_system_monitor_receive_device_exit_4:
	if (device_info->path_parent != NULL)
	{
		free((char*) device_info->path_parent);
	}
*/

device_system_monitor_receive_device_exit_3:
	if (device_info->fstype != NULL)
	{
		free((char*) device_info->fstype);
	}

	if (device_info->label != NULL)
	{
		free((char*) device_info->label);
	}

	free((char*) device_info->path);

device_system_monitor_receive_device_exit_2:
	free(device_info);

device_system_monitor_receive_device_exit_1:
	return result;
}
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
static int open_devd_socket(void)
{
	int fd;
	struct sockaddr_un sockaddr;

	fd = socket(AF_LOCAL, SOCK_SEQPACKET, 0);
	if (fd == -1)
	{
		WRITE_LOG(LOG_ERR, "Failed opening local socket");
		return -1;
	}

	memset(&sockaddr, 0, sizeof(struct sockaddr_un));
	sockaddr.sun_family = AF_LOCAL;
	strncpy(sockaddr.sun_path, SEQPACKETPIPE, sizeof(sockaddr.sun_path) - 1);

	if (connect(fd, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_un)) == -1)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed connecting to %s", SEQPACKETPIPE);
		close(fd);
		return -1;
	}

	return fd;
}

static int helper_get_device_subtype(const char *device_name, dtmd_removable_media_subtype_t *media_subtype)
{
	int result;
	struct cam_device *cgd_device;
	union ccb *ccbu;

	cgd_device = cam_open_device(device_name, O_RDWR);
	if (cgd_device == NULL)
	{
		result = result_fail;
		goto helper_get_device_subtype_exit_1;
	}

	ccbu = cam_getccb(cgd_device);
	if (ccbu == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Error: can't get cam ccb for device %s", device_name);
		result = result_fatal_error;
		goto helper_get_device_subtype_error_1;
	}

	memset(ccbu + sizeof(struct ccb_hdr), 0, sizeof(struct ccb_pathinq) - sizeof(struct ccb_hdr));
	ccbu->ccb_h.func_code = XPT_GDEV_TYPE;

	if (cam_send_ccb(cgd_device, ccbu) < 0)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Error: can't send cam ccb for device %s", device_name);
		result = result_fatal_error;
		goto helper_get_device_subtype_error_2;
	}

	if ((ccbu->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Error: received invalid ccb status for device %s: %x", device_name, ccbu->ccb_h.status & CAM_STATUS_MASK);
		result = result_fatal_error;
		goto helper_get_device_subtype_error_2;
	}

	*media_subtype = helper_get_device_subtype_from_data(&(ccbu->cgd.ident_data));
	result = result_success;

helper_get_device_subtype_error_2:
	cam_freeccb(ccbu);

helper_get_device_subtype_error_1:
	cam_close_device(cgd_device);

helper_get_device_subtype_exit_1:
	return result;
}

static int device_system_monitor_receive_device(int fd, dtmd_info_t **device, dtmd_device_action_type_t *action)
{
	char reply[IFLIST_REPLY_BUFFER];
	ssize_t len, pos;
	int result;

	struct gmesh mesh;
	struct gclass *classp_part, *classp_label;
	struct ggeom *gp_part, *gp_label;
	struct gprovider *pp_part;
	struct gconfig *conf;
	char *raw_label;

	dtmd_removable_media_subtype_t media_subtype;
	dtmd_info_t *device_info;
	dtmd_device_action_type_t action_type = dtmd_device_action_unknown;
	char *devname = NULL;
	char *devd_system = NULL;
	char *devd_subsystem = NULL;

	len = recv(fd, reply, sizeof(reply) - 1, MSG_WAITALL);
	if (len > 0)
	{
		reply[len] = 0;

		for (pos = 0; pos < len; ++pos)
		{
			if (isspace(reply[pos]))
			{
				reply[pos] = 0;
			}
		}

		if (strncmp(reply, DEVD_NOTIFICATION_STRING, strlen(DEVD_NOTIFICATION_STRING)) != 0)
		{
			result = result_fail;
			goto device_system_monitor_receive_device_exit_1;
		}

		for (pos = 1; pos < len; pos += strlen(&(reply[pos])) + 1)
		{
			if (strncmp(&(reply[pos]), DEVD_SYSTEM_STRING, strlen(DEVD_SYSTEM_STRING)) == 0)
			{
				devd_system = &(reply[pos + strlen(DEVD_SYSTEM_STRING)]);
			}
			else if (strncmp(&(reply[pos]), DEVD_SUBSYSTEM_STRING, strlen(DEVD_SUBSYSTEM_STRING)) == 0)
			{
				devd_subsystem = &(reply[pos + strlen(DEVD_SUBSYSTEM_STRING)]);
			}
			else if (strncmp(&(reply[pos]), DEVD_TYPE_STRING, strlen(DEVD_TYPE_STRING)) == 0)
			{
				if (strcmp(&(reply[pos + strlen(DEVD_TYPE_STRING)]), DEVD_STRING_TYPE_CREATE) == 0)
				{
					action_type = dtmd_device_action_add;
				}
				else if (strcmp(&(reply[pos + strlen(DEVD_TYPE_STRING)]), DEVD_STRING_TYPE_DESTROY) == 0)
				{
					action_type = dtmd_device_action_remove;
				}
				else if (strcmp(&(reply[pos + strlen(DEVD_TYPE_STRING)]), DEVD_STRING_TYPE_MEDIACHANGE) == 0)
				{
					action_type = dtmd_device_action_change;
				}
			}
			else if (strncmp(&(reply[pos]), DEVD_CDEV_STRING, strlen(DEVD_CDEV_STRING)) == 0)
			{
				devname = &(reply[pos + strlen(DEVD_CDEV_STRING)]);
			}
		}

		if ((action_type == dtmd_device_action_unknown)
			|| (devd_system == NULL)
			|| (devd_subsystem == NULL)
			|| (devname == NULL)
			|| (strcmp(devd_system, DEVD_STRING_SYSTEM_DEVFS) != 0)
			|| (strcmp(devd_subsystem, DEVD_STRING_SUBSYSTEM_CDEV) != 0)
			|| (strncmp(devname, DEVD_SKIP_DEVICE, strlen(DEVD_SKIP_DEVICE)) == 0)
			|| (strchr(devname, '/') != NULL))
		{
			result = result_fail;
			goto device_system_monitor_receive_device_exit_1;
		}

		device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
		if (device_info == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto device_system_monitor_receive_device_exit_1;
		}

		switch (action_type)
		{
		case dtmd_device_action_add:
		case dtmd_device_action_online:
		case dtmd_device_action_change:
			result = helper_get_device_subtype(devname, &media_subtype);

			switch (result)
			{
			case result_success:
				// It's device
				device_info->media_subtype = media_subtype;
				device_info->path_parent   = strdup(dtmd_root_device_path);
				if (device_info->path_parent == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					result = result_fatal_error;
					goto device_system_monitor_receive_device_exit_2;
				}

				switch (device_info->media_subtype)
				{
				case dtmd_removable_media_subtype_removable_disk:
				case dtmd_removable_media_subtype_sd_card:
					device_info->media_type = dtmd_removable_media_type_stateless_device;
					break;

				case dtmd_removable_media_subtype_cdrom:
					device_info->media_type = dtmd_removable_media_type_stateful_device;
					break;

				case dtmd_removable_media_subtype_unknown_or_persistent:
				default:
					device_info->media_type = dtmd_removable_media_type_unknown_or_persistent;
					break;
				}

				device_info->fstype = NULL;
				device_info->label  = NULL;
				device_info->state  = dtmd_removable_media_state_unknown;
				break;

			case result_fail:
				// It's not a device, try to check if that is a partition
				result = geom_gettree(&mesh);
				if (result != 0)
				{
					WRITE_LOG(LOG_WARNING, "Failed to get geom tree");
					result = result_fatal_error;
					goto device_system_monitor_receive_device_exit_2;
				}

				classp_part = find_class(&mesh, GEOM_CLASS_PART);
				if (classp_part == NULL)
				{
					WRITE_LOG_ARGS(LOG_WARNING, "Geom class \"%s\" not found", GEOM_CLASS_PART);
					result = result_fatal_error;
					goto device_system_monitor_receive_device_exit_4;
				}

				classp_label = find_class(&mesh, GEOM_CLASS_LABEL);
				if (classp_label == NULL)
				{
					WRITE_LOG_ARGS(LOG_WARNING, "Geom class \"%s\" not found", GEOM_CLASS_LABEL);
					result = result_fatal_error;
					goto device_system_monitor_receive_device_exit_4;
				}

				LIST_FOREACH(gp_part, &classp_part->lg_geom, lg_geom)
				{
					LIST_FOREACH(pp_part, &gp_part->lg_provider, lg_provider)
					{
						if (strcmp(pp_part->lg_name, devname) == 0)
						{
							device_info->path_parent = (char*) malloc(strlen(devices_dir "/") + strlen(gp_part->lg_name) + 1);
							if (device_info->path_parent == NULL)
							{
								WRITE_LOG(LOG_ERR, "Memory allocation failure");
								result = result_fatal_error;
								goto device_system_monitor_receive_device_exit_4;
							}

							strcpy((char*) device_info->path_parent, devices_dir "/");
							strcat((char*) device_info->path_parent, gp_part->lg_name);

							result = helper_get_device_subtype(device_info->path_parent, &(device_info->media_subtype));
							if (result != result_success)
							{
								goto device_system_monitor_receive_device_exit_5;
							}

							device_info->media_type = dtmd_removable_media_type_device_partition;
							device_info->state      = dtmd_removable_media_state_unknown;

							device_info->fstype = NULL;
							device_info->label  = NULL;

							conf = find_config_param(pp_part, GEOM_CLASS_PART_FSTYPE_PARAM);
							if (conf != NULL)
							{
								// TODO: check that filesystem actually exists
								device_info->fstype = strdup(conf->lg_val);
								if (device_info->fstype == NULL)
								{
									WRITE_LOG(LOG_ERR, "Memory allocation failure");
									result = result_fatal_error;
									goto device_system_monitor_receive_device_exit_5;
								}

								gp_label = find_geom(classp_label, pp_part->lg_name);
								if ((gp_label != NULL) && (! LIST_EMPTY(&gp_label->lg_provider)))
								{
									raw_label = strchr(LIST_FIRST(&gp_label->lg_provider)->lg_name, '/');
									if (raw_label == NULL)
									{
										raw_label = LIST_FIRST(&gp_label->lg_provider)->lg_name;
									}

									device_info->label = strdup(raw_label);
									if (device_info->label == NULL)
									{
										WRITE_LOG(LOG_ERR, "Memory allocation failure");
										result = result_fatal_error;
										goto device_system_monitor_receive_device_exit_6;
									}
								}
							}

							goto device_system_monitor_receive_device_found_partition;
						}
					}
				}

				// if action is change, tolerate the error
				if (action_type != dtmd_device_action_change)
				{
					/* NOTE: disabled log here because it writes about devices which should be just ignored
					WRITE_LOG_ARGS(LOG_WARNING, "Couldn't find device type of device \"%s\"", devname);
					*/
					result = result_fail;
					goto device_system_monitor_receive_device_exit_4;
				}

				device_info->media_subtype = dtmd_removable_media_subtype_unknown_or_persistent;
				device_info->media_type    = dtmd_removable_media_type_unknown_or_persistent;
				device_info->path_parent   = NULL;
				device_info->fstype        = NULL;
				device_info->label         = NULL;
				device_info->state         = dtmd_removable_media_state_unknown;

device_system_monitor_receive_device_found_partition:
				geom_deletetree(&mesh);
				break;

			case result_fatal_error:
			default:
				goto device_system_monitor_receive_device_exit_2;
			}
			break;

		case dtmd_device_action_remove:
		case dtmd_device_action_offline:
			device_info->media_subtype = dtmd_removable_media_subtype_unknown_or_persistent;
			device_info->media_type    = dtmd_removable_media_type_unknown_or_persistent;
			device_info->path_parent   = NULL;
			device_info->fstype        = NULL;
			device_info->label         = NULL;
			device_info->state         = dtmd_removable_media_state_unknown;
			break;
		}

		device_info->path = (char*) malloc(strlen(devices_dir "/") + strlen(devname) + 1);
		if (device_info->path == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			result = result_fatal_error;
			goto device_system_monitor_receive_device_exit_3;
		}

		strcpy((char*) device_info->path, devices_dir "/");
		strcat((char*) device_info->path, devname);

		device_info->private_data = NULL;

		*device = device_info;
		*action = action_type;
		return result_success;
	}
	else if (len < 0)
	{
		WRITE_LOG(LOG_ERR, "Failed to receive data from devd socket");
		result = result_fatal_error;
		goto device_system_monitor_receive_device_exit_1;
	}

	return result_fail;

device_system_monitor_receive_device_exit_6:
	if (device_info->fstype != NULL)
	{
		free((char*) device_info->fstype);
	}

	if (device_info->label != NULL)
	{
		free((char*) device_info->label);
	}

device_system_monitor_receive_device_exit_5:
	free((char*) device_info->path_parent);

device_system_monitor_receive_device_exit_4:
	geom_deletetree(&mesh);
	free(device_info);
	return result;

/*
device_system_monitor_receive_device_exit_4a:
	free((char*) device_info->path);
*/

device_system_monitor_receive_device_exit_3:
	if (device_info->fstype != NULL)
	{
		free((char*) device_info->fstype);
	}

	if (device_info->label != NULL)
	{
		free((char*) device_info->label);
	}

	if (device_info->path_parent != NULL)
	{
		free((char*) device_info->path_parent);
	}

device_system_monitor_receive_device_exit_2:
	free(device_info);

device_system_monitor_receive_device_exit_1:
	return result;
}
#endif /* (defined OS_FreeBSD) */

static int device_system_monitor_add_item(dtmd_device_monitor_t *monitor, dtmd_info_t *device, dtmd_device_action_type_t action)
{
	char data = 1;
	dtmd_monitor_item_t *monitor_item;

	monitor_item = (dtmd_monitor_item_t*) malloc(sizeof(dtmd_monitor_item_t));
	if (monitor_item == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
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

	return result_success;

device_system_monitor_add_item_error_2:
	free(monitor_item);

device_system_monitor_add_item_error_1:
	return result_fatal_error;
}

static void* device_system_worker_function(void *arg)
{
	dtmd_device_system_t *device_system;
	struct pollfd fds[2];
	char data;
	int rc;
	dtmd_info_t *device;
	dtmd_device_action_type_t action;
	dtmd_device_monitor_t *monitor_iter;

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
			case result_success:
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

					for (monitor_iter = device_system->first_monitor; monitor_iter != NULL; monitor_iter = monitor_iter->next)
					{
						if (device_system_monitor_add_item(monitor_iter, device, action) < 0)
						{
							goto device_system_worker_function_error_3;
						}
					}

					pthread_mutex_unlock(&(device_system->control_mutex));

					device_system_free_device(device);
					break;

				case dtmd_device_action_unknown:
				default:
					device_system_free_device(device);
					break;
				}
				break;

			case result_fail:
				break;

			default:
				goto device_system_worker_function_error_1;
				/* break; */
			}
		}
	}

device_system_worker_function_exit:
	pthread_mutex_lock(&(device_system->control_mutex));

	data = 2;

	for (monitor_iter = device_system->first_monitor; monitor_iter != NULL; monitor_iter = monitor_iter->next)
	{
		write(monitor_iter->data_pipe[1], &data, 1);
	}

	pthread_mutex_unlock(&(device_system->control_mutex));

	goto device_system_worker_function_terminate;

device_system_worker_function_error_3:
	device_system_free_device(device);

	goto device_system_worker_function_error_1_locked;

device_system_worker_function_error_2:
	device_system_free_device(device);

device_system_worker_function_error_1:
	pthread_mutex_lock(&(device_system->control_mutex));

device_system_worker_function_error_1_locked:
	data = 0;

	for (monitor_iter = device_system->first_monitor; monitor_iter != NULL; monitor_iter = monitor_iter->next)
	{
		write(monitor_iter->data_pipe[1], &data, 1);
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
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto device_system_init_error_1;
	}

	device_system->first_enumeration = NULL;
	device_system->last_enumeration  = NULL;
	device_system->first_monitor     = NULL;
	device_system->last_monitor      = NULL;

#if (defined OS_Linux)
	device_system->events_fd = open_netlink_socket();
#endif /* (defined OS_Linux) */
#if (defined OS_FreeBSD)
	device_system->events_fd = open_devd_socket();
#endif /* (defined OS_FreeBSD) */
	if (device_system->events_fd < 0)
	{
		goto device_system_init_error_2;
	}

	if (pthread_mutexattr_init(&mutex_attr) != 0)
	{
		WRITE_LOG(LOG_ERR, "Pthread initialization failure");
		goto device_system_init_error_3;
	}

	if (pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE) != 0)
	{
		WRITE_LOG(LOG_ERR, "Pthread initialization failure");
		goto device_system_init_error_4;
	}

	if (pthread_mutex_init(&(device_system->control_mutex), &mutex_attr) != 0)
	{
		WRITE_LOG(LOG_ERR, "Pthread initialization failure");
		goto device_system_init_error_4;
	}

	if (pipe(device_system->worker_control_pipe) < 0)
	{
		WRITE_LOG(LOG_ERR, "Pipe() failed");
		goto device_system_init_error_5;
	}

	if ((pthread_create(&(device_system->worker_thread), NULL, &device_system_worker_function, device_system)) != 0)
	{
		WRITE_LOG(LOG_ERR, "Pthread initialization failure");
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
	close(device_system->events_fd);

device_system_init_error_2:
	free(device_system);

device_system_init_error_1:
	return NULL;
}

static void helper_free_enumeration(dtmd_device_enumeration_t *enumeration)
{
	dtmd_enumeration_item_t *cur;
	dtmd_enumeration_item_t *next;

	pthread_mutex_lock(&(enumeration->system->control_mutex));

	if (enumeration->next)
	{
		enumeration->next->prev = enumeration->prev;
	}

	if (enumeration->prev)
	{
		enumeration->prev->next = enumeration->next;
	}

	if (enumeration->system->first_enumeration == enumeration)
	{
		enumeration->system->first_enumeration = enumeration->next;
	}

	if (enumeration->system->last_enumeration == enumeration)
	{
		enumeration->system->last_enumeration = enumeration->prev;
	}

	pthread_mutex_unlock(&(enumeration->system->control_mutex));

	next = enumeration->first;

	while (next)
	{
		cur = next;
		next = cur->next;

		device_system_free_device(cur->item);
		free(cur);
	}

	free(enumeration);
}

static void helper_free_monitor(dtmd_device_monitor_t *monitor)
{
	dtmd_monitor_item_t *item, *delete_item;

	pthread_mutex_lock(&(monitor->system->control_mutex));

	if (monitor->next)
	{
		monitor->next->prev = monitor->prev;
	}

	if (monitor->prev)
	{
		monitor->prev->next = monitor->next;
	}

	if (monitor->system->first_monitor == monitor)
	{
		monitor->system->first_monitor = monitor->next;
	}

	if (monitor->system->last_monitor == monitor)
	{
		monitor->system->last_monitor = monitor->prev;
	}

	pthread_mutex_unlock(&(monitor->system->control_mutex));

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
	char data = 0;

	if (system != NULL)
	{
		write(system->worker_control_pipe[1], &data, sizeof(char));
		pthread_join(system->worker_thread, NULL);
		close(system->events_fd);

		if (system->first_enumeration != NULL)
		{
			helper_free_enumeration(system->first_enumeration);
		}

		while (system->first_monitor != NULL)
		{
			helper_free_monitor(system->first_monitor);
		}

		close(system->worker_control_pipe[0]);
		close(system->worker_control_pipe[1]);

		pthread_mutex_destroy(&(system->control_mutex));

		free(system);
	}
}

dtmd_device_enumeration_t* device_system_enumerate_devices(dtmd_device_system_t *system)
{
	dtmd_device_enumeration_t *enumeration;

	if (system == NULL)
	{
		goto device_system_enumerate_devices_error_1;
	}

	enumeration = (dtmd_device_enumeration_t*) malloc(sizeof(dtmd_device_enumeration_t));
	if (enumeration == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto device_system_enumerate_devices_error_1;
	}

	enumeration->system  = system;
	enumeration->first   = NULL;
	enumeration->last    = NULL;
	enumeration->current = NULL;
	enumeration->next    = NULL;

	if (is_result_failure(device_system_run_device_enumeration(enumeration)))
	{
		goto device_system_enumerate_devices_error_2;
	}

	if (pthread_mutex_lock(&(system->control_mutex)) != 0)
	{
		WRITE_LOG(LOG_ERR, "Failed to obtain mutex");
		goto device_system_enumerate_devices_error_2;
	}

	enumeration->prev = system->last_enumeration;

	if (system->first_enumeration == NULL)
	{
		system->first_enumeration = enumeration;
	}

	if (system->last_enumeration != NULL)
	{
		system->last_enumeration->next = enumeration;
	}

	system->last_enumeration = enumeration;

	pthread_mutex_unlock(&(system->control_mutex));

	return enumeration;

/*
device_system_enumerate_devices_error_3:
	pthread_mutex_unlock(&(system->control_mutex));
*/

device_system_enumerate_devices_error_2:
	helper_free_enumeration(enumeration);

device_system_enumerate_devices_error_1:
	return NULL;
}

void device_system_finish_enumerate_devices(dtmd_device_enumeration_t *enumeration)
{
	if (enumeration != NULL)
	{
		helper_free_enumeration(enumeration);
	}
}

int device_system_next_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t **device)
{
#ifndef NDEBUG
	if ((enumeration == NULL)
		|| (device == NULL))
	{
		return result_bug;
	}
#endif /* NDEBUG */

	if (enumeration->current != NULL)
	{
		*device = enumeration->current->item;
		enumeration->current = enumeration->current->next;
		return result_success;
	}
	else
	{
		*device = NULL;
		return result_fail;
	}
}

void device_system_free_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t *device)
{
	// NOTE: do nothing, it's freed on enumeration free
}

dtmd_device_monitor_t* device_system_start_monitoring(dtmd_device_system_t *system)
{
	dtmd_device_monitor_t *monitor;

	if (system == NULL)
	{
		goto device_system_start_monitoring_error_1;
	}

	monitor = (dtmd_device_monitor_t*) malloc(sizeof(dtmd_device_monitor_t));
	if (monitor == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto device_system_start_monitoring_error_1;
	}

	monitor->system = system;
	monitor->first  = NULL;
	monitor->last   = NULL;
	monitor->next   = NULL;

	if (pipe(monitor->data_pipe) < 0)
	{
		WRITE_LOG(LOG_ERR, "Pipe() failed");
		goto device_system_start_monitoring_error_2;
	}

	if (pthread_mutex_lock(&(system->control_mutex)) != 0)
	{
		WRITE_LOG(LOG_ERR, "Failed to obtain mutex");
		goto device_system_start_monitoring_error_3;
	}

	monitor->prev = system->last_monitor;

	if (system->first_monitor == NULL)
	{
		system->first_monitor = monitor;
	}

	if (system->last_monitor != NULL)
	{
		system->last_monitor->next = monitor;
	}

	system->last_monitor = monitor;

	pthread_mutex_unlock(&(system->control_mutex));

	return monitor;

/*
device_system_start_monitoring_error_4:
	pthread_mutex_unlock(&(system->control_mutex));
*/

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
	if (monitor != NULL)
	{
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

#ifdef NDEBUG
	if ((monitor == NULL)
		|| (device == NULL)
		|| (action == NULL))
	{
		rc = result_bug;
		goto device_system_monitor_get_device_error_1;
	}
#endif /* NDEBUG */

	if (pthread_mutex_lock(&(monitor->system->control_mutex)) != 0)
	{
		WRITE_LOG(LOG_ERR, "Failed to obtain mutex");
		rc = result_fatal_error;
		goto device_system_monitor_get_device_error_1;
	}

	rc = read(monitor->data_pipe[0], &data, 1);
	if (rc != 1)
	{
		WRITE_LOG(LOG_ERR, "Failed to read data");
		rc = result_fatal_error;
		goto device_system_monitor_get_device_error_2;
	}

	switch (data)
	{
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

			rc = result_success;
			/*goto device_system_monitor_get_device_error_2; */
			break;
		}

		// NOTE: passthrough

	case 0: // error
	case 2: // exit
		*device = NULL;
		*action = dtmd_device_action_unknown;
		rc = ((data == 0) ? (result_fatal_error) : (result_fail));
		/*goto device_system_monitor_get_device_error_2; */
		break;
	}

device_system_monitor_get_device_error_2:
	pthread_mutex_unlock(&(monitor->system->control_mutex));

device_system_monitor_get_device_error_1:
	return rc;
}

void device_system_monitor_free_device(dtmd_device_monitor_t *monitor, dtmd_info_t *device)
{
	if ((monitor != NULL)
		&& (device != NULL))
	{
		device_system_free_device(device);
	}
}
