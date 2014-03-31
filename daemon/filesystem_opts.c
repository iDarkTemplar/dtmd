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

#include "daemon/filesystem_opts.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mount.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* option: flag and strings
MS_BIND (Linux 2.4 onward)
	bind
MS_DIRSYNC (since Linux 2.5.19)
	dirsync
MS_MANDLOCK
	mand / nomand
MS_MOVE
MS_NOATIME
	noatime / atime
MS_NODEV
	nodev / dev
MS_NODIRATIME
	nodiratime / diratime
MS_NOEXEC
	noexec / exec
MS_NOSUID
	nosuid / suid
MS_RDONLY
	ro / rw
MS_RELATIME (Since Linux 2.6.20)
	relatime
MS_REMOUNT
	remount
MS_SILENT (since Linux 2.6.17)
	silent / loud
MS_STRICTATIME (Since Linux 2.6.30)
	strictatime
MS_SYNCHRONOUS
	sync
 */

struct string_to_mount_flag
{
	const char * const option;
	const unsigned long flag;
	const unsigned char enabled;
};

static const struct string_to_mount_flag string_to_mount_flag_list[] =
{
	{ "dirsync",     MS_DIRSYNC,     1 },
	{ "mand",        MS_MANDLOCK,    1 },
	{ "nomand",      MS_MANDLOCK,    0 },
	{ "noatime",     MS_NOATIME,     1 },
	{ "atime",       MS_NOATIME,     0 },
	{ "nodev",       MS_NODEV,       1 },
	{ "dev",         MS_NODEV,       0 },
	{ "nodiratime",  MS_NODIRATIME,  1 },
	{ "diratime",    MS_NODIRATIME,  0 },
	{ "noexec",      MS_NOEXEC,      1 },
	{ "exec",        MS_NOEXEC,      0 },
	{ "nosuid",      MS_NOSUID,      1 },
	{ "suid",        MS_NOSUID,      0 },
	{ "ro",          MS_RDONLY,      1 },
	{ "rw",          MS_RDONLY,      0 },
	{ "relatime",    MS_RELATIME,    1 },
	{ "silent",      MS_SILENT,      1 },
	{ "loud",        MS_SILENT,      0 },
	{ "strictatime", MS_STRICTATIME, 1 },
	{ "sync",        MS_SYNCHRONOUS, 1 },
	{ NULL,          0,              0 }
};
#if 0
struct dtmd_mount_option
{
	const char * const option;
	const unsigned char has_param;
};
#endif
static const struct dtmd_mount_option vfat_allow[] =
{
	{ "flush",        0 },
	{ "utf8=",        1 },
	{ "shortname=",   1 },
	{ "umask=",       1 },
	{ "dmask=",       1 },
	{ "fmask=",       1 },
	{ "codepage=",    1 },
	{ "iocharset=",   1 },
	{ "showexec",     0 },
	{ "blocksize=",   1 },
	{ "allow_utime=", 1 },
	{ "check=",       1 },
	{ "conv=",        1 },
	{ NULL,           0 }
};

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
static const struct dtmd_mount_option ntfs3g_allow[] =
{
	{ "umask=",        1 },
	{ "dmask=",        1 },
	{ "fmask=",        1 },
	{ "iocharset=",    1 },
	{ "utf8",          0 },
	{ "windows_names", 0 },
	{ "allow_other",   0 },
	{ NULL,            0 }
};
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

static const struct dtmd_mount_option iso9660_allow[] =
{
	{ "norock",     0 },
	{ "nojoliet",   0 },
	{ "iocharset=", 1 },
	{ "mode=",      1 },
	{ "dmode=",     1 },
	{ "utf8",       0 },
	{ "block=",     1 },
	{ "conv=",      1 },
	{ NULL,         0 }
};

