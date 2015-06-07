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

#if (defined OS_FreeBSD)
#define _WITH_DPRINTF
#endif /* (defined OS_FreeBSD) */

#include "daemon/filesystem_opts.h"

#include "daemon/lists.h"
#include "daemon/log.h"
#include "daemon/return_codes.h"

#include <dtmd-misc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mount.h>

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

#if (defined OS_Linux)
static const struct dtmd_string_to_mount_flag string_to_mount_flag_list[] =
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
	{ "nosync",      MS_SYNCHRONOUS, 0 },
	{ NULL,          0,              0 }
};

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

// TODO: ntfs-3g does NOT support all flags for other file systems. Discern them
static const struct dtmd_filesystem_options filesystem_mount_options[] =
{
	{
		NULL, /* NOT EXTERNAL MOUNT */
		"vfat",
		vfat_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,shortname=mixed,dmask=0077,utf8=1,flush"
	},
	{
		"ntfs-3g", /* EXTERNAL MOUNT */
		"ntfs-3g",
		ntfs3g_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,allow_other,windows_names,dmask=0077"
	},
	{
		"ntfs-3g", /* EXTERNAL MOUNT */
		"ntfs",
		ntfs3g_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,allow_other,windows_names,dmask=0077"
	},
	{
		NULL, /* NOT EXTERNAL MOUNT */
		"iso9660",
		iso9660_allow,
		"uid=",
		"gid=",
		"ro,nodev,nosuid,iocharset=utf8,mode=0400,dmode=0500"
	},
	{
		NULL, /* NOT EXTERNAL MOUNT */
		"udf",
		udf_allow,
		"uid=",
		"gid=",
		"ro,nodev,nosuid,iocharset=utf8,umask=0077"
	},
	{
		NULL,
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
	{ "nosync",     0 },
	{ "dirsync",    0 },
	{ NULL,         0 }
};
#else /* (defined OS_Linux) */
#if (defined OS_FreeBSD)
static const struct dtmd_string_to_mount_flag string_to_mount_flag_list[] =
{
	{ "ro",         MNT_RDONLY,      1 },
	{ "rw",         MNT_RDONLY,      0 },
	{ "noexec",     MNT_NOEXEC,      1 },
	{ "exec",       MNT_NOEXEC,      0 },
	{ "nosuid",     MNT_NOSUID,      1 },
	{ "suid",       MNT_NOSUID,      0 },
	{ "noatime",    MNT_NOATIME,     1 },
	{ "atime",      MNT_NOATIME,     0 },
	{ "noclusterr", MNT_NOCLUSTERR,  1 },
	{ "clusterr",   MNT_NOCLUSTERR,  0 },
	{ "noclusterw", MNT_NOCLUSTERW,  1 },
	{ "clusterw",   MNT_NOCLUSTERW,  0 },
	{ "sync",       MNT_SYNCHRONOUS, 1 },
	{ "nosync",     MNT_SYNCHRONOUS, 0 },
	{ "async",      MNT_ASYNC,       1 },
	{ "noasync",    MNT_ASYNC,       0 },
#ifdef MNT_ACLS
	{ "acls",       MNT_ACLS,        1 },
	{ "noacls",     MNT_ACLS,        0 },
#endif /* MNT_ACLS */
#ifdef MNT_NODEV
	{ "dev",        MNT_NODEV,       0 },
	{ "nodev",      MNT_NODEV,       1 },
#endif /* MNT_NODEV */
	{ NULL,         0,               0 }
};

static const struct dtmd_mount_option vfat_allow[] =
{
	{ "large",      0, NULL  },
	{ "longnames",  0, NULL  },
	{ "shortnames", 0, NULL  },
	{ "nowin95",    0, NULL  },
	{ "dmask=",     1, "-M " },
	{ "fmask=",     1, "-m " },
	{ "codepage=",  1, "-D " },
	{ "iocharset=", 1, "-L " },
	{ NULL,         0, NULL  }
};

static const struct dtmd_mount_option ntfs3g_allow[] =
{
	{ "umask=",        1, NULL },
	{ "dmask=",        1, NULL },
	{ "fmask=",        1, NULL },
	{ "iocharset=",    1, NULL },
	{ "utf8",          0, NULL },
	{ "windows_names", 0, NULL },
	{ "allow_other",   0, NULL },
	{ NULL,            0, NULL }
};

// TODO: enable iso9660 and udf filesystems when cd/dvd disks are supported
#if 0
static const struct dtmd_mount_option iso9660_allow[] =
{
	{ "extatt",       0, NULL  },
	{ "gens",         0, NULL  },
	{ "nojoliet",     0, NULL  },
	{ "norrip",       0, NULL  },
	{ "brokenjoliet", 0, NULL  },
	{ "iocharset=",   1, "-C " },
	{ NULL,           0, NULL  }
};

static const struct dtmd_mount_option udf_allow[] =
{
	{ "iocharset=",   1, "-C " },
	{ NULL,           0, NULL  }
};
#endif /* 0 */

// TODO: ntfs-3g does NOT support all flags for other file systems. Discern them
static const struct dtmd_filesystem_options filesystem_mount_options[] =
{
	{
		"msdosfs",
		"mount_msdosfs",
		"fat32",
		vfat_allow,
		"uid=",
		"-u ",
		"gid=",
		"-g ",
		"rw"
#ifdef MNT_NODEV
		",nodev"
#endif /* MNT_NODEV */
		",dmask=755"
	},
	{
		"ntfs",
		"ntfs-3g",
		"ntfs",
		ntfs3g_allow,
		"uid=",
		NULL,
		"gid=",
		NULL,
		"rw"
#ifdef MNT_NODEV
		",nodev"
#endif /* MNT_NODEV */
		",nosuid,allow_other,windows_names,dmask=0077"
	},
// TODO: enable iso9660 and udf filesystems when cd/dvd disks are supported
#if 0
	{
		"cd9660",
		"mount_cd9660",
		"cd9660",
		iso9660_allow,
		NULL,
		NULL,
		NULL,
		NULL,
		"ro"
#ifdef MNT_NODEV
		",nodev"
#endif /* MNT_NODEV */
		",nosuid,iocharset=utf8"
	},
	{
		"udf",
		"mount_udf",
		"udf",
		udf_allow,
		NULL,
		NULL,
		NULL,
		NULL,
		"ro"
#ifdef MNT_NODEV
		",nodev"
#endif /* MNT_NODEV */
		",nosuid,iocharset=utf8"
	},
#endif /* 0 */
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	}
};

