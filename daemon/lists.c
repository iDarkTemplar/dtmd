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

struct removable_media *removable_media_root = NULL;

struct client *client_root = NULL;
size_t clients_count = 0;

static int find_media_private(const char *path,
	struct removable_media *parent_media_ptr,
	struct removable_media **media_ptr)
{
	int rc;
	struct removable_media *iter_media_ptr = NULL;

	for (iter_media_ptr = parent_media_ptr; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		if (strcmp(path, iter_media_ptr->path) == 0)
		{
			*media_ptr = iter_media_ptr;
			return result_success;
		}

		rc = find_media_private(path, iter_media_ptr, media_ptr);
		if (is_result_successful(rc))
		{
			return result_success;
		}
	}

	return result_fail;
}

struct removable_media* find_media(const char *path)
{
	int rc;
	struct removable_media *media_ptr = NULL;

	rc = find_media_private(path, removable_media_root, &media_ptr);
	if (is_result_successful(rc))
	{
		return media_ptr;
	}
	else
	{
		return NULL;
	}
}

static void remove_media_helper(struct removable_media *media_ptr)
{
	// first unlink node
	if (media_ptr->prev_node != NULL)
	{
		media_ptr->prev_node->next_node = media_ptr->next_node;
	}

	if (media_ptr->next_node != NULL)
	{
		media_ptr->next_node->prev_node = media_ptr->prev_node;
	}

	// then recursively free children
	while (media_ptr->first_child != NULL)
	{
		remove_media_helper(media_ptr->first_child);
	}

	notify_removable_device_removed(media_ptr->path);

	// and free node itself
	free(media_ptr->path);

	if (media_ptr->fstype != NULL)
	{
		free(media_ptr->fstype);
	}

	if (media_ptr->label != NULL)
	{
		free(media_ptr->label);
	}

	if (media_ptr->mnt_point != NULL)
	{
		free(media_ptr->mnt_point);
	}

	if (media_ptr->mnt_opts != NULL)
	{
		free(media_ptr->mnt_opts);
	}

	free(media_ptr);
}

int add_media(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
	int rc;
	int is_parent_path = 0;
	struct removable_media *media_ptr = NULL;
	struct removable_media *constructed_media = NULL;

	if (strcmp(parent_path, dtmd_root_device_path) == 0)
	{
		is_parent_path = 1;
	}

	if (!is_parent_path)
	{
		rc = find_media_private(parent_path, removable_media_root, &media_ptr);
		if (is_result_failure(rc))
		{
			return rc;
		}
	}

	constructed_media = (struct removable_media *) malloc(sizeof(struct removable_media));
	if (constructed_media == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_media_error_1;
	}

	constructed_media->path = strdup(path);
	if (constructed_media->path == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		goto add_media_error_2;
	}

	constructed_media->type = media_type;
	constructed_media->subtype = media_subtype;
	constructed_media->state = state;

	if (fstype != NULL)
	{
		constructed_media->fstype = strdup(fstype);
		if (constructed_media->fstype == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto add_media_error_3;
		}
	}
	else
	{
		constructed_media->fstype = NULL;
	}

	if (label != NULL)
	{
		constructed_media->label = strdup(label);
		if (constructed_media->label == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto add_media_error_4;
		}
	}
	else
	{
		constructed_media->label = NULL;
	}

	if (mnt_point != NULL)
	{
		constructed_media->mnt_point = strdup(mnt_point);
		if (constructed_media->mnt_point == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto add_media_error_5;
		}
	}
	else
	{
		constructed_media->mnt_point = NULL;
	}

	if (mnt_opts != NULL)
	{
		constructed_media->mnt_opts = strdup(mnt_opts);
		if (constructed_media->mnt_opts == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			goto add_media_error_6;
		}
	}
	else
	{
		constructed_media->mnt_opts = NULL;
	}

	constructed_media->first_child = NULL;
	constructed_media->last_child = NULL;

	if (is_parent_path)
	{
		constructed_media->parent = NULL;

		if (removable_media_root != NULL)
		{
			media_ptr = removable_media_root;

			while (media_ptr->next_node != NULL)
			{
				media_ptr = media_ptr->next_node;
			}

			constructed_media->prev_node = media_ptr;
			media_ptr->next_node = constructed_media;
		}
		else
		{
			removable_media_root = constructed_media;
			constructed_media->prev_node = NULL;
		}
	}
	else
	{
		constructed_media->parent = media_ptr;

		if (media_ptr->last_child != NULL)
		{
			media_ptr->last_child->next_node = constructed_media;
			constructed_media->prev_node = media_ptr->last_child;
			media_ptr->last_child = constructed_media;
		}
		else
		{
			media_ptr->first_child = media_ptr->last_child = constructed_media;
			constructed_media->prev_node = NULL;
		}
	}

	constructed_media->next_node = NULL;

	notify_removable_device_added(parent_path,
		path,
		media_type,
		media_subtype,
		state,
		fstype,
		label,
		mnt_point,
		mnt_opts);

	return result_success;

add_media_error_7:
	if (constructed_media->mnt_opts != NULL)
	{
		free(constructed_media->mnt_opts);
	}

add_media_error_6:
	if (constructed_media->mnt_point != NULL)
	{
		free(constructed_media->mnt_point);
	}

add_media_error_5:
	if (constructed_media->label != NULL)
	{
		free(constructed_media->label);
	}

add_media_error_4:
	if (constructed_media->fstype != NULL)
	{
		free(constructed_media->fstype);
	}

add_media_error_3:
	free(constructed_media->path);

add_media_error_2:
	free(constructed_media);

add_media_error_1:
	return result_fatal_error;
}

