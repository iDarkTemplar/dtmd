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

#ifndef FILESYSTEM_OPTS_H
#define FILESYSTEM_OPTS_H

#ifdef __cplusplus
extern "C" {
#endif

struct dtmd_mount_option
{
	const char * const option;
	const unsigned char has_param;
};

struct dtmd_filesystem_options
{
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	const char * const external_fstype;
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
	const char * const fstype;
	const struct dtmd_mount_option * const options;
	const char * const option_uid;
	const char * const option_gid;
	const char * const defaults;
};

const struct dtmd_filesystem_options* dtmd_get_fsopts_for_fstype(const char *fstype);
int dtmd_is_option_allowed(const char *option, unsigned int option_len, const struct dtmd_filesystem_options *filesystem_list);
int dtmd_are_options_supported(const char *filesystem, const char *options_list);

#ifdef __cplusplus
}
#endif

#endif /* FILESYSTEM_OPTS_H */
