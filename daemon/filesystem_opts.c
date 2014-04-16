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
	{ "dirsync",    0 },
	{ NULL,         0 }
};

const struct dtmd_filesystem_options* get_fsopts_for_fs(const char *filesystem)
{
	const struct dtmd_filesystem_options *fsopts = filesystem_mount_options;

	if (filesystem == NULL)
	{
		return NULL;
	}

	for (;;)
	{
		if (fsopts->fstype == NULL)
		{
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

	return NULL;
}

static const struct dtmd_filesystem_options* dtmd_get_fsopts_for_fstype(const char *fstype)
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
		return -1;
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
				option_item = (struct dtmd_fsopts_list_item*) malloc(sizeof(struct dtmd_fsopts_list_item));
				if (option_item == NULL)
				{
					return -1;
				}

				tmp = realloc(fsopts_list->options, (fsopts_list->options_count + 1) * sizeof(struct dtmd_fsopts_list_item*));
				if (tmp == NULL)
				{
					free(option_item);
					return -1;
				}

				fsopts_list->options = (struct dtmd_fsopts_list_item**) tmp;
				++(fsopts_list->options_count);

				fsopts_list->options[fsopts_list->options_count - 1] = option_item;
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

	return 1;
}

int fsopts_generate_string(dtmd_fsopts_list_t *fsopts_list,
	unsigned int *options_full_string_length,
	char *options_full_string_buffer,
	unsigned int options_full_string_buffer_size,
	unsigned int *options_string_length,
	char *options_string_buffer,
	unsigned int options_string_buffer_size,
	unsigned long *mount_flags)
{
	unsigned int string_len_full = 0;
	unsigned int string_len = 0;
	unsigned int index;
	unsigned int current_item_full = 0;
	unsigned int current_item = 0;
	unsigned long flags = 0;

	if (fsopts_list == NULL)
	{
		return -1;
	}

	if (fsopts_list->options != NULL)
	{
		for (index = 0; index < fsopts_list->options_count; ++index)
		{
			// full string
			if (current_item_full > 0)
			{
				if ((options_full_string_buffer != NULL)
					&& (string_len_full + 1 <= options_full_string_buffer_size))
				{
					options_full_string_buffer[string_len_full] = ',';
				}

				++string_len_full;
			}

			if (options_full_string_buffer != NULL)
			{
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
			}

			string_len_full += fsopts_list->options[index]->option_full_len;
			++current_item_full;

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
		}
	}

	// uid
	if ((fsopts_list->option_uid.id_option_value != NULL)
		&& (fsopts_list->option_uid.id_option != NULL))
	{
		// full string
		if (current_item_full > 0)
		{
			if ((options_full_string_buffer != NULL)
				&& (string_len_full + 1 <= options_full_string_buffer_size))
			{
				options_full_string_buffer[string_len_full] = ',';
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
	}

	// gid
	if ((fsopts_list->option_gid.id_option_value != NULL)
		&& (fsopts_list->option_gid.id_option != NULL))
	{
		// full string
		if (current_item_full > 0)
		{
			if ((options_full_string_buffer != NULL)
				&& (string_len_full + 1 <= options_full_string_buffer_size))
			{
				options_full_string_buffer[string_len_full] = ',';
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
	}

	if (options_full_string_length != NULL)
	{
		*options_full_string_length = string_len_full;
	}

	if (options_string_length != NULL)
	{
		*options_string_length = string_len;
	}

	if (mount_flags != NULL)
	{
		*mount_flags = flags;
	}

	return 1;
}