int remove_media(const char *path)
{
	int rc;
	struct removable_media *media_ptr = NULL;

	rc = find_media_private(path, removable_media_root, &media_ptr);
	if (is_result_failure(rc))
	{
		return rc;
	}

	// make sure removable_media_root stays valid
	if (media_ptr == removable_media_root)
	{
		removable_media_root = media_ptr->next_node;
	}

	remove_media_helper(media_ptr);

	return result_success;
}

int change_media(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
	int rc;
	struct removable_media *media_ptr = NULL;

	rc = find_media_private(path, removable_media_root, &media_ptr);
	if (is_result_failure(rc))
	{
		WRITE_LOG_ARGS(LOG_ERR, "Caught false event about stateful device change: device name %s", path);
		return rc;
	}

	/* Check parent path, it must not change */
	if (parent_path != ((media_ptr->parent != NULL) ? media_ptr->parent->path : dtmd_root_device_path))
	{
		WRITE_LOG_ARGS(LOG_ERR,
			"Parent path for device \"%s\" changed from \"%s\" to \"%s\"",
			path,
			((media_ptr->parent != NULL) ? media_ptr->parent->path : dtmd_root_device_path),
			parent_path);

		return result_bug;
	}

	if ((media_ptr->type == media_type)
		&& (media_ptr->subtype == media_subtype)
		&& (media_ptr->state == state)
		&& (((media_ptr->fstype == NULL)
				&& (fstype == NULL))
			|| ((media_ptr->fstype != NULL)
				&& (fstype != NULL)
				&& (strcmp(media_ptr->fstype, fstype) == 0)))
		&& (((media_ptr->label == NULL)
				&& (label == NULL))
			|| ((media_ptr->label != NULL)
				&& (label != NULL)
				&& (strcmp(media_ptr->label, label) == 0)))
		&& (((media_ptr->mnt_point == NULL)
				&& (mnt_point == NULL))
			|| ((media_ptr->mnt_point != NULL)
				&& (mnt_point != NULL)
				&& (strcmp(media_ptr->mnt_point, mnt_point) == 0)))
		&& (((media_ptr->mnt_opts == NULL)
				&& (mnt_opts == NULL))
			|| ((media_ptr->mnt_opts != NULL)
				&& (mnt_opts != NULL)
				&& (strcmp(media_ptr->mnt_opts, mnt_opts) == 0))))
	{
		// nothing seems to have changed
		return result_fail;
	}

	if ((media_ptr->fstype != NULL)
		&& (((fstype != NULL) && (strcmp(media_ptr->fstype, fstype) != 0))
			|| (fstype == NULL)))
	{
		free(media_ptr->fstype);
		media_ptr->fstype = NULL;
	}

	if ((media_ptr->fstype == NULL) && (fstype != NULL))
	{
		media_ptr->fstype = strdup(fstype);
		if (media_ptr->fstype == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			return result_fatal_error;
		}
	}

	if ((media_ptr->label != NULL)
		&& (((label != NULL) && (is_result_failure(compare_labels(media_ptr->label, label))))
			|| (label == NULL)))
	{
		free(media_ptr->label);
		media_ptr->label = NULL;
	}

	if ((media_ptr->label == NULL) && (label != NULL))
	{
		media_ptr->label = decode_label(label);
		if (media_ptr->label == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			return result_fatal_error;
		}
	}

	if ((media_ptr->mnt_point != NULL)
		&& (((mnt_point != NULL) && (strcmp(media_ptr->mnt_point, mnt_point) != 0))
			|| (mnt_point == NULL)))
	{
		free(media_ptr->mnt_point);
		media_ptr->mnt_point = NULL;
	}

	if ((media_ptr->mnt_point == NULL) && (mnt_point != NULL))
	{
		media_ptr->mnt_point = strdup(mnt_point);
		if (media_ptr->mnt_point == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			return result_fatal_error;
		}
	}

	if ((media_ptr->mnt_opts != NULL)
		&& (((mnt_opts != NULL) && (strcmp(media_ptr->mnt_opts, mnt_opts) != 0))
			|| (mnt_opts == NULL)))
	{
		free(media_ptr->mnt_opts);
		media_ptr->mnt_opts = NULL;
	}

	if ((media_ptr->mnt_opts == NULL) && (mnt_opts != NULL))
	{
		media_ptr->mnt_opts = strdup(mnt_opts);
		if (media_ptr->mnt_opts == NULL)
		{
			WRITE_LOG(LOG_ERR, "Memory allocation failure");
			return result_fatal_error;
		}
	}

	return result_success;
}

