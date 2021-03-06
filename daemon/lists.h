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

#ifndef DTMD_LISTS_H
#define DTMD_LISTS_H

#include <dtmd.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct client
{
	int clientfd;
	size_t buf_used;
	char buf[dtmd_command_max_length + 1];

	struct client *next_node;
	struct client *prev_node;
};

typedef struct dtmd_removable_media_private
{
	dtmd_removable_media_t parent;

#if (defined OS_Linux)
	char *sysfs_path;
#endif /* (defined OS_Linux) */
	int mount_counter;
} dtmd_removable_media_private_t;

extern dtmd_removable_media_t *removable_media_root;

extern struct client *client_root;
extern size_t clients_count;

int add_media(const char *parent_path,
	const char *path,
#if (defined OS_Linux)
	const char *sysfs_path,
#endif /* (defined OS_Linux) */
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts);

int remove_media(const char *path);

int change_media(const char *parent_path,
	const char *path,
#if (defined OS_Linux)
	const char *sysfs_path,
#endif /* (defined OS_Linux) */
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts);

void remove_all_media(void);

int add_client(int client_fd);
int remove_client(int client_fd);

void remove_all_clients(void);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LISTS_H */
