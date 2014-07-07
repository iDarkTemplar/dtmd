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
#include "daemon/actions.h"
#include "daemon/log.h"
#include "daemon/return_codes.h"

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
			return result_fail;
		}
	}

	cur_media = (struct removable_media*) malloc(sizeof(struct removable_media));
	if (cur_media == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_media_block_error_1;
	}

	cur_media->path = strdup(path);
	if (cur_media->path == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_media_block_error_2;
	}

	cur_media->partition        = NULL;
	cur_media->partitions_count = 0;
	cur_media->type             = media_type;

	tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*)*(media_count+1));
	if (tmp == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_media_block_error_3;
	}

	++media_count;
	media = tmp;
	media[media_count-1] = cur_media;

	notify_add_disk(cur_media->path, cur_media->type);

	return result_success;

add_media_block_error_3:
	free(cur_media->path);

add_media_block_error_2:
	free(cur_media);

add_media_block_error_1:
	return result_fatal_error;
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

				// TODO: consider non-fatal error on shrinking realloc failure
				tmp = (struct removable_media**) realloc(media, sizeof(struct removable_media*) * media_count);
				if (tmp == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					return result_fatal_error;
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
					if (del->partition[j]->mnt_point != NULL)
					{
						notify_unmount(del->partition[j]->path, del->partition[j]->mnt_point);
					}

					notify_remove_partition(del->partition[j]->path);

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

			notify_remove_disk(del->path);

			free(del->path);
			free(del);

			return result_success;
		}
	}

	return result_fail;
}

int add_media_partition(const char *block, dtmd_removable_media_type_t media_type, const char *partition, const char *fstype, const char *label)
{
	int rc;
	unsigned int i;
	unsigned int j;
	struct removable_partition *cur_partition;
	struct removable_partition **tmp;

	rc = add_media_block(block, media_type);
	if (is_result_fatal_error(rc))
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
					return result_fail;
				}
			}

			cur_partition = (struct removable_partition*) malloc(sizeof(struct removable_partition));
			if (cur_partition == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				goto add_media_partition_error_1;
			}

			cur_partition->path = strdup(partition);
			if (cur_partition->path == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				goto add_media_partition_error_2;
			}

			cur_partition->fstype = strdup(fstype);
			if (cur_partition->fstype == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				goto add_media_partition_error_3;
			}

			if (label != NULL)
			{
				cur_partition->label = decode_label(label);
				if (cur_partition->label == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
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
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				goto add_media_partition_error_5;
			}

			++(media[i]->partitions_count);
			media[i]->partition = tmp;
			media[i]->partition[media[i]->partitions_count - 1] = cur_partition;

			notify_add_partition(cur_partition->path, cur_partition->fstype, cur_partition->label, media[i]->path);

			return result_success;
		}
	}

	WRITE_LOG(LOG_ERR, "BUG: reached code which should be unreachable");
	return result_bug;

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
	return result_fatal_error;
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

						// TODO: consider non-fatal error on shrinking realloc failure
						tmp = (struct removable_partition**) realloc(media[i]->partition, sizeof(struct removable_partition*) * media[i]->partitions_count);
						if (tmp == NULL)
						{
							WRITE_LOG(LOG_ERR, "Memory allocation failure");
							return result_fatal_error;
						}

						media[i]->partition = tmp;
					}
					else
					{
						free(media[i]->partition);
						media[i]->partition = NULL;
					}

					if (del->mnt_point != NULL)
					{
						notify_unmount(del->path, del->mnt_point);
					}

					notify_remove_partition(del->path);

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

					return result_success;
				}
			}
		}
	}

	return result_fail;
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
					if (media[i]->partition[j]->mnt_point != NULL)
					{
						notify_unmount(media[i]->partition[j]->path, media[i]->partition[j]->mnt_point);
					}

					notify_remove_partition(media[i]->partition[j]->path);

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

			notify_remove_disk(media[i]->path);

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
			return result_fail;
		}
	}

	cur_media = (struct removable_stateful_media*) malloc(sizeof(struct removable_stateful_media));
	if (cur_media == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_stateful_media_error_1;
	}

	cur_media->path = strdup(path);
	if (cur_media->path == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_stateful_media_error_2;
	}

	if (fstype != NULL)
	{
		cur_media->fstype = strdup(fstype);
		if (cur_media->fstype == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
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
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
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
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_stateful_media_error_5;
	}

	++stateful_media_count;
	stateful_media = tmp;
	stateful_media[stateful_media_count-1] = cur_media;

	notify_add_stateful_device(cur_media->path, cur_media->type, cur_media->state, cur_media->fstype, cur_media->label);

	return result_success;

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
	return result_fatal_error;
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

				// TODO: consider non-fatal error on shrinking realloc failure
				tmp = (struct removable_stateful_media**) realloc(stateful_media, sizeof(struct removable_stateful_media*) * stateful_media_count);
				if (tmp == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					return result_fatal_error;
				}

				stateful_media = tmp;
			}
			else
			{
				free(stateful_media);
				stateful_media = NULL;
			}

			if (del->mnt_point != NULL)
			{
				notify_unmount(del->path, del->mnt_point);
			}

			notify_remove_stateful_device(del->path);

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

			return result_success;
		}
	}

	return result_fail;
}

