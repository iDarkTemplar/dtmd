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

#include "actions.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int parse_command(int client_number)
{
	char *par1;
	char *par2;
	char *par3;
	unsigned char *cur;
	unsigned char *tmp;
	int len;
	unsigned int i;
	unsigned int j;

	//len = strlen(clients[client_number]->buf);

	if (strncmp((const char*)clients[client_number]->buf, "enum_all()\n", strlen("enum_all()\n")) == 0)
	{
		// enum_all

		for (i = 0; i < media_count; ++i)
		{
			print_device(client_number, media[i]);

			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				print_partition(client_number, media[i], j);
			}
		}

		dprintf(clients[client_number]->clientfd, "done(enum_all)\n");
	}
	else if (strncmp((const char*)clients[client_number]->buf, "list_device(\"", strlen("list_device(\"")) == 0)
	{
		// list_device device_path

		cur = &(clients[client_number]->buf[strlen("list_device(\"")+1]);
		len = 0;
		tmp = cur;

		while (((*tmp) != '\"') && ((*tmp) != 0))
		{
			++len;
			++tmp;
		}

		if ((len == 0) || (*tmp == 0))
		{
			return -1;
		}

		par1 = (char*) malloc(len + 1);
		if (par1 == NULL)
		{
			return -1;
		}

		memcpy(par1, cur, len);
		par1[len] = 0;

		if (strncmp((const char*)&(cur[len]), "\")\n", strlen("\")\n")) != 0)
		{
			free(par1);
			return -1;
		}

		for (i = 0; i < media_count; ++i)
		{
			if (strcmp(media[i]->path, par1) == 0)
			{
				print_device(client_number, media[i]);

				for (j = 0; j < media[i]->partitions_count; ++j)
				{
					print_partition(client_number, media[i], j);
				}

				break;
			}
		}

		dprintf(clients[client_number]->clientfd, "done(list_device, \"%s\")\n",
			par1);

		free(par1);
	}
	else if (strncmp((const char*)clients[client_number]->buf, "list_device(\"", strlen("list_device(\"")) == 0)
	{
		// mount device_path, mount_point, mount_options
		// TODO: check
	}
	else if (0)
	{
		// unmount device_path, mount_point
		// TODO: check
	}
	else
	{
		return -1;
	}

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
	dprintf(clients[client_number]->clientfd, "device(\"%s\", \"%s\")\n",
		media->path,
		removable_type_to_string(media->type));

	return 0;
}

int print_partition(int client_number, struct removable_media *media, unsigned int partition)
{
	if (partition > media->partitions_count)
	{
		return -1;
	}

	dprintf(clients[client_number]->clientfd, "partition(\"%s\", \"%s\", ",
		media->partition[partition]->path,
		media->partition[partition]->type);

	if (media->partition[partition]->label != NULL)
	{
		dprintf(clients[client_number]->clientfd, "\"%s\", ",
			media->partition[partition]->label);
	}
	else
	{
		dprintf(clients[client_number]->clientfd, "nil, ");
	}

	dprintf(clients[client_number]->clientfd, "\"%s\", ",
		media->path);

	if (media->partition[partition]->mnt_point != NULL)
	{
		dprintf(clients[client_number]->clientfd, "\"%s\", ",
			media->partition[partition]->mnt_point);
	}
	else
	{
		dprintf(clients[client_number]->clientfd, "nil, ");
	}

	if (media->partition[partition]->mnt_opts != NULL)
	{
		dprintf(clients[client_number]->clientfd, "\"%s\")\n",
			media->partition[partition]->mnt_opts);
	}
	else
	{
		dprintf(clients[client_number]->clientfd, "nil)\n");
	}

	return 0;
}
