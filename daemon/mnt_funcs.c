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
#include "daemon/log.h"
#include "daemon/return_codes.h"

#include <mntent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

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
		WRITE_LOG_ARGS(LOG_ERR, "Failed opening file '%s'", dtmd_internal_mounts_file);
		goto check_mount_changes_error_1;
	}

	// stateless devices
	for (i = 0; i < media_count; ++i)
	{
		for (j = 0; j < media[i]->partitions_count; ++j)
		{
			media[i]->partition[j]->is_mounted <<= 1;
		}
	}

	// stateful devices
	for (i = 0; i < stateful_media_count; ++i)
	{
		stateful_media[i]->is_mounted <<= 1;
	}

	while ((ent = getmntent(mntfile)) != NULL)
	{
		// stateless devices
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
									WRITE_LOG(LOG_ERR, "Memory allocation failure");
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
									WRITE_LOG(LOG_ERR, "Memory allocation failure");
									goto check_mount_changes_error_2;
								}
							}
						}
						else
						{
							media[i]->partition[j]->is_mounted &= ~is_mounted_now;
						}
					}

					goto check_mount_changes_break_cycles;
				}
			}
		}

		// stateful devices
		for (i = 0; i < stateful_media_count; ++i)
		{
			if (strcmp(stateful_media[i]->path, ent->mnt_fsname) == 0)
			{
				// skip devices mounted multiple times
				if (!(stateful_media[i]->is_mounted & is_mounted_now))
				{
					if ((ent->mnt_dir != NULL) && (ent->mnt_opts != NULL))
					{
						stateful_media[i]->is_mounted |= is_mounted_now;

						if ((stateful_media[i]->mnt_point == NULL) || (strcmp(stateful_media[i]->mnt_point, ent->mnt_dir) != 0))
						{
							if (stateful_media[i]->mnt_point != NULL)
							{
								free(stateful_media[i]->mnt_point);
							}

							stateful_media[i]->mnt_point = strdup(ent->mnt_dir);
							if (stateful_media[i]->mnt_point == NULL)
							{
								WRITE_LOG(LOG_ERR, "Memory allocation failure");
								goto check_mount_changes_error_2;
							}
						}

						if ((stateful_media[i]->mnt_opts == NULL) || (strcmp(stateful_media[i]->mnt_opts, ent->mnt_opts) != 0))
						{
							if (stateful_media[i]->mnt_opts != NULL)
							{
								free(stateful_media[i]->mnt_opts);
							}

							stateful_media[i]->mnt_opts = strdup(ent->mnt_opts);
							if (stateful_media[i]->mnt_opts == NULL)
							{
								WRITE_LOG(LOG_ERR, "Memory allocation failure");
								goto check_mount_changes_error_2;
							}
						}
					}
					else
					{
						stateful_media[i]->is_mounted &= ~is_mounted_now;
					}
				}

				goto check_mount_changes_break_cycles;
			}
		}

	check_mount_changes_break_cycles:
		;
	}

	endmntent(mntfile);

	// stateless devices
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

	// stateful devices
	for (i = 0; i < stateful_media_count; ++i)
	{
		if (stateful_media[i]->is_mounted & is_mounted_now)
		{
			if (!(stateful_media[i]->is_mounted & is_mounted_last))
			{
				notify_mount(stateful_media[i]->path, stateful_media[i]->mnt_point, stateful_media[i]->mnt_opts);
				stateful_media[i]->is_mounted = is_mounted_now;
			}
		}
		else
		{
			if (stateful_media[i]->is_mounted & is_mounted_last)
			{
				notify_unmount(stateful_media[i]->path, stateful_media[i]->mnt_point);
				stateful_media[i]->is_mounted = 0;

				if (stateful_media[i]->mnt_point != NULL)
				{
					free(stateful_media[i]->mnt_point);
					stateful_media[i]->mnt_point = NULL;
				}

				if (stateful_media[i]->mnt_opts != NULL)
				{
					free(stateful_media[i]->mnt_opts);
					stateful_media[i]->mnt_opts = NULL;
				}
			}
		}
	}

	return result_success;

