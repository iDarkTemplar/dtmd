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

#include "udev.h"

#include <libudev.h>
#include <stdlib.h>
#include <string.h>

int is_device_removable(struct udev_device *device)
{
	const char *removable;
	const char *id_bus;
	const char *id_drive_flash_sd;
	const char *id_drive_media_flash_sd;

	removable               = udev_device_get_sysattr_value(device, "removable");
	id_bus                  = udev_device_get_property_value(device, "ID_BUS");
	id_drive_flash_sd       = udev_device_get_property_value(device, "ID_DRIVE_FLASH_SD");
	id_drive_media_flash_sd = udev_device_get_property_value(device, "ID_DRIVE_MEDIA_FLASH_SD");

	if ((removable != NULL) && ((strcmp(removable, "1") == 0)
		|| ((id_bus != NULL) && (strcmp(id_bus, "usb") == 0))
		|| ((id_drive_flash_sd != NULL) && (strcmp(id_drive_flash_sd, "1") == 0))
		|| ((id_drive_media_flash_sd != NULL) && (strcmp(id_drive_media_flash_sd, "1") == 0))))
	{
		return 1;
	}

	return 0;
}