static const struct dtmd_mount_option udf_allow[] =
{
	{ "iocharset=", 1 },
	{ "umask=",     1 },
	{ "mode=",      1 },
	{ "dmode=",     1 },
	{ "undelete",   0 },
	{ NULL,         0 }
};
#if 0
struct dtmd_filesystem_options
{
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	const char * const external_fstype;
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
	const char * const fstype;
	const struct dtmd_mount_option * const options;
	const char * const option_uid;
	const char * const option_gid;
	const char * const defaults;
};
#endif
static const struct dtmd_filesystem_options filesystem_mount_options[] =
{
	{
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
		NULL, /* NOT EXTERNAL MOUNT */
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		"vfat",
		vfat_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,shortname=mixed,dmask=0077,utf8=1,flush"
	},
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	{
		"ntfs-3g", /* EXTERNAL MOUNT */
		"ntfs-3g",
		ntfs3g_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,allow_other,dmask=0077"
	},
	{
		"ntfs-3g", /* EXTERNAL MOUNT */
		"ntfs",
		ntfs3g_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,allow_other,dmask=0077"
	},
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
	{
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
		NULL, /* NOT EXTERNAL MOUNT */
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		"iso9660",
		iso9660_allow,
		"uid=",
		"gid=",
		"ro,nodev,nosuid,iocharset=utf8,mode=0400,dmode=0500"
	},
	{
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
		NULL, /* NOT EXTERNAL MOUNT */
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		"udf",
		udf_allow,
		"uid=",
		"gid=",
		"ro,nodev,nosuid,iocharset=utf8,umask=0077"
	},
	{
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
		NULL,
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	}
};

static const struct dtmd_mount_option any_fs_allowed_list[] =
{
	{ "exec",       0 },
	{ "noexec",     0 },
	{ "nodev",      0 },
	{ "nosuid",     0 },
	{ "atime",      0 },
	{ "noatime",    0 },
	{ "nodiratime", 0 },
	{ "ro",         0 },
	{ "rw",         0 },
	{ "sync",       0 },
	{ "dirsync",    0 },
	{ NULL,         0 }
};

int dtmd_are_options_supported(const char *filesystem, const char *options_list)
{
	const char *opt_start;
	const char *opt_end;
	unsigned int opt_len;

	const struct dtmd_filesystem_options *fsopts;

	fsopts = dtmd_get_fsopts_for_fstype(filesystem);

	opt_start = options_list;

	if (*opt_start != 0)
	{
		while (opt_start != NULL)
		{
			opt_end = strchr(opt_start, ',');

			if (opt_end != NULL)
			{
				opt_len = opt_end - opt_start;
				++opt_end;
			}
			else
			{
				opt_len = strlen(opt_start);
			}

			if (opt_len == 0)
			{
				return 0;
			}

			// check option
			if (!dtmd_is_option_allowed(opt_start, opt_len, fsopts))
			{
				return 0;
			}

			opt_start = opt_end;
		}
	}

	return 1;
}

int dtmd_is_option_allowed(const char *option, unsigned int option_len, const struct dtmd_filesystem_options *filesystem_list)
{
	const struct dtmd_mount_option *option_list;
	unsigned int minlen;
	const struct dtmd_mount_option *options_lists_array[2];
	unsigned int array_index;
	unsigned int array_size;

	options_lists_array[0] = any_fs_allowed_list;

	if (filesystem_list != NULL)
	{
		options_lists_array[1] = filesystem_list->options;
		array_size = sizeof(options_lists_array)/sizeof(options_lists_array[0]);
	}
	else
	{
		options_lists_array[1] = NULL;
		array_size = sizeof(options_lists_array)/sizeof(options_lists_array[0]) - 1;
	}

	for (array_index = 0; array_index < array_size; ++array_index)
	{
		for (option_list = options_lists_array[array_index]; option_list->option != NULL; ++option_list)
		{
			if (option_list->has_param)
			{
				minlen = strlen(option_list->option);

				if ((option_len > minlen) && (strncmp(option, option_list->option, minlen) == 0))
				{
					return 1;
				}
			}
			else
			{
				if ((strlen(option_list->option) == option_len) && (strncmp(option, option_list->option, option_len) == 0))
				{
					return 1;
				}
			}
		}
	}

/* NOTE: uid and gid options explicitly are not allowed
	if (filesystem_list != NULL)
	{
		if ((filesystem_list->option_uid != NULL)
			&& (option_len > (minlen = strlen(filesystem_list->option_uid)))
			&& (strncmp(option, filesystem_list->option_uid, minlen) == 0))
		{
			return 1;
		}

		if ((filesystem_list->option_gid != NULL)
			&& (option_len > (minlen = strlen(filesystem_list->option_gid)))
			&& (strncmp(option, filesystem_list->option_gid, minlen) == 0))
		{
			return 1;
		}
	}
*/

	return 0;
}

