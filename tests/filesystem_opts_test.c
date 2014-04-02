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
#include <dtmd-filesystem-opts.h>
#include "tests/dt_tests.h"

#include <sys/mount.h>

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

	dtmd_fsopts_result_t fsopts_result;
	unsigned int len, len_full;
	unsigned long flags;

	uid_t uid = 21;
	gid_t gid = 987;

	tests_init();

	(void)argc;
	(void)argv;

	fsopts_result = dtmd_fsopts_generate_string(filesystem_opts_vfat, "vfat", NULL, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 1: vfat");
	test_compare_comment(len_full == 142, "Test 1: vfat");
	test_compare_comment(len == 142, "Test 1: vfat");
	test_compare_comment(flags == 0, "Test 1: vfat");

	fsopts_result = dtmd_fsopts_generate_string(filesystem_opts_ntfs_3g, "ntfs-3g", NULL, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_external_mount, "Test 2: ntfs-3g");
	test_compare_comment(len_full == 71, "Test 2: ntfs-3g");
	test_compare_comment(len == 71, "Test 2: ntfs-3g");
	test_compare_comment(flags == 0, "Test 2: ntfs-3g");

	fsopts_result = dtmd_fsopts_generate_string(filesystem_opts_iso9660, "iso9660", NULL, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 3: iso9660");
	test_compare_comment(len_full == 73, "Test 3: iso9660");
	test_compare_comment(len == 73, "Test 3: iso9660");
	test_compare_comment(flags == 0, "Test 3: iso9660");

	fsopts_result = dtmd_fsopts_generate_string(filesystem_opts_udf, "udf", NULL, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 4: udf");
	test_compare_comment(len_full == 53, "Test 4: udf");
	test_compare_comment(len == 53, "Test 4: udf");
	test_compare_comment(flags == 0, "Test 4: udf");

	fsopts_result = dtmd_fsopts_generate_string(default_opts1, NULL, NULL, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 5: default options set 1");
	test_compare_comment(len_full == 37, "Test 5: default options set 1");
	test_compare_comment(len == 0, "Test 5: default options set 1");
	test_compare_comment(flags == (MS_NODIRATIME | MS_RDONLY | MS_SYNCHRONOUS | MS_DIRSYNC), "Test 5: default options set 1");

	fsopts_result = dtmd_fsopts_generate_string(default_opts2, NULL, NULL, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 6: default options set 2");
	test_compare_comment(len_full == 30, "Test 6: default options set 2");
	test_compare_comment(len == 0, "Test 6: default options set 2");
	test_compare_comment(flags == (MS_NOEXEC | MS_NODEV | MS_NOSUID | MS_NOATIME), "Test 6: default options set 2");

	fsopts_result = dtmd_fsopts_generate_string(invalid_opts1, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL);

	test_compare_comment(fsopts_result == dtmd_fsopts_not_supported, "Test 7: invalid options set 1");

	fsopts_result = dtmd_fsopts_generate_string(invalid_opts2, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL);

	test_compare_comment(fsopts_result == dtmd_fsopts_not_supported, "Test 8: invalid options set 2");

	fsopts_result = dtmd_fsopts_generate_string(invalid_opts_vfat, "vfat", NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL);

	test_compare_comment(fsopts_result == dtmd_fsopts_not_supported, "Test 9: invalid options set for vfat");

	fsopts_result = dtmd_fsopts_generate_string("", "vfat", &uid, NULL, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 10: vfat with uid");
	test_compare_comment(len_full == 6, "Test 10: vfat with uid");
	test_compare_comment(len == 6, "Test 10: vfat with uid");
	test_compare_comment(flags == 0, "Test 10: vfat with uid");

	fsopts_result = dtmd_fsopts_generate_string("", "vfat", NULL, &gid, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 11: vfat with gid");
	test_compare_comment(len_full == 7, "Test 11: vfat with gid");
	test_compare_comment(len == 7, "Test 11: vfat with gid");
	test_compare_comment(flags == 0, "Test 11: vfat with gid");

	fsopts_result = dtmd_fsopts_generate_string("", "vfat", &uid, &gid, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 12: vfat with uid and gid");
	test_compare_comment(len_full == 14, "Test 12: vfat with uid and gid");
	test_compare_comment(len == 14, "Test 12: vfat with uid and gid");
	test_compare_comment(flags == 0, "Test 12: vfat with uid and gid");

	fsopts_result = dtmd_fsopts_generate_string(filesystem_opts_vfat, "vfat", &uid, &gid, &len_full, NULL, 0, &len, NULL, 0, &flags);

	test_compare_comment(fsopts_result == dtmd_fsopts_internal_mount, "Test 13: vfat with options, uid and gid");
	test_compare_comment(len_full == 157, "Test 13: vfat with options, uid and gid");
	test_compare_comment(len == 157, "Test 13: vfat with options, uid and gid");
	test_compare_comment(flags == 0, "Test 13: vfat with options, uid and gid");

	return tests_result();
}
