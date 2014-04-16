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

#include "daemon/filesystem_mnt.h"

#include "daemon/dtmd-internal.h"
#include "daemon/lists.h"
#include "daemon/mnt_funcs.h"
#include "daemon/filesystem_opts.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mount.h>
#include <stdio.h>
#include <sys/stat.h>

#if OS == Linux
#define __USE_GNU
#endif /* OS == Linux */

#include <sys/socket.h>

typedef enum dir_state
{
	dir_state_not_dir = 0,
	dir_state_empty,
	dir_state_not_empty
} dir_state_t;

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
static const char * const mount_ext_cmd = "/bin/mount";
static const char * const unmount_ext_cmd = "/bin/umount";
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

#if OS == Linux
static int get_credentials(int socket_fd, uid_t *uid, gid_t *gid)
{
	struct ucred credentials;
	unsigned int ucred_length = sizeof(struct ucred);

	if ((getsockopt(socket_fd, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length) != 0)
		|| (ucred_length != sizeof(struct ucred)))
	{
		return -1;
	}

	*uid = credentials.uid;
	*gid = credentials.gid;

	return 1;
}
#endif /* OS == Linux */

/*
freebsd:
struct xucred peercred;
LOCAL_PEERCRED
peercred.cr_version == XUCRED_VERSION
*uid = peercred.cr_uid;
*gid = peercred.cr_gid;
*/

static dir_state_t get_dir_state(const char *dirname)
{
	int n = 0;
	DIR *dir;

	dir = opendir(dirname);
	if (dir == NULL)
	{
		return dir_state_not_dir;
	}

	while (readdir(dir) != NULL)
	{
		if(++n > 2)
		{
			break;
		}
	}

	closedir(dir);

	if (n <= 2)
	{
		return dir_state_empty;
	}
	else
	{
		return dir_state_not_empty;
	}
}

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
static int invoke_mount_external(int client_number,
	const char *path,
	const char *mount_path,
	const char *fstype,
	dtmd_fsopts_list_t *fsopts_list)
{
	int result;

	int total_len;
	int mount_flags_start;
	char *mount_cmd;
	unsigned int string_full_len;

	result = fsopts_generate_string(fsopts_list, &string_full_len, NULL, 0, NULL, NULL, 0, NULL);
	if (result != 1)
	{
		goto invoke_mount_external_error_1;
	}

	// calculate total length
	mount_flags_start = strlen(mount_ext_cmd) + strlen(" -t ") + strlen(fstype) + 1 + strlen(path) + 1 + strlen(mount_path);

	if (string_full_len > 0)
	{
		mount_flags_start += strlen(" -o ");
	}

	total_len = mount_flags_start + string_full_len;

	mount_cmd = (char*) malloc(total_len + 1);
	if (mount_cmd == NULL)
	{
		result = -1;
		goto invoke_mount_external_error_1;
	}

	strcpy(mount_cmd, mount_ext_cmd);
	strcat(mount_cmd, " -t ");
	strcat(mount_cmd, fstype);
	strcat(mount_cmd, " ");
	strcat(mount_cmd, path);
	strcat(mount_cmd, " ");
	strcat(mount_cmd, mount_path);

	// create flags and string
	if (string_full_len > 0)
	{
		strcat(mount_cmd, " -o ");

		result = fsopts_generate_string(fsopts_list, NULL, &(mount_cmd[mount_flags_start]), string_full_len, NULL, NULL, 0, NULL);
		if (result != 1)
		{
			goto invoke_mount_external_error_2;
		}
	}

	mount_cmd[total_len] = 0;

	result = system(mount_cmd);

	free(mount_cmd);

	switch (result)
	{
	case 0:  /* success */
	case 16: /* problems writing or locking /etc/mtab */
		return 1;
	case -1:
		return -1;
	default:
		return 0;
	}

invoke_mount_external_error_2:
	free(mount_cmd);

invoke_mount_external_error_1:
	return result;
}
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

