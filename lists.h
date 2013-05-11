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

struct removable_media
{
	char *path;
	char **partition;
	unsigned int partitions_count;
};

extern struct removable_media **media;
extern unsigned int media_count;

extern int *clients;
extern unsigned int clients_count;

int add_media_block(const char *path);
int remove_media_block(const char *path);
int add_media_partition(const char *block, const char *partition);
int remove_media_partition(const char *block, const char *partition);
void remove_all_media(void);

int add_client(int client);
int remove_client(int client);
void remove_all_clients(void);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LISTS_H */
