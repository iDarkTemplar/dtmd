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

#ifndef DTMD_INTERNAL_H
#define DTMD_INTERNAL_H

#define dtmd_internal_mount_dir "/media"

#if (defined OS_Linux)

#define dtmd_internal_mounts_file "/proc/self/mounts"

#if (!defined MTAB_READONLY)
#ifndef MTAB_DIR
#error MTAB_DIR is not defined
#endif /* MTAB_DIR */

#define dtmd_internal_mtab_file MTAB_DIR "/mtab"
#define dtmd_internal_mtab_temporary MTAB_DIR "/.mtab.dtmd"
#endif /* (!defined MTAB_READONLY) */

#endif /* (defined OS_Linux) */

#endif /* DTMD_INTERNAL_H */