static int invoke_mount_internal(int client_number,
	const char *path,
	const char *mount_path,
	const char *fstype,
	dtmd_fsopts_list_t *fsopts_list)
{
	unsigned long mount_flags = 0;
	char *mount_opts;
	char *mount_full_opts;
	int result;
	unsigned int string_full_len;
	unsigned int string_len;

	result = fsopts_generate_string(fsopts_list, &string_full_len, NULL, 0, &string_len, NULL, 0, NULL);
	if (result != 1)
	{
		goto invoke_mount_internal_error_1;
	}

	mount_full_opts = (char*) malloc(string_full_len + 1);
	if (mount_full_opts == NULL)
	{
		result = -1;
		goto invoke_mount_internal_error_1;
	}

	mount_opts = (char*) malloc(string_len + 1);
	if (mount_opts == NULL)
	{
		result = -1;
		goto invoke_mount_internal_error_2;
	}

	result = fsopts_generate_string(fsopts_list, NULL, mount_full_opts, string_full_len, NULL, mount_opts, string_len, &mount_flags);
	if (result != 1)
	{
		goto invoke_mount_internal_error_3;
	}

	mount_full_opts[string_full_len] = 0;
	mount_opts[string_len] = 0;

	result = mount(path, mount_path, fstype, mount_flags, mount_opts);

	if (result == 0)
	{
		result = add_to_mtab(path, mount_path, fstype, mount_full_opts);
		if (result == 1)
		{
			result = 1;
		}
		else
		{
			// NOTE: failing to modify /etc/mtab is non-fatal error
			result = 0;
		}
	}
	else
	{
		result = 0;
	}

	free(mount_full_opts);
	free(mount_opts);

	return result;

invoke_mount_internal_error_3:
	free(mount_opts);

invoke_mount_internal_error_2:
	free(mount_full_opts);

invoke_mount_internal_error_1:
	return result;
}

static char* calculate_path(const char *path, const char *label, enum mount_by_value_enum *mount_type)
{
	const char *mount_dev_start;

	int mount_dev_len;
	int mount_path_len;

	char *mount_path;

	// calculate mount point
	switch (*mount_type)
	{
	case mount_by_device_label:
		if (label != NULL)
		{
			mount_dev_len = strlen(label);
			if (mount_dev_len > 0)
			{
				break;
			}
		}

		*mount_type = mount_by_device_name;
		// NOTE: passthrough

	case mount_by_device_name:
		mount_dev_start = strrchr(path, '/');
		if (mount_dev_start == NULL)
		{
			return NULL;
		}

		++mount_dev_start;
		mount_dev_len = strlen(mount_dev_start);

		if (mount_dev_len == 0)
		{
			return NULL;
		}
		break;

	default:
		return NULL;
	}

	mount_path_len = strlen((mount_dir != NULL) ? mount_dir : dtmd_internal_mount_dir);
	mount_dev_len += mount_path_len + 1;

	mount_path = (char*) malloc(mount_dev_len + 1);
	if (mount_path == NULL)
	{
		return NULL;
	}

	memcpy(mount_path, (mount_dir != NULL) ? mount_dir : dtmd_internal_mount_dir, mount_path_len);
	mount_path[mount_path_len] = '/';

	switch (*mount_type)
	{
	case mount_by_device_label:
		memcpy(&mount_path[mount_path_len + 1], label, mount_dev_len - mount_path_len - 1);
		break;

	case mount_by_device_name:
		memcpy(&mount_path[mount_path_len + 1], mount_dev_start, mount_dev_len - mount_path_len - 1);
		break;
	}

	mount_path[mount_dev_len] = 0;

	return mount_path;
}

