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

#ifdef __cplusplus
extern "C" {
#endif

struct removable_partition
{
	char *path;
	char *type;
	char *label; // optional
	unsigned char is_mounted;
	char *mnt_point; // optional
	char *mnt_opts; // optional
};

struct removable_media
{
	char *path;
	unsigned char type;

	unsigned int partitions_count;
	struct removable_partition **partition;
};

enum removable_media_type
{
	unknown_or_persistent = 0,
	cdrom                 = 1,
	removable_disk        = 2,
	sd_card               = 3
};

struct client
{
	int clientfd;
	unsigned char *buf;
	unsigned int buf_size;
	unsigned int buf_used;
};

extern struct removable_media **media;
extern unsigned int media_count;

extern struct client **clients;
extern unsigned int clients_count;

int add_media_block(const char *path, unsigned char media_type);
int remove_media_block(const char *path);
int add_media_partition(const char *block, unsigned char media_type, const char *partition, const char *type, const char *label);
int remove_media_partition(const char *block, const char *partition);
void remove_all_media(void);

int add_client(int client);
int remove_client(int client);
void remove_all_clients(void);

const char* removable_type_to_string(enum removable_media_type removable_type);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LISTS_H */