static struct dtmd_mount_option* find_option_in_list(const char *option, unsigned int option_len, const struct dtmd_filesystem_options *filesystem_list)
{
	const struct dtmd_mount_option *option_list;
	unsigned int minlen;
	const struct dtmd_mount_option *options_lists_array[2];
	unsigned int array_index;
	unsigned int array_size;

	options_lists_array[0] = any_fs_allowed_list;

	if (filesystem_list != NULL)
	{
		options_lists_array[1] = filesystem_list->options;
		array_size = sizeof(options_lists_array)/sizeof(options_lists_array[0]);
	}
	else
	{
		options_lists_array[1] = NULL;
		array_size = sizeof(options_lists_array)/sizeof(options_lists_array[0]) - 1;
	}

	for (array_index = 0; array_index < array_size; ++array_index)
	{
		for (option_list = options_lists_array[array_index]; option_list->option != NULL; ++option_list)
		{
			if (option_list->has_param)
			{
				minlen = strlen(option_list->option);

				if ((option_len > minlen) && (strncmp(option, option_list->option, minlen) == 0))
				{
					return option_list;
				}
			}
			else
			{
				if ((strlen(option_list->option) == option_len) && (strncmp(option, option_list->option, option_len) == 0))
				{
					return option_list;
				}
			}
		}
	}

	return NULL;
}
const struct dtmd_filesystem_options* dtmd_get_fsopts_for_fstype(const char *fstype)
{
	const struct dtmd_filesystem_options *fsopts = filesystem_mount_options;

	for (;;)
	{
		if (fsopts->fstype == NULL)
		{
			return NULL;
		}

		if (strcmp(fsopts->fstype, fstype) == 0)
		{
			return fsopts;
		}

		++fsopts;
	}
}

struct dtmd_internal_fsopts_item
{
	string_to_mount_flag option;
	unsigned int option_full_len;
	unsigned int option_len;
};

struct dtmd_internal_fsopts_id
{
	const char *id_option;
	unsigned int id_option_len;

	char *id_option_value;
	unsigned int id_option_value_len;
};

typedef struct dtmd_internal_fsopts
{
	struct dtmd_internal_fsopts_item **options;
	unsigned int options_count;

	struct dtmd_internal_fsopts_id option_uid;
	struct dtmd_internal_fsopts_id option_gid;
} dtmd_internal_fsopts_t;

static void init_options_list_id(struct dtmd_internal_fsopts_id *id)
{
	id->id_option           = NULL;
	id->id_option_len       = 0;
	id->id_option_value     = NULL;
	id->id_option_value_len = 0;
}

static void init_options_list(dtmd_internal_fsopts_t *fsopts_list)
{
	fsopts_list->options       = NULL;
	fsopts_list->options_count = 0;

	init_options_list_id(&(fsopts_list->option_uid));
	init_options_list_id(&(fsopts_list->option_gid));
}

static void free_options_list_id(struct dtmd_internal_fsopts_id *id)
{
	id->id_option = NULL;
	id->id_option_len = 0;

	if (id->id_option_value != NULL)
	{
		free(id->id_option_value);
		id->id_option_value = NULL;
	}

	id->id_option_value_len = 0;
}

static void free_options_list(dtmd_internal_fsopts_t *fsopts_list)
{
	unsigned int index = 0;

	if (fsopts_list->options != NULL)
	{
		for ( ; index < fsopts_list->options_count; ++index)
		{
			if (fsopts_list->options[index] != NULL)
			{
				free(fsopts_list->options[index]);
			}
		}

		free(fsopts_list->options);
		fsopts_list->options = NULL;
	}

	fsopts_list->options_count = 0;

	free_options_list_id(&(fsopts_list->option_uid));
	free_options_list_id(&(fsopts_list->option_gid));
}

