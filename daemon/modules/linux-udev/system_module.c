/*
 * Copyright (C) 2016-2019 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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
#include <dtmd-misc.h>

#include <libudev.h>
#include <stdlib.h>
#include <string.h>

#define ID_CDROM_MEDIA_STATE_BLANK "blank"
#define ID_CDROM_MEDIA_STATE_COMPLETE "complete"

struct dtmd_device_enumeration
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *dev_list_entry;
};

static dtmd_removable_media_type_t get_device_subtype(struct udev_device *device)
{
	const char *removable;
	const char *id_bus;
	const char *id_cdrom;
	const char *id_drive_flash_sd;
	const char *id_drive_media_flash_sd;

	removable               = udev_device_get_sysattr_value(device, "removable");
	id_bus                  = udev_device_get_property_value(device, "ID_BUS");
	id_cdrom                = udev_device_get_property_value(device, "ID_CDROM");
	id_drive_flash_sd       = udev_device_get_property_value(device, "ID_DRIVE_FLASH_SD");
	id_drive_media_flash_sd = udev_device_get_property_value(device, "ID_DRIVE_MEDIA_FLASH_SD");

	if ((removable != NULL) && ((strcmp(removable, "1") == 0)
		|| ((id_bus != NULL) && (strcmp(id_bus, "usb") == 0))
		|| ((id_drive_flash_sd != NULL) && (strcmp(id_drive_flash_sd, "1") == 0))
		|| ((id_drive_media_flash_sd != NULL) && (strcmp(id_drive_media_flash_sd, "1") == 0))))
	{
		if ((id_cdrom != NULL) && (strcmp(id_cdrom, "1") == 0))
		{
			return dtmd_removable_media_subtype_cdrom;
		}
		else if (((id_drive_flash_sd != NULL) && (strcmp(id_drive_flash_sd, "1") == 0))
			|| ((id_drive_media_flash_sd != NULL) && (strcmp(id_drive_media_flash_sd, "1") == 0)))
		{
			return dtmd_removable_media_subtype_sd_card;
		}
		else
		{
			return dtmd_removable_media_subtype_removable_disk;
		}
	}
	else
	{
		return dtmd_removable_media_subtype_unknown_or_persistent;
	}
}

static void device_system_free_device(dtmd_info_t *device)
{
	if (device->private_data != NULL)
	{
		udev_device_unref(device->private_data);
	}

	free(device);
}

static void device_system_fill_device(struct udev_device *dev, const char *path, dtmd_info_t *device_info)
{
	const char *state;
	struct udev_device *parent_device;

	parent_device = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");

	device_info->path          = path;
	device_info->media_subtype = get_device_subtype(dev);
	device_info->sysfs_path    = ((parent_device != NULL) ? udev_device_get_syspath(parent_device) : NULL);
	device_info->private_data  = dev;

	if (device_info->media_subtype == dtmd_removable_media_subtype_cdrom)
	{
		device_info->media_type = dtmd_removable_media_type_stateful_device;
		device_info->fstype     = udev_device_get_property_value(dev, "ID_FS_TYPE");
		device_info->label      = udev_device_get_property_value(dev, "ID_FS_LABEL_ENC");
		state                   = udev_device_get_property_value(dev, "ID_CDROM_MEDIA_STATE");

		device_info->state = dtmd_removable_media_state_empty;

		if (state != NULL)
		{
			if ((device_info->fstype != NULL) && (strcmp(state, ID_CDROM_MEDIA_STATE_COMPLETE) == 0))
			{
				device_info->state = dtmd_removable_media_state_ok;
			}
			else if (strcmp(state, ID_CDROM_MEDIA_STATE_BLANK) == 0)
			{
				device_info->state = dtmd_removable_media_state_clear;
			}
		}
		else
		{
			/* Some fake CDROMs don't provide state, in that case rely on fstype availability */
			if (device_info->fstype != NULL)
			{
				device_info->state = dtmd_removable_media_state_ok;
			}
		}
	}
	else
	{
		device_info->media_type = dtmd_removable_media_type_stateless_device;
		device_info->fstype     = NULL;
		device_info->label      = NULL;
		device_info->state      = dtmd_removable_media_state_unknown;
	}

	device_info->path_parent = dtmd_root_device_path;
}

