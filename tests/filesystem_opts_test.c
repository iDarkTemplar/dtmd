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

#include <stdlib.h>
#include <string.h>
#include "daemon/filesystem_opts.h"
#include "tests/dt_tests.h"

#include <sys/mount.h>

// required to disable logging output and meet linking requirements
int use_syslog = 0;
int daemonize = 1;

#define get_fsopts(fstype) \
	fsopts_##fstype = get_fsopts_for_fs(#fstype); \
	if (fsopts_##fstype == NULL) \
	{ \
		printf("Couldn't get fsopts for " #fstype); \
		return -1; \
	}

int main(int argc, char **argv)
{
	char *filesystem_opts_vfat = "flush,utf8=1,shortname=mixed,umask=000,dmask=000,fmask=000,codepage=cp1251,iocharset=utf8,showexec,blocksize=4096,allow_utime=1,check=0,conv=1";
	char *filesystem_opts_ntfs_3g = "umask=000,dmask=000,fmask=000,iocharset=utf-8,windows_names,allow_other";
	char *filesystem_opts_iso9660 = "norock,nojoliet,iocharset=utf-8,mode=000,dmode=000,utf8,block=4096,conv=1";
	char *filesystem_opts_udf = "iocharset=utf-8,umask=000,mode=000,dmode=000,undelete";

	char *default_opts1 = "exec,atime,nodiratime,ro,sync,dirsync";
	char *default_opts2 = "exec,noexec,nodev,nosuid,atime,noatime,ro,rw";

	char *invalid_opts1 = "exec,execno,reallyanoption";
	char *invalid_opts2 = "exec=yes";

	char *invalid_opts_vfat = "utf8";

	unsigned int len, len_full;
	unsigned long flags;

	const struct dtmd_filesystem_options *fsopts_vfat;
	const struct dtmd_filesystem_options *fsopts_ntfs;
	const struct dtmd_filesystem_options *fsopts_iso9660;
	const struct dtmd_filesystem_options *fsopts_udf;

	dtmd_fsopts_list_t fsopts_list;

	uid_t uid = 21;
	gid_t gid = 987;

	tests_init();

	(void)argc;
	(void)argv;

	get_fsopts(vfat);
	get_fsopts(ntfs);
	get_fsopts(iso9660);
	get_fsopts(udf);

	// Test 1: vfat
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(filesystem_opts_vfat, fsopts_vfat, NULL, NULL, &fsopts_list) == 1, "Test 1: vfat", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 1: vfat", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 142, "Test 1: vfat");
	test_compare_comment(len == 142, "Test 1: vfat");
	test_compare_comment(flags == 0, "Test 1: vfat");

	// Test 2: ntfs-3g
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(filesystem_opts_ntfs_3g, fsopts_ntfs, NULL, NULL, &fsopts_list) == 1, "Test 2: ntfs-3g", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 2: ntfs-3g", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 71, "Test 2: ntfs-3g");
	test_compare_comment(len == 71, "Test 2: ntfs-3g");
	test_compare_comment(flags == 0, "Test 2: ntfs-3g");

	// Test 3: iso9660
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(filesystem_opts_iso9660, fsopts_iso9660, NULL, NULL, &fsopts_list) == 1, "Test 3: iso9660", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 3: iso9660", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 73, "Test 3: iso9660");
	test_compare_comment(len == 73, "Test 3: iso9660");
	test_compare_comment(flags == 0, "Test 3: iso9660");

	// Test 4: udf
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(filesystem_opts_udf, fsopts_udf, NULL, NULL, &fsopts_list) == 1, "Test 4: udf", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 4: udf", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 53, "Test 4: udf");
	test_compare_comment(len == 53, "Test 4: udf");
	test_compare_comment(flags == 0, "Test 4: udf");

	// Test 5: default options set 1
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(default_opts1, NULL, NULL, NULL, &fsopts_list) == 1, "Test 5: default options set 1", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 5: default options set 1", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 37, "Test 5: default options set 1");
	test_compare_comment(len == 0, "Test 5: default options set 1");
	test_compare_comment(flags == (MS_NODIRATIME | MS_RDONLY | MS_SYNCHRONOUS | MS_DIRSYNC), "Test 5: default options set 1");

	// Test 6: default options set 2
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(default_opts2, NULL, NULL, NULL, &fsopts_list) == 1, "Test 6: default options set 2", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 6: default options set 2", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 30, "Test 6: default options set 2");
	test_compare_comment(len == 0, "Test 6: default options set 2");
	test_compare_comment(flags == (MS_NOEXEC | MS_NODEV | MS_NOSUID | MS_NOATIME), "Test 6: default options set 2");

	// Test 7: invalid options set 1
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(invalid_opts1, NULL, NULL, NULL, &fsopts_list) == 0, "Test 7: invalid options set 1", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	// Test 8: invalid options set 2
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(invalid_opts2, NULL, NULL, NULL, &fsopts_list) == 0, "Test 8: invalid options set 2", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	// Test 9: invalid options set for vfat
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(invalid_opts_vfat, fsopts_vfat, NULL, NULL, &fsopts_list) == 0, "Test 9: invalid options set for vfat", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	// Test 10: vfat with uid
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list("", fsopts_vfat, &uid, NULL, &fsopts_list) == 1, "Test 10: vfat with uid", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 10: vfat with uid", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 6, "Test 10: vfat with uid");
	test_compare_comment(len == 6, "Test 10: vfat with uid");
	test_compare_comment(flags == 0, "Test 10: vfat with uid");

	// Test 11: vfat with gid
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list("", fsopts_vfat, NULL, &gid, &fsopts_list) == 1, "Test 11: vfat with gid", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 11: vfat with gid", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 7, "Test 11: vfat with gid");
	test_compare_comment(len == 7, "Test 11: vfat with gid");
	test_compare_comment(flags == 0, "Test 11: vfat with gid");

	// Test 12: vfat with uid and gid
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list("", fsopts_vfat, &uid, &gid, &fsopts_list) == 1, "Test 12: vfat with uid and gid", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 12: vfat with uid and gid", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 14, "Test 12: vfat with uid and gid");
	test_compare_comment(len == 14, "Test 12: vfat with uid and gid");
	test_compare_comment(flags == 0, "Test 12: vfat with uid and gid");

	// Test 13: vfat with options, uid and gid
	init_options_list(&fsopts_list);
	test_compare_comment_deinit(convert_options_to_list(filesystem_opts_vfat, fsopts_vfat, &uid, &gid, &fsopts_list) == 1, "Test 13: vfat with options, uid and gid", free_options_list(&fsopts_list));
	test_compare_comment_deinit(fsopts_generate_string(&fsopts_list, &len_full, NULL, 0, &len, NULL, 0, &flags) == 1, "Test 13: vfat with options, uid and gid", free_options_list(&fsopts_list));
	free_options_list(&fsopts_list);

	test_compare_comment(len_full == 157, "Test 13: vfat with options, uid and gid");
	test_compare_comment(len == 157, "Test 13: vfat with options, uid and gid");
	test_compare_comment(flags == 0, "Test 13: vfat with options, uid and gid");

	return tests_result();
}