static const struct dtmd_mount_option any_fs_allowed_list[] =
{
	{ "noatime",    0, NULL },
	{ "atime",      0, NULL },
	{ "noexec",     0, NULL },
	{ "nosuid",     0, NULL },
	{ "ro",         0, NULL },
	{ "rw",         0, NULL },
	{ "noclusterr", 0, NULL },
	{ "clusterr",   0, NULL },
	{ "noclusterw", 0, NULL },
	{ "clusterw",   0, NULL },
	{ "sync",       0, NULL },
	{ "nosync",     0, NULL },
	{ "async",      0, NULL },
	{ "noasync",    0, NULL },
#ifdef MNT_ACLS
	{ "acls",       0, NULL },
	{ "noacls",     0, NULL },
#endif /* MNT_NOACLS */
#ifdef MNT_NODEV
	{ "nodev",      0, NULL },
#endif /* MNT_NODEV */
	{ NULL,         0, NULL }
};

#else /* (defined OS_FreeBSD) */
#error Unsupported OS
#endif /* (defined OS_FreeBSD) */
#endif /* (defined OS_Linux) */

const struct dtmd_filesystem_options* get_fsopts_for_fs(const char *filesystem)
{
	const struct dtmd_filesystem_options *fsopts = filesystem_mount_options;

	if (filesystem == NULL)
	{
		WRITE_LOG(LOG_ERR, "Bug: parameter filesystem is empty");
		return NULL;
	}

	for (;;)
	{
		if (fsopts->fstype == NULL)
		{
			WRITE_LOG_ARGS(LOG_WARNING, "Can't find filesystem options for filesystem '%s'", filesystem);
			return NULL;
		}

		if (strcmp(fsopts->fstype, filesystem) == 0)
		{
			return fsopts;
		}

		++fsopts;
	}
}

