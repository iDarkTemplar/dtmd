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

#ifndef DTMD_LISTS_H
#define DTMD_LISTS_H

#include <dtmd.h>

#ifdef __cplusplus
extern "C" {
#endif

struct removable_partition
{
	char *path;
	char *fstype; // optional
	char *label; // optional
	unsigned char is_mounted;
	char *mnt_point; // optional
	char *mnt_opts; // optional
};

struct removable_media
{
	char *path;
	dtmd_removable_media_type_t type;

	unsigned int partitions_count;
	struct removable_partition **partition;
};

struct removable_stateful_media
{
	char *path;
	dtmd_removable_media_type_t type;
	dtmd_removable_media_state_t state;
	char *fstype; // optional
	char *label; // optional
	unsigned char is_mounted;
	char *mnt_point; // optional
	char *mnt_opts; // optional
};

struct client
{
	int clientfd;
	unsigned int buf_used;
	char buf[dtmd_command_max_length + 1];
};

extern struct removable_media **media;
extern unsigned int media_count;

extern struct removable_stateful_media **stateful_media;
extern unsigned int stateful_media_count;

extern struct client **clients;
extern unsigned int clients_count;

int add_media_block(const char *path, dtmd_removable_media_type_t media_type);
int remove_media_block(const char *path);
int change_media_block(const char *path, dtmd_removable_media_type_t media_type);

int add_media_partition(const char *block, dtmd_removable_media_type_t media_type, const char *partition, const char *fstype, const char *label);
int remove_media_partition(const char *block, const char *partition);
int change_media_partition(const char *block, dtmd_removable_media_type_t media_type, const char *partition, const char *fstype, const char *label);

void remove_all_media(void);

int add_stateful_media(const char *path, dtmd_removable_media_type_t media_type, dtmd_removable_media_state_t state, const char *fstype, const char *label);
int remove_stateful_media(const char *path);
int change_stateful_media(const char *path, dtmd_removable_media_type_t media_type, dtmd_removable_media_state_t state, const char *fstype, const char *label);

void remove_all_stateful_media(void);

int add_client(int client);
int remove_client(int client);

void remove_all_clients(void);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LISTS_H */