check_mount_changes_error_2:
	endmntent(mntfile);

check_mount_changes_error_1:
	return result_fatal_error;
}

int point_mount_count(const char *path, int max)
{
	int result = 0;
	FILE *mntfile;
	struct mntent *ent;

	mntfile = setmntent(dtmd_internal_mounts_file, "r");
	if (mntfile == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed opening file '%s'", dtmd_internal_mounts_file);
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

int add_to_mtab(const char *path, const char *mount_point, const char *type, const char *mount_opts)
{
	int result;
	FILE *mntfile;
	struct mntent ent;

	mntfile = setmntent(dtmd_internal_mtab_file, "a");
	if (mntfile == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed opening file '%s'", dtmd_internal_mtab_file);
		return -1;
	}

	ent.mnt_dir    = (char*) mount_point;
	ent.mnt_fsname = (char*) path;
	ent.mnt_type   = (char*) type;
	ent.mnt_opts   = (char*) mount_opts;
	ent.mnt_freq   = 0;
	ent.mnt_passno = 0;

	result = addmntent(mntfile, &ent);

	endmntent(mntfile);

	if (result == 0)
	{
		return result_success;
	}
	else
	{
		return result_fatal_error;
	}
}

int remove_from_mtab(const char *path, const char *mount_point, const char *type)
{
	FILE *mntfile_old;
	FILE *mntfile_new;
	struct mntent *ent;
	struct stat stats;

	mntfile_old = setmntent(dtmd_internal_mtab_file, "r");
	if (mntfile_old == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed opening file '%s'", dtmd_internal_mtab_file);
		return result_fatal_error;
	}

	mntfile_new = setmntent(dtmd_internal_mtab_temporary, "w");
	if (mntfile_new == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed opening file '%s' for writing", dtmd_internal_mtab_temporary);
		endmntent(mntfile_old);
		return result_fatal_error;
	}

	while ((ent = getmntent(mntfile_old)) != NULL)
	{
		if ((strcmp(ent->mnt_dir, mount_point) != 0)
			|| (strcmp(ent->mnt_fsname, path) != 0)
			|| (strcmp(ent->mnt_type, type) != 0))
		{
			if (addmntent(mntfile_new, ent) != 0)
			{
				WRITE_LOG_ARGS(LOG_ERR, "Failed copying data to file '%s'", dtmd_internal_mtab_temporary);
				endmntent(mntfile_old);
				endmntent(mntfile_new);
				unlink(dtmd_internal_mtab_temporary);
				return result_fatal_error;
			}
		}
	}

	endmntent(mntfile_old);
	endmntent(mntfile_new);

	if (stat(dtmd_internal_mtab_file, &stats) != 0)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed obtaining file properties for file '%s'", dtmd_internal_mtab_file);
		unlink(dtmd_internal_mtab_temporary);
		return result_fatal_error;
	}

	if (chmod(dtmd_internal_mtab_temporary, stats.st_mode) != 0)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed changing file mode for file '%s'", dtmd_internal_mtab_temporary);
		unlink(dtmd_internal_mtab_temporary);
		return result_fatal_error;
	}

	if (chown(dtmd_internal_mtab_temporary, stats.st_uid, stats.st_gid) != 0)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed changing file owner for file '%s'", dtmd_internal_mtab_temporary);
		unlink(dtmd_internal_mtab_temporary);
		return result_fatal_error;
	}

	if (rename(dtmd_internal_mtab_temporary, dtmd_internal_mtab_file) != 0)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed renaming file '%s' to '%s'", dtmd_internal_mtab_temporary, dtmd_internal_mtab_file);
		unlink(dtmd_internal_mtab_temporary);
		return result_fatal_error;
	}

	return result_success;
}
