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

#include "daemon/filesystems.h"

#include "daemon/lists.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mount.h>
#include <stdio.h>

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

struct mount_option
{
	const char * const option;
	const unsigned char has_param;
};

struct filesystem_options
{
	const char * const fstype;
	const struct mount_option * const options;
	const char * const option_uid;
	const char * const option_gid;
	const char * const defaults;
};

static const struct mount_option vfat_allow[] =
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
	{ "umask=",       1 },
	{ "dmask=",       1 },
	{ "fmask=",       1 },
	{ "allow_utime=", 1 },
	{ "check=",       1 },
	{ "conv=",        1 },
	{ NULL,           0 }
};

static const struct mount_option ntfs3g_allow[] =
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

static const struct mount_option iso9660_allow[] =
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

static const struct mount_option udf_allow[] =
{
	{ "iocharset=", 1 },
	{ "umask=",     1 },
	{ "mode=",      1 },
	{ "dmode=",     1 },
	{ "undelete",   0 },
	{ NULL,         0 }
};

static const struct filesystem_options filesystem_mount_options[] =
{
	{
		"vfat",
		vfat_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,shortname=mixed,dmask=0077,utf8=1,flush"
	},
	{
		"ntfs-3g",
		ntfs3g_allow,
		"uid=",
		"gid=",
		"rw,nodev,nosuid,allow_other,blksize=4096,dmask=0077"
	},
	{
		"iso9660",
		iso9660_allow,
		"uid=",
		"gid=",
		"ro,nodev,nosuid,iocharset=utf8,mode=0400,dmode=0500"
	},
	{
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
		NULL
	}
};

static const struct mount_option any_fs_allowed_list[] =
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

static int is_option_allowed(const char *option, unsigned int option_len, const struct filesystem_options *filesystem_list)
{
	const struct mount_option *option_list;
	unsigned int minlen;
	const struct mount_option *options_lists_array[2];
	unsigned int array_index;

	options_lists_array[0] = any_fs_allowed_list;
	options_lists_array[1] = filesystem_list->options;

	for (array_index = 0; array_index < sizeof(options_lists_array)/sizeof(options_lists_array[0]); ++array_index)
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

	return 0;
}

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
	struct dirent *d;
	DIR *dir;

	dir = opendir(dirname);
	if (dir == NULL)
	{
		return dir_state_not_dir;
	}

	while ((d = readdir(dir)) != NULL)
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

