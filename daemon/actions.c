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

#if (defined OS_FreeBSD)
#define _WITH_DPRINTF
#endif /* (defined OS_FreeBSD) */

#include "daemon/actions.h"

#include "daemon/filesystem_mnt.h"
#include "daemon/filesystem_opts.h"
#include "daemon/return_codes.h"

#include <dtmd.h>

#include <stdio.h>
#include <string.h>

static int print_devices_count(int client_number);

static int print_device(int client_number, unsigned int device);

static int print_partition(int client_number, unsigned int device, unsigned int partition);

static int print_stateful_devices_count(int client_number);

static int print_stateful_device(int client_number, unsigned int device);

int invoke_command(int client_number, dtmd_command_t *cmd)
{
	unsigned int i;
	unsigned int j;
	int rc;
	dtmd_error_code_t error_code;

	if ((strcmp(cmd->cmd, dtmd_command_enum_all) == 0) && (cmd->args_count == 0))
	{
		if (dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_enum_all "\")\n") < 0)
		{
			return result_client_error;
		}

		rc = print_devices_count(client_number);
		if (is_result_failure(rc))
		{
			return rc;
		}

		for (i = 0; i < media_count; ++i)
		{
			rc = print_device(client_number, i);
			if (is_result_failure(rc))
			{
				return rc;
			}

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				rc = print_partition(client_number, i, j);
				if (is_result_failure(rc))
				{
					return rc;
				}
			}
		}

		rc = print_stateful_devices_count(client_number);
		if (is_result_failure(rc))
		{
			return rc;
		}

		for (i = 0; i < stateful_media_count; ++i)
		{
			rc = print_stateful_device(client_number, i);
			if (is_result_failure(rc))
			{
				return rc;
			}
		}

		if (dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_enum_all "\")\n") < 0)
		{
			return result_client_error;
		}

		return result_success;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_device) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		for (i = 0; i < media_count; ++i)
		{
			if (strcmp(media[i]->path, cmd->args[0]) == 0)
			{
				break;
			}
		}

		if (i < media_count)
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_device "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}

			rc = print_device(client_number, i);
			if (is_result_failure(rc))
			{
				return rc;
			}

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				rc = print_partition(client_number, i, j);
				if (is_result_failure(rc))
				{
					return rc;
				}
			}

			if (dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_list_device "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}

			return result_success;
		}
		else
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_device "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(dtmd_error_code_no_such_removable_device)) < 0)
			{
				return result_client_error;
			}

			return result_fail;
		}
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_partition) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		for (i = 0; i < media_count; ++i)
		{
			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				if (strcmp(media[i]->partition[j]->path, cmd->args[0]) == 0)
				{
					goto invoke_command_print_partition_exit_loop;
				}
			}
		}

		invoke_command_print_partition_exit_loop:

		if (i < media_count)
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_partition "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}

			rc = print_partition(client_number, i, j);
			if (is_result_failure(rc))
			{
				return rc;
			}

			if (dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_list_partition "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}

			return result_success;
		}
		else
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_partition "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(dtmd_error_code_no_such_removable_device)) < 0)
			{
				return result_client_error;
			}

			return result_fail;
		}
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_stateful_device) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		for (i = 0; i < stateful_media_count; ++i)
		{
			if (strcmp(stateful_media[i]->path, cmd->args[0]) == 0)
			{
				break;
			}
		}

		if (i < stateful_media_count)
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_stateful_device "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}

			rc = print_stateful_device(client_number, i);
			if (is_result_failure(rc))
			{
				return rc;
			}

			if (dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_list_stateful_device "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}

			return result_success;
		}
		else
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_stateful_device "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(dtmd_error_code_no_such_removable_device)) < 0)
			{
				return result_client_error;
			}

			return result_fail;
		}
	}
	else if ((strcmp(cmd->cmd, dtmd_command_mount) == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL))
	{
		rc = invoke_mount(client_number, cmd->args[0], cmd->args[1], mount_by_value, &error_code);

		if (is_result_successful(rc))
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_succeeded "(\"" dtmd_command_mount "\", \"%s\", %s%s%s)\n",
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
			if (dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_mount "\", \"%s\", %s%s%s, \"%s\")\n",
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
		rc = invoke_unmount(client_number, cmd->args[0], &error_code);

		if (is_result_successful(rc))
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_succeeded "(\"" dtmd_command_unmount "\", \"%s\")\n", cmd->args[0]) < 0)
			{
				return result_client_error;
			}
		}
		else
		{
			if (dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_unmount "\", \"%s\", \"%s\")\n", cmd->args[0], dtmd_error_code_to_string(error_code)) < 0)
			{
				return result_client_error;
			}
		}

		return rc;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_supported_filesystems) == 0) && (cmd->args_count == 0))
	{
		return invoke_list_supported_filesystems(client_number);
	}
	else if ((strcmp(cmd->cmd, dtmd_command_list_supported_filesystem_options) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		return invoke_list_supported_filesystem_options(client_number, cmd->args[0]);
	}
	else
	{
		return result_fail;
	}
}

