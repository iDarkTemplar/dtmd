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

struct dtmd_mount_option
{
	const char * option;
	unsigned char has_param;
};

struct dtmd_filesystem_options
{
	const char * const external_fstype;
	const char * const fstype;
	const struct dtmd_mount_option * const options;
	const char * const option_uid;
	const char * const option_gid;
	const char * const defaults;
};

struct dtmd_string_to_mount_flag
{
	const char *option;
	unsigned long flag;
	unsigned char enabled;
};

struct dtmd_fsopts_list_item
{
	struct dtmd_string_to_mount_flag option;
	unsigned int option_full_len;
	unsigned int option_len;
};

struct dtmd_fsopts_list_id
{
	const char *id_option;
	unsigned int id_option_len;

	char *id_option_value;
	unsigned int id_option_value_len;
};

typedef struct dtmd_fsopts_list
{
	struct dtmd_fsopts_list_item **options;
	unsigned int options_count;

	struct dtmd_fsopts_list_id option_uid;
	struct dtmd_fsopts_list_id option_gid;
} dtmd_fsopts_list_t;

const struct dtmd_filesystem_options* get_fsopts_for_fs(const char *filesystem);

void init_options_list(dtmd_fsopts_list_t *fsopts_list);
int convert_options_to_list(const char *options_list, const struct dtmd_filesystem_options *fsopts, uid_t *uid, gid_t *gid, dtmd_fsopts_list_t *fsopts_list);
void free_options_list(dtmd_fsopts_list_t *fsopts_list);

int fsopts_generate_string(dtmd_fsopts_list_t *fsopts_list,
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
