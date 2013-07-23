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

#ifndef DTMD_LIBRARY_H
#define DTMD_LIBRARY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dtmd_library dtmd_t;
typedef void (*dtmd_callback)(void *arg);

dtmd_t* dtmd_init(void);
void dtmd_deinit(dtmd_t *handle);

void dtmd_set_callback(dtmd_t *handle, dtmd_callback callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_LIBRARY_H */
