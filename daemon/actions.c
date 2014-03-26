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

#include "daemon/actions.h"

#include "daemon/filesystem_mnt.h"

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

	if ((strcmp(cmd->cmd, dtmd_command_enum_all) == 0) && (cmd->args_count == 0))
	{
		dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_enum_all "\")\n");

		print_devices_count(client_number);

		for (i = 0; i < media_count; ++i)
		{
			print_device(client_number, i);

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				print_partition(client_number, i, j);
			}
		}

		print_stateful_devices_count(client_number);

		for (i = 0; i < stateful_media_count; ++i)
		{
			print_stateful_device(client_number, i);
		}

		dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_enum_all "\")\n");

		return 1;
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
			dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_device "\", \"%s\")\n", cmd->args[0]);

			print_device(client_number, i);

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				print_partition(client_number, i, j);
			}

			dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_list_device "\", \"%s\")\n", cmd->args[0]);

			return 1;
		}
		else
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_device "\", \"%s\")\n", cmd->args[0]);

			return 0;
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
			dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_partition "\", \"%s\")\n", cmd->args[0]);
			print_partition(client_number, i, j);
			dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_list_partition "\", \"%s\")\n", cmd->args[0]);

			return 1;
		}
		else
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_partition "\", \"%s\")\n", cmd->args[0]);

			return 0;
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
			dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_stateful_device "\", \"%s\")\n", cmd->args[0]);

			print_stateful_device(client_number, i);

			dprintf(clients[client_number]->clientfd, dtmd_response_finished "(\"" dtmd_command_list_stateful_device "\", \"%s\")\n", cmd->args[0]);

			return 1;
		}
		else
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_stateful_device "\", \"%s\")\n", cmd->args[0]);

			return 0;
		}
	}
	else if ((strcmp(cmd->cmd, dtmd_command_mount) == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL))
	{
		rc = invoke_mount(client_number, cmd->args[0], cmd->args[1], mount_by_value);

		if (rc > 0)
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_succeeded "(\"" dtmd_command_mount "\", \"%s\", %s%s%s)\n",
				cmd->args[0],
				((cmd->args[1] != NULL) ? ("\"") : ("")),
				((cmd->args[1] != NULL) ? (cmd->args[1]) : ("nil")),
				((cmd->args[1] != NULL) ? ("\"") : ("")));
		}
		else if (rc == 0)
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_mount "\", \"%s\", %s%s%s)\n",
				cmd->args[0],
				((cmd->args[1] != NULL) ? ("\"") : ("")),
				((cmd->args[1] != NULL) ? (cmd->args[1]) : ("nil")),
				((cmd->args[1] != NULL) ? ("\"") : ("")));
		}

		return rc;
	}
	else if ((strcmp(cmd->cmd, dtmd_command_unmount) == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
	{
		rc = invoke_unmount(client_number, cmd->args[0]);

		if (rc > 0)
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_succeeded "(\"" dtmd_command_unmount "\", \"%s\")\n", cmd->args[0]);
		}
		else if (rc == 0)
		{
			dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_unmount "\", \"%s\")\n", cmd->args[0]);
		}

		return rc;
	}
	else
	{
		return 0;
	}
}

int notify_add_disk(const char *path, dtmd_removable_media_type_t type)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_add_disk "(\"%s\", \"%s\")\n", path, dtmd_device_type_to_string(type));
	}

	return 0;
}

int notify_remove_disk(const char *path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_remove_disk "(\"%s\")\n", path);
	}

	return 0;
}

int notify_add_partition(const char *path, const char *fstype, const char *label, const char *parent_path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_add_partition "(\"%s\", \"%s\", %s%s%s, \"%s\")\n",
			path,
			fstype,
			(label != NULL) ? ("\"") : (""),
			(label != NULL) ? (label) : ("nil"),
			(label != NULL) ? ("\"") : (""),
			parent_path);
	}

	return 0;
}

int notify_remove_partition(const char *path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_remove_partition "(\"%s\")\n", path);
	}

	return 0;
}

int notify_add_stateful_device(const char *path, dtmd_removable_media_type_t type, dtmd_removable_media_state_t state, const char *fstype, const char *label)
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

	return 0;
}

int notify_remove_stateful_device(const char *path)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_remove_stateful_device "(\"%s\")\n", path);
	}

	return 0;
}

int notify_stateful_device_changed(const char *path, dtmd_removable_media_type_t type, dtmd_removable_media_state_t state, const char *fstype, const char *label)
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

	return 0;
}

int notify_mount(const char *path, const char *mount_point, const char *mount_options)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_mount "(\"%s\", \"%s\", \"%s\")\n", path, mount_point, mount_options);
	}

	return 0;
}

int notify_unmount(const char *path, const char *mount_point)
{
	unsigned int i;

	for (i = 0; i < clients_count; ++i)
	{
		dprintf(clients[i]->clientfd, dtmd_notification_unmount "(\"%s\", \"%s\")\n", path, mount_point);
	}

	return 0;
}

static int print_devices_count(int client_number)
{
	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_devices "(\"%d\")\n",
		media_count) < 0)
	{
		return -1;
	}

	return 0;
}

static int print_device(int client_number, unsigned int device)
{
#ifndef NDEBUG
	if (device > media_count)
	{
		return -1;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_device "(\"%s\", \"%s\", \"%d\")\n",
		media[device]->path,
		dtmd_device_type_to_string(media[device]->type),
		media[device]->partitions_count) < 0)
	{
		return -1;
	}

	return 0;
}

static int print_partition(int client_number, unsigned int device, unsigned int partition)
{
#ifndef NDEBUG
	if ((device > media_count) || (partition > media[device]->partitions_count))
	{
		return -1;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_partition "(\"%s\", \"%s\", %s%s%s, \"%s\", %s%s%s, %s%s%s)\n",
		media[device]->partition[partition]->path,
		media[device]->partition[partition]->fstype,
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
		return -1;
	}

	return 0;
}

static int print_stateful_devices_count(int client_number)
{
	if (dprintf(clients[client_number]->clientfd, dtmd_response_argument_stateful_devices "(\"%d\")\n",
		stateful_media_count) < 0)
	{
		return -1;
	}

	return 0;
}

static int print_stateful_device(int client_number, unsigned int device)
{
#ifndef NDEBUG
	if (device > stateful_media_count)
	{
		return -1;
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
		return -1;
	}

	return 0;
}