static const struct dtmd_mount_option* find_option_in_list(const char *option, unsigned int option_len, const struct dtmd_filesystem_options *filesystem_list)
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

	WRITE_LOG_ARGS(LOG_NOTICE, "Filesystem option '%s' is not allowed", option);
	return NULL;
}

static void init_options_list_id(struct dtmd_fsopts_list_id *id)
{
	id->id_option           = NULL;
	id->id_option_len       = 0;
	id->id_option_value     = NULL;
	id->id_option_value_len = 0;
}

void init_options_list(dtmd_fsopts_list_t *fsopts_list)
{
	if (fsopts_list != NULL)
	{
		fsopts_list->options       = NULL;
		fsopts_list->options_count = 0;

		init_options_list_id(&(fsopts_list->option_uid));
		init_options_list_id(&(fsopts_list->option_gid));
	}
}

static void free_options_list_id(struct dtmd_fsopts_list_id *id)
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

void free_options_list(dtmd_fsopts_list_t *fsopts_list)
{
	unsigned int index = 0;

	if (fsopts_list != NULL)
	{
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
}

int convert_options_to_list(const char *options_list, const struct dtmd_filesystem_options *fsopts, uid_t *uid, gid_t *gid, dtmd_fsopts_list_t *fsopts_list)
{
	const struct dtmd_string_to_mount_flag *mntflagslist;
	const struct dtmd_mount_option *option_params;

	const char *opt_start;
	const char *opt_end;
	unsigned int opt_len;
	int result;

	unsigned int option_index;

	struct dtmd_fsopts_list_item *option_item;
	void *tmp;

	if ((options_list == NULL)
		|| (fsopts_list == NULL))
	{
		WRITE_LOG(LOG_ERR, "Bug: parameter 'options list' or 'filesystem options list' is empty");
		return result_bug;
	}

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
				WRITE_LOG(LOG_WARNING, "Got empty parameter in options list");
				return result_fail;
			}

			// check option
			option_params = find_option_in_list(opt_start, opt_len, fsopts);
			if (option_params == NULL)
			{
				return result_fail;
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
				option_item = (struct dtmd_fsopts_list_item*) malloc(sizeof(struct dtmd_fsopts_list_item));
				if (option_item == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					return result_fatal_error;
				}

				tmp = realloc(fsopts_list->options, (fsopts_list->options_count + 1) * sizeof(struct dtmd_fsopts_list_item*));
				if (tmp == NULL)
				{
					WRITE_LOG(LOG_ERR, "Memory allocation failure");
					free(option_item);
					return result_fatal_error;
				}

				fsopts_list->options = (struct dtmd_fsopts_list_item**) tmp;
				++(fsopts_list->options_count);

				fsopts_list->options[fsopts_list->options_count - 1] = option_item;
			}

			// fill item
			option_item->option.option         = opt_start;
			option_item->option_full_len       = opt_len;
#if (defined OS_FreeBSD)
			option_item->transformation_string = option_params->transformation;
#endif /* (defined OS_FreeBSD) */

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
				WRITE_LOG(LOG_ERR, "Uid to string conversion failed");
				return result_fatal_error;
			}

			fsopts_list->option_uid.id_option_value = (char*) malloc(result + 1);
			if (fsopts_list->option_uid.id_option_value == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				return result_fatal_error;
			}

			result = snprintf(fsopts_list->option_uid.id_option_value, result + 1, "%u", *uid);
			if (result < 1)
			{
				WRITE_LOG(LOG_ERR, "Uid to string conversion failed");
				return result_fatal_error;
			}

			fsopts_list->option_uid.id_option_value_len = result;
#if (defined OS_FreeBSD)
			if (fsopts->option_uid_transformation == NULL)
			{
#endif /* (defined OS_FreeBSD) */
				fsopts_list->option_uid.id_option     = fsopts->option_uid;
				fsopts_list->option_uid.id_option_len = strlen(fsopts->option_uid);
#if (defined OS_FreeBSD)
				fsopts_list->option_uid.transformed   = 0;
			}
			else
			{
				fsopts_list->option_uid.id_option     = fsopts->option_uid_transformation;
				fsopts_list->option_uid.id_option_len = strlen(fsopts->option_uid_transformation);
				fsopts_list->option_uid.transformed   = 1;
			}
