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

#if (defined OS_FreeBSD)
#include "daemon/filesystem_opts.h"
#endif /* (defined OS_FreeBSD) */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#if (defined OS_Linux)
#include <mntent.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/mount.h>
#endif /* (defined OS_FreeBSD) */

#define is_mounted_now   (1<<0)
#define is_mounted_last  (1<<1)
#define is_processed     (1<<2)
#define is_state_changed (1<<3)

#if (defined OS_Linux)
int init_mount_monitoring(void)
{
	int mountfd;

	mountfd = open(dtmd_internal_mounts_file, O_RDONLY);
	if (mountfd < 0)
	{
		WRITE_LOG(LOG_ERR, "Error opening mounts file descriptor");
	}

	return mountfd;
}
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
int init_mount_monitoring(void)
{
	int queuefd;
	int rc;
	struct kevent evt;

	queuefd = kqueue();
	if (queuefd < 0)
	{
		WRITE_LOG(LOG_ERR, "Error opening kqueue");
		return queuefd;
	}

	EV_SET(&evt, 0, EVFILT_FS, EV_ADD, 0, 0, 0);
	rc = kevent(queuefd, &evt, 1, NULL, 0, NULL);
	if (rc < 0)
	{
		WRITE_LOG(LOG_ERR, "Error adding mount/unmount notification event to kqueue");
		close(queuefd);
		return rc;
	}

	return queuefd;
}
#endif /* (defined OS_FreeBSD) */

int close_mount_monitoring(int monitorfd)
{
	return close(monitorfd);
}

static void mark_each_device_recursive(dtmd_removable_media_t *media_ptr)
{
	dtmd_removable_media_t *iter_media_ptr;

	for (iter_media_ptr = media_ptr->first_child; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		mark_each_device_recursive(iter_media_ptr);
	}

	media_ptr->private_data = (void*) (((size_t) media_ptr->private_data) << 1);
}

static void process_changes_recursive(dtmd_removable_media_t *media_ptr)
{
	dtmd_removable_media_t *iter_media_ptr;

	if (((size_t) media_ptr->private_data) & is_mounted_now)
	{
		if (!(((size_t) media_ptr->private_data) & is_mounted_last))
		{
			media_ptr->private_data = (void*) (((size_t) media_ptr->private_data) | is_state_changed);
		}
	}
	else
	{
		if (((size_t) media_ptr->private_data) & is_mounted_last)
		{
			media_ptr->private_data = (void*) (((size_t) media_ptr->private_data) | is_state_changed);

			if (media_ptr->mnt_point != NULL)
			{
				free(media_ptr->mnt_point);
				media_ptr->mnt_point = NULL;
			}

			if (media_ptr->mnt_opts != NULL)
			{
				free(media_ptr->mnt_opts);
				media_ptr->mnt_opts = NULL;
			}
		}
	}

	if (((size_t) media_ptr->private_data) & is_state_changed)
	{
		notify_removable_device_changed(
			((media_ptr->parent != NULL) ? media_ptr->parent->path : dtmd_root_device_path),
			media_ptr->path,
			media_ptr->type,
			media_ptr->subtype,
			media_ptr->state,
			media_ptr->fstype,
			media_ptr->label,
			media_ptr->mnt_point,
			media_ptr->mnt_opts);
	}

	media_ptr->private_data = (void*) (((size_t) media_ptr->private_data) & is_mounted_now);

	for (iter_media_ptr = media_ptr->first_child; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		process_changes_recursive(iter_media_ptr);
	}
}

