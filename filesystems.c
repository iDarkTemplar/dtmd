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

#include "filesystems.h"

#include <string.h>

struct mount_option
{
	const char * const option;
	const unsigned char has_param;
};

struct mount_option_spec
{
	const char *option;
	const char *param;
};

struct default_mount_options
{
	const struct mount_option_spec * const options;
	const unsigned char allow_uid :1;
	const unsigned char allow_gid :1;
};

struct filesystem_options
{
	const char * const fstype;
	const struct default_mount_options defaults;
	const struct mount_option * const options;
	const char * const option_uid;
	const char * const option_gid;
};

static const struct mount_option_spec vfat_defaults[] =
{
	{ "shortname=", "mixed" },
	{ "dmask=", "0077" },
	{ "utf8=", "1"},
	{ NULL, NULL }
};

static const struct mount_option vfat_allow[] =
{
	{ "flush",      0 },
	{ "utf8=",      1 },
	{ "shortname=", 1 },
	{ "umask=",     1 },
	{ "dmask=",     1 },
	{ "fmask=",     1 },
	{ "codepage=",  1 },
	{ "iocharset=", 1 },
	{ "usefree",    0 },
	{ "showexec",   0 },
	{ NULL,         0 }
};

static const struct mount_option_spec ntfs_defaults[] =
{
	{ "dmask=", "0077" },
	{ "fmask=", "0177" },
	{ NULL, NULL }
};

static const struct mount_option ntfs_allow[] =
{
	{ "umask=", 1 },
	{ "dmask=", 1 },
	{ "fmask=", 1 },
	{ NULL,     0 }
};

static const struct mount_option_spec iso9660_defaults[] =
{
	{ "iocharset=", "utf8" },
	{ "mode=", "0400" },
	{ "dmode=", "0500" },
	{ NULL, NULL }
};

static const struct mount_option iso9660_allow[] =
{
	{ "norock",     0 },
	{ "nojoliet",   0 },
	{ "iocharset=", 1 },
	{ "mode=",      1 },
	{ "dmode=",     1 },
	{ NULL,         0 }
};

static const struct mount_option_spec udf_defaults[] =
{
	{ "iocharset=", "utf8" },
	{ "umask=", "0077" },
	{ NULL, NULL }
};

static const struct mount_option udf_allow[] =
{
	{ "iocharset=", 1 },
	{ "umask=",     1 },
	{ "mode=",      1 },
	{ "dmode=",     1 },
	{ NULL,         0 }
};

static const struct filesystem_options filesystem_mount_options[] =
{
	{
		"vfat",
		{
			vfat_defaults,
			1,
			1
		},
		vfat_allow,
		"uid=",
		"gid="
	},
	{
		"ntfs",
		{
			ntfs_defaults,
			1,
			1
		},
		ntfs_allow,
		"uid=",
		"gid="
	},
	{
		"iso9660",
		{
			iso9660_defaults,
			1,
			1
		},
		iso9660_allow,
		"uid=",
		"gid="
	},
	{
		"udf",
		{
			udf_defaults,
			1,
			1
		},
		udf_allow,
		"uid=",
		"gid="
	},
	{
		NULL,
		{
			NULL,
			0,
			0
		},
		NULL,
		NULL,
		NULL
	}
};

const struct mount_option any_fs_allowed_list[] =
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

int is_option_allowed(const char *fstype, const char *option)
{
	const struct mount_option *option_list;
	const struct filesystem_options *filesystem_list;

	for (option_list = any_fs_allowed_list; option_list->option != NULL; ++option_list)
	{
		if (option_list->has_param)
		{
			if (strncmp(option, option_list->option, strlen(option_list->option)) == 0)
			{
				return 1;
			}
		}
		else
		{
			if (strcmp(option, option_list->option) == 0)
			{
				return 1;
			}
		}
	}

	for (filesystem_list = filesystem_mount_options; (filesystem_list->fstype != NULL) && (strcmp(filesystem_list->fstype, fstype) != 0); ++filesystem_list)
	{
	}

	if (filesystem_list->fstype != NULL)
	{
		if ((filesystem_list->option_uid != NULL) && (strncmp(option, filesystem_list->option_uid, strlen(filesystem_list->option_uid)) == 0))
		{
			return 1;
		}

		if ((filesystem_list->option_gid != NULL) && (strncmp(option, filesystem_list->option_gid, strlen(filesystem_list->option_gid)) == 0))
		{
			return 1;
		}

		for (option_list = filesystem_list->options; option_list->option != NULL; ++option_list)
		{
			if (option_list->has_param)
			{
				if (strncmp(option, option_list->option, strlen(option_list->option)) == 0)
				{
					return 1;
				}
			}
			else
			{
				if (strcmp(option, option_list->option) == 0)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}
