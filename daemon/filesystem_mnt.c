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
#include "daemon/log.h"
#include "daemon/return_codes.h"

#include <dtmd-misc.h>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mount.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#if (defined OS_Linux) && (!defined __USE_GNU)
#define __USE_GNU
#endif /* (defined OS_Linux) && (!defined __USE_GNU) */

#include <sys/socket.h>

#if (defined OS_FreeBSD)
#include <sys/un.h>
#include <sys/ucred.h>
#endif /* (defined OS_FreeBSD) */

typedef enum dir_state
{
	dir_state_not_dir = 0,
	dir_state_empty,
	dir_state_not_empty
} dir_state_t;

#if (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)
static const char * const mount_ext_cmd = "mount";
#endif /* (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT) */

#if ((defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)) || (defined OS_FreeBSD)
static const char * const unmount_ext_cmd = "umount";
#endif /* ((defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)) || (defined OS_FreeBSD) */

#if (defined OS_Linux)
static int get_credentials(int socket_fd, uid_t *uid, gid_t *gid)
{
	struct ucred credentials;
	socklen_t ucred_length = sizeof(struct ucred);

	if ((getsockopt(socket_fd, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length) != 0)
		|| (ucred_length != sizeof(struct ucred)))
	{
		WRITE_LOG(LOG_ERR, "Failed obtaining credentials of client");
		return result_client_error;
	}

	*uid = credentials.uid;
	*gid = credentials.gid;

	return result_success;
}
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
static int get_credentials(int socket_fd, uid_t *uid, gid_t *gid)
{
	struct xucred credentials;
	socklen_t xucred_length = sizeof(struct xucred);

	if (((getsockopt(socket_fd, 0, LOCAL_PEERCRED, &credentials, &xucred_length)) != 0)
		 || (xucred_length != sizeof(struct xucred))
		|| (credentials.cr_version != XUCRED_VERSION))
	{
		WRITE_LOG(LOG_ERR, "Failed obtaining credentials of client");
		return result_client_error;
	}

	*uid = credentials.cr_uid;
	*gid = credentials.cr_gid;

	return result_success;
}
#endif /* (defined OS_FreeBSD) */

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

#if (defined OS_Linux)
#if (!defined DISABLE_EXT_MOUNT)
static int invoke_mount_external(struct client *client_ptr,
	const char *path,
	const char *mount_path,
	const char *fstype,
	dtmd_fsopts_list_t *fsopts_list)
{
	int result;

	int total_len;
	int mount_flags_start;
	char *mount_cmd;
	size_t string_full_len;

	result = fsopts_generate_string(fsopts_list, &string_full_len, NULL, 0, NULL, NULL, 0, NULL);
	if (is_result_failure(result))
	{
		goto invoke_mount_external_error_1;
	}

	// calculate total length
	mount_flags_start = strlen(mount_ext_cmd) + strlen(" -t ") + strlen(fstype) + 1 + strlen(path) + 2 + strlen(mount_path) + 1;

	if (string_full_len > 0)
	{
		mount_flags_start += strlen(" -o ");
	}

	total_len = mount_flags_start + string_full_len;

	mount_cmd = (char*) malloc(total_len + 1);
	if (mount_cmd == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto invoke_mount_external_error_1;
	}

	strcpy(mount_cmd, mount_ext_cmd);
	strcat(mount_cmd, " -t ");
	strcat(mount_cmd, fstype);
	strcat(mount_cmd, " ");
	strcat(mount_cmd, path);
	strcat(mount_cmd, " '");
	strcat(mount_cmd, mount_path);
	strcat(mount_cmd, "'");

	// create flags and string
	if (string_full_len > 0)
	{
		strcat(mount_cmd, " -o ");

		result = fsopts_generate_string(fsopts_list, NULL, &(mount_cmd[mount_flags_start]), string_full_len, NULL, NULL, 0, NULL);
		if (is_result_failure(result))
		{
			goto invoke_mount_external_error_2;
		}
	}

	mount_cmd[total_len] = 0;

	result = system(mount_cmd);

	free(mount_cmd);

	switch (result)
	{
	case 16: /* problems writing or locking /etc/mtab */
		WRITE_LOG(LOG_WARNING, "Failed to modify /etc/mtab");
		/* NOTE: fallthrough */
	case 0:  /* success */
		WRITE_LOG_ARGS(LOG_INFO, "Mounted device '%s' to path '%s'", path, mount_path);
		return result_success;
	default:
		WRITE_LOG_ARGS(LOG_WARNING, "Failed mounting device '%s' to path '%s' using external mount: error, code %d", path, mount_path, result);
		return result_fail;
	}

invoke_mount_external_error_2:
	free(mount_cmd);

invoke_mount_external_error_1:
	return result;
}
#endif /* (!defined DISABLE_EXT_MOUNT) */

