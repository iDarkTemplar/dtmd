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

#include "daemon/mnt_funcs.h"

#include "daemon/dtmd-internal.h"

#include "daemon/lists.h"
#include "daemon/actions.h"

#include <mntent.h>
#include <stdlib.h>
#include <string.h>

#define is_mounted_now 1
#define is_mounted_last 2

int check_mount_changes(void)
{
	unsigned int i;
	unsigned int j;
	FILE *mntfile;
	struct mntent *ent;

	mntfile = setmntent(dtmd_internal_mounts_file, "r");
	if (mntfile == NULL)
	{
		goto check_mount_changes_error_1;
	}

	for (i = 0; i < media_count; ++i)
	{
		for (j = 0; j < media[i]->partitions_count; ++j)
		{
			media[i]->partition[j]->is_mounted <<= 1;
		}
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
					if (!(media[i]->partition[j]->is_mounted & is_mounted_now))
					{
						if ((ent->mnt_dir != NULL) && (ent->mnt_opts != NULL))
						{
							media[i]->partition[j]->is_mounted |= is_mounted_now;

							if ((media[i]->partition[j]->mnt_point == NULL) || (strcmp(media[i]->partition[j]->mnt_point, ent->mnt_dir) != 0))
							{
								if (media[i]->partition[j]->mnt_point != NULL)
								{
									free(media[i]->partition[j]->mnt_point);
								}

								media[i]->partition[j]->mnt_point = strdup(ent->mnt_dir);
								if (media[i]->partition[j]->mnt_point == NULL)
								{
									goto check_mount_changes_error_2;
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
									goto check_mount_changes_error_2;
								}
							}
						}
						else
						{
							media[i]->partition[j]->is_mounted &= ~is_mounted_now;
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
			if (media[i]->partition[j]->is_mounted & is_mounted_now)
			{
				if (!(media[i]->partition[j]->is_mounted & is_mounted_last))
				{
					notify_mount(media[i]->partition[j]->path, media[i]->partition[j]->mnt_point, media[i]->partition[j]->mnt_opts);
					media[i]->partition[j]->is_mounted = is_mounted_now;
				}
			}
			else
			{
				if (media[i]->partition[j]->is_mounted & is_mounted_last)
				{
					notify_unmount(media[i]->partition[j]->path, media[i]->partition[j]->mnt_point);
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
		}
	}

	return 0;

check_mount_changes_error_2:
	endmntent(mntfile);

check_mount_changes_error_1:
	return -1;
}

int point_mount_count(const char *path, int max)
{
	int result = 0;
	FILE *mntfile;
	struct mntent *ent;

	mntfile = setmntent(dtmd_internal_mounts_file, "r");
	if (mntfile == NULL)
	{
		return -1;
	}

	while ((ent = getmntent(mntfile)) != NULL)
	{
		if (strcmp(ent->mnt_dir, path) == 0)
		{
			++result;
			if ((max > 0) && (result == max))
			{
				break;
			}
		}
	}

	endmntent(mntfile);

	return result;
}
