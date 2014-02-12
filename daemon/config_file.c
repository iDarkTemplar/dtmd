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

#include "daemon/config_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef CONFIG_DIR
#error CONFIG_DIR is not defined
#endif /* CONFIG_DIR */

#define config_filename CONFIG_DIR "/dtmd.conf"

int unmount_on_exit = 0;
enum mount_by_value_enum mount_by_value = mount_by_device_name;

static const char *config_unmount_on_exit = "unmount_on_exit";
static const char *config_unmount_on_exit_yes = "yes";
static const char *config_unmount_on_exit_no = "no";

static const char *config_mount_by = "mount_by";
static const char *config_mount_by_name = "name";
static const char *config_mount_by_label = "label";

static void process_config_value(const char *key_start, size_t key_length, const char *value_start, size_t value_length)
{
	if ((key_length == strlen(config_unmount_on_exit))
		&& (strncmp(key_start, config_unmount_on_exit, key_length) == 0))
	{
		if ((value_length == strlen(config_unmount_on_exit_yes))
			&& (strncmp(value_start, config_unmount_on_exit_yes, value_length) == 0))
		{
			unmount_on_exit = 1;
		}
		else if ((value_length == strlen(config_unmount_on_exit_no))
			&& (strncmp(value_start, config_unmount_on_exit_no, value_length)) == 0)
		{
			unmount_on_exit = 0;
		}
	}

	if ((key_length == strlen(config_mount_by))
		&& (strncmp(key_start, config_mount_by, key_length) == 0))
	{
		if ((value_length == strlen(config_mount_by_name))
			&& (strncmp(value_start, config_mount_by_name, value_length) == 0))
		{
			mount_by_value = mount_by_device_name;
		}
		else if ((value_length == strlen(config_mount_by_label))
			&& (strncmp(value_start, config_mount_by_label, value_length)) == 0)
		{
			mount_by_value = mount_by_device_label;
		}
	}
}

void read_config(void)
{
	FILE *file;
	char *buffer = NULL;
	size_t buffer_size = 0;
	ssize_t read_size;
	ssize_t i;
	ssize_t key_start, key_end;
	ssize_t equal_start;
	ssize_t value_start, value_end;

	file = fopen(config_filename, "r");
	if (file == NULL)
	{
		return;
	}

	while ((read_size = getline(&buffer, &buffer_size, file)) > 0)
	{
		key_start   = key_end   = -1;
		equal_start = -1;
		value_start = value_end = -1;

		for (i = 0; i < read_size; ++i)
		{
			if ((isalnum(buffer[i])) || (buffer[i] == '_'))
			{
				if (key_start == -1)
				{
					key_start = i;
				}
				else if ((key_start != -1) && (key_end == -1))
				{
					// continue;
				}
				else if ((equal_start != -1) && (value_start == -1))
				{
					value_start = i;
				}
				else if ((value_start != -1) && (value_end == -1))
				{
					// continue
				}
				else
				{
					// error on line
					break;
				}
			}
			else if (buffer[i] == '=')
			{
				if ((key_start != -1) && (equal_start == -1))
				{
					equal_start = i;

					if (key_end == -1)
					{
						key_end = i;
					}
				}
				else
				{
					// error on line
					break;
				}
			}
			else if ((isblank(buffer[i])) || (buffer[i] == '#'))
			{
				if ((key_start != -1) && (key_end == -1))
				{
					key_end = i;
				}
				else if ((value_start != -1) && (value_end == -1))
				{
					value_end = i;
				}

				if (buffer[i] == '#')
				{
					// comment
					break;
				}
			}
			else if (buffer[i] == '\n')
			{
				if ((value_start != -1) && (value_end == -1))
				{
					value_end = i;
				}
				else
				{
					// error on line
					break;
				}
			}
			else
			{
				// error on line
				break;
			}
		}

		if (value_end != -1)
		{
			process_config_value(&(buffer[key_start]), key_end - key_start, &(buffer[value_start]), value_end - value_start);
		}
	}

	fclose(file);

	if (buffer != NULL)
	{
		free(buffer);
	}
}

void free_config(void)
{
}