static int convert_options_to_list(const char *options_list, const struct dtmd_filesystem_options *fsopts, uid_t *uid, gid_t *gid, dtmd_internal_fsopts_t *fsopts_list)
{
	const struct string_to_mount_flag *mntflagslist;
	struct dtmd_mount_option *option_params;

	const char *opt_start;
	const char *opt_end;
	unsigned int opt_len;
	int result;

	unsigned int option_index;

	struct dtmd_internal_fsopts_item *option_item;
	void *tmp;

	opt_start = options_list;

	if (*opt_start != 0)
	{
		while (opt_start != NULL)
		{
			opt_end = strchr(opt_start, ',');

			if (opt_end != NULL)
			{
				opt_len = opt_end - opt_start;
				++opt_end;
			}
			else
			{
				opt_len = strlen(opt_start);
			}

			if (opt_len == 0)
			{
				return 0;
			}

			// check option
			option_params = find_option_in_list(opt_start, opt_len, fsopts);
			if (option_params == NULL)
			{
				return 0;
			}

			mntflagslist = string_to_mount_flag_list;
			while (mntflagslist->option != NULL)
			{
				if (opt_len == strlen(mntflagslist->option)
					&& (strncmp(opt_start, mntflagslist->option, opt_len) == 0))
				{
					break;
				}

				++mntflagslist;
			}

			// check that option is not duplicated
			option_index = 0;

			if (fsopts_list->options != NULL)
			{
				if (mntflagslist->option == NULL)
				{
					for ( ; option_index < fsopts_list->options_count; ++option_index)
					{
						if (strncmp(opt_start, fsopts_list->options[option_index]->option.option, fsopts_list->options[option_index]->option_len) == 0)
						{
							break;
						}
					}
				}
				else
				{
					for ( ; option_index < fsopts_list->options_count; ++option_index)
					{
						if (mntflagslist->flag == fsopts_list->options[option_index]->option.flag)
						{
							break;
						}
					}
				}
			}

			if (option_index < fsopts_list->options_count)
			{
				// found
				option_item = fsopts_list->options[option_index];

				for ( ; option_index < fsopts_list->options_count - 1; ++option_index)
				{
					fsopts_list->options[option_index] = fsopts_list->options[option_index + 1];
				}

				fsopts_list->options[option_index] = option_item;
			}
			else
			{
				// not found
				option_item = (struct dtmd_internal_fsopts_item*) malloc(sizeof(struct dtmd_internal_fsopts_item));
				if (option_item == NULL)
				{
					return -1;
				}

				tmp = realloc(fsopts_list->options, (fsopts_list->options_count + 1) * sizeof(struct dtmd_internal_fsopts_item*));
				if (tmp == NULL)
				{
					free(option_item);
					return -1;
				}

				fsopts_list->options = (struct dtmd_internal_fsopts_item**) tmp;
				++(fsopts_list->options_count);
			}

			// fill item
			option_item->option.option   = opt_start;
			option_item->option_full_len = opt_len;

			if (mntflagslist->option == NULL)
			{
				option_item->option.flag     = 0;
				option_item->option.enabled  = 0;
				option_item->option_len      = strlen(option_params->option);
			}
			else
			{
				option_item->option.flag     = mntflagslist->flag;
				option_item->option.enabled  = mntflagslist->enabled;
				option_item->option_len      = opt_len;
			}

			opt_start = opt_end;
		}
	}

	// add uid/gid
	if (fsopts != NULL)
	{
		if ((uid != NULL) && (fsopts->option_uid != NULL))
		{
			result = snprintf(NULL, 0, "%u", *uid);
			if (result < 1)
			{
				return -1;
			}

			fsopts_list->option_uid.id_option_value = (char*) malloc(result + 1);
			if (fsopts_list->option_uid.id_option_value == NULL)
			{
				return -1;
			}

			result = snprintf(fsopts_list->option_uid.id_option_value, result + 1, "%u", *uid);
			if (result < 1)
			{
				return -1;
			}

			fsopts_list->option_uid.id_option_value_len = result;
			fsopts_list->option_uid.id_option     = fsopts->option_uid;
			fsopts_list->option_uid.id_option_len = strlen(fsopts->option_uid);
		}

		if ((gid != NULL) && (fsopts->option_gid != NULL))
		{
			result = snprintf(NULL, 0, "%u", *gid);
			if (result < 1)
			{
				return -1;
			}

			fsopts_list->option_gid.id_option_value = (char*) malloc(result + 1);
			if (fsopts_list->option_gid.id_option_value == NULL)
			{
				return -1;
			}

			result = snprintf(fsopts_list->option_gid.id_option_value, result + 1, "%u", *gid);
			if (result < 1)
			{
				return -1;
			}

			fsopts_list->option_gid.id_option_value_len = result;
			fsopts_list->option_gid.id_option     = fsopts->option_gid;
			fsopts_list->option_gid.id_option_len = strlen(fsopts->option_gid);
		}
	}

	return -1;
}