#endif /* (defined OS_FreeBSD) */
		}

		if ((gid != NULL) && (fsopts->option_gid != NULL))
		{
			result = snprintf(NULL, 0, "%u", *gid);
			if (result < 1)
			{
				WRITE_LOG(LOG_ERR, "Gid to string conversion failed");
				return result_fatal_error;
			}

			fsopts_list->option_gid.id_option_value = (char*) malloc(result + 1);
			if (fsopts_list->option_gid.id_option_value == NULL)
			{
				WRITE_LOG(LOG_ERR, "Memory allocation failure");
				return result_fatal_error;
			}

			result = snprintf(fsopts_list->option_gid.id_option_value, result + 1, "%u", *gid);
			if (result < 1)
			{
				WRITE_LOG(LOG_ERR, "Gid to string conversion failed");
				return result_fatal_error;
			}

			fsopts_list->option_gid.id_option_value_len = result;
#if (defined OS_FreeBSD)
			if (fsopts->option_gid_transformation == NULL)
			{
#endif /* (defined OS_FreeBSD) */
				fsopts_list->option_gid.id_option     = fsopts->option_gid;
				fsopts_list->option_gid.id_option_len = strlen(fsopts->option_gid);
#if (defined OS_FreeBSD)
				fsopts_list->option_gid.transformed   = 0;
			}
			else
			{
				fsopts_list->option_gid.id_option     = fsopts->option_gid_transformation;
				fsopts_list->option_gid.id_option_len = strlen(fsopts->option_gid_transformation);
				fsopts_list->option_gid.transformed   = 1;
			}
#endif /* (defined OS_FreeBSD) */
		}
	}

	return result_success;
}

