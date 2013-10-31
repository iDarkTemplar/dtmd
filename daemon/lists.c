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

#include "daemon/lists.h"

#include "daemon/label.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct removable_media **media = NULL;
unsigned int media_count = 0;

struct removable_stateful_media **stateful_media = NULL;
unsigned int stateful_media_count = 0;

struct client **clients = NULL;
unsigned int clients_count = 0;

int add_media_block(const char *path, dtmd_removable_media_type_t media_type)
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
		goto add_media_block_error_1;
	}

	cur_media->path = strdup(path);
	if (cur_media->path == NULL)
	{
		goto add_media_block_error_2;
	}

	cur_media->partition        = NULL;
	cur_media->partitions_count = 0;
	cur_media->type             = media_type;

	tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*)*(media_count+1));
	if (tmp == NULL)
	{
		goto add_media_block_error_3;
	}

	++media_count;
	media = tmp;
	media[media_count-1] = cur_media;

	return 1;

add_media_block_error_3:
	free(cur_media->path);

add_media_block_error_2:
	free(cur_media);

add_media_block_error_1:
	return -1;
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

					if (del->partition[j]->mnt_point != NULL)
					{
						free(del->partition[j]->mnt_point);
					}

					if (del->partition[j]->mnt_opts != NULL)
					{
						free(del->partition[j]->mnt_opts);
					}

					free(del->partition[j]->path);
					free(del->partition[j]->fstype);
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

int add_media_partition(const char *block, dtmd_removable_media_type_t media_type, const char *partition, const char *fstype, const char *label)
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
				goto add_media_partition_error_1;
			}

			cur_partition->path = strdup(partition);
			if (cur_partition->path == NULL)
			{
				goto add_media_partition_error_2;
			}

			cur_partition->fstype = strdup(fstype);
			if (cur_partition->fstype == NULL)
			{
				goto add_media_partition_error_3;
			}

			if (label != NULL)
			{
				cur_partition->label = decode_label(label);
				if (cur_partition->label == NULL)
				{
					goto add_media_partition_error_4;
				}
			}
			else
			{
				cur_partition->label = NULL;
			}

			cur_partition->mnt_point  = NULL;
			cur_partition->mnt_opts   = NULL;
			cur_partition->is_mounted = 0;

			tmp = (struct removable_partition**) realloc(media[i]->partition, sizeof(struct removable_partition*) * (media[i]->partitions_count + 1));
			if (tmp == NULL)
			{
				goto add_media_partition_error_5;
			}

			++(media[i]->partitions_count);
			media[i]->partition = tmp;
			media[i]->partition[media[i]->partitions_count - 1] = cur_partition;

			return 1;
		}
	}

	return -2; // -2 means bug

add_media_partition_error_5:
	if (cur_partition->label != NULL)
	{
		free(cur_partition->label);
	}

add_media_partition_error_4:
	free(cur_partition->fstype);

add_media_partition_error_3:
	free(cur_partition->path);

add_media_partition_error_2:
	free(cur_partition);

add_media_partition_error_1:
	return -1;
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
					free(del->fstype);
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
					free(media[i]->partition[j]->fstype);
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

