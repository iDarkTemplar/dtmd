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

#include <string.h>

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

	return 0;
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