int fsopts_generate_string(dtmd_fsopts_list_t *fsopts_list,
	unsigned int *options_full_string_length,
	char *options_full_string_buffer,
	unsigned int options_full_string_buffer_size
#if (defined OS_Linux)
	,
	unsigned int *options_string_length,
	char *options_string_buffer,
	unsigned int options_string_buffer_size,
	unsigned long *mount_flags
#endif /* (defined OS_Linux) */
	)
{
	unsigned int string_len_full = 0;
	unsigned int current_item_full = 0;
	unsigned int index;

#if (defined OS_Linux)
	unsigned int string_len = 0;
	unsigned int current_item = 0;
	unsigned long flags = 0;
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
	unsigned int transformation;
#endif /* (defined OS_FreeBSD) */

	if (fsopts_list == NULL)
	{
		WRITE_LOG(LOG_ERR, "BUG: parameter 'filesystem options list' is empty");
		return result_bug;
	}

#if (defined OS_FreeBSD)
	for (transformation = 0; transformation < 2; ++transformation)
	{
#endif /* (defined OS_FreeBSD) */
		if (fsopts_list->options != NULL)
		{
			for (index = 0; index < fsopts_list->options_count; ++index)
			{
#if (defined OS_FreeBSD)
				if ((transformation && (fsopts_list->options[index]->transformation_string != NULL))
					|| ((!transformation) && (fsopts_list->options[index]->transformation_string == NULL)))
				{
#endif /* (defined OS_FreeBSD) */
					// full string
					if (current_item_full > 0)
					{
						if ((options_full_string_buffer != NULL)
							&& (string_len_full + 1 <= options_full_string_buffer_size))
						{
#if (defined OS_Linux)
							options_full_string_buffer[string_len_full] = ',';
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
							options_full_string_buffer[string_len_full] = ((transformation == 0) ? ',' : ' ');
#endif /* (defined OS_FreeBSD) */
						}

						++string_len_full;
					}
#if (defined OS_FreeBSD)
					else
					{
						// TODO: use strncat for copying instead of manual checks and memcpy?
						if (options_full_string_buffer != NULL)
						{
							if (string_len_full + strlen("-o ") <= options_full_string_buffer_size)
							{
								memcpy(&(options_full_string_buffer[string_len_full]),
									"-o ",
									strlen("-o "));
							}
							else if (string_len_full < options_full_string_buffer_size)
							{
								memcpy(&(options_full_string_buffer[string_len_full]),
									"-o ",
									options_full_string_buffer_size - string_len_full);
							}
						}

						string_len_full += strlen("-o ");
					}
#endif /* (defined OS_FreeBSD) */

					if (options_full_string_buffer != NULL)
					{
#if (defined OS_FreeBSD)
						if (fsopts_list->options[index]->transformation_string == NULL)
						{
#endif /* (defined OS_FreeBSD) */
							if (string_len_full + fsopts_list->options[index]->option_full_len <= options_full_string_buffer_size)
							{
								memcpy(&(options_full_string_buffer[string_len_full]),
									fsopts_list->options[index]->option.option,
									fsopts_list->options[index]->option_full_len);
							}
							else if (string_len_full < options_full_string_buffer_size)
							{
								memcpy(&(options_full_string_buffer[string_len_full]),
									fsopts_list->options[index]->option.option,
									options_full_string_buffer_size - string_len_full);
							}
#if (defined OS_FreeBSD)
						}
						else
						{
							if (string_len_full + strlen(fsopts_list->options[index]->transformation_string) <= options_full_string_buffer_size)
							{
								memcpy(&(options_full_string_buffer[string_len_full]),
									fsopts_list->options[index]->transformation_string,
									strlen(fsopts_list->options[index]->transformation_string));

								if (fsopts_list->options[index]->option_full_len != fsopts_list->options[index]->option_len)
								{
									if (string_len_full + strlen(fsopts_list->options[index]->transformation_string) + fsopts_list->options[index]->option_full_len -fsopts_list->options[index]->option_len <= options_full_string_buffer_size)
									{
										memcpy(&(options_full_string_buffer[string_len_full + strlen(fsopts_list->options[index]->transformation_string)]),
											&(fsopts_list->options[index]->option.option[fsopts_list->options[index]->option_len]),
											fsopts_list->options[index]->option_full_len - fsopts_list->options[index]->option_len);
									}
									else if (string_len_full + strlen(fsopts_list->options[index]->transformation_string) < options_full_string_buffer_size)
									{
										memcpy(&(options_full_string_buffer[string_len_full + strlen(fsopts_list->options[index]->transformation_string)]),
											&(fsopts_list->options[index]->option.option[fsopts_list->options[index]->option_len]),
											options_full_string_buffer_size - string_len_full - strlen(fsopts_list->options[index]->transformation_string));
									}
								}
							}
							else if (string_len_full < options_full_string_buffer_size)
							{
								memcpy(&(options_full_string_buffer[string_len_full]),
									fsopts_list->options[index]->transformation_string,
									options_full_string_buffer_size - string_len_full);
							}
						}
#endif /* (defined OS_FreeBSD) */
					}

#if (defined OS_FreeBSD)
					if (fsopts_list->options[index]->transformation_string == NULL)
					{
#endif /* (defined OS_FreeBSD) */
						string_len_full += fsopts_list->options[index]->option_full_len;
#if (defined OS_FreeBSD)
					}
					else
					{
						string_len_full += fsopts_list->options[index]->option_full_len - fsopts_list->options[index]->option_len + strlen(fsopts_list->options[index]->transformation_string);
					}
#endif /* (defined OS_FreeBSD) */
					++current_item_full;
#if (defined OS_FreeBSD)
				}
#endif /* (defined OS_FreeBSD) */

#if (defined OS_Linux)
				// string
				if (!fsopts_list->options[index]->option.flag)
				{
					if (current_item > 0)
					{
						if ((options_string_buffer != NULL)
							&& (string_len + 1 <= options_string_buffer_size))
						{
							options_string_buffer[string_len] = ',';
						}

						++string_len;
					}

					if (options_string_buffer != NULL)
					{
						if (string_len + fsopts_list->options[index]->option_full_len <= options_string_buffer_size)
						{
							memcpy(&(options_string_buffer[string_len]),
								fsopts_list->options[index]->option.option,
								fsopts_list->options[index]->option_full_len);
						}
						else if (string_len < options_string_buffer_size)
						{
							memcpy(&(options_string_buffer[string_len]),
								fsopts_list->options[index]->option.option,
								options_string_buffer_size - string_len);
						}
					}

					string_len += fsopts_list->options[index]->option_full_len;
					++current_item;
				}
				else
				{
					if (fsopts_list->options[index]->option.enabled)
					{
						flags |= fsopts_list->options[index]->option.flag;
					}
					else
					{
						flags &= ~(fsopts_list->options[index]->option.flag);
					}
				}
#endif /* (defined OS_Linux) */
			}
		}

		// uid
		if ((fsopts_list->option_uid.id_option_value != NULL)
			&& (fsopts_list->option_uid.id_option != NULL))
		{
#if (defined OS_FreeBSD)
			if (transformation == fsopts_list->option_uid.transformed)
			{
#endif /* (defined OS_FreeBSD) */
				// full string
				if (current_item_full > 0)
				{
					if ((options_full_string_buffer != NULL)
						&& (string_len_full + 1 <= options_full_string_buffer_size))
					{
#if (defined OS_Linux)
							options_full_string_buffer[string_len_full] = ',';
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
							options_full_string_buffer[string_len_full] = ((transformation == 0) ? ',' : ' ');
#endif /* (defined OS_FreeBSD) */
					}

					++string_len_full;
				}

				if (options_full_string_buffer != NULL)
				{
					if (string_len_full + fsopts_list->option_uid.id_option_len <= options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_uid.id_option,
							fsopts_list->option_uid.id_option_len);
					}
					else if (string_len_full < options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_uid.id_option,
							options_full_string_buffer_size - string_len_full);
					}
				}

				string_len_full += fsopts_list->option_uid.id_option_len;

				if (options_full_string_buffer != NULL)
				{
					if (string_len_full + fsopts_list->option_uid.id_option_value_len <= options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_uid.id_option_value,
							fsopts_list->option_uid.id_option_value_len);
					}
					else if (string_len_full < options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_uid.id_option_value,
							options_full_string_buffer_size - string_len_full);
					}
				}

				string_len_full += fsopts_list->option_uid.id_option_value_len;
				++current_item_full;