int change_stateful_media(const char *path, dtmd_removable_media_type_t media_type, dtmd_removable_media_state_t state, const char *fstype, const char *label)
{
	unsigned int i;

	for (i = 0; i < stateful_media_count; ++i)
	{
		if (strcmp(stateful_media[i]->path, path) == 0)
		{
			if ((stateful_media[i]->type == media_type)
				&& (stateful_media[i]->state == state)
				&& (((stateful_media[i]->fstype == NULL)
						&& (fstype == NULL))
					|| ((stateful_media[i]->fstype != NULL)
						&& (fstype != NULL)
						&& (strcmp(stateful_media[i]->fstype, fstype) == 0)))
				&& (((stateful_media[i]->label == NULL)
						&& (label == NULL))
					|| ((stateful_media[i]->label != NULL)
						&& (label != NULL)
					&& (strcmp(stateful_media[i]->label, label) == 0))))
			{
				// nothing seems to have changed
				return result_fail;
			}

			if (stateful_media[i]->mnt_point != NULL)
			{
				notify_unmount(stateful_media[i]->path, stateful_media[i]->mnt_point);
				stateful_media[i]->is_mounted = 0;
			}

			if (stateful_media[i]->fstype != NULL)
			{
				free(stateful_media[i]->fstype);
				stateful_media[i]->fstype = NULL;
			}

			if (stateful_media[i]->label != NULL)
			{
				free(stateful_media[i]->label);
				stateful_media[i]->label = NULL;
			}

			if (stateful_media[i]->mnt_point != NULL)
			{
				free(stateful_media[i]->mnt_point);
				stateful_media[i]->mnt_point = NULL;
			}

			if (stateful_media[i]->mnt_opts != NULL)
			{
				free(stateful_media[i]->mnt_opts);
				stateful_media[i]->mnt_opts = NULL;
			}

			stateful_media[i]->type  = media_type;
			stateful_media[i]->state = state;

			if (fstype != NULL)
			{
				stateful_media[i]->fstype = strdup(fstype);
				if (stateful_media[i]->fstype == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					return result_fatal_error;
				}
			}

			if (label != NULL)
			{
				stateful_media[i]->label = decode_label(label);
				if (stateful_media[i]->label == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					return result_fatal_error;
				}
			}

			notify_stateful_device_changed(stateful_media[i]->path,
				stateful_media[i]->type,
				stateful_media[i]->state,
				stateful_media[i]->fstype,
				stateful_media[i]->label);

			return result_success;
		}
	}

	WRITE_LOG_ARGS(LOG_ERR, "Caught false event about stateful device change: device name %s", path);
	return result_fail;
}

void remove_all_stateful_media(void)
{
	unsigned int i;

	if (stateful_media_count > 0)
	{
		for (i = 0; i < stateful_media_count; ++i)
		{
			if (stateful_media[i]->mnt_point != NULL)
			{
				notify_unmount(stateful_media[i]->path, stateful_media[i]->mnt_point);
			}

			notify_remove_stateful_device(stateful_media[i]->path);

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
			return result_fail;
		}
	}

	cur_client = (struct client*) malloc(sizeof(struct client));
	if (cur_client == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_client_error_1;
	}

	cur_client->clientfd = client;
	cur_client->buf_used = 0;

	tmp = (struct client**) realloc(clients, sizeof(struct client*)*(clients_count+1));
	if (tmp == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_client_error_2;
	}

	++clients_count;
	clients = tmp;
	clients[clients_count-1] = cur_client;

	return result_success;

add_client_error_2:
	free(cur_client);

add_client_error_1:
	shutdown(client, SHUT_RDWR);
	close(client);
	return result_fatal_error;
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

				// TODO: consider non-fatal error on shrinking realloc failure
				tmp = (struct client**) realloc(clients, sizeof(struct client*)*clients_count);
				if (tmp == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					return result_fatal_error;
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

			return result_success;
		}
	}

	return result_fail;
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
