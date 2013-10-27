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

#include <libudev.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct dtmd_device_enumeration
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *dev_list_entry;
};

static dtmd_removable_media_type_t get_device_type(struct udev_device *device)
{
	const char *removable;
	const char *id_bus;
	const char *is_cdrom;
	const char *id_drive_flash_sd;
	const char *id_drive_media_flash_sd;

	removable               = udev_device_get_sysattr_value(device, "removable");
	id_bus                  = udev_device_get_property_value(device, "ID_BUS");
	is_cdrom                = udev_device_get_property_value(device, "ID_CDROM");
	id_drive_flash_sd       = udev_device_get_property_value(device, "ID_DRIVE_FLASH_SD");
	id_drive_media_flash_sd = udev_device_get_property_value(device, "ID_DRIVE_MEDIA_FLASH_SD");

	if ((removable != NULL) && ((strcmp(removable, "1") == 0)
		|| ((id_bus != NULL) && (strcmp(id_bus, "usb") == 0))
		|| ((id_drive_flash_sd != NULL) && (strcmp(id_drive_flash_sd, "1") == 0))
		|| ((id_drive_media_flash_sd != NULL) && (strcmp(id_drive_media_flash_sd, "1") == 0))))
	{
		if ((is_cdrom != NULL) && (strcmp(is_cdrom, "1") == 0))
		{
			return cdrom;
		}
		else if (((id_drive_flash_sd != NULL) && (strcmp(id_drive_flash_sd, "1") == 0))
			|| ((id_drive_media_flash_sd != NULL) && (strcmp(id_drive_media_flash_sd, "1") == 0)))
		{
			return sd_card;
		}
		else
		{
			return removable_disk;
		}
	}
	else
	{
		return unknown_or_persistent;
	}
}

static void device_system_free_device(dtmd_info_t *device)
{
	switch (device->type)
	{
	case dtmd_info_device:
		udev_device_unref(device->device.private_data);
		break;

	case dtmd_info_partition:
		udev_device_unref(device->partition.private_data);
		break;
	}

	free(device);
}

dtmd_device_system_t* device_system_init()
{
	return (dtmd_device_system_t*) udev_new();
}

void device_system_deinit(dtmd_device_system_t *system)
{
	if (system != NULL)
	{
		udev_unref((struct udev*) system);
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
		goto device_system_enumerate_devices_error_1;
	}

	enumeration->enumerate = udev_enumerate_new((struct udev*) system);
	if (enumeration->enumerate == NULL)
	{
		goto device_system_enumerate_devices_error_2;
	}

	if (udev_enumerate_add_match_subsystem(enumeration->enumerate, "block") < 0)
	{
		goto device_system_enumerate_devices_error_3;
	}

	if (udev_enumerate_scan_devices(enumeration->enumerate) < 0)
	{
		goto device_system_enumerate_devices_error_3;
	}

	enumeration->dev_list_entry = udev_enumerate_get_list_entry(enumeration->enumerate);

	return enumeration;

device_system_enumerate_devices_error_3:
	udev_enumerate_unref(enumeration->enumerate);

device_system_enumerate_devices_error_2:
	free(enumeration);

device_system_enumerate_devices_error_1:
	return NULL;
}

void device_system_finish_enumerate_devices(dtmd_device_enumeration_t *enumeration)
{
	if (enumeration != NULL)
	{
		udev_enumerate_unref(enumeration->enumerate);
		free(enumeration);
	}
}
#include <stdio.h>
int device_system_next_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t **device)
{
	const char *path;
	const char *path_parent;
	const char *devtype;
	const char *fstype;
	const char *label;

	dtmd_removable_media_type_t media_type;

	struct udev_list_entry *dev_list_entry;

	struct udev_device *dev;
	struct udev_device *dev_parent;

	dtmd_info_t *device_info;

	if ((enumeration == NULL)
		|| (device == NULL))
	{
		return -1;
	}

	for (dev_list_entry = enumeration->dev_list_entry; dev_list_entry != NULL; dev_list_entry = enumeration->dev_list_entry)
	{
		if (enumeration->dev_list_entry != NULL)
		{
			enumeration->dev_list_entry = udev_list_entry_get_next(enumeration->dev_list_entry);
		}

		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev_enumerate_get_udev(enumeration->enumerate), path);
		if (dev == NULL)
		{
			return -1;
		}

		path = udev_device_get_devnode(dev);
		if (path == NULL)
		{
			udev_device_unref(dev);
			return -1;
		}

		devtype = udev_device_get_devtype(dev);

		if (strcmp(devtype, "disk") == 0)
		{
			media_type = get_device_type(dev);

			if (media_type != unknown_or_persistent)
			{
				device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
				if (device_info == NULL)
				{
					udev_device_unref(dev);
					return -1;
				}

				device_info->type                = dtmd_info_device;
				device_info->device.path         = path;
				device_info->device.media_type   = media_type;
				device_info->device.private_data = dev;

				*device = device_info;
				return 1;
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

				if ((media_type != unknown_or_persistent) && (fstype != NULL))
				{
					device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
					if (device_info == NULL)
					{
						udev_device_unref(dev);
						return -1;
					}

					device_info->type                   = dtmd_info_partition;
					device_info->partition.path         = path;
					device_info->partition.fstype       = fstype;
					device_info->partition.label        = label;
					device_info->partition.path_parent  = path_parent;
					device_info->partition.media_type   = media_type;
					device_info->partition.private_data = dev;

					*device = device_info;
					return 1;
				}
			}
		}

		udev_device_unref(dev);
	}

	*device = NULL;
	return 0;
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
	struct udev_monitor *mon;

	if (system == NULL)
	{
		goto device_system_start_monitoring_error_1;
	}

	mon = udev_monitor_new_from_netlink((struct udev*) system, "udev");
	if (mon == NULL)
	{
		goto device_system_start_monitoring_error_1;
	}

	if (udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL) < 0)
	{
		goto device_system_start_monitoring_error_2;
	}

	if (udev_monitor_enable_receiving(mon) < 0)
	{
		goto device_system_start_monitoring_error_2;
	}

	return (dtmd_device_monitor_t*) mon;