#if (defined OS_FreeBSD)
			}
#endif /* (defined OS_FreeBSD) */

#if (defined OS_Linux)
			// string
			if (current_item > 0)
			{
				if ((options_string_buffer != NULL)
					&& (string_len + 1 <= options_string_buffer_size))
				{
					options_string_buffer[string_len] = ',';
				}

				++string_len;
			}

			if (options_string_buffer != NULL)
			{
				if (string_len + fsopts_list->option_uid.id_option_len <= options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_uid.id_option,
						fsopts_list->option_uid.id_option_len);
				}
				else if (string_len < options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_uid.id_option,
						options_string_buffer_size - string_len);
				}
			}

			string_len += fsopts_list->option_uid.id_option_len;

			if (options_string_buffer != NULL)
			{
				if (string_len + fsopts_list->option_uid.id_option_value_len <= options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_uid.id_option_value,
						fsopts_list->option_uid.id_option_value_len);
				}
				else if (string_len < options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_uid.id_option_value,
						options_string_buffer_size - string_len);
				}
			}

			string_len += fsopts_list->option_uid.id_option_value_len;
			++current_item;
#endif /* (defined OS_Linux) */
		}

		// gid
		if ((fsopts_list->option_gid.id_option_value != NULL)
			&& (fsopts_list->option_gid.id_option != NULL))
		{
#if (defined OS_FreeBSD)
			if (transformation == fsopts_list->option_gid.transformed)
			{
#endif /* (defined OS_FreeBSD) */
				// full string
				if (current_item_full > 0)
				{
					if ((options_full_string_buffer != NULL)
						&& (string_len_full + 1 <= options_full_string_buffer_size))
					{
#if (defined OS_Linux)
						options_full_string_buffer[string_len_full] = ',';
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
						options_full_string_buffer[string_len_full] = ((transformation == 0) ? ',' : ' ');
#endif /* (defined OS_FreeBSD) */
					}

					++string_len_full;
				}

				if (options_full_string_buffer != NULL)
				{
					if (string_len_full + fsopts_list->option_gid.id_option_len <= options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_gid.id_option,
							fsopts_list->option_gid.id_option_len);
					}
					else if (string_len_full < options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_gid.id_option,
							options_full_string_buffer_size - string_len_full);
					}
				}

				string_len_full += fsopts_list->option_gid.id_option_len;

				if (options_full_string_buffer != NULL)
				{
					if (string_len_full + fsopts_list->option_gid.id_option_value_len <= options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_gid.id_option_value,
							fsopts_list->option_gid.id_option_value_len);
					}
					else if (string_len_full < options_full_string_buffer_size)
					{
						memcpy(&(options_full_string_buffer[string_len_full]),
							fsopts_list->option_gid.id_option_value,
							options_full_string_buffer_size - string_len_full);
					}
				}

				string_len_full += fsopts_list->option_gid.id_option_value_len;
				++current_item_full;