#if (defined OS_Linux)
int check_mount_changes(void)
{
	FILE *mntfile;
	struct mntent *ent;
	dtmd_removable_media_t *iter_media_ptr;

	mntfile = setmntent(dtmd_internal_mounts_file, "r");
	if (mntfile == NULL)
	{
		WRITE_LOG_ARGS(LOG_ERR, "Failed opening file '%s'", dtmd_internal_mounts_file);
		goto check_mount_changes_error_1;
	}

	for (iter_media_ptr = removable_media_root; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		mark_each_device_recursive(iter_media_ptr);
	}

	while ((ent = getmntent(mntfile)) != NULL)
	{
		iter_media_ptr = dtmd_find_media(ent->mnt_fsname, removable_media_root);
		if (iter_media_ptr != NULL)
		{
			// skip devices mounted multiple times
			if (!(((size_t) iter_media_ptr->private_data) & is_processed))
			{
				iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_processed);

				if ((ent->mnt_dir != NULL) && (ent->mnt_opts != NULL))
				{
					iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_mounted_now);

					if ((iter_media_ptr->mnt_point == NULL) || (strcmp(iter_media_ptr->mnt_point, ent->mnt_dir) != 0))
					{
						if (iter_media_ptr->mnt_point != NULL)
						{
							free(iter_media_ptr->mnt_point);
						}

						iter_media_ptr->mnt_point = strdup(ent->mnt_dir);
						if (iter_media_ptr->mnt_point == NULL)
						{
							WRITE_LOG(LOG_ERR, "Memory allocation failure");
							goto check_mount_changes_error_2;
						}

						iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_state_changed);
					}

					if ((iter_media_ptr->mnt_opts == NULL) || (strcmp(iter_media_ptr->mnt_opts, ent->mnt_opts) != 0))
					{
						if (iter_media_ptr->mnt_opts != NULL)
						{
							free(iter_media_ptr->mnt_opts);
						}

						iter_media_ptr->mnt_opts = strdup(ent->mnt_opts);
						if (iter_media_ptr->mnt_opts == NULL)
						{
							WRITE_LOG(LOG_ERR, "Memory allocation failure");
							goto check_mount_changes_error_2;
						}

						iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_state_changed);
					}
				}
				else
				{
					iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) & ~is_mounted_now);
				}
			}
		}
	}

	endmntent(mntfile);

	for (iter_media_ptr = removable_media_root; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		process_changes_recursive(iter_media_ptr);
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
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
int check_mount_changes(int mountfd)
{
	int rc;
	struct kevent evt;
	struct timespec timeout;
	int count, current;
	struct statfs *mounts;
	char *options;
	dtmd_removable_media_t *iter_media_ptr;

	// TODO: merge it with Linux version?

	if (mountfd >= 0)
	{
		timeout.tv_sec  = 0;
		timeout.tv_nsec = 0;

		rc = kevent(mountfd, NULL, 0, &evt, 1, &timeout);
		if (rc == 0)
		{
			return result_fail;
		}
		else if (rc < 0)
		{
			return result_fatal_error;
		}

		if (!(evt.fflags & (VQ_MOUNT | VQ_UNMOUNT)))
		{
			return result_fail;
		}
	}

	count = getmntinfo(&mounts, MNT_WAIT);
	if (count == 0)
	{
		WRITE_LOG(LOG_ERR, "Failed obtaining mount info");
		return result_fatal_error;
	}

	for (iter_media_ptr = removable_media_root; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		mark_each_device_recursive(iter_media_ptr);
	}

	for (current = 0; current < count; ++current)
	{
		iter_media_ptr = dtmd_find_media(mounts[current].f_mntfromname, removable_media_root);
		if (iter_media_ptr != NULL)
		{
			// skip devices mounted multiple times
			if (!(((size_t) iter_media_ptr->private_data) & is_processed))
			{
				iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_processed | is_mounted_now);

				if ((iter_media_ptr->mnt_point == NULL) || (strcmp(iter_media_ptr->mnt_point, mounts[current].f_mntonname) != 0))
				{
					if (iter_media_ptr->mnt_point != NULL)
					{
						free(iter_media_ptr->mnt_point);
					}

					iter_media_ptr->mnt_point = strdup(mounts[current].f_mntonname);
					if (iter_media_ptr->mnt_point == NULL)
					{
						WRITE_LOG(LOG_ERR, "Memory allocation failure");
						goto check_mount_changes_error_1;
					}

					iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_state_changed);
				}

				options = convert_option_flags_to_string(mounts[current].f_flags);
				if (options == NULL)
				{
					goto check_mount_changes_error_1;
				}

				if ((iter_media_ptr->mnt_opts == NULL) || (strcmp(iter_media_ptr->mnt_opts, options) != 0))
				{
					if (iter_media_ptr->mnt_opts != NULL)
					{
						free(iter_media_ptr->mnt_opts);
					}

					iter_media_ptr->mnt_opts = options;
					iter_media_ptr->private_data = (void*) (((size_t) iter_media_ptr->private_data) | is_state_changed);
				}
				else
				{
					free(options);
				}
			}
		}
	}

	for (iter_media_ptr = removable_media_root; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		process_changes_recursive(iter_media_ptr);
	}

	return result_success;

check_mount_changes_error_1:
	return result_fatal_error;
}

int point_mount_count(const char *path, int max)
{
	int result = 0;
	int count, i;
	struct statfs *mounts;

	count = getmntinfo(&mounts, MNT_WAIT);
	if (count == 0)
	{
		WRITE_LOG(LOG_ERR, "Failed obtaining mount info");
		return -1;
	}

	for (i = 0; i < count; ++i)
	{
		if (strcmp(mounts[i].f_mntonname, path) == 0)
		{
			++result;
			if ((max > 0) && (result == max))
			{
				break;
			}
		}
	}

	return result;
}

#endif /* (defined OS_FreeBSD) */

#if (defined OS_Linux)
#if (!defined MTAB_READONLY)
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
#endif /* (!defined MTAB_READONLY) */
#endif /* (defined OS_Linux) */
