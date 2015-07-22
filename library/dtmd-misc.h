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

#ifndef DTMD_MISC_H
#define DTMD_MISC_H

#include <dtmd.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dtmd_command
{
	char *cmd;
	size_t args_count;
	char **args;
} dtmd_command_t;

int dtmd_validate_command(const char *buffer);
dtmd_command_t* dtmd_parse_command(const char *buffer);
void dtmd_free_command(dtmd_command_t *cmd);

const char* dtmd_device_type_to_string(dtmd_removable_media_type_t type);
dtmd_removable_media_type_t dtmd_string_to_device_type(const char *string);

const char* dtmd_device_state_to_string(dtmd_removable_media_state_t state);
dtmd_removable_media_state_t dtmd_string_to_device_state(const char *string);

const char* dtmd_error_code_to_string(dtmd_error_code_t code);
dtmd_error_code_t dtmd_string_to_error_code(const char *string);

#ifdef __cplusplus
}
#endif

#endif /* DTMD_MISC_H */
