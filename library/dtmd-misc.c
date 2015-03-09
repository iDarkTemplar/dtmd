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

#include <dtmd-misc.h>

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

int dtmd_validate_command(const char *buffer)
{
	const char *cur;
	int i;

	if (buffer == NULL)
	{
		return 0;
	}

	cur = buffer;
	i   = 0;

	for (;;)
	{
		if ((!isalnum(*cur)) && ((*cur) != '_'))
		{
			if (((*cur) == '(') && (i != 0))
			{
				break;
			}
			else
			{
				return 0;
			}
		}

		++i;
		++cur;
	}

	++cur;

	if (((*cur) == ')') && ((*(cur+1)) == '\n'))
	{
		return 1;
	}

	for (;;)
	{
		if ((*cur) == '\"')
		{
			++cur;

			for (;;)
			{
				if (((*cur) == '\n') || ((*cur) == 0))
				{
					return 0;
				}

				if ((*cur) == '\"')
				{
					break;
				}

				++cur;
			}

			++cur;
		}
		else if (((*cur) == 'n') && ((*(cur+1)) == 'i') && ((*(cur+2)) == 'l'))
		{
			cur += 3;
		}
		else
		{
			return 0;
		}

		if (((*cur) != ',') || ((*(cur+1)) != ' '))
		{
			break;
		}

		cur += 2;
	}

	if (((*cur) == ')') && ((*(cur+1)) == '\n'))
	{
		return 1;
	}

	return 0;
}

dtmd_command_t* dtmd_parse_command(const char *buffer)
{
	const char *cur;
	const char *start;
	dtmd_command_t *result;
	int i;
	char *tmp_str;
	char **tmp;

	if (buffer == NULL)
	{
		goto parse_command_error_1;
	}

	result = (dtmd_command_t*) malloc(sizeof(dtmd_command_t));
	if (result == NULL)
	{
		goto parse_command_error_1;
	}

	result->cmd        = NULL;
	result->args_count = 0;
	result->args       = NULL;

	cur = buffer;
	i   = 0;

	for (;;)
	{
		if ((!isalnum(*cur)) && ((*cur) != '_'))
		{
			if (((*cur) == '(') && (i != 0))
			{
				break;
			}
			else
			{
				goto parse_command_error_2;
			}
		}

		++i;
		++cur;
	}

	result->cmd = (char*) malloc((i+1)*sizeof(char));
	if (result->cmd == NULL)
	{
		goto parse_command_error_2;
	}

	memcpy(result->cmd, buffer, i);
	result->cmd[i] = 0;

	++cur;

	if (((*cur) == ')') && ((*(cur+1)) == '\n'))
	{
		return result;
	}

	for (;;)
	{
		if ((*cur) == '\"')
		{
			++cur;

			start = cur;
			i     = 0;

			for (;;)
			{
				if (((*cur) == '\n') || ((*cur) == 0))
				{
					goto parse_command_error_2;
				}

				if ((*cur) == '\"')
				{
					break;
				}

				++i;
				++cur;
			}

			tmp_str = (char*) malloc((i+1)*sizeof(char));
			if (tmp_str == NULL)
			{
				goto parse_command_error_2;
			}

			tmp = (char**) realloc(result->args, (result->args_count+1)*sizeof(char*));
			if (tmp == NULL)
			{
				free(tmp_str);
				goto parse_command_error_2;
			}

			result->args = tmp;
			memcpy(tmp_str, start, i);
			tmp_str[i] = 0;
			result->args[result->args_count] = tmp_str;
			++(result->args_count);

			++cur;
		}
		else if (((*cur) == 'n') && ((*(cur+1)) == 'i') && ((*(cur+2)) == 'l'))
		{
			tmp = (char**) realloc(result->args, (result->args_count+1)*sizeof(char*));
			if (tmp == NULL)
			{
				goto parse_command_error_2;
			}

			result->args = tmp;
			result->args[result->args_count] = NULL;
			++(result->args_count);

			cur += 3;
		}
		else
		{
			goto parse_command_error_2;
		}

		if (((*cur) != ',') || ((*(cur+1)) != ' '))
		{
			break;
		}

		cur += 2;
	}

	if (((*cur) == ')') && ((*(cur+1)) == '\n'))
	{
		return result;
	}

parse_command_error_2:
	dtmd_free_command(result);

parse_command_error_1:
	return NULL;
}

void dtmd_free_command(dtmd_command_t *cmd)
{
	unsigned int i;

	if (cmd != NULL)
	{
		if (cmd->cmd != NULL)
		{
			free(cmd->cmd);
		}

		if (cmd->args != NULL)
		{
			for (i = 0; i < cmd->args_count; ++i)
			{
				if (cmd->args[i] != NULL)
				{
					free(cmd->args[i]);
				}
			}

			free(cmd->args);
		}

		free(cmd);
	}
}

