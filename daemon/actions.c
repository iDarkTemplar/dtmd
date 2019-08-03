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

#if (defined OS_FreeBSD)
#define _WITH_DPRINTF
#endif /* (defined OS_FreeBSD) */

#include "daemon/actions.h"

#include "daemon/filesystem_mnt.h"
#include "daemon/filesystem_opts.h"
#include "daemon/poweroff.h"
#include "daemon/return_codes.h"

#include <dtmd.h>

#include <stdio.h>
#include <string.h>

static int print_removable_device_common(const char *action,
	struct client *client_ptr,
	const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts);

static int print_all_removable_devices_recursive(struct client *client_ptr, dtmd_removable_media_t *media_ptr);

static int print_all_removable_devices(struct client *client_ptr);

int invoke_command(struct client *client_ptr, dt_command_t *cmd)
{
	int rc;
	int is_parent_path = 0;
	dtmd_error_code_t error_code;
	dtmd_removable_media_t *media_ptr = NULL;

	if ((strcmp(cmd->cmd, dtmd_command_list_all_removable_devices) == 0) && (cmd->args_count == 0))
	{
		if (dprintf(client_ptr->clientfd, dtmd_response_started "(\"" dtmd_command_list_all_removable_devices "\")\n") < 0)
		{
			return result_client_error;
		}

		rc = print_all_removable_devices(client_ptr);
		if (is_result_failure(rc))
		{
			return rc;
		}

		if (dprintf(client_ptr->clientfd, dtmd_response_finished "(\"" dtmd_command_list_all_removable_devices "\")\n") < 0)
		{
			return result_client_error;
		}

		return result_success;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_removable_device) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		if (strcmp(cmd->args[0], dtmd_root_device_path) == 0)
		{
			is_parent_path = 1;
		}
		else
		{
			media_ptr = dtmd_find_media(cmd->args[0], removable_media_root);
			if (media_ptr == NULL)
			{
				if (dprintf(client_ptr->clientfd, dtmd_response_failed "(\"" dtmd_command_list_removable_device "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(dtmd_error_code_no_such_removable_device)) < 0)
				{
					return result_client_error;
				}

				return result_fail;
			}
		}

		if (dprintf(client_ptr->clientfd, dtmd_response_started "(\"" dtmd_command_list_removable_device "\", \"%s\")\n", cmd->args[0]) < 0)
		{
			return result_client_error;
		}

		if (is_parent_path)
		{
			rc = print_all_removable_devices(client_ptr);
		}
		else
		{
			rc = print_all_removable_devices_recursive(client_ptr, media_ptr);
		}

		if (is_result_failure(rc))
		{
			return rc;
		}

		if (dprintf(client_ptr->clientfd, dtmd_response_finished "(\"" dtmd_command_list_removable_device "\", \"%s\")\n", cmd->args[0]) < 0)
		{
			return result_client_error;
		}

		return result_success;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_mount) == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL))
	{
		rc = invoke_mount(client_ptr, cmd->args[0], cmd->args[1], mount_by_value, &error_code);

		if (is_result_successful(rc))
		{
			if (dprintf(client_ptr->clientfd, dtmd_response_succeeded "(\"" dtmd_command_mount "\", \"%s\", %s%s%s)\n",
				cmd->args[0],
				((cmd->args[1] != NULL) ? ("\"") : ("")),
				((cmd->args[1] != NULL) ? (cmd->args[1]) : ("nil")),
				((cmd->args[1] != NULL) ? ("\"") : (""))) < 0)
			{
				return result_client_error;
			}
		}
		else
		{
			if (dprintf(client_ptr->clientfd, dtmd_response_failed "(\"" dtmd_command_mount "\", \"%s\", %s%s%s, \"%s\")\n",
				cmd->args[0],
				((cmd->args[1] != NULL) ? ("\"") : ("")),
				((cmd->args[1] != NULL) ? (cmd->args[1]) : ("nil")),
				((cmd->args[1] != NULL) ? ("\"") : ("")),
				dtmd_error_code_to_string(error_code)) < 0)
			{
				return result_client_error;
			}
		}

		return rc;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_unmount) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		rc = invoke_unmount(client_ptr, cmd->args[0], &error_code);

		if (is_result_successful(rc))
		{
			if (dprintf(client_ptr->clientfd, dtmd_response_succeeded "(\"" dtmd_command_unmount "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}
		}
		else
		{
			if (dprintf(client_ptr->clientfd, dtmd_response_failed "(\"" dtmd_command_unmount "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(error_code)) < 0)
			{
				return result_client_error;
			}
		}

		return rc;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_supported_filesystems) == 0) && (cmd->args_count == 0))
	{
		return invoke_list_supported_filesystems(client_ptr);
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_supported_filesystem_options) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		return invoke_list_supported_filesystem_options(client_ptr, cmd->args[0]);
	}
	else if ((strcmp(cmd->cmd, dtmd_command_poweroff) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		rc = invoke_poweroff(client_ptr, cmd->args[0], &error_code);

		if (is_result_successful(rc))
		{
			if (dprintf(client_ptr->clientfd, dtmd_response_succeeded "(\"" dtmd_command_poweroff "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}
		}
		else
		{
			if (dprintf(client_ptr->clientfd, dtmd_response_failed "(\"" dtmd_command_poweroff "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(error_code)) < 0)
			{
				return result_client_error;
			}
		}

		return rc;
	}
	else
	{
		return result_fail;
	}
}

void notify_removable_device_added(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
	struct client *cur_client;

	for (cur_client = client_root; cur_client != NULL; cur_client = cur_client->next_node)
	{
		print_removable_device_common(dtmd_notification_removable_device_added,
			cur_client,
			parent_path,
			path,
			media_type,
			media_subtype,
			state,
			fstype,
			label,
			mnt_point,
			mnt_opts);
	}
}

void notify_removable_device_removed(const char *path)
{
	struct client *cur_client;

	for (cur_client = client_root; cur_client != NULL; cur_client = cur_client->next_node)
	{
		dprintf(cur_client->clientfd, dtmd_notification_removable_device_removed "(\"%s\")\n", path);
	}
}

void notify_removable_device_changed(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
	struct client *cur_client;

	for (cur_client = client_root; cur_client != NULL; cur_client = cur_client->next_node)
	{
		print_removable_device_common(dtmd_notification_removable_device_changed,
			cur_client,
			parent_path,
			path,
			media_type,
			media_subtype,
			state,
			fstype,
			label,
			mnt_point,
			mnt_opts);
	}
}

void notify_removable_device_mounted(const char *path, const char *mount_point, const char *mount_options)
{
	struct client *cur_client;

	for (cur_client = client_root; cur_client != NULL; cur_client = cur_client->next_node)
	{
		dprintf(cur_client->clientfd, dtmd_notification_removable_device_mounted "(\"%s\", \"%s\", \"%s\")\n", path, mount_point, mount_options);
	}
}

void notify_removable_device_unmounted(const char *path, const char *mount_point)
{
	struct client *cur_client;

	for (cur_client = client_root; cur_client != NULL; cur_client = cur_client->next_node)
	{
		dprintf(cur_client->clientfd, dtmd_notification_removable_device_unmounted "(\"%s\", \"%s\")\n", path, mount_point);
	}
}

static int print_removable_device_common(const char *action,
	struct client *client_ptr,
	const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
	int rc = result_success;

	switch (media_type)
	{
	case dtmd_removable_media_type_device_partition:
		if (dprintf(client_ptr->clientfd, "%s(\"%s\", \"%s\", \"%s\", %s%s%s, %s%s%s, %s%s%s, %s%s%s)\n",
			action,
			parent_path,
			path,
			dtmd_device_type_to_string(media_type),
			((fstype != NULL) ? ("\"") : ("")),
			((fstype != NULL) ? (fstype) : ("nil")),
			((fstype != NULL) ? ("\"") : ("")),
			((label != NULL) ? ("\"") : ("")),
			((label != NULL) ? (label) : ("nil")),
			((label != NULL) ? ("\"") : ("")),
			((mnt_point != NULL) ? ("\"") : ("")),
			((mnt_point != NULL) ? (mnt_point) : ("nil")),
			((mnt_point != NULL) ? ("\"") : ("")),
			((mnt_opts != NULL) ? ("\"") : ("")),
			((mnt_opts != NULL) ? (mnt_opts) : ("nil")),
			((mnt_opts != NULL) ? ("\"") : (""))) < 0)
		{
			rc = result_client_error;
		}
		break;

	case dtmd_removable_media_type_stateless_device:
		if (dprintf(client_ptr->clientfd, "%s(\"%s\", \"%s\", \"%s\", \"%s\")\n",
			action,
			parent_path,
			path,
			dtmd_device_type_to_string(media_type),
			dtmd_device_subtype_to_string(media_subtype)) < 0)
		{
			rc = result_client_error;
		}
		break;

	case dtmd_removable_media_type_stateful_device:
		if (dprintf(client_ptr->clientfd, "%s(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", %s%s%s, %s%s%s, %s%s%s, %s%s%s)\n",
			action,
			parent_path,
			path,
			dtmd_device_type_to_string(media_type),
			dtmd_device_subtype_to_string(media_subtype),
			dtmd_device_state_to_string(state),
			((fstype != NULL) ? ("\"") : ("")),
			((fstype != NULL) ? (fstype) : ("nil")),
			((fstype != NULL) ? ("\"") : ("")),
			((label != NULL) ? ("\"") : ("")),
			((label != NULL) ? (label) : ("nil")),
			((label != NULL) ? ("\"") : ("")),
			((mnt_point != NULL) ? ("\"") : ("")),
			((mnt_point != NULL) ? (mnt_point) : ("nil")),
			((mnt_point != NULL) ? ("\"") : ("")),
			((mnt_opts != NULL) ? ("\"") : ("")),
			((mnt_opts != NULL) ? (mnt_opts) : ("nil")),
			((mnt_opts != NULL) ? ("\"") : (""))) < 0)
		{
			rc = result_client_error;
		}
		break;

	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		rc = result_fail;
		break;
	}

	return rc;
}

static int print_all_removable_devices_recursive(struct client *client_ptr, dtmd_removable_media_t *media_ptr)
{
	int rc;
	dtmd_removable_media_t *iter_media_ptr;

	rc = print_removable_device_common(dtmd_response_argument_removable_device,
		client_ptr,
		((media_ptr->parent != NULL) ? media_ptr->parent->path : dtmd_root_device_path),
		media_ptr->path,
		media_ptr->type,
		media_ptr->subtype,
		media_ptr->state,
		media_ptr->fstype,
		media_ptr->label,
		media_ptr->mnt_point,
		media_ptr->mnt_opts);

	if (is_result_failure(rc))
	{
		return rc;
	}

	for (iter_media_ptr = media_ptr->children_list; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		rc = print_all_removable_devices_recursive(client_ptr, iter_media_ptr);
		if (is_result_failure(rc))
		{
			return rc;
		}
	}

	return result_success;
}

static int print_all_removable_devices(struct client *client_ptr)
{
	int rc;
	dtmd_removable_media_t *iter_media_ptr;

	for (iter_media_ptr = removable_media_root; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		rc = print_all_removable_devices_recursive(client_ptr, iter_media_ptr);
		if (is_result_failure(rc))
		{
			return rc;
		}
	}

	return result_success;
}