static int invoke_mount_internal(struct client *client_ptr,
	const char *path,
	const char *mount_path,
	const char *fstype,
	dtmd_fsopts_list_t *fsopts_list)
{
	unsigned long mount_flags = 0;
	char *mount_opts;
	char *mount_full_opts;
	int result;
	size_t string_full_len;
	size_t string_len;

	result = fsopts_generate_string(fsopts_list, &string_full_len, NULL, 0, &string_len, NULL, 0, NULL);
	if (is_result_failure(result))
	{
		goto invoke_mount_internal_error_1;
	}

	mount_full_opts = (char*) malloc(string_full_len + 1);
	if (mount_full_opts == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto invoke_mount_internal_error_1;
	}

	mount_opts = (char*) malloc(string_len + 1);
	if (mount_opts == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto invoke_mount_internal_error_2;
	}

	result = fsopts_generate_string(fsopts_list, NULL, mount_full_opts, string_full_len, NULL, mount_opts, string_len, &mount_flags);
	if (is_result_failure(result))
	{
		goto invoke_mount_internal_error_3;
	}

	mount_full_opts[string_full_len] = 0;
	mount_opts[string_len] = 0;

	result = mount(path, mount_path, fstype, mount_flags, mount_opts);

	if (result == 0)
	{
		if (is_result_successful(is_mtab_writable()))
		{
			result = add_to_mtab(path, mount_path, fstype, mount_full_opts);
			if (is_result_failure(result))
			{
				// NOTE: failing to modify /etc/mtab is non-fatal error
				WRITE_LOG(LOG_WARNING, "Failed to modify " dtmd_internal_mtab_file );
			}
		}

		result = result_success;

		WRITE_LOG_ARGS(LOG_INFO, "Mounted device '%s' to path '%s'", path, mount_path);
	}
	else
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed mounting device '%s' to path '%s'", path, mount_path);
		result = result_fail;
	}

invoke_mount_internal_error_3:
	free(mount_opts);

invoke_mount_internal_error_2:
	free(mount_full_opts);

invoke_mount_internal_error_1:
	return result;
}
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
static int invoke_mount_external(struct client *client_ptr,
	const char *path,
	const char *mount_path,
	const char *mount_cmd_exe,
	dtmd_fsopts_list_t *fsopts_list)
{

	int result;

	int total_len;
	int mount_flags_start;
	char *mount_cmd;
	size_t string_full_len;

	result = fsopts_generate_string(fsopts_list, &string_full_len, NULL, 0);
	if (is_result_failure(result))
	{
		goto invoke_mount_external_error_1;
	}

	// calculate total length
	mount_flags_start = strlen(mount_cmd_exe) + 1 + strlen(path) + 2 + strlen(mount_path) + 1;

	if (string_full_len > 0)
	{
		mount_flags_start += strlen(" ");
	}

	total_len = mount_flags_start + string_full_len;

	mount_cmd = (char*) malloc(total_len + 1);
	if (mount_cmd == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		result = result_fatal_error;
		goto invoke_mount_external_error_1;
	}

	strcpy(mount_cmd, mount_cmd_exe);

	// create flags and string
	if (string_full_len > 0)
	{
		strcat(mount_cmd, " ");

		result = fsopts_generate_string(fsopts_list, NULL, &(mount_cmd[strlen(mount_cmd_exe) + strlen(" ")]), string_full_len);
		if (is_result_failure(result))
		{
			goto invoke_mount_external_error_2;
		}

		mount_cmd[strlen(mount_cmd_exe) + strlen(" ") + string_full_len] = 0;
	}

	strcat(mount_cmd, " ");
	strcat(mount_cmd, path);
	strcat(mount_cmd, " '");
	strcat(mount_cmd, mount_path);
	strcat(mount_cmd, "'");

	result = system(mount_cmd);

	free(mount_cmd);

	switch (result)
	{
	case 16: /* problems writing or locking /etc/mtab */
		WRITE_LOG(LOG_WARNING, "Failed to modify /etc/mtab");
		/* NOTE: fallthrough */
	case 0:  /* success */
		WRITE_LOG_ARGS(LOG_INFO, "Mounted device '%s' to path '%s'", path, mount_path);
		return result_success;
	default:
		WRITE_LOG_ARGS(LOG_WARNING, "Failed mounting device '%s' to path '%s' using external mount: error, code %d", path, mount_path, result);
		return result_fail;
	}

invoke_mount_external_error_2:
	free(mount_cmd);

invoke_mount_external_error_1:
	return result;
}
#endif /* (defined OS_FreeBSD) */

