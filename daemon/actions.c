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

#include <stdio.h>
#include <string.h>

static int print_devices_count(unsigned int client_number);

static int print_device(unsigned int client_number, unsigned int device);

static int print_partition(unsigned int client_number, unsigned int device, unsigned int partition);

int invoke_command(unsigned int client_number, dtmd_command_t *cmd)
{
	unsigned int i;
	unsigned int j;

	if ((strcmp(cmd->cmd, "enum_all") == 0) && (cmd->args_count == 0))
	{
		dprintf(clients[client_number]->clientfd, "started(\"enum_all\")\n");

		print_devices_count(client_number);

		for (i = 0; i < media_count; ++i)
		{
			print_device(client_number, i);

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				print_partition(client_number, i, j);
			}
		}

		dprintf(clients[client_number]->clientfd, "finished(\"enum_all\")\n");
	}
	else if ((strcmp(cmd->cmd, "list_device") == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
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
			dprintf(clients[client_number]->clientfd, "started(\"list_device\", \"%s\")\n", cmd->args[0]);

			print_device(client_number, i);

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				print_partition(client_number, i, j);
			}

			dprintf(clients[client_number]->clientfd, "finished(\"list_device\", \"%s\")\n", cmd->args[0]);
		}
		else
		{
			dprintf(clients[client_number]->clientfd, "failed(\"list_device\", \"%s\")\n", cmd->args[0]);
		}
	}
	else if ((strcmp(cmd->cmd, "list_partition") == 0) && (cmd->args_count == 1) && (cmd->args[0] != NULL))
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
			dprintf(clients[client_number]->clientfd, "started(\"list_partition\", \"%s\")\n", cmd->args[0]);
			print_partition(client_number, i, j);
			dprintf(clients[client_number]->clientfd, "finished(\"list_partition\", \"%s\")\n", cmd->args[0]);
		}
		else
		{
			dprintf(clients[client_number]->clientfd, "failed(\"list_partition\", \"%s\")\n", cmd->args[0]);
		}
	}
	else if ((strcmp(cmd->cmd, "mount") == 0) && (cmd->args_count == 3) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
	{

		dprintf(clients[client_number]->clientfd, "failed(\"mount\", \"%s\", \"%s\", %s%s%s)\n",
			cmd->args[0],
			cmd->args[1],
			((cmd->args[2] != NULL) ? ("\"") : ("")),
			((cmd->args[2] != NULL) ? (cmd->args[2]) : ("nil")),
			((cmd->args[2] != NULL) ? ("\"") : ("")));

		dprintf(clients[client_number]->clientfd, "succeeded(\"mount\", \"%s\", \"%s\", %s%s%s)\n",
			cmd->args[0],
			cmd->args[1],
			((cmd->args[2] != NULL) ? ("\"") : ("")),
			((cmd->args[2] != NULL) ? (cmd->args[2]) : ("nil")),
			((cmd->args[2] != NULL) ? ("\"") : ("")));

		// TODO: implement mount and notifications
	}
	else if ((strcmp(cmd->cmd, "unmount") == 0) && (cmd->args_count == 2) && (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
	{
		dprintf(clients[client_number]->clientfd, "failed(\"unmount\", \"%s\", \"%s\")\n", cmd->args[0], cmd->args[1]);
		dprintf(clients[client_number]->clientfd, "succeeded(\"unmount\", \"%s\", \"%s\")\n", cmd->args[0], cmd->args[1]);

		// TODO: implement unmount and notifications
	}
	else
	{
		return -1;
	}

	// TODO: issue command
	return 0;
}

// TODO: send UID/GID with SCM_CREDENTIALS

//struct ucred credentials;
//int ucred_length = sizeof(struct ucred);

/* fill in the user data structure */
/*
if(getsockopt(connection_fd, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length))
{
printf("could obtain credentials from unix domain socket");
return 1;
}
*/

/* the process ID of the process on the other side of the socket */
//credentials.pid;

/* the effective UID of the process on the other side of the socket  */
//credentials.uid;

/* the effective primary GID of the process on the other side of the socket */
//credentials.gid;

/* To get supplemental groups, we will have to look them up in our account
   database, after a reverse lookup on the UID to get the account name.
   We can take this opportunity to check to see if this is a legit account.
*/

int send_notification(const char *type, const char *device)
{
	return 0;
}

static int print_devices_count(unsigned int client_number)
{
	if (dprintf(clients[client_number]->clientfd, "devices(\"%d\")\n",
		media_count) < 0)
	{
		return -1;
	}

	return 0;
}

static int print_device(unsigned int client_number, unsigned int device)
{
#ifndef NDEBUG
	if (device > media_count)
	{
		return -1;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, "device(\"%s\", \"%s\", \"%d\")\n",
		media[device]->path,
		removable_type_to_string(media[device]->type),
		media[device]->partitions_count) < 0)
	{
		return -1;
	}

	return 0;
}

static int print_partition(unsigned int client_number, unsigned int device, unsigned int partition)
{
#ifndef NDEBUG
	if ((device > media_count) || (partition > media[device]->partitions_count))
	{
		return -1;
	}
#endif /* NDEBUG */

	if (dprintf(clients[client_number]->clientfd, "partition(\"%s\", \"%s\", %s%s%s, \"%s\", %s%s%s, %s%s%s)\n",
		media[device]->partition[partition]->path,
		media[device]->partition[partition]->type,
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
