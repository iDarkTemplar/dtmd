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

#include "lists.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct removable_media **media = NULL;
unsigned int media_count = 0;

struct client **clients = NULL;
unsigned int clients_count = 0;

static const char *str_unknown_or_persistent = "unknown";
static const char *str_cdrom                 = "cdrom";
static const char *str_removable_disk        = "disk";
static const char *str_sd_card               = "sdcard";

int add_media_block(const char *path, unsigned char media_type)
{
	unsigned int i;
	struct removable_media *cur_media;
	struct removable_media **tmp;

	for (i = 0; i < media_count; ++i)
	{
		if (strcmp(media[i]->path, path) == 0)
		{
			return 0;
		}
	}

	cur_media = (struct removable_media*) malloc(sizeof(struct removable_media));
	if (cur_media == NULL)
	{
		return -1;
	}

	cur_media->path = strdup(path);
	if (cur_media->path == NULL)
	{
		free(cur_media);
		return -1;
	}

	cur_media->partition = NULL;
	cur_media->partitions_count = 0;
	cur_media->type = media_type;

	tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*)*(media_count+1));
	if (tmp == NULL)
	{
		free(cur_media->path);
		free(cur_media);
		return -1;
	}

	++media_count;
	media = tmp;
	media[media_count-1] = cur_media;

	return 1;
}

int remove_media_block(const char *path)
{
	unsigned int i;
	unsigned int j;
	struct removable_media **tmp;
	struct removable_media *del;

	for (i = 0; i < media_count; ++i)
	{
		if (strcmp(media[i]->path, path) == 0)
		{
			del = media[i];
			--media_count;

			if (media_count > 0)
			{
				media[i] = media[media_count];
				media[media_count] = del;

				tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*) * media_count);
				if (tmp == NULL)
				{
					return -1;
				}

				media = tmp;
			}
			else
			{
				free(media);
				media = NULL;
			}

			if (del->partitions_count != 0)
			{
				for (j = 0; j < del->partitions_count; ++j)
				{
					if (del->partition[j]->label != NULL)
					{
						free(del->partition[j]->label);
					}

					free(del->partition[j]->path);
					free(del->partition[j]->type);
					free(del->partition[j]);
				}

				free(del->partition);
			}

			free(del->path);
			free(del);

			return 1;
		}
	}

	return 0;
}

int add_media_partition(const char *block, unsigned char media_type, const char *partition, const char *type, const char *label)
{
	int rc;
	unsigned int i;
	unsigned int j;
	struct removable_partition *cur_partition;
	struct removable_partition **tmp;

	rc = add_media_block(block, media_type);
	if (rc < 0)
	{
		return rc;
	}

	for (i = 0; i < media_count; ++i)
	{
		if (strcmp(media[i]->path, block) == 0)
		{
			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				if (strcmp(media[i]->partition[j]->path, partition) == 0)
				{
					return 0;
				}
			}

			cur_partition = (struct removable_partition*) malloc(sizeof(struct removable_partition));
			if (cur_partition == NULL)
			{
				return -1;
			}

			cur_partition->path = strdup(partition);
			if (cur_partition->path == NULL)
			{
				free(cur_partition);
				return -1;
			}

			cur_partition->type = strdup(type);
			if (cur_partition->type == NULL)
			{
				free(cur_partition->path);
				free(cur_partition);
				return -1;
			}

			if (label != NULL)
			{
				cur_partition->label = strdup(label);
				if (cur_partition->label == NULL)
				{
					free(cur_partition->type);
					free(cur_partition->path);
					free(cur_partition);
					return -1;
				}
			}
			else
			{
				cur_partition->label = NULL;
			}

			cur_partition->mnt_point = NULL;
			cur_partition->mnt_opts  = NULL;

			tmp = (struct removable_partition**) realloc(media[i]->partition, sizeof(struct removable_partition*) * (media[i]->partitions_count + 1));
			if (tmp == NULL)
			{
				if (cur_partition->label != NULL)
				{
					free(cur_partition->label);
				}

				free(cur_partition->type);
				free(cur_partition->path);
				free(cur_partition);
				return -1;
			}

			++(media[i]->partitions_count);
			media[i]->partition = tmp;
			media[i]->partition[media[i]->partitions_count - 1] = cur_partition;

			return 1;
		}
	}

	return -2; // -2 means BUG
}