device_system_start_monitoring_error_2:
	udev_monitor_unref(mon);

device_system_start_monitoring_error_1:
	return NULL;
}

void device_system_stop_monitoring(dtmd_device_monitor_t *monitor)
{
	if (monitor != NULL)
	{
		udev_monitor_unref((struct udev_monitor*) monitor);
	}
}

int device_system_get_monitor_fd(dtmd_device_monitor_t *monitor)
{
	if (monitor != NULL)
	{
		return udev_monitor_get_fd((struct udev_monitor*) monitor);
	}
	else
	{
		return -1;
	}
}

int device_system_monitor_get_device(dtmd_device_monitor_t *monitor, dtmd_info_t **device, dtmd_device_action_type_t *action)
{
	const char *path;
	const char *path_parent;
	const char *devtype;
	const char *fstype;
	const char *label;
	const char *action_str;

	dtmd_device_action_type_t act;
	dtmd_removable_media_type_t media_type;

	struct udev_device *dev;
	struct udev_device *dev_parent;

	dtmd_info_t *device_info;

	dev = udev_monitor_receive_device((struct udev_monitor*) monitor);
	if (dev != NULL)
	{
		path       = udev_device_get_devnode(dev);
		action_str = udev_device_get_action(dev);
		devtype    = udev_device_get_devtype(dev);

		if ((action_str != NULL) && (path != NULL))
		{
			if (strcmp(action_str, "add") == 0)
			{
				act = dtmd_device_action_add;
			}
			else if (strcmp(action_str, "online") == 0)
			{
				act = dtmd_device_action_online;
			}
			else if (strcmp(action_str, "remove") == 0)
			{
				act = dtmd_device_action_remove;
			}
			else if (strcmp(action_str, "offline") == 0)
			{
				act = dtmd_device_action_offline;
			}
			else if (strcmp(action_str, "change") == 0)
			{
				act = dtmd_device_action_change;
			}
			else
			{
				udev_device_unref(dev);
				goto device_system_monitor_get_device_exit;
			}

			if (strcmp(devtype, "disk") == 0)
			{
				media_type = get_device_type(dev);

				device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
				if (device_info == NULL)
				{
					udev_device_unref(dev);
					return -1;
				}

				device_info->type                = dtmd_info_device;
				device_info->device.path         = path;
				device_info->device.media_type   = media_type;
				device_info->device.private_data = dev;

				*device = device_info;
				*action = act;
				return 1;
			}
			else if (strcmp(devtype, "partition") == 0)
			{
				dev_parent = udev_device_get_parent_with_subsystem_devtype(dev, "block", "disk");
				if (dev_parent != NULL)
				{
					path_parent = udev_device_get_devnode(dev_parent);
					media_type  = get_device_type(dev_parent);
				}
				else
				{
					path_parent = NULL;
					media_type  = unknown_or_persistent;
				}

				fstype = udev_device_get_property_value(dev, "ID_FS_TYPE");
				label  = udev_device_get_property_value(dev, "ID_FS_LABEL_ENC");

				device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
				if (device_info == NULL)
				{
					udev_device_unref(dev);
					return -1;
				}

				device_info->type                   = dtmd_info_partition;
				device_info->partition.path         = path;
				device_info->partition.fstype       = fstype;
				device_info->partition.label        = label;
				device_info->partition.path_parent  = path_parent;
				device_info->partition.media_type   = media_type;
				device_info->partition.private_data = dev;

				*device = device_info;
				*action = act;
				return 1;
			}
		}

		udev_device_unref(dev);
	}
	else
	{
		return -1;
	}

device_system_monitor_get_device_exit:
	*device = NULL;
	*action = dtmd_device_action_unknown;
	return 0;
}

void device_system_monitor_free_device(dtmd_device_monitor_t *monitor, dtmd_info_t *device)
{
	if ((monitor != NULL)
		&& (device != NULL))
	{
		device_system_free_device(device);
	}
}