#if (defined OS_FreeBSD)
			}
#endif /* (defined OS_FreeBSD) */

#if (defined OS_Linux)
			// string
			if (current_item > 0)
			{
				if ((options_string_buffer != NULL)
					&& (string_len + 1 <= options_string_buffer_size))
				{
					options_string_buffer[string_len] = ',';
				}

				++string_len;
			}

			if (options_string_buffer != NULL)
			{
				if (string_len + fsopts_list->option_gid.id_option_len <= options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_gid.id_option,
						fsopts_list->option_gid.id_option_len);
				}
				else if (string_len < options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_gid.id_option,
						options_string_buffer_size - string_len);
				}
			}

			string_len += fsopts_list->option_gid.id_option_len;

			if (options_string_buffer != NULL)
			{
				if (string_len + fsopts_list->option_gid.id_option_value_len <= options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_gid.id_option_value,
						fsopts_list->option_gid.id_option_value_len);
				}
				else if (string_len < options_string_buffer_size)
				{
					memcpy(&(options_string_buffer[string_len]),
						fsopts_list->option_gid.id_option_value,
						options_string_buffer_size - string_len);
				}
			}

			string_len += fsopts_list->option_gid.id_option_value_len;
			++current_item;
#endif /* (defined OS_Linux) */
		}
#if (defined OS_FreeBSD)
	}
#endif /* (defined OS_FreeBSD) */

	if (options_full_string_length != NULL)
	{
		*options_full_string_length = string_len_full;
	}

#if (defined OS_Linux)
	if (options_string_length != NULL)
	{
		*options_string_length = string_len;
	}

	if (mount_flags != NULL)
	{
		*mount_flags = flags;
	}
#endif /* (defined OS_Linux) */

	return result_success;
}

int invoke_list_supported_filesystems(int client_number)
{
	const struct dtmd_filesystem_options *fsopts = filesystem_mount_options;
	int first = 1;

	if (dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_supported_filesystems "\")\n" dtmd_response_argument_supported_filesystems_lists "(") < 0)
	{
		return result_client_error;
	}

	for (;;)
	{
		if ((fsopts == NULL) || (fsopts->fstype == NULL))
		{
			break;
		}

#if (defined OS_Linux)
#if (!defined DISABLE_EXT_MOUNT)
#else /* (!defined DISABLE_EXT_MOUNT) */
		if (fsopts->external_fstype == NULL)
		{
#endif /* (!defined DISABLE_EXT_MOUNT) */
#endif /* (defined OS_Linux) */
			if (first != 0)
			{
				first = 0;
			}
			else
			{
				if (dprintf(clients[client_number]->clientfd, ", ") < 0)
				{
					return result_client_error;
				}
			}

			if (dprintf(clients[client_number]->clientfd, "\"%s\"", fsopts->fstype) < 0)
			{
				return result_client_error;
			}
#if (defined OS_Linux)
#if (!defined DISABLE_EXT_MOUNT)
#else /* (!defined DISABLE_EXT_MOUNT) */
		}
#endif /* (!defined DISABLE_EXT_MOUNT) */
#endif /* (defined OS_Linux) */

		++fsopts;
	}

	if (dprintf(clients[client_number]->clientfd, ")\n" dtmd_response_finished "(\"" dtmd_command_list_supported_filesystems "\")\n") < 0)
	{
		return result_client_error;
	}

	return result_success;
}