static void device_system_fill_partition(struct udev_device *dev, const char *path, dtmd_info_t *device_info)
{
	struct udev_device *dev_parent;

	dev_parent = udev_device_get_parent_with_subsystem_devtype(dev, "block", "disk");
	if (dev_parent != NULL)
	{
		device_info->path_parent   = udev_device_get_devnode(dev_parent);
		device_info->media_subtype = get_device_subtype(dev_parent);
	}
	else
	{
		device_info->path_parent   = dtmd_root_device_path;
		device_info->media_subtype = dtmd_removable_media_subtype_unknown_or_persistent;
	}

	device_info->media_type   = dtmd_removable_media_type_device_partition;
	device_info->state        = dtmd_removable_media_state_unknown;
	device_info->path         = path;
	device_info->sysfs_path   = NULL;
	device_info->fstype       = udev_device_get_property_value(dev, "ID_FS_TYPE");
	device_info->label        = udev_device_get_property_value(dev, "ID_FS_LABEL_ENC");
	device_info->private_data = dev;
}

dtmd_device_system_t* device_system_init(void)
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
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto device_system_enumerate_devices_error_1;
	}

	enumeration->enumerate = udev_enumerate_new((struct udev*) system);
	if (enumeration->enumerate == NULL)
	{
		WRITE_LOG(LOG_ERR, "Udev failure");
		goto device_system_enumerate_devices_error_2;
	}

	if (udev_enumerate_add_match_subsystem(enumeration->enumerate, "block") < 0)
	{
		WRITE_LOG(LOG_ERR, "Udev failure");
		goto device_system_enumerate_devices_error_3;
	}

	if (udev_enumerate_scan_devices(enumeration->enumerate) < 0)
	{
		WRITE_LOG(LOG_ERR, "Udev failure");
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

int device_system_next_enumerated_device(dtmd_device_enumeration_t *enumeration, dtmd_info_t **device)
{
	const char *path;
	const char *devtype;

	struct udev_list_entry *dev_list_entry;

	struct udev_device *dev;

	dtmd_info_t *device_info;

#ifndef NDEBUG
	if ((enumeration == NULL)
		|| (device == NULL))
	{
		return result_bug;
	}
#endif /* NDEBUG */

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
			WRITE_LOG(LOG_ERR, "Udev failure");
			return result_fatal_error;
		}

		path = udev_device_get_devnode(dev);
		if (path == NULL)
		{
			WRITE_LOG(LOG_ERR, "Udev failure");
			udev_device_unref(dev);
			return result_fatal_error;
		}

		devtype = udev_device_get_devtype(dev);

		if (strcmp(devtype, "disk") == 0)
		{
			device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
			if (device_info == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				udev_device_unref(dev);
				return result_fatal_error;
			}

			device_system_fill_device(dev, path, device_info);

			*device = device_info;
			return result_success;
		}
		else if (strcmp(devtype, "partition") == 0)
		{
			device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
			if (device_info == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				udev_device_unref(dev);
				return result_fatal_error;
			}

			device_system_fill_partition(dev, path, device_info);

			*device = device_info;
			return result_success;
		}

		udev_device_unref(dev);
	}

	*device = NULL;
	return result_fail;
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
		WRITE_LOG(LOG_ERR, "Udev failure");
		goto device_system_start_monitoring_error_1;
	}

	if (udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL) < 0)
	{
		WRITE_LOG(LOG_ERR, "Udev failure");
		goto device_system_start_monitoring_error_2;
	}

	if (udev_monitor_enable_receiving(mon) < 0)
	{
		WRITE_LOG(LOG_ERR, "Udev failure");
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
	const char *devtype;
	const char *action_str;

	dtmd_device_action_type_t act;

	struct udev_device *dev;

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
				device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
				if (device_info == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					udev_device_unref(dev);
					return result_fatal_error;
				}

				device_system_fill_device(dev, path, device_info);

				*device = device_info;
				*action = act;
				return result_success;
			}
			else if (strcmp(devtype, "partition") == 0)
			{
				device_info = (dtmd_info_t*) malloc(sizeof(dtmd_info_t));
				if (device_info == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					udev_device_unref(dev);
					return result_fatal_error;
				}

				device_system_fill_partition(dev, path, device_info);

				*device = device_info;
				*action = act;
				return result_success;
			}
		}

		udev_device_unref(dev);
	}
	else
	{
		WRITE_LOG(LOG_ERR, "Udev failed to receive device");
		return result_fatal_error;
	}

device_system_monitor_get_device_exit:
	*device = NULL;
	*action = dtmd_device_action_unknown;
	return result_fail;
}

void device_system_monitor_free_device(dtmd_device_monitor_t *monitor, dtmd_info_t *device)
{
	if ((monitor != NULL)
		&& (device != NULL))
	{
		device_system_free_device(device);
	}
}