static char* calculate_path(const char *path, const char *label, enum mount_by_value_enum *mount_type)
{
	const char *mount_dev_start = NULL;

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
			WRITE_LOG(LOG_ERR, "Invalid device name is used for mounting");
			return NULL;
		}

		++mount_dev_start;
		mount_dev_len = strlen(mount_dev_start);

		if (mount_dev_len == 0)
		{
			WRITE_LOG(LOG_ERR, "Invalid device name is used for mounting");
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
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
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

int invoke_mount(struct client *client_ptr, const char *path, const char *mount_options, enum mount_by_value_enum mount_type, dtmd_error_code_t *error_code)
{
	int result;
	dtmd_removable_media_t *media_ptr;

	char *mount_path;
	const char *mandatory_mount_options;

	const struct dtmd_filesystem_options *fsopts;
	dtmd_fsopts_list_t fsopts_list;

	uid_t uid;
	gid_t gid;

	media_ptr = dtmd_find_media(path, removable_media_root);
	if (media_ptr == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed mounting device '%s': device does not exist or is not ready", path);
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_no_such_removable_device;
		}

		goto invoke_mount_error_1;
	}

	if (media_ptr->fstype == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed mounting device '%s': device doesn't have recognized filesystem", path);
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_fstype_not_recognized;
		}

		goto invoke_mount_error_1;
	}

	if (media_ptr->mnt_point != NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed mounting device '%s': device is already mounted", path);
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_device_already_mounted;
		}

		goto invoke_mount_error_1;
	}

	result = get_credentials(client_ptr->clientfd, &uid, &gid);
	if (is_result_failure(result))
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		goto invoke_mount_error_1;
	}

	fsopts = get_fsopts_for_fs(media_ptr->fstype);
	if (fsopts == NULL)
	{
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_unsupported_fstype;
		}

		goto invoke_mount_error_1;
	}

	if (mount_options == NULL)
	{
#if (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)
		if (fsopts->external_fstype != NULL)
		{
			mount_options = get_default_mount_options_for_fs_from_config(fsopts->external_fstype);
		}
		else
		{
#endif /* (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT) */
			mount_options = get_default_mount_options_for_fs_from_config(fsopts->fstype);
#if (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)
		}
#endif /* (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT) */
	}

	if (mount_options == NULL)
	{
		mount_options = fsopts->defaults;
	}

	init_options_list(&fsopts_list);

	result = convert_options_to_list(mount_options, fsopts, &uid, &gid, &fsopts_list);
	if (is_result_failure(result))
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_failed_parsing_mount_options;
		}

		goto invoke_mount_error_2;
	}

#if (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)
	if (fsopts->external_fstype != NULL)
	{
		mandatory_mount_options = get_mandatory_mount_options_for_fs_from_config(fsopts->external_fstype);
	}
	else
	{
#endif /* (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT) */
		mandatory_mount_options = get_mandatory_mount_options_for_fs_from_config(fsopts->fstype);
#if (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)
	}
