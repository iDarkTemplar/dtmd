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

#if (defined OS_FreeBSD)
#include <stdint.h>
#endif /* (defined OS_FreeBSD) */

#ifdef __cplusplus
extern "C" {
#endif

struct dtmd_mount_option
{
	const char * const option;
	unsigned char has_param;
#if (defined OS_FreeBSD)
	const char * const transformation;
#endif /* (defined OS_FreeBSD) */
};

struct dtmd_mount_option_list
{
	const struct dtmd_mount_option * const item;
};

struct dtmd_filesystem_options
{
	const char * const external_fstype;

#if (defined OS_FreeBSD)
	const char * const mount_cmd;
#endif /* (defined OS_FreeBSD) */

	const char * const fstype;
	const struct dtmd_mount_option_list * const options;
	const char * const option_uid;

#if (defined OS_FreeBSD)
	const char * const option_uid_transformation;
#endif /* (defined OS_FreeBSD) */

	const char * const option_gid;

#if (defined OS_FreeBSD)
	const char * const option_gid_transformation;
#endif /* (defined OS_FreeBSD) */

	const char * const defaults;
	const char * const mandatory_options;
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

#if (defined OS_FreeBSD)
	const char *transformation_string;
#endif /* (defined OS_FreeBSD) */
};

struct dtmd_fsopts_list_id
{
	const char * id_option;
	unsigned int id_option_len;

	char *id_option_value;
	unsigned int id_option_value_len;

#if (defined OS_FreeBSD)
	unsigned char transformed;
#endif /* (defined OS_FreeBSD) */
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
	unsigned int options_full_string_buffer_size
#if (defined OS_Linux)
	,
	unsigned int *options_string_length,
	char *options_string_buffer,
	unsigned int options_string_buffer_size,
	unsigned long *mount_flags
#endif /* (defined OS_Linux) */
	);

int invoke_list_supported_filesystems(int client_number);
int invoke_list_supported_filesystem_options(int client_number, const char *filesystem);

#if (defined OS_FreeBSD)
char* convert_option_flags_to_string(uint64_t flags);
#endif /* (defined OS_FreeBSD) */

#ifdef __cplusplus
}
#endif

#endif /* FILESYSTEM_OPTS_H */
