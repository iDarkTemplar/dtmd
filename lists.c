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

int *clients = NULL;
unsigned int clients_count = 0;

int add_media_block(const char *path)
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
				for (j = i+1; j < media_count+1; ++j)
				{
					media[j-1] = media[j];
				}

				media[media_count] = del;

				tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*)*media_count);
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

int add_media_partition(const char *block, const char *partition)
{
	int rc;
	unsigned int i;
	unsigned int j;
	char *cur_partition;
	char **tmp;

	rc = add_media_block(block);
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
				if (strcmp(media[i]->partition[j], partition) == 0)
				{
					return 0;
				}
			}

			cur_partition = strdup(partition);
			if (cur_partition == NULL)
			{
				return -1;
			}

			tmp = (char**) realloc(media[i]->partition, sizeof(char*)*(media[i]->partitions_count + 1));
			if (tmp == NULL)
			{
				free(cur_partition);
				return -1;
			}

			media[i]->partition = tmp;
			++(media[i]->partitions_count);
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
	unsigned int k;
	char **tmp;
	char *del;

	for (i = 0; i < media_count; ++i)
	{
		if (((block == NULL) || (strcmp(block, media[i]->path) == 0)) && (media[i]->partitions_count > 0))
		{
			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				if (strcmp(partition, media[i]->partition[j]) == 0)
				{
					del = media[i]->partition[j];
					--(media[i]->partitions_count);

					if (media[i]->partitions_count > 0)
					{
						for (k = j+1; k < media[i]->partitions_count+1; ++k)
						{
							media[i]->partition[k-1] = media[i]->partition[k];
						}

						media[i]->partition[media[i]->partitions_count] = del;

						tmp = (char**) realloc(media[i]->partition, sizeof(char*) * media[i]->partitions_count);
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
	int *tmp;

	for (i = 0; i < clients_count; ++i)
	{
		if (clients[i] == client)
		{
			return 0;
		}
	}

	tmp = (int*) realloc(clients, sizeof(int)*(clients_count+1));
	if (tmp == NULL)
	{
		shutdown(client, SHUT_RDWR);
		close(client);
		return -1;
	}

	++clients_count;
	clients = tmp;
	clients[clients_count-1] = client;

	return 1;
}

int remove_client(int client)
{
	unsigned int i;
	unsigned int j;
	int *tmp;
	int del;

	for (i = 0; i < clients_count; ++i)
	{
		if (clients[i] == client)
		{
			del = clients[i];
			--clients_count;

			if (clients_count > 0)
			{
				for (j = i+1; j < clients_count+1; ++j)
				{
					clients[j-1] = clients[j];
				}

				clients[clients_count] = del;

				tmp = (int*) realloc(clients, sizeof(int)*media_count);
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

			shutdown(del, SHUT_RDWR);
			close(del);

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
			shutdown(clients[i], SHUT_RDWR);
			close(clients[i]);
		}

		free(clients);

		clients_count = 0;
		clients = NULL;
	}
}