int invoke_list_supported_filesystem_options(int client_number, const char *filesystem)
{
	const struct dtmd_filesystem_options *fsopts;
	const struct dtmd_mount_option *option_list;
	const struct dtmd_mount_option *options_lists_array[2];
	unsigned int array_index;
	int first = 1;

	fsopts = get_fsopts_for_fs(filesystem);
#if (defined OS_Linux) && (defined DISABLE_EXT_MOUNT)
	if ((fsopts == NULL) || (fsopts->external_fstype != NULL))
#else /* (defined OS_Linux) && (defined DISABLE_EXT_MOUNT) */
	if (fsopts == NULL)
#endif /* (defined OS_Linux) && (defined DISABLE_EXT_MOUNT) */
	{
		if (dprintf(clients[client_number]->clientfd, dtmd_response_failed "(\"" dtmd_command_list_supported_filesystem_options "\", \"%s\", \"%s\")\n", filesystem, dtmd_error_code_to_string(dtmd_error_code_unsupported_fstype)) < 0)
		{
			return result_client_error;
		}

		return result_fail;
	}

	options_lists_array[0] = any_fs_allowed_list;
	options_lists_array[1] = fsopts->options;

	if (dprintf(clients[client_number]->clientfd, dtmd_response_started "(\"" dtmd_command_list_supported_filesystem_options "\", \"%s\")\n" dtmd_response_argument_supported_filesystem_options_lists "(", filesystem) < 0)
	{
		return result_client_error;
	}

	for (array_index = 0; array_index < 2; ++array_index)
	{
		for (option_list = options_lists_array[array_index]; option_list->option != NULL; ++option_list)
		{
			if (first != 0)
			{
				first = 0;
			}
			else
			{
				if (dprintf(clients[client_number]->clientfd, ", ") < 0)
				{
					return result_client_error;
				}
			}

			if (dprintf(clients[client_number]->clientfd, "\"%s\"", option_list->option) < 0)
			{
				return result_client_error;
			}
		}
	}

	if (dprintf(clients[client_number]->clientfd, ")\n" dtmd_response_finished "(\"" dtmd_command_list_supported_filesystem_options "\", \"%s\")\n", filesystem) < 0)
	{
		return result_client_error;
	}

	return result_success;
}

#if (defined OS_FreeBSD)
char* convert_option_flags_to_string(uint64_t flags)
{
	int length = 0;
	int count = 0;
	char *result;
	const struct dtmd_string_to_mount_flag *mntflagslist;

	// TODO: a loop or a helper function?

	mntflagslist = string_to_mount_flag_list;
	while (mntflagslist->option != NULL)
	{
		if (((mntflagslist->enabled) && ((flags & mntflagslist->flag) == mntflagslist->flag))
			|| ((!(mntflagslist->enabled)) && ((flags & mntflagslist->flag) == 0)))
		{
			length += strlen(mntflagslist->option);
			if (count > 0)
			{
				++length;
			}

			++count;
		}

		++mntflagslist;
	}

	result = (char*) malloc(length + 1);
	if (result == NULL)
	{
		WRITE_LOG(LOG_ERR, "Memory allocation failure");
		return NULL;
	}

	length = 0;
	mntflagslist = string_to_mount_flag_list;
	while (mntflagslist->option != NULL)
	{
		if (((mntflagslist->enabled) && ((flags & mntflagslist->flag) == mntflagslist->flag))
			|| ((!(mntflagslist->enabled)) && ((flags & mntflagslist->flag) == 0)))
		{
			strcpy(&(result[length]), mntflagslist->option);
			length += strlen(mntflagslist->option);

			if (count > 0)
			{
				strcpy(&(result[length]), ",");
				++length;
			}

			++count;
		}

		++mntflagslist;
	}

	return result;
}

#endif /* (defined OS_FreeBSD) */
