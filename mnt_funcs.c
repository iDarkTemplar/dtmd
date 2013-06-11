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

#include "mnt_funcs.h"

#include <mntent.h>
#include <stdlib.h>
#include <string.h>

int get_mount_params(const char *device, char **mount_point, char **mount_opts)
{
	FILE *mntfile;
	struct mntent *ent;

	*mount_point = NULL;
	*mount_opts  = NULL;

	mntfile = setmntent("/proc/mounts", "r");
	if (mntfile == NULL)
	{
		return -1;
	}

	while ((ent = getmntent(mntfile)) != NULL)
	{
		if (strcmp(device, ent->mnt_fsname) == 0)
		{
			if (ent->mnt_dir != NULL)
			{
				*mount_point = strdup(ent->mnt_dir);
				if ((*mount_point) == NULL)
				{
					endmntent(mntfile);

					return -1;
				}
			}

			if (ent->mnt_opts != NULL)
			{
				*mount_opts = strdup(ent->mnt_opts);
				if ((*mount_opts) == NULL)
				{
					if ((*mount_point) != NULL)
					{
						free(*mount_point);
						*mount_point = NULL;
					}

					endmntent(mntfile);

					return -1;
				}
			}

			endmntent(mntfile);

			return 1;
		}
	}

	endmntent(mntfile);

	return 0;
}
