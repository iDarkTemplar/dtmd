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

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dtmd_fsopts_result
{
	dtmd_fsopts_error = -1,
	dtmd_fsopts_not_supported = 0,
	dtmd_fsopts_internal_mount = 1,
	dtmd_fsopts_external_mount = 2
} dtmd_fsopts_result_t;

dtmd_fsopts_result_t dtmd_fsopts_fstype(const char *filesystem);
const char* dtmd_fsopts_get_fstype_string(const char *filesystem);

dtmd_fsopts_result_t dtmd_fsopts_generate_string(const char *options_list,
	const char *filesystem,
	uid_t *uid,
	gid_t *gid,
	unsigned int *options_full_string_length,
	char *options_full_string_buffer,
	unsigned int options_full_string_buffer_size,
	unsigned int *options_string_length,
	char *options_string_buffer,
	unsigned int options_string_buffer_size,
	unsigned long *mount_flags);

#ifdef __cplusplus
}
#endif

#endif /* FILESYSTEM_OPTS_H */