const char* dtmd_device_type_to_string(dtmd_removable_media_type_t type)
{
	switch (type)
	{
	case dtmd_removable_media_cdrom:
		return dtmd_string_device_cdrom;

	case dtmd_removable_media_removable_disk:
		return dtmd_string_device_removable_disk;

	case dtmd_removable_media_sd_card:
		return dtmd_string_device_sd_card;

	case dtmd_removable_media_unknown_or_persistent:
	default:
		return dtmd_string_device_unknown_or_persistent;
	}
}

dtmd_removable_media_type_t dtmd_string_to_device_type(const char *string)
{
	if (string != NULL)
	{
		if (strcmp(string, dtmd_string_device_cdrom) == 0)
		{
			return dtmd_removable_media_cdrom;
		}
		else if (strcmp(string, dtmd_string_device_removable_disk) == 0)
		{
			return dtmd_removable_media_removable_disk;
		}
		else if (strcmp(string, dtmd_string_device_sd_card) == 0)
		{
			return dtmd_removable_media_sd_card;
		}
	}

	return dtmd_removable_media_unknown_or_persistent;
}

const char* dtmd_device_state_to_string(dtmd_removable_media_state_t state)
{
	switch (state)
	{
	case dtmd_removable_media_state_empty:
		return dtmd_string_state_empty;

	case dtmd_removable_media_state_clear:
		return dtmd_string_state_clear;

	case dtmd_removable_media_state_ok:
		return dtmd_string_state_ok;

	case dtmd_removable_media_state_unknown:
	default:
		return dtmd_string_state_unknown;
	}
}

dtmd_removable_media_state_t dtmd_string_to_device_state(const char *string)
{
	if (string != NULL)
	{
		if (strcmp(string, dtmd_string_state_empty) == 0)
		{
			return dtmd_removable_media_state_empty;
		}
		else if (strcmp(string, dtmd_string_state_clear) == 0)
		{
			return dtmd_removable_media_state_clear;
		}
		else if (strcmp(string, dtmd_string_state_ok) == 0)
		{
			return dtmd_removable_media_state_ok;
		}
	}

	return dtmd_removable_media_state_unknown;
}

const char* dtmd_error_code_to_string(dtmd_error_code_t code)
{
	switch (code)
	{
	case dtmd_error_code_generic_error:
		return dtmd_string_error_code_generic_error;

	case dtmd_error_code_no_such_device:
		return dtmd_string_error_code_no_such_device;

	case dtmd_error_code_fstype_not_recognized:
		return dtmd_string_error_code_fstype_not_recognized;

	case dtmd_error_code_unsupported_fstype:
		return dtmd_string_error_code_unsupported_fstype;

	case dtmd_error_code_device_already_mounted:
		return dtmd_string_error_code_device_already_mounted;

	case dtmd_error_code_device_not_mounted:
		return dtmd_string_error_code_device_not_mounted;

	case dtmd_error_code_failed_parsing_mount_options:
		return dtmd_string_error_code_failed_parsing_mount_options;

	case dtmd_error_code_mount_point_busy:
		return dtmd_string_error_code_failed_parsing_mount_options;

	case dtmd_error_code_unknown:
	default:
		return dtmd_string_error_code_unknown;
	}
}

dtmd_error_code_t dtmd_string_to_error_code(const char *string)
{
	if (string != NULL)
	{
		if (strcmp(string, dtmd_string_error_code_generic_error) == 0)
		{
			return dtmd_error_code_generic_error;
		}
		else if (strcmp(string, dtmd_string_error_code_no_such_device) == 0)
		{
			return dtmd_error_code_no_such_device;
		}
		else if (strcmp(string, dtmd_string_error_code_fstype_not_recognized) == 0)
		{
			return dtmd_error_code_fstype_not_recognized;
		}
		else if (strcmp(string, dtmd_string_error_code_unsupported_fstype) == 0)
		{
			return dtmd_error_code_unsupported_fstype;
		}
		else if (strcmp(string, dtmd_string_error_code_device_already_mounted) == 0)
		{
			return dtmd_error_code_device_already_mounted;
		}
		else if (strcmp(string, dtmd_string_error_code_device_not_mounted) == 0)
		{
			return dtmd_error_code_device_not_mounted;
		}
		else if (strcmp(string, dtmd_string_error_code_failed_parsing_mount_options) == 0)
		{
			return dtmd_error_code_failed_parsing_mount_options;
		}
		else if (strcmp(string, dtmd_string_error_code_mount_point_busy) == 0)
		{
			return dtmd_error_code_mount_point_busy;
		}
	}

	return dtmd_error_code_unknown;
}
