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

#ifndef DTMD_RETURN_CODES_H
#define DTMD_RETURN_CODES_H

#define result_success 1 /* > 0 means success, it doesn't have to be 1 */
#define result_fail 0 /* Generic fail, non-fatal, may be part of normal daemon operation */
#define result_client_error -1 /* fatal client error, client must be disconnected */
#define result_fatal_error -2 /* fatal daemon error, daemon must be stopped */
#define result_bug -3 /* daemon bug, should never happen, but if it happens, maybe panic? */

#define is_result_successful(x) ((x) > 0)
#define is_result_failure(x) ((x) <= 0)
#define is_result_nonfatal_error(x) ((x) == 0)
#define is_result_fatal_error(x) ((x) < 0)

#endif /* DTMD_RETURN_CODES_H */