int add_stateful_media(const char *path, dtmd_removable_media_type_t media_type, dtmd_removable_media_state_t state, const char *fstype, const char *label)
{
	unsigned int i;
	struct removable_stateful_media *cur_media;
	struct removable_stateful_media **tmp;

	for (i = 0; i < stateful_media_count; ++i)
	{
		if (strcmp(stateful_media[i]->path, path) == 0)
		{
			return 0;
		}
	}

	cur_media = (struct removable_stateful_media*) malloc(sizeof(struct removable_stateful_media));
	if (cur_media == NULL)
	{
		goto add_stateful_media_error_1;
	}

	cur_media->path = strdup(path);
	if (cur_media->path == NULL)
	{
		goto add_stateful_media_error_2;
	}

	if (fstype != NULL)
	{
		cur_media->fstype = strdup(fstype);
		if (cur_media->fstype == NULL)
		{
			goto add_stateful_media_error_3;
		}
	}
	else
	{
		cur_media->fstype = NULL;
	}

	if (label != NULL)
	{
		cur_media->label = decode_label(label);
		if (cur_media->label == NULL)
		{
			goto add_stateful_media_error_4;
		}
	}
	else
	{
		cur_media->label = NULL;
	}

	cur_media->type       = media_type;
	cur_media->state      = state;
	cur_media->is_mounted = 0;
	cur_media->mnt_point  = NULL;
	cur_media->mnt_opts   = NULL;

	tmp = (struct removable_stateful_media**) realloc(stateful_media, sizeof(struct removable_stateful_media*)*(stateful_media_count+1));
	if (tmp == NULL)
	{
		goto add_stateful_media_error_5;
	}

	++stateful_media_count;
	stateful_media = tmp;
	stateful_media[stateful_media_count-1] = cur_media;

	return 1;

add_stateful_media_error_5:
	if (cur_media->label != NULL)
	{
		free(cur_media->label);
	}

add_stateful_media_error_4:
	if (cur_media->fstype != NULL)
	{
		free(cur_media->fstype);
	}

add_stateful_media_error_3:
	free(cur_media->path);

add_stateful_media_error_2:
	free(cur_media);

add_stateful_media_error_1:
	return -1;
}

int remove_stateful_media(const char *path)
{
	unsigned int i;
	struct removable_stateful_media **tmp;
	struct removable_stateful_media *del;

	for (i = 0; i < stateful_media_count; ++i)
	{
		if (strcmp(stateful_media[i]->path, path) == 0)
		{
			del = stateful_media[i];
			--stateful_media_count;

			if (stateful_media_count > 0)
			{
				stateful_media[i] = stateful_media[stateful_media_count];
				stateful_media[stateful_media_count] = del;

				tmp = (struct removable_stateful_media**) realloc(stateful_media, sizeof(struct removable_stateful_media*) * stateful_media_count);
				if (tmp == NULL)
				{
					return -1;
				}

				stateful_media = tmp;
			}
			else
			{
				free(stateful_media);
				stateful_media = NULL;
			}

			if (del->fstype != NULL)
			{
				free(del->fstype);
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
			free(del);

			return 1;
		}
	}

	return 0;
}

void remove_all_stateful_media(void)
{
	unsigned int i;

	if (stateful_media_count > 0)
	{
		for (i = 0; i < stateful_media_count; ++i)
		{
			if (stateful_media[i]->fstype != NULL)
			{
				free(stateful_media[i]->fstype);
			}

			if (stateful_media[i]->label != NULL)
			{
				free(stateful_media[i]->label);
			}

			if (stateful_media[i]->mnt_point != NULL)
			{
				free(stateful_media[i]->mnt_point);
			}

			if (stateful_media[i]->mnt_opts != NULL)
			{
				free(stateful_media[i]->mnt_opts);
			}

			free(stateful_media[i]->path);
			free(stateful_media[i]);
		}

		free(stateful_media);

		stateful_media_count = 0;
		stateful_media = NULL;
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
		goto add_client_error_1;
	}

	cur_client->clientfd = client;
	cur_client->buf_used = 0;

	tmp = (struct client**) realloc(clients, sizeof(struct client*)*(clients_count+1));
	if (tmp == NULL)
	{
		goto add_client_error_2;
	}

	++clients_count;
	clients = tmp;
	clients[clients_count-1] = cur_client;

	return 1;

add_client_error_2:
	free(cur_client);

add_client_error_1:
	shutdown(client, SHUT_RDWR);
	close(client);
	return -1;
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
		for (i = 0; i < clients_count; ++i)
		{
			shutdown(clients[i]->clientfd, SHUT_RDWR);
			close(clients[i]->clientfd);
			free(clients[i]);
		}

		free(clients);

		clients_count = 0;
		clients = NULL;
	}
}