void notify_add_disk(const char *path, dtmd_removable_media_type_t type)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_add_disk "(\"%s\", \"%s\")\n", path, dtmd_device_type_to_string(type));
	}
}

void notify_remove_disk(const char *path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_remove_disk "(\"%s\")\n", path);
	}
}

void notify_disk_changed(const char *path, dtmd_removable_media_type_t type)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_disk_changed "(\"%s\", \"%s\")\n", path, dtmd_device_type_to_string(type));
	}
}

void notify_add_partition(const char *path, const char *fstype, const char *label, const char *parent_path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_add_partition "(\"%s\", %s%s%s, %s%s%s, \"%s\")\n",
			path,
			(fstype != NULL) ? ("\"") : (""),
			(fstype != NULL) ? (fstype) : ("nil"),
			(fstype != NULL) ? ("\"") : (""),
			(label != NULL) ? ("\"") : (""),
			(label != NULL) ? (label) : ("nil"),
			(label != NULL) ? ("\"") : (""),
			parent_path);
	}
}

void notify_remove_partition(const char *path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_remove_partition "(\"%s\")\n", path);
	}
}

void notify_partition_changed(const char *path, const char *fstype, const char *label, const char *parent_path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_partition_changed "(\"%s\", %s%s%s, %s%s%s, \"%s\")\n",
			path,
			(fstype != NULL) ? ("\"") : (""),
			(fstype != NULL) ? (fstype) : ("nil"),
			(fstype != NULL) ? ("\"") : (""),
			(label != NULL) ? ("\"") : (""),
			(label != NULL) ? (label) : ("nil"),
			(label != NULL) ? ("\"") : (""),
			parent_path);
	}
}

void notify_add_stateful_device(const char *path, dtmd_removable_media_type_t type, dtmd_removable_media_state_t state, const char *fstype, const char *label)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_add_stateful_device "(\"%s\", \"%s\", \"%s\", %s%s%s, %s%s%s)\n",
			path,
			dtmd_device_type_to_string(type),
			dtmd_device_state_to_string(state),
			(fstype != NULL) ? ("\"") : (""),
			(fstype != NULL) ? (fstype) : ("nil"),
			(fstype != NULL) ? ("\"") : (""),
			(label != NULL) ? ("\"") : (""),
			(label != NULL) ? (label) : ("nil"),
			(label != NULL) ? ("\"") : (""));
	}
}

void notify_remove_stateful_device(const char *path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_remove_stateful_device "(\"%s\")\n", path);
	}
}

void notify_stateful_device_changed(const char *path, dtmd_removable_media_type_t type, dtmd_removable_media_state_t state, const char *fstype, const char *label)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_stateful_device_changed "(\"%s\", \"%s\", \"%s\", %s%s%s, %s%s%s)\n",
			path,
			dtmd_device_type_to_string(type),
			dtmd_device_state_to_string(state),
			(fstype != NULL) ? ("\"") : (""),
			(fstype != NULL) ? (fstype) : ("nil"),
			(fstype != NULL) ? ("\"") : (""),
			(label != NULL) ? ("\"") : (""),
			(label != NULL) ? (label) : ("nil"),
			(label != NULL) ? ("\"") : (""));
	}
}

void notify_mount(const char *path, const char *mount_point, const char *mount_options)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_mount "(\"%s\", \"%s\", \"%s\")\n", path, mount_point, mount_options);
	}
}