#endif /* (defined OS_Linux) && (!defined DISABLE_EXT_MOUNT) */

	if (mandatory_mount_options == NULL)
	{
		mandatory_mount_options = fsopts->mandatory_options;
	}

	result = convert_options_to_list(mandatory_mount_options, fsopts, NULL, NULL, &fsopts_list);
	if (is_result_failure(result))
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		goto invoke_mount_error_2;
	}

	for (;;)
	{
		mount_path = calculate_path(path, media_ptr->label, &mount_type);
		if (mount_path == NULL)
		{
			result = result_fatal_error;

			if (error_code != NULL)
			{
				*error_code = dtmd_error_code_generic_error;
			}

			goto invoke_mount_error_2;
		}

		// check mount point
		result = point_mount_count(mount_path, 1);
		if (result != 0)
		{
			if (result < 0)
			{
				result = result_fatal_error;

				if (error_code != NULL)
				{
					*error_code = dtmd_error_code_generic_error;
				}

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
					WRITE_LOG_ARGS(LOG_WARNING, "Could not find suitable mount point for device '%s'", path);
					result = result_fail;

					if (error_code != NULL)
					{
						*error_code = dtmd_error_code_mount_point_busy;
					}

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
			WRITE_LOG_ARGS(LOG_WARNING, "Failed to create directory '%s'", mount_path);
			result = result_fail;

			if (error_code != NULL)
			{
				*error_code = dtmd_error_code_mount_point_busy;
			}

			goto invoke_mount_error_3;
		}
	}

#if (defined OS_Linux)
#if (!defined DISABLE_EXT_MOUNT)
	if (fsopts->external_fstype != NULL)
	{
		result = invoke_mount_external(client_ptr, path, mount_path, fsopts->external_fstype, &fsopts_list);
	}
	else
	{
#endif /* (!defined DISABLE_EXT_MOUNT) */
		result = invoke_mount_internal(client_ptr, path, mount_path, fsopts->fstype, &fsopts_list);
#if (!defined DISABLE_EXT_MOUNT)
	}
#endif /* (!defined DISABLE_EXT_MOUNT) */
#else /* (defined OS_Linux) */
#if (defined OS_FreeBSD)
	result = invoke_mount_external(client_ptr, path, mount_path, fsopts->mount_cmd, &fsopts_list);
#else /* (defined OS_FreeBSD) */
#error Unsupported OS
#endif /* (defined OS_FreeBSD) */
#endif /* (defined OS_Linux) */

	if (is_result_failure(result))
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		rmdir(mount_path);
	}

invoke_mount_error_3:
	free(mount_path);

invoke_mount_error_2:
	free_options_list(&fsopts_list);

invoke_mount_error_1:
	return result;
}

#if ((defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)) || (defined OS_FreeBSD)
static int invoke_unmount_external(struct client *client_ptr, const char *path, const char *mnt_point, const char *fstype, dtmd_error_code_t *error_code)
{
	int result;
	int unmount_cmd_len;
	char *unmount_cmd;

	unmount_cmd_len = strlen(unmount_ext_cmd) + strlen(" -t ") + strlen(fstype) + 2 + strlen(mnt_point) + 1;

	unmount_cmd = (char*) malloc(unmount_cmd_len + 1);
	if (unmount_cmd == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		return result_fatal_error;
	}

	strcpy(unmount_cmd, unmount_ext_cmd);
	strcat(unmount_cmd, " -t ");
	strcat(unmount_cmd, fstype);
	strcat(unmount_cmd, " \"");
	strcat(unmount_cmd, mnt_point);
	strcat(unmount_cmd, "\"");

	result = system(unmount_cmd);

	free(unmount_cmd);

	switch (result)
	{
	case 0:
		WRITE_LOG_ARGS(LOG_INFO, "Unmounted device '%s' from path '%s'", path, mnt_point);
		return result_success;

	default:
		WRITE_LOG_ARGS(LOG_WARNING, "Failed unmounting device '%s' from path '%s' using external umount: error, code %d", path, mnt_point, result);

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		return result_fail;
	}
}
#endif /* ((defined OS_Linux) && (!defined DISABLE_EXT_MOUNT)) || (defined OS_FreeBSD) */

