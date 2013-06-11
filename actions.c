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

#include "lists.h"

#include <string.h>
#include <stdlib.h>

int parse_command(int client_number)
{
	char *par1;
	char *par2;
	char *par3;
	char *cur;
	char *tmp;
	int len;

	//len = strlen(clients[client_number]->buf);

	if (strncmp(clients[client_number]->buf, "enum_all()\n", strlen("enum_all()\n")) == 0)
	{
		// enum_all

		// TODO: enum_all
	}
	else if (strncmp(clients[client_number]->buf, "list_device(\"", strlen("list_device(\"")) == 0)
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

		if (strncmp(&(cur[len]), "\")\n", strlen("\")\n")) != 0)
		{
			free(par1);
			return -1;
		}

		// TODO: list_device

		free(par1);
	}
	else if (strncmp(clients[client_number]->buf, "list_device(\"", strlen("list_device(\"")) == 0)
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
