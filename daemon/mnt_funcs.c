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

#include "lists.h"

#include <mntent.h>
#include <stdlib.h>
#include <string.h>

/*
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
*/

int check_mount_changes(void)
{
	unsigned int i;
	unsigned int j;
	FILE *mntfile;
	struct mntent *ent;

	for (i = 0; i < media_count; ++i)
	{
		for (j = 0; j < media[i]->partitions_count; ++j)
		{
			media[i]->partition[j]->is_mounted = 0;
		}
	}

	mntfile = setmntent("/proc/mounts", "r");
	if (mntfile == NULL)
	{
		return -1;
	}

	while ((ent = getmntent(mntfile)) != NULL)
	{
		for (i = 0; i < media_count; ++i)
		{
			for (j = 0; j < media[i]->partitions_count; ++j)
			{
				if (strcmp(media[i]->partition[j]->path, ent->mnt_fsname) == 0)
				{
					// skip devices mounted multiple times
					if (media[i]->partition[j]->is_mounted == 0)
					{
						if ((ent->mnt_dir != NULL) && (ent->mnt_opts != NULL))
						{
							media[i]->partition[j]->is_mounted = 1;

							if ((media[i]->partition[j]->mnt_point == NULL) || (strcmp(media[i]->partition[j]->mnt_point, ent->mnt_dir) != 0))
							{
								if (media[i]->partition[j]->mnt_point != NULL)
								{
									free(media[i]->partition[j]->mnt_point);
								}

								media[i]->partition[j]->mnt_point = strdup(ent->mnt_dir);
								if (media[i]->partition[j]->mnt_point == NULL)
								{
									endmntent(mntfile);
									return -1;
								}
							}

							if ((media[i]->partition[j]->mnt_opts == NULL) || (strcmp(media[i]->partition[j]->mnt_opts, ent->mnt_opts) != 0))
							{
								if (media[i]->partition[j]->mnt_opts != NULL)
								{
									free(media[i]->partition[j]->mnt_opts);
								}

								media[i]->partition[j]->mnt_opts = strdup(ent->mnt_opts);
								if (media[i]->partition[j]->mnt_opts == NULL)
								{
									endmntent(mntfile);
									return -1;
								}
							}
						}
						else
						{
							media[i]->partition[j]->is_mounted = 0;

							if (media[i]->partition[j]->mnt_point != NULL)
							{
								free(media[i]->partition[j]->mnt_point);
								media[i]->partition[j]->mnt_point = NULL;
							}

							if (media[i]->partition[j]->mnt_opts != NULL)
							{
								free(media[i]->partition[j]->mnt_opts);
								media[i]->partition[j]->mnt_opts = NULL;
							}
						}
					}

					// break from both cycles
					i = media_count;
					break;
				}
			}
		}
	}

	endmntent(mntfile);

	for (i = 0; i < media_count; ++i)
	{
		for (j = 0; j < media[i]->partitions_count; ++j)
		{
			if (media[i]->partition[j]->is_mounted == 0)
			{
				if (media[i]->partition[j]->mnt_point != NULL)
				{
					free(media[i]->partition[j]->mnt_point);
					media[i]->partition[j]->mnt_point = NULL;
				}

				if (media[i]->partition[j]->mnt_opts != NULL)
				{
					free(media[i]->partition[j]->mnt_opts);
					media[i]->partition[j]->mnt_opts = NULL;
				}
			}
		}
	}

	return 0;
}
