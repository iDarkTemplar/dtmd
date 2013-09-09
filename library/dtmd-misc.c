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

static const char *str_unknown_or_persistent = dtmd_string_device_unknown_or_persistent;
static const char *str_cdrom                 = dtmd_string_device_cdrom;
static const char *str_removable_disk        = dtmd_string_device_removable_disk;
static const char *str_sd_card               = dtmd_string_device_sd_card;

dtmd_command_t* dtmd_parse_command(char *buffer)
{
	char *cur;
	char *start;
	dtmd_command_t *result;
	int i;
	char *tmp_str;
	char **tmp;

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
	int i;

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
	case cdrom:
		return str_cdrom;

	case removable_disk:
		return str_removable_disk;

	case sd_card:
		return str_sd_card;

	case unknown_or_persistent:
	default:
		return str_unknown_or_persistent;
	}
}

dtmd_removable_media_type_t dtmd_string_to_device_type(const char *string)
{
	if (strcmp(string, str_cdrom) == 0)
	{
		return cdrom;
	}
	else if (strcmp(string, str_removable_disk) == 0)
	{
		return removable_disk;
	}
	else if (strcmp(string, str_sd_card) == 0)
	{
		return sd_card;
	}
	else
	{
		return unknown_or_persistent;
	}
}

/*
	\a – Bell (beep)
	\b – Backspace
	\f – Formfeed
	\n – New line
	\r – Carriage return
	\t – Horizontal tab
	\\ – Backslash
	\' – Single quotation mark
	\" – Double quotation mark
	\ooo – Octal representation
	\xdd – Hexadecimal representation
*/

const char* dtmd_decode_label(const char *label)
{
	char *result;
	char *cur_result;
	int i;
	int k;

	result = malloc(strlen(label)+1);
	if (result == NULL)
	{
		return NULL;
	}

	cur_result = result;

	while (*label)
	{
		if ((*label) == '\\')
		{
			++label;

			if ((*label) == 0)
			{
				free(result);
				return NULL;
			}

			switch (*label)
			{
			case 'a':
				*cur_result = '\a';
				++cur_result;
				++label;
				break;

			case 'b':
				*cur_result = '\b';
				++cur_result;
				++label;
				break;

			case 'n':
				*cur_result = '\n';
				++cur_result;
				++label;
				break;

			case 'r':
				*cur_result = '\r';
				++cur_result;
				++label;
				break;

			case 't':
				*cur_result = '\t';
				++cur_result;
				++label;
				break;

			case '\\':
				*cur_result = '\\';
				++cur_result;
				++label;
				break;

			case '\'':
				*cur_result = '\'';
				++cur_result;
				++label;
				break;

			case '\"':
				*cur_result = '\"';
				++cur_result;
				++label;
				break;

			case 'x':
				k = 0;

				for (i = 1; i < 3; ++i)
				{
					if (!isxdigit(label[i]))
					{
						free(result);
						return NULL;
					}

					k *= 16;

					if ((label[i] >= '0') && (label[i] <= '9'))
					{
						k += label[i] - '0';
					}
					else
					{
						k += tolower(label[i]) - 'a' + 10;
					}
				}
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				k = 0;

				for (i = 1; i < 4; ++i)
				{
					if ((label[i] < '0') || (label[i] > '7'))
					{
						free(result);
						return NULL;
					}

					k *= 7;
					k += label[i] - '0';
				}

				*cur_result = k;
				++cur_result;
				label += 3;
				break;

			default:
				*cur_result = '\\';
				++cur_result;
				*cur_result = *label;
				++cur_result;
				++label;
			}
		}
		else
		{
			*cur_result = *label;
		}

		++cur_result;
		++label;
	}

	*cur_result = 0;

	return result;
}

void dtmd_free_decoded_label(const char *label)
{
	if (label != NULL)
	{
		free((char*)label);
	}
}