int invoke_mount(unsigned int client_number, const char *path, const char *mount_options)
{
	unsigned int dev, part;
	const struct filesystem_options *fsopts;
	const struct string_to_mount_flag *mntflagslist;

	unsigned long mount_flags = 0;
	char *mount_opts = NULL;
	unsigned int mount_opts_len = 0;
	unsigned int mount_opts_len_cur = 0;

	const char *opt_start;
	const char *opt_end;
	unsigned int opt_len;
	int result;
#if OS == Linux
	uid_t uid;
	gid_t gid;
#endif /* OS == Linux */

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
	if (dev >= media_count)
	{
		result = 0;
		goto invoke_mount_error_1;
	}

	fsopts = filesystem_mount_options;

	for (;;)
	{
		if (fsopts->fstype == NULL)
		{
			result = 0;
			goto invoke_mount_error_1;
		}

		if (strcmp(fsopts->fstype, media[dev]->partition[part]->type) == 0)
		{
			break;
		}

		++fsopts;
	}

	if (mount_options == NULL)
	{
		mount_options = fsopts->defaults;
	}

	// check flags
	opt_start = mount_options;

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
			goto invoke_mount_error_1;
		}

		// check option
		if (!is_option_allowed(opt_start, opt_len, fsopts))
		{
			result = 0;
			goto invoke_mount_error_1;
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

		if (mntflagslist->option == NULL)
		{
			mount_opts_len += opt_len;
			++mount_flags;
		}

		opt_start = opt_end;
	}

	if (mount_flags > 1)
	{
		mount_opts_len += mount_flags - 1;
	}

	// add uid/gid
	if ((fsopts->option_uid != NULL) || (fsopts->option_gid != NULL))
	{
		if (get_credentials(clients[client_number]->clientfd, &uid, &gid) != 1)
		{
			result = -1;
			goto invoke_mount_error_1;
		}

		if (fsopts->option_uid != NULL)
		{
			result = snprintf(NULL, 0, "%d", uid);
			if (result < 1)
			{
				result = -1;
				goto invoke_mount_error_1;
			}

			if (mount_opts_len != 0)
			{
				++mount_opts_len;
			}

			mount_opts_len += strlen(fsopts->option_uid) + result;
		}

		if (fsopts->option_gid != NULL)
		{
			result = snprintf(NULL, 0, "%d", gid);
			if (result < 1)
			{
				result = -1;
				goto invoke_mount_error_1;
			}

			if (mount_opts_len != 0)
			{
				++mount_opts_len;
			}

			mount_opts_len += strlen(fsopts->option_gid) + result;
		}
	}

	// create flags string
	if (mount_opts_len > 0)
	{
		mount_opts = (char*) malloc(mount_opts_len+1);
		if (mount_opts == NULL)
		{
			result = -1;
			goto invoke_mount_error_1;
		}

		mount_flags = 0;
		opt_start = mount_options;

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
				goto invoke_mount_error_2;
			}

			// check option
			if (!is_option_allowed(opt_start, opt_len, fsopts))
			{
				result = 0;
				goto invoke_mount_error_2;
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

			if (mntflagslist->option == NULL)
			{
				if (mount_opts_len_cur != 0)
				{
					mount_opts[mount_opts_len_cur] = ',';
					++mount_opts_len_cur;
				}

				memcpy(&mount_opts[mount_opts_len_cur], opt_start, opt_len);
				mount_opts_len_cur += opt_len;
			}
			else
			{
				if (mntflagslist->enabled)
				{
					mount_flags |= mntflagslist->flag;
				}
				else
				{
					mount_flags &= ~(mntflagslist->flag);
				}
			}

			opt_start = opt_end;
		}

		// add uid/gid
		if (fsopts->option_uid != NULL)
		{
			if (mount_opts_len_cur != 0)
			{
				mount_opts[mount_opts_len_cur] = ',';
				++mount_opts_len_cur;
			}

			result = strlen(fsopts->option_uid);

			memcpy(&mount_opts[mount_opts_len_cur], fsopts->option_uid, result);
			mount_opts_len_cur += result;

			result = snprintf(&mount_opts[mount_opts_len_cur], mount_opts_len + mount_opts_len_cur + 1, "%d", uid);
			if (result < 1)
			{
				result = -1;
				goto invoke_mount_error_2;
			}

			mount_opts_len_cur += result;
		}

		if (fsopts->option_gid != NULL)
		{
			if (mount_opts_len_cur != 0)
			{
				mount_opts[mount_opts_len_cur] = ',';
				++mount_opts_len_cur;
			}

			result = strlen(fsopts->option_gid);

			memcpy(&mount_opts[mount_opts_len_cur], fsopts->option_gid, result);
			mount_opts_len_cur += result;

			result = snprintf(&mount_opts[mount_opts_len_cur], mount_opts_len + mount_opts_len_cur + 1, "%d", gid);
			if (result < 1)
			{
				result = -1;
				goto invoke_mount_error_2;
			}

			mount_opts_len_cur += result;
		}

		mount_opts[mount_opts_len_cur] = 0;

		printf("flags: %lu\nopts: %s\ndefs: %s\n", mount_flags, mount_opts, fsopts->defaults);
		free(mount_opts);
	}

	return 0;

	// check that it's not already mounted
	// check if there is nothing mounted in mount point
	// check that directory is empty or create it if it doesn't exist

	return 1;

invoke_mount_error_2:
	free(mount_opts);

invoke_mount_error_1:
	return result;
}

int invoke_unmount(unsigned int client_number, const char *path)
{
	//char *mount_point;
	// TODO: implement



	// check that there is exactly one mount on mount point and it's our device
	// check that directory is empty and remove it

	//if (get_dir_state(mount_point) == dir_state_empty)
	//{
	//	rmdir(mount_point);
	//}

	return 0;
}