void remove_all_media(void)
{
	struct removable_media *next = NULL;
	struct removable_media *cur = NULL;

	next = removable_media_root;

	while (next != NULL)
	{
		cur = next;
		next = cur->next_node;
		remove_media_helper(cur);
	}

	removable_media_root = NULL;
}

int add_client(int client_fd)
{
	struct client *cur_client;
	struct client *client_iter;

	cur_client = client_root;

	while (cur_client != NULL)
	{
		client_iter = cur_client;
		cur_client = cur_client->next_node;

		if (client_iter->clientfd == client_fd)
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

	if (client_iter != NULL)
	{
		client_iter->next_node = cur_client;
		cur_client->prev_node = client_iter;
	}
	else
	{
		cur_client->prev_node = NULL;
		client_root = cur_client;
	}

	cur_client->next_node = NULL;

	++clients_count;

	return result_success;

/*
add_client_error_2:
	free(cur_client);
*/

add_client_error_1:
	shutdown(client_fd, SHUT_RDWR);
	close(client_fd);
	return result_fatal_error;
}

int remove_client(int client_fd)
{
	struct client *cur_client;

	for (cur_client = client_root; cur_client != NULL; cur_client = cur_client->next_node)
	{
		if (cur_client->clientfd == client_fd)
		{
			break;
		}
	}

	if (cur_client == NULL)
	{
		return result_fail;
	}

	if (cur_client->prev_node != NULL)
	{
		cur_client->prev_node->next_node = cur_client->next_node;
	}

	if (cur_client->next_node != NULL)
	{
		cur_client->next_node->prev_node = cur_client->prev_node;
	}

	// make sure client_root stays valid
	if (cur_client == client_root)
	{
		client_root = cur_client->next_node;
	}

	shutdown(cur_client->clientfd, SHUT_RDWR);
	close(cur_client->clientfd);
	free(cur_client);
	--clients_count;

	return result_success;
}

void remove_all_clients(void)
{
	struct client *next;
	struct client *cur;

	next = client_root;

	while (next != NULL)
	{
		cur = next;
		next = cur->next_node;

		shutdown(cur->clientfd, SHUT_RDWR);
		close(cur->clientfd);
		free(cur);
	}

	client_root = NULL;
	clients_count = 0;
}