void notify_unmount(const char *path, const char *mount_point)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_unmount "(\"%s\", \"%s\")\n", path, mount_point);
	}
}

static int print_devices_count(int client_number)
{
	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_devices "(\"%d\")\n",
		media_count) < 0)
	{
		return result_client_error;
	}

	return result_success;
}

static int print_device(int client_number, unsigned int device)
{
#ifndef NDEBUG
	if (device > media_count)
	{
		return result_bug;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_device "(\"%s\", \"%s\", \"%d\")\n",
		media[device]->path,
		dtmd_device_type_to_string(media[device]->type),
		media[device]->partitions_count) < 0)
	{
		return result_client_error;
	}

	return result_success;
}

static int print_partition(int client_number, unsigned int device, unsigned int partition)
{
#ifndef NDEBUG
	if ((device > media_count) || (partition > media[device]->partitions_count))
	{
		return result_bug;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_partition "(\"%s\", %s%s%s, %s%s%s, \"%s\", %s%s%s, %s%s%s)\n",
		media[device]->partition[partition]->path,
		((media[device]->partition[partition]->fstype != NULL) ? ("\"") : ("")),
		((media[device]->partition[partition]->fstype != NULL) ? (media[device]->partition[partition]->fstype) : ("nil")),
		((media[device]->partition[partition]->fstype != NULL) ? ("\"") : ("")),
		((media[device]->partition[partition]->label != NULL) ? ("\"") : ("")),
		((media[device]->partition[partition]->label != NULL) ? (media[device]->partition[partition]->label) : ("nil")),
		((media[device]->partition[partition]->label != NULL) ? ("\"") : ("")),
		media[device]->path,
		((media[device]->partition[partition]->mnt_point != NULL) ? ("\"") : ("")),
		((media[device]->partition[partition]->mnt_point != NULL) ? (media[device]->partition[partition]->mnt_point) : ("nil")),
		((media[device]->partition[partition]->mnt_point != NULL) ? ("\"") : ("")),
		((media[device]->partition[partition]->mnt_opts != NULL) ? ("\"") : ("")),
		((media[device]->partition[partition]->mnt_opts != NULL) ? (media[device]->partition[partition]->mnt_opts) : ("nil")),
		((media[device]->partition[partition]->mnt_opts != NULL) ? ("\"") : (""))) < 0)
	{
		return result_client_error;
	}

	return result_success;
}

static int print_stateful_devices_count(int client_number)
{
	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_stateful_devices "(\"%d\")\n",
		stateful_media_count) < 0)
	{
		return result_client_error;
	}

	return result_success;
}

static int print_stateful_device(int client_number, unsigned int device)
{
#ifndef NDEBUG
	if (device > stateful_media_count)
	{
		return result_bug;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_stateful_device "(\"%s\", \"%s\", \"%s\", %s%s%s, %s%s%s, %s%s%s, %s%s%s)\n",
		stateful_media[device]->path,
		dtmd_device_type_to_string(stateful_media[device]->type),
		dtmd_device_state_to_string(stateful_media[device]->state),
		((stateful_media[device]->fstype != NULL) ? ("\"") : ("")),
		((stateful_media[device]->fstype != NULL) ? (stateful_media[device]->fstype) : ("nil")),
		((stateful_media[device]->fstype != NULL) ? ("\"") : ("")),
		((stateful_media[device]->label != NULL) ? ("\"") : ("")),
		((stateful_media[device]->label != NULL) ? (stateful_media[device]->label) : ("nil")),
		((stateful_media[device]->label != NULL) ? ("\"") : ("")),
		((stateful_media[device]->mnt_point != NULL) ? ("\"") : ("")),
		((stateful_media[device]->mnt_point != NULL) ? (stateful_media[device]->mnt_point) : ("nil")),
		((stateful_media[device]->mnt_point != NULL) ? ("\"") : ("")),
		((stateful_media[device]->mnt_opts != NULL) ? ("\"") : ("")),
		((stateful_media[device]->mnt_opts != NULL) ? (stateful_media[device]->mnt_opts) : ("nil")),
		((stateful_media[device]->mnt_opts != NULL) ? ("\"") : (""))) < 0)
	{
		return result_client_error;
	}

	return result_success;
}