#if (defined OS_Linux)
static int invoke_unmount_internal(struct client *client_ptr, const char *path, const char *mnt_point, const char *fstype, dtmd_error_code_t *error_code)
{
	int result;
	int saved_errno;

	result = point_mount_count(mnt_point, 2);
	if (result != 1)
	{
		if (result < 0)
		{
			if (error_code != NULL)
			{
				*error_code = dtmd_error_code_generic_error;
			}

			return result_fatal_error;
		}
		else
		{
			if (error_code != NULL)
			{
				*error_code = dtmd_error_code_device_not_mounted;
			}

			return result_fail;
		}
	}

	// TODO: check that it's original mounter who requests unmount or root?

	result = umount(mnt_point);

	if (result != 0)
	{
		saved_errno = errno;

		WRITE_LOG_ARGS(LOG_WARNING, "Failed unmounting device '%s' from path '%s'", path, mnt_point);

		if (error_code != NULL)
		{
			if (saved_errno == EBUSY)
			{
				*error_code = dtmd_error_code_mount_point_busy;
			}
			else
			{
				*error_code = dtmd_error_code_generic_error;
			}
		}

		result = result_fail;
	}
	else
	{
		if (is_result_successful(is_mtab_writable()))
		{
			result = remove_from_mtab(path, mnt_point, fstype);
			if (is_result_failure(result))
			{
				// NOTE: failing to modify /etc/mtab is non-fatal error
				WRITE_LOG(LOG_WARNING, "Failed to modify " dtmd_internal_mtab_file);
			}
		}

		WRITE_LOG_ARGS(LOG_INFO, "Unmounted device '%s' from path '%s'", path, mnt_point);
		result = result_success;
	}

	return result;
}
#endif /* (defined OS_Linux) */

static int invoke_unmount_common(struct client *client_ptr, const char *path, const char *mnt_point, const char *fstype, dtmd_error_code_t *error_code)
{
	int result;

	const struct dtmd_filesystem_options *fsopts;

	fsopts = get_fsopts_for_fs(fstype);
	if (fsopts == NULL)
	{
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_unsupported_fstype;
		}

		goto invoke_unmount_common_error_1;
	}

#if (defined OS_Linux)
#if (!defined DISABLE_EXT_MOUNT)
	if (fsopts->external_fstype != NULL)
	{
		result = invoke_unmount_external(client_ptr, path, mnt_point, fsopts->external_fstype, error_code);
	}
	else
	{
#endif /* (!defined DISABLE_EXT_MOUNT) */
		result = invoke_unmount_internal(client_ptr, path, mnt_point, fsopts->fstype, error_code);
#if (!defined DISABLE_EXT_MOUNT)
	}
#endif /* (!defined DISABLE_EXT_MOUNT) */
#else /* (defined OS_Linux) */
#if (defined OS_FreeBSD)
	result = invoke_unmount_external(client_ptr, path, mnt_point, fsopts->external_fstype, error_code);
#else /* (defined OS_FreeBSD) */
#error Unsupported OS
#endif /* (defined OS_FreeBSD) */
#endif /* (defined OS_Linux) */

	if (is_result_successful(result))
	{
		if (get_dir_state(mnt_point) == dir_state_empty)
		{
			rmdir(mnt_point);
		}
	}

invoke_unmount_common_error_1:
	return result;
}

int invoke_unmount(struct client *client_ptr, const char *path, dtmd_error_code_t *error_code)
{
	dtmd_removable_media_t *media_ptr;

	media_ptr = dtmd_find_media(path, removable_media_root);
	if (media_ptr == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed unmounting device '%s': device does not exist", path);

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_no_such_removable_device;
		}

		return result_fail;
	}

	if (media_ptr->mnt_point == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed unmounting device '%s': device is not mounted", path);

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_device_not_mounted;
		}

		return result_fail;
	}

	return invoke_unmount_common(client_ptr, path, media_ptr->mnt_point, media_ptr->fstype, error_code);
}

static int invoke_unmount_recursive(struct client *client_ptr, dtmd_removable_media_t *media_ptr)
{
	int result;
	dtmd_removable_media_t *iter_media_ptr;

	for (iter_media_ptr = media_ptr->children_list; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		result = invoke_unmount_recursive(client_ptr, iter_media_ptr);
		if (is_result_fatal_error(result))
		{
			return result;
		}
	}

	if (media_ptr->mnt_point != NULL)
	{
		result = invoke_unmount_common(client_ptr, media_ptr->path, media_ptr->mnt_point, media_ptr->fstype, NULL);
		if (is_result_fatal_error(result))
		{
			return result;
		}
	}

	return result_success;
}

int invoke_unmount_all(struct client *client_ptr)
{
	int result;
	dtmd_removable_media_t *iter_media_ptr;

	for (iter_media_ptr = removable_media_root; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		result = invoke_unmount_recursive(client_ptr, iter_media_ptr);
		if (is_result_fatal_error(result))
		{
			return result;
		}
	}

	return result_success;
}
