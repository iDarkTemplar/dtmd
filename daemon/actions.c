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

int invoke_command(int client_number, struct dtmd_command *cmd)
{
	unsigned int i;
	unsigned int j;

	if ((strcmp((char*) cmd->cmd, "enum_all") == 0) && (cmd->args_count == 0))
	{
		dprintf(clients[client_number]->clientfd, "started(\"enum_all\")\n");

		for (i = 0; i < media_count; ++i)
		{
			print_device(client_number, media[i]);

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				print_partition(client_number, media[i], j);
			}
		}

		dprintf(clients[client_number]->clientfd, "finished(\"enum_all\")\n");
	}
	else if ((strcmp((char*) cmd->cmd, "list_device") == 0) && (cmd->args_count == 1))
	{

	}
	else if ((strcmp((char*) cmd->cmd, "mount") == 0) && (cmd->args_count == 3))
	{

	}
	else if ((strcmp((char*) cmd->cmd, "unmount") == 0) && (cmd->args_count == 2))
	{

	}
	else
	{
		return -1;
	}
/*
	"enum_all"
		returns:
		device: "path, type"
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
	"list_device":
		input:
		"device path"
		returns:
		device: "path, type"
		partition: "path, fstype, label (or NULL), parent_path, mount_point (or NULL), mount_options (or NULL)"
	"mount"
		input:
		"path, mount_point, mount_options?"
		returns:
		broadcast "mount"
		or
		single "mount_failed"
	"unmount"
		input:
		"path, mount_point"
		returns:
		broadcast "unmount"
		or
		single "mount_failed"
*/
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

int print_device(int client_number, struct removable_media *media)
{
	if (dprintf(clients[client_number]->clientfd, "device(\"%s\", \"%s\")\n",
		media->path,
		removable_type_to_string(media->type)) < 0)
	{
		return -1;
	}

	return 0;
}

int print_partition(int client_number, struct removable_media *media, unsigned int partition)
{
	if (partition > media->partitions_count)
	{
		return -1;
	}

	if (dprintf(clients[client_number]->clientfd, "partition(\"%s\", \"%s\", %s%s%s, \"%s\", %s%s%s, %s%s%s)\n",
		media->partition[partition]->path,
		media->partition[partition]->type,
		((media->partition[partition]->label != NULL) ? ("\"") : ("")),
		((media->partition[partition]->label != NULL) ? (media->partition[partition]->label) : ("nil")),
		((media->partition[partition]->label != NULL) ? ("\"") : ("")),
		media->path,
		((media->partition[partition]->mnt_point != NULL) ? ("\"") : ("")),
		((media->partition[partition]->mnt_point != NULL) ? (media->partition[partition]->mnt_point) : ("nil")),
		((media->partition[partition]->mnt_point != NULL) ? ("\"") : ("")),
		((media->partition[partition]->mnt_opts != NULL) ? ("\"") : ("")),
		((media->partition[partition]->mnt_opts != NULL) ? (media->partition[partition]->mnt_opts) : ("nil")),
		((media->partition[partition]->mnt_opts != NULL) ? ("\"") : (""))) < 0)
	{
		return -1;
	}

	return 0;
}