dtmd_fsopts_result_t dtmd_fsopts_get_info(const char *options_list,
	const char *filesystem,
	uid_t *uid,
	gid_t *gid,
	unsigned int *options_string_length,
	unsigned int *mtab_options_string_length)
{
	const struct string_to_mount_flag *mntflagslist;
	const struct dtmd_filesystem_options *fsopts;

	unsigned int mount_opts_len_cur = 0;
	unsigned int mount_opts_len_count = 0;

	unsigned int mount_all_opts_len_cur = 0;
	unsigned int mount_all_opts_len_count = 0;

	const char *opt_start;
	const char *opt_end;
	unsigned int opt_len;
	int id_len;
	dtmd_fsopts_result_t result;

	fsopts = dtmd_get_fsopts_for_fstype(filesystem);

	if (options_list != NULL)
	{
		opt_start = options_list;
	}
	else if (fsopts != NULL)
	{
		opt_start = fsopts->defaults;
	}
	else
	{
		result = dtmd_fsopts_not_supported;
		goto dtmd_fsopts_get_info_error_1;
	}

	if (*opt_start != 0)
	{
		while (opt_start != NULL)
		{
			opt_end = strchr(opt_start, ',');

			if (opt_end != NULL)
			{
				opt_len = opt_end - opt_start;
				++opt_end;
			}
			else
			{
				opt_len = strlen(opt_start);
			}

			if (opt_len == 0)
			{
				result = dtmd_fsopts_not_supported;
				goto dtmd_fsopts_get_info_error_1;
			}

			// check option
			if (!dtmd_is_option_allowed(opt_start, opt_len, fsopts))
			{
				result = dtmd_fsopts_not_supported;
				goto dtmd_fsopts_get_info_error_1;
			}

			mntflagslist = string_to_mount_flag_list;
			while (mntflagslist->option != NULL)
			{
				if (opt_len == strlen(mntflagslist->option)
					&& (strncmp(opt_start, mntflagslist->option, opt_len) == 0))
				{
					break;
				}

				++mntflagslist;
			}

			if ((mntflagslist->option == NULL)
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
				|| ((fsopts != NULL) && (fsopts->external_fstype != NULL))
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
				)
			{
				mount_opts_len_cur += opt_len;
				++mount_opts_len_count;
			}

			mount_all_opts_len_cur += opt_len;
			++mount_all_opts_len_count;

			opt_start = opt_end;
		}
	}

	// add uid/gid
	if (fsopts != NULL)
	{
		if ((uid != NULL) && (fsopts->option_uid != NULL))
		{
			id_len = snprintf(NULL, 0, "%d", *uid);
			if (id_len < 1)
			{
				result = dtmd_fsopts_error;
				goto dtmd_fsopts_get_info_error_1;
			}

			mount_opts_len_cur += strlen(fsopts->option_uid) + id_len;
			++mount_opts_len_count;

			mount_all_opts_len_cur += strlen(fsopts->option_uid) + id_len;
			++mount_all_opts_len_count;
		}

		if ((gid != NULL) && (fsopts->option_gid != NULL))
		{
			id_len = snprintf(NULL, 0, "%d", *gid);
			if (id_len < 1)
			{
				result = dtmd_fsopts_error;
				goto dtmd_fsopts_get_info_error_1;
			}

			mount_opts_len_cur += strlen(fsopts->option_gid) + id_len;
			++mount_opts_len_count;

			mount_all_opts_len_cur += strlen(fsopts->option_gid) + id_len;
			++mount_all_opts_len_count;
		}
	}

	if (mount_opts_len_count > 1)
	{
		mount_opts_len_cur += mount_opts_len_count - 1;
	}

	if (mount_all_opts_len_count > 1)
	{
		mount_all_opts_len_cur += mount_all_opts_len_count - 1;
	}

	if (options_string_length != NULL)
	{
		*options_string_length = mount_opts_len_cur;
	}

	if (mtab_options_string_length != NULL)
	{
		*mtab_options_string_length = mount_all_opts_len_cur;
	}

#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	if ((fsopts != NULL) && (fsopts->external_fstype != NULL))
	{
		return dtmd_fsopts_external_mount;
	}
	else
	{
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
		return dtmd_fsopts_internal_mount;
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
	}
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */

dtmd_fsopts_get_info_error_1:
	return result;
}

