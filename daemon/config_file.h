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

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

enum mount_by_value_enum
{
	mount_by_device_name = 0,
	mount_by_device_label
};

extern int unmount_on_exit;
extern enum mount_by_value_enum mount_by_value;

void read_config(void);
void free_config(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_FILE_H */
