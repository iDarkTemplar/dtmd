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

#ifndef MNT_FUNCS_H
#define MNT_FUNCS_H

#ifdef __cplusplus
extern "C" {
#endif

int init_mount_monitoring(void);
int close_mount_monitoring(int monitorfd);

#if (defined OS_Linux)
int check_mount_changes(void);
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
/* pass -1 to check for changes even while there are no events */
int check_mount_changes(int mountfd);
#endif /* (defined OS_FreeBSD) */

int point_mount_count(const char *path, int max);

#if (defined OS_Linux)
int add_to_mtab(const char *path, const char *mount_point, const char *type, const char *mount_opts);
int remove_from_mtab(const char *path, const char *mount_point, const char *type);
#endif /* (defined OS_Linux) */

#ifdef __cplusplus
}
#endif

#endif /* MNT_FUNCS_H */