int dtmd_fsopts_generate_string(const char *options_list,
	const char *filesystem,
	uid_t *uid,
	gid_t *gid,
	char *options_string_buffer,
	unsigned int options_string_buffer_size,
	char *mtab_options_string_buffer,
	unsigned int mtab_options_string_buffer_size,
	unsigned long *mount_flags)
{
	const struct string_to_mount_flag *mntflagslist;
	const struct dtmd_filesystem_options *fsopts;

	unsigned int mount_opts_len_cur = 0;
	unsigned int mount_all_opts_len_cur = 0;

	const char *opt_start;
	const char *opt_end;
	unsigned int opt_len;
	int result;

	fsopts = dtmd_get_fsopts_for_fstype(filesystem);

	if (options_list != NULL)
	{
		opt_start = options_list;
	}
	else if (fsopts != NULL)
	{
		opt_start = fsopts->defaults;
	}
	else
	{
		result = 0;
		goto dtmd_fsopts_generate_string_error_1;
	}

	while (opt_start != NULL)
	{
		opt_end = strchr(opt_start, ',');

		if (opt_end != NULL)
		{
			opt_len = opt_end - opt_start;
			++opt_end;
		}
		else
		{
			opt_len = strlen(opt_start);
		}

		if (opt_len == 0)
		{
			result = 0;
			goto dtmd_fsopts_generate_string_error_1;
		}

		// check option
		if (!dtmd_is_option_allowed(opt_start, opt_len, fsopts))
		{
			result = 0;
			goto dtmd_fsopts_generate_string_error_1;
		}

		// mount options
		mntflagslist = string_to_mount_flag_list;
		while (mntflagslist->option != NULL)
		{
			if (opt_len == strlen(mntflagslist->option)
				&& (strncmp(opt_start, mntflagslist->option, opt_len) == 0))
			{
				break;
			}

			++mntflagslist;
		}

		if ((mntflagslist->option == NULL)
#if (OS == Linux) && (!defined DISABLE_EXT_MOUNT)
				|| ((fsopts != NULL) && (fsopts->external_fstype != NULL))
#endif /* (OS == Linux) && (!defined DISABLE_EXT_MOUNT) */
			)
		{
			if (options_string_buffer != NULL)
			{
				if (mount_opts_len_cur != 0)
				{
					options_string_buffer[mount_opts_len_cur] = ',';
					++mount_opts_len_cur;
				}

				memcpy(&options_string_buffer[mount_opts_len_cur], opt_start, opt_len);
				mount_opts_len_cur += opt_len;
			}
		}
		else
		{
			if (mount_flags != NULL)
			{
				if (mntflagslist->enabled)
				{
					*mount_flags |= mntflagslist->flag;
				}
				else
				{
					*mount_flags &= ~(mntflagslist->flag);
				}
			}
		}

		if (mtab_options_string_buffer != NULL)
		{
			// all mount options
			if (mount_all_opts_len_cur != 0)
			{
				mtab_options_string_buffer[mount_all_opts_len_cur] = ',';
				++mount_all_opts_len_cur;
			}

			memcpy(&mtab_options_string_buffer[mount_all_opts_len_cur], opt_start, opt_len);
			mount_all_opts_len_cur += opt_len;
		}

		opt_start = opt_end;
	}

	// add uid/gid
	if ((uid != NULL) && (fsopts->option_uid != NULL))
	{
		if (options_string_buffer != NULL)
		{
			// mount options
			if (mount_opts_len_cur != 0)
			{
				options_string_buffer[mount_opts_len_cur] = ',';
				++mount_opts_len_cur;
			}

			result = strlen(fsopts->option_uid);

			memcpy(&options_string_buffer[mount_opts_len_cur], fsopts->option_uid, result);
			mount_opts_len_cur += result;

			result = snprintf(&options_string_buffer[mount_opts_len_cur], options_string_buffer_size - mount_opts_len_cur + 1, "%d", *uid);
			if (result < 1)
			{
				result = -1;
				goto dtmd_fsopts_generate_string_error_1;
			}

			mount_opts_len_cur += result;
		}

		if (mtab_options_string_buffer != NULL)
		{
			// all mount options
			if (mount_all_opts_len_cur != 0)
			{
				mtab_options_string_buffer[mount_all_opts_len_cur] = ',';
				++mount_all_opts_len_cur;
			}

			result = strlen(fsopts->option_uid);

			memcpy(&mtab_options_string_buffer[mount_all_opts_len_cur], fsopts->option_uid, result);
			mount_all_opts_len_cur += result;

			result = snprintf(&mtab_options_string_buffer[mount_all_opts_len_cur], mtab_options_string_buffer_size - mount_all_opts_len_cur + 1, "%d", *uid);
			if (result < 1)
			{
				result = -1;
				goto dtmd_fsopts_generate_string_error_1;
			}

			mount_all_opts_len_cur += result;
		}
	}

	if ((gid != NULL) && (fsopts->option_gid != NULL))
	{
		if (options_string_buffer != NULL)
		{
			// mount options
			if (mount_opts_len_cur != 0)
			{
				options_string_buffer[mount_opts_len_cur] = ',';
				++mount_opts_len_cur;
			}

			result = strlen(fsopts->option_gid);

			memcpy(&options_string_buffer[mount_opts_len_cur], fsopts->option_gid, result);
			mount_opts_len_cur += result;

			result = snprintf(&options_string_buffer[mount_opts_len_cur], options_string_buffer_size - mount_opts_len_cur + 1, "%d", *gid);
			if (result < 1)
			{
				result = -1;
				goto dtmd_fsopts_generate_string_error_1;
			}

			mount_opts_len_cur += result;
		}

		if (mtab_options_string_buffer != NULL)
		{
			// all mount options
			if (mount_all_opts_len_cur != 0)
			{
				mtab_options_string_buffer[mount_all_opts_len_cur] = ',';
				++mount_all_opts_len_cur;
			}

			result = strlen(fsopts->option_gid);

			memcpy(&mtab_options_string_buffer[mount_all_opts_len_cur], fsopts->option_gid, result);
			mount_all_opts_len_cur += result;

			result = snprintf(&mtab_options_string_buffer[mount_all_opts_len_cur], mtab_options_string_buffer_size - mount_all_opts_len_cur + 1, "%d", *gid);
			if (result < 1)
			{
				result = -1;
				goto dtmd_fsopts_generate_string_error_1;
			}

			mount_all_opts_len_cur += result;
		}
	}

	return 1;

dtmd_fsopts_generate_string_error_1:
	return result;
}
