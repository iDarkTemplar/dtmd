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

#ifndef DTMD_H
#define DTMD_H

#define dtmd_daemon_socket_addr "/var/run/dtmd.socket"
#define dtmd_command_max_length 4096

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dtmd_removable_media_type
{
	dtmd_removable_media_unknown_or_persistent = 0,
	dtmd_removable_media_cdrom                 = 1,
	dtmd_removable_media_removable_disk        = 2,
	dtmd_removable_media_sd_card               = 3
} dtmd_removable_media_type_t;

typedef enum dtmd_removable_media_state
{
	dtmd_removable_media_state_unknown = 0,
	dtmd_removable_media_state_empty   = 1,
	dtmd_removable_media_state_clear   = 2,
	dtmd_removable_media_state_ok      = 3
} dtmd_removable_media_state_t;

#ifdef __cplusplus
}
#endif

#define dtmd_string_device_unknown_or_persistent "unknown"
#define dtmd_string_device_cdrom                 "cdrom"
#define dtmd_string_device_removable_disk        "removable disk"
#define dtmd_string_device_sd_card               "sdcard"

#define dtmd_string_state_unknown "unknown"
#define dtmd_string_state_empty   "empty"
#define dtmd_string_state_clear   "clear"
#define dtmd_string_state_ok      "ok"

#endif /* DTMD_H */