int remove_media_partition(const char *block, const char *partition)
{
	unsigned int i;
	unsigned int j;
	struct removable_partition **tmp;
	struct removable_partition *del;

	for (i = 0; i < media_count; ++i)
	{
		if (((block == NULL) || (strcmp(block, media[i]->path) == 0)) && (media[i]->partitions_count > 0))
		{
			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				if (strcmp(partition, media[i]->partition[j]->path) == 0)
				{
					del = media[i]->partition[j];
					--(media[i]->partitions_count);

					if (media[i]->partitions_count > 0)
					{
						media[i]->partition[j] = media[i]->partition[media[i]->partitions_count];
						media[i]->partition[media[i]->partitions_count] = del;

						tmp = (struct removable_partition**) realloc(media[i]->partition, sizeof(struct removable_partition*) * media[i]->partitions_count);
						if (tmp == NULL)
						{
							return -1;
						}

						media[i]->partition = tmp;
					}
					else
					{
						free(media[i]->partition);
						media[i]->partition = NULL;
					}

					if (del->label != NULL)
					{
						free(del->label);
					}

					if (del->mnt_point != NULL)
					{
						free(del->mnt_point);
					}

					if (del->mnt_opts != NULL)
					{
						free(del->mnt_opts);
					}

					free(del->path);
					free(del->type);
					free(del);

					return 1;
				}
			}
		}
	}

	return 0;
}

void remove_all_media(void)
{
	unsigned int i;
	unsigned int j;

	if (media_count > 0)
	{
		for (i = 0; i < media_count; ++i)
		{
			if (media[i]->partitions_count > 0)
			{
				for (j = 0; j < media[i]->partitions_count; ++j)
				{
					if (media[i]->partition[j]->label != NULL)
					{
						free(media[i]->partition[j]->label);
					}

					if (media[i]->partition[j]->mnt_point != NULL)
					{
						free(media[i]->partition[j]->mnt_point);
					}

					if (media[i]->partition[j]->mnt_opts != NULL)
					{
						free(media[i]->partition[j]->mnt_opts);
					}

					free(media[i]->partition[j]->path);
					free(media[i]->partition[j]->type);
					free(media[i]->partition[j]);
				}

				free(media[i]->partition);
			}

			free(media[i]->path);
			free(media[i]);
		}

		free(media);

		media_count = 0;
		media = NULL;
	}
}

int add_client(int client)
{
	unsigned int i;
	struct client *cur_client;
	struct client **tmp;

	for (i = 0; i < clients_count; ++i)
	{
		if (clients[i]->clientfd == client)
		{
			return 0;
		}
	}

	cur_client = (struct client*) malloc(sizeof(struct client));
	if (cur_client == NULL)
	{
		shutdown(client, SHUT_RDWR);
		close(client);
		return -1;
	}

	cur_client->clientfd = client;
	cur_client->buf      = NULL;
	cur_client->buf_size = 0;
	cur_client->buf_used = 0;

	tmp = (struct client**) realloc(clients, sizeof(struct client*)*(clients_count+1));
	if (tmp == NULL)
	{
		free(cur_client);
		shutdown(client, SHUT_RDWR);
		close(client);
		return -1;
	}

	++clients_count;
	clients = tmp;
	clients[clients_count-1] = cur_client;

	return 1;
}

int remove_client(int client)
{
	unsigned int i;
	struct client **tmp;
	struct client *del;

	for (i = 0; i < clients_count; ++i)
	{
		if (clients[i]->clientfd == client)
		{
			del = clients[i];
			--clients_count;

			if (clients_count > 0)
			{
				clients[i] = clients[clients_count];
				clients[clients_count] = del;

				tmp = (struct client**) realloc(clients, sizeof(struct client*)*clients_count);
				if (tmp == NULL)
				{
					return -1;
				}

				clients = tmp;
			}
			else
			{
				free(clients);
				clients = NULL;
			}

			shutdown(del->clientfd, SHUT_RDWR);
			close(del->clientfd);

			if (del->buf != NULL)
			{
				free(del->buf);
			}

			free(del);

			return 1;
		}
	}

	return 0;
}

void remove_all_clients(void)
{
	unsigned int i;

	if (clients != NULL)
	{
		for (i = 0; i < media_count; ++i)
		{
			shutdown(clients[i]->clientfd, SHUT_RDWR);
			close(clients[i]->clientfd);

			if (clients[i]->buf != NULL)
			{
				free(clients[i]->buf);
			}

			free(clients[i]);
		}

		free(clients);

		clients_count = 0;
		clients = NULL;
	}
}

const char* removable_type_to_string(enum removable_media_type removable_type)
{
	switch (removable_type)
	{
	case cdrom:
		return str_cdrom;
	case removable_disk:
		return str_removable_disk;
	case sd_card:
		return str_sd_card;
	case unknown_or_persistent:
	default:
		return str_unknown_or_persistent;
	}
}
