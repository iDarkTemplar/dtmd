/*
 * Copyright (C) 2017 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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
#include "daemon/lists.h"
#include "daemon/return_codes.h"
#include "tests/dt_tests.h"

int use_syslog = 0;
int daemonize = 1;

void notify_removable_device_added(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
}

void notify_removable_device_changed(const char *parent_path,
	const char *path,
	dtmd_removable_media_type_t media_type,
	dtmd_removable_media_subtype_t media_subtype,
	dtmd_removable_media_state_t state,
	const char *fstype,
	const char *label,
	const char *mnt_point,
	const char *mnt_opts)
{
}

void notify_removable_device_mounted(const char *path, const char *mount_point, const char *mount_options)
{
}

void notify_removable_device_unmounted(const char *path, const char *mount_point)
{
}

void notify_removable_device_removed(const char *path)
{
}

int main(int argc, char **argv)
{
	tests_init();

	(void)argc;
	(void)argv;

	// media structures memory test (use with valgrind)

	test_compare(is_result_successful(add_media("/","/dev/sdd", dtmd_removable_media_type_stateless_device, dtmd_removable_media_subtype_removable_disk, dtmd_removable_media_state_unknown, NULL, NULL, NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd3", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd2", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive2", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd1", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));

	test_compare(is_result_successful(remove_media("/dev/sdd3")));
	test_compare(is_result_successful(remove_media("/dev/sdd2")));
	test_compare(is_result_successful(remove_media("/dev/sdd1")));
	test_compare(is_result_successful(remove_media("/dev/sdd")));

	test_compare(is_result_successful(add_media("/","/dev/sdd", dtmd_removable_media_type_stateless_device, dtmd_removable_media_subtype_removable_disk, dtmd_removable_media_state_unknown, NULL, NULL, NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd1", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd2", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive2", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd3", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));

	test_compare(is_result_successful(remove_media("/dev/sdd3")));
	test_compare(is_result_successful(remove_media("/dev/sdd2")));
	test_compare(is_result_successful(remove_media("/dev/sdd1")));
	test_compare(is_result_successful(remove_media("/dev/sdd")));

	test_compare(is_result_successful(add_media("/","/dev/sdd", dtmd_removable_media_type_stateless_device, dtmd_removable_media_subtype_removable_disk, dtmd_removable_media_state_unknown, NULL, NULL, NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd3", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd2", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive2", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd1", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));

	test_compare(is_result_successful(remove_media("/dev/sdd1")));
	test_compare(is_result_successful(remove_media("/dev/sdd2")));
	test_compare(is_result_successful(remove_media("/dev/sdd3")));
	test_compare(is_result_successful(remove_media("/dev/sdd")));

	test_compare(is_result_successful(add_media("/","/dev/sdd", dtmd_removable_media_type_stateless_device, dtmd_removable_media_subtype_removable_disk, dtmd_removable_media_state_unknown, NULL, NULL, NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd1", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd2", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive2", NULL, NULL)));
	test_compare(is_result_successful(add_media("/dev/sdd","/dev/sdd3", dtmd_removable_media_type_stateless_device, dtmd_removable_media_type_device_partition, dtmd_removable_media_state_unknown, "dummy", "drive1", NULL, NULL)));

	test_compare(is_result_successful(remove_media("/dev/sdd1")));
	test_compare(is_result_successful(remove_media("/dev/sdd2")));
	test_compare(is_result_successful(remove_media("/dev/sdd3")));
	test_compare(is_result_successful(remove_media("/dev/sdd")));

	return tests_result();
}