int invoke_mount(int client_number, const char *path, const char *mount_options, enum mount_by_value_enum mount_type)
{
	int result;
	unsigned int dev, part;

	char *mount_path;

	const char *local_mnt_point;
	const char *local_fstype;
	const char *local_label;

	const struct dtmd_filesystem_options *fsopts;
	dtmd_fsopts_list_t fsopts_list;

	uid_t uid;
	gid_t gid;

	for (dev = 0; dev < media_count; ++dev)
	{
		for (part = 0; part < media[dev]->partitions_count; ++part)
		{
			if (strcmp(media[dev]->partition[part]->path, path) == 0)
			{
				goto invoke_mount_exit_loop;
			}
		}
	}

invoke_mount_exit_loop:
	if (dev < media_count)
	{
		local_mnt_point = media[dev]->partition[part]->mnt_point;
		local_fstype    = media[dev]->partition[part]->fstype;
		local_label     = media[dev]->partition[part]->label;
	}
	else
	{
		for (dev = 0; dev < stateful_media_count; ++dev)
		{
			if (strcmp(stateful_media[dev]->path, path) == 0)
			{
				break;
			}
		}

		if ((dev >= stateful_media_count) || (stateful_media[dev]->state != dtmd_removable_media_state_ok))
		{
			result = 0;
			goto invoke_mount_error_1;
		}

		local_mnt_point = stateful_media[dev]->mnt_point;
		local_fstype    = stateful_media[dev]->fstype;
		local_label     = stateful_media[dev]->label;
	}

	if (local_mnt_point != NULL)
	{
		result = 0;
		goto invoke_mount_error_1;
	}

	if (get_credentials(clients[client_number]->clientfd, &uid, &gid) != 1)
	{
		result = -1;
		goto invoke_mount_error_1;
	}

	fsopts = get_fsopts_for_fs(local_fstype);
	if (fsopts == NULL)
	{
		result = -1;
		goto invoke_mount_error_1;
	}

	init_options_list(&fsopts_list);

	result = convert_options_to_list(mount_options, fsopts, &uid, &gid, &fsopts_list);
	if (result != 1)
	{
		goto invoke_mount_error_2;
	}

	for (;;)
	{
		mount_path = calculate_path(path, local_label, &mount_type);
		if (mount_path == NULL)
		{
			result = -1;
			goto invoke_mount_error_2;
		}

		// check mount point
		result = point_mount_count(mount_path, 1);
		if (result != 0)
		{
			if (result < 0)
			{
				result = -1;
				goto invoke_mount_error_3;
			}
			else
			{
				switch (mount_type)
				{
				case mount_by_device_label:
					mount_type = mount_by_device_name;
					break;

				case mount_by_device_name:
					result = 0;
					goto invoke_mount_error_3;
				}
			}

			free(mount_path);
			continue;
		}

		break;
	}

	if (get_dir_state(mount_path) == dir_state_not_dir)
	{
		// TODO: create directory permissions with DMASK in mind?
		result = mkdir(mount_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		if (result != 0)
		{
			// NOTE: failing to create directory is non-fatal error
			result = 0;
			goto invoke_mount_error_3;
		}
	}

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	if (fsopts->external_fstype != NULL)
	{
		result = invoke_mount_external(client_number, path, mount_path, fsopts->external_fstype, &fsopts_list);
	}
	else
	{
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		result = invoke_mount_internal(client_number, path, mount_path, fsopts->fstype, &fsopts_list);
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	}
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

	if (result != 1)
	{
		rmdir(mount_path);
	}

	free(mount_path);

	return result;

invoke_mount_error_3:
	free(mount_path);

invoke_mount_error_2:
	free_options_list(&fsopts_list);

invoke_mount_error_1:
	return result;
}

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
static int invoke_unmount_external(int client_number, const char *path, const char *mnt_point, const char *fstype)
{
	int result;
	int unmount_cmd_len;
	char *unmount_cmd;

	unmount_cmd_len = strlen(unmount_ext_cmd) + strlen(" -t ") + strlen(fstype) + 1 + strlen(mnt_point);

	unmount_cmd = (char*) malloc(unmount_cmd_len + 1);
	if (unmount_cmd == NULL)
	{
		return -1;
	}

	strcpy(unmount_cmd, unmount_ext_cmd);
	strcat(unmount_cmd, " -t ");
	strcat(unmount_cmd, fstype);
	strcat(unmount_cmd, " ");
	strcat(unmount_cmd, mnt_point);

	result = system(unmount_cmd);

	free(unmount_cmd);

	switch (result)
	{
	case 0:
		return 1;
	case -1:
		return -1;
	default:
		return 0;
	}
}
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

static int invoke_unmount_internal(int client_number, const char *path, const char *mnt_point, const char *fstype)
{
	int result;

	result = point_mount_count(mnt_point, 2);
	if (result != 1)
	{
		if (result < 0)
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}

	// TODO: check that it's original mounter who requests unmount or root?

	result = umount(mnt_point);
	if (result != 0)
	{
		return 0;
	}

	result = remove_from_mtab(path, mnt_point, fstype);
	if (result != 1)
	{
		// NOTE: failing to modify /etc/mtab is non-fatal error
		result = 0;
	}

	return result;
}

static int invoke_unmount_common(int client_number, const char *path, const char *mnt_point, const char *fstype)
{
	int result;

	const struct dtmd_filesystem_options *fsopts;

	fsopts = get_fsopts_for_fs(fstype);
	if (fsopts == NULL)
	{
		result = -1;
		goto invoke_unmount_common_error_1;
	}

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	if (fsopts->external_fstype != NULL)
	{
		result = invoke_unmount_external(client_number, path, mnt_point, fsopts->external_fstype);;
	}
	else
	{
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		result = invoke_unmount_internal(client_number, path, mnt_point, fsopts->fstype);
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	}
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

	if (result == 1)
	{
		if (get_dir_state(mnt_point) == dir_state_empty)
		{
			rmdir(mnt_point);
		}
	}

invoke_unmount_common_error_1:
	return result;
}

int invoke_unmount(int client_number, const char *path)
{
	unsigned int dev, part;
	const char *local_mnt_point;
	const char *local_fstype;

	for (dev = 0; dev < media_count; ++dev)
	{
		for (part = 0; part < media[dev]->partitions_count; ++part)
		{
			if (strcmp(media[dev]->partition[part]->path, path) == 0)
			{
				goto invoke_unmount_exit_loop;
			}
		}
	}

invoke_unmount_exit_loop:
	if (dev < media_count)
	{
		local_mnt_point = media[dev]->partition[part]->mnt_point;
		local_fstype    = media[dev]->partition[part]->fstype;
	}
	else
	{
		for (dev = 0; dev < stateful_media_count; ++dev)
		{
			if (strcmp(stateful_media[dev]->path, path) == 0)
			{
				break;
			}
		}

		if (dev >= stateful_media_count)
		{
			return 0;
		}

		local_mnt_point = stateful_media[dev]->mnt_point;
		local_fstype    = stateful_media[dev]->fstype;
	}

	if (local_mnt_point == NULL)
	{
		return 0;
	}

	return invoke_unmount_common(client_number, path, local_mnt_point, local_fstype);
}

int invoke_unmount_all(int client_number)
{
	unsigned int dev, part;
	int result;

	for (dev = 0; dev < media_count; ++dev)
	{
		for (part = 0; part < media[dev]->partitions_count; ++part)
		{
			if (media[dev]->partition[part]->mnt_point != NULL)
			{
				result = invoke_unmount_common(client_number, media[dev]->partition[part]->path, media[dev]->partition[part]->mnt_point, media[dev]->partition[part]->fstype);
				if (result < 0)
				{
					return result;
				}
			}
		}
	}

	for (dev = 0; dev < stateful_media_count; ++dev)
	{
		if (stateful_media[dev]->mnt_point != NULL)
		{
			result = invoke_unmount_common(client_number, stateful_media[dev]->path, stateful_media[dev]->mnt_point, stateful_media[dev]->fstype);
			if (result < 0)
			{
				return result;
			}
		}
	}

	return 1;
}
