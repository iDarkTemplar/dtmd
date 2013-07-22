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

#include "commands.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

struct command* parse_command(unsigned char *buffer)
{
	unsigned char *cur;
	unsigned char *start;
	struct command *result;
	int i;
	unsigned char *tmp_str;
	unsigned char **tmp;

	result = (struct command*) malloc(sizeof(struct command));
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

	result->cmd = (unsigned char*) malloc((i+1)*sizeof(unsigned char));
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

	if ((*cur) != '\"')
	{
		goto parse_command_error_2;
	}

	++cur;

	for (;;)
	{
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

		tmp_str = (unsigned char*) malloc((i+1)*sizeof(unsigned char));
		if (tmp_str == NULL)
		{
			goto parse_command_error_2;
		}

		tmp = (unsigned char**) realloc(result->args, (result->args_count+1)*sizeof(unsigned char*));
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

		if (((*cur) != ',') || ((*(cur+1)) != ' ') || ((*(cur+2)) != '\"'))
		{
			break;
		}

		cur += 3;
	}

	if (((*cur) == ')') && ((*(cur+1)) == '\n'))
	{
		return result;
	}

parse_command_error_2:
	free_command(result);

parse_command_error_1:
	return NULL;
}

void free_command(struct command *cmd)
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
				free(cmd->args[i]);
			}

			free(cmd->args);
		}

		free(cmd);
	}
}
