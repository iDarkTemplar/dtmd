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
#include "filesystem_opts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef CONFIG_DIR
#error CONFIG_DIR is not defined
#endif /* CONFIG_DIR */

#define config_filename CONFIG_DIR "/dtmd.conf"

int unmount_on_exit = 0;
enum mount_by_value_enum mount_by_value = mount_by_device_name;
char *mount_dir = NULL;

struct default_mount_opts
{
	char *fs_type;
	char *opts;
};

struct default_mount_opts **default_mount_opts_array = NULL;
unsigned int default_mount_opts_array_size = 0;

static const char *config_unmount_on_exit = "unmount_on_exit";
static const char *config_unmount_on_exit_yes = "yes";
static const char *config_unmount_on_exit_no = "no";

static const char *config_mount_by = "mount_by";
static const char *config_mount_by_name = "name";
static const char *config_mount_by_label = "label";

static const char *config_mount_dir = "mount_dir";

static const char *config_default_mount_opts = "default_mount_opts_";

static int insert_mount_opts_into_array(char *fs_name, char *fs_opts)
{
	void *tmp;
	struct default_mount_opts *array_item;
	unsigned int item = 0;

	for ( ; item < default_mount_opts_array_size; ++item)
	{
		if (strcmp(fs_name, default_mount_opts_array[item]->fs_type) == 0)
		{
			break;
		}
	}

	if (item < default_mount_opts_array_size)
	{
		free(fs_name);
		free(default_mount_opts_array[item]->opts);
		default_mount_opts_array[item]->opts = fs_opts;

		return 0;
	}
	else
	{
		array_item = (struct default_mount_opts*) malloc(sizeof(struct default_mount_opts));
		if (array_item == NULL)
		{
			goto insert_mount_opts_into_array_error_1;
		}

		tmp = realloc(default_mount_opts_array, (default_mount_opts_array_size + 1) * (sizeof(struct default_mount_opts*)));
		if (tmp == NULL)
		{
			goto insert_mount_opts_into_array_error_2;
		}

		default_mount_opts_array = (struct default_mount_opts**) tmp;
		++default_mount_opts_array_size;
		array_item->fs_type = fs_name;
		array_item->opts = fs_opts;
		default_mount_opts_array[default_mount_opts_array_size - 1] = array_item;

		return 1;
	}

insert_mount_opts_into_array_error_2:
	free(array_item);

insert_mount_opts_into_array_error_1:
	free(fs_name);
	free(fs_opts);

	return -1;
}

static void free_mount_opts_array(void)
{
	unsigned int item = 0;

	if (default_mount_opts_array != NULL)
	{
		for ( ; item < default_mount_opts_array_size; ++item)
		{
			if (default_mount_opts_array[item] != NULL)
			{
				if (default_mount_opts_array[item]->fs_type != NULL)
				{
					free(default_mount_opts_array[item]->fs_type);
				}

				if (default_mount_opts_array[item]->opts != NULL)
				{
					free(default_mount_opts_array[item]->opts);
				}

				free(default_mount_opts_array[item]);
			}
		}

		free(default_mount_opts_array);
		default_mount_opts_array = NULL;
		default_mount_opts_array_size = 0;
	}
}

static int process_config_value(const char *key, const char *value)
{
	struct stat st;
	char *fs_name;
	char *fs_opts;

	if (strcmp(key, config_unmount_on_exit) == 0)
	{
		if (strcmp(value, config_unmount_on_exit_yes) == 0)
		{
			unmount_on_exit = 1;
			return 1;
		}
		else if (strcmp(value, config_unmount_on_exit_no) == 0)
		{
			unmount_on_exit = 0;
			return 1;
		}
	}
	else if (strcmp(key, config_mount_by) == 0)
	{
		if (strcmp(value, config_mount_by_name) == 0)
		{
			mount_by_value = mount_by_device_name;
			return 1;
		}
		else if (strcmp(value, config_mount_by_label) == 0)
		{
			mount_by_value = mount_by_device_label;
			return 1;
		}
	}
	else if (strcmp(key, config_mount_dir) == 0)
	{
		if ((stat(value, &st) == 0) && (S_ISDIR(st.st_mode)))
		{
			if (mount_dir != NULL)
			{
				free(mount_dir);
			}

			mount_dir = strdup(value);
			if (mount_dir != NULL)
			{
				return 1;
			}
		}
	}
	else if (strncmp(key, config_default_mount_opts, strlen(config_default_mount_opts)) == 0)
	{
		if (strlen(key) > strlen(config_default_mount_opts))
		{
			if ((strlen(value) > 1)
				&& (value[0] == '\"')
				&& (value[strlen(value) - 1] == '\"'))
			{
				fs_name = strdup(&(key[strlen(config_default_mount_opts)]));

				if (fs_name != NULL)
				{
					fs_opts = malloc(strlen(value) - 1);
					if (fs_opts != NULL)
					{
						memcpy(fs_opts, &(value[1]), strlen(value) - 2);
						fs_opts[strlen(value) - 2] = 0;

						switch (dtmd_fsopts_generate_string(fs_opts, fs_name,
							NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL))
						{
						case dtmd_fsopts_internal_mount:
						case dtmd_fsopts_external_mount:
							return insert_mount_opts_into_array(fs_name, fs_opts);

						case dtmd_fsopts_error:
						case dtmd_fsopts_not_supported:
						default:
							free(fs_name);
							free(fs_opts);
							return -1;
						}
					}
					else
					{
						free(fs_name);
					}
				}
			}
		}
	}

	return -1;
}

int read_config(void)
{
	FILE *file;
	char *buffer = NULL;
	size_t buffer_size = 0;
	ssize_t read_size;
	ssize_t i;
	ssize_t key_start, key_end;
	ssize_t equal_start;
	ssize_t value_start, value_end;
	int inside_quotes = 0;
	int rc = read_config_return_ok;
	int line_num = 0;

	file = fopen(config_filename, "r");
	if (file == NULL)
	{
		return read_config_return_no_file;
	}

	while ((read_size = getline(&buffer, &buffer_size, file)) > 0)
	{
		++line_num;

		key_start   = key_end   = -1;
		equal_start = -1;
		value_start = value_end = -1;

		for (i = 0; i < read_size; ++i)
		{
			if ((isalnum(buffer[i]))
				|| (buffer[i] == '_')
				|| (buffer[i] == '-')
				|| (buffer[i] == '.')
				|| (buffer[i] == '/')
				|| (buffer[i] == '\"')
				|| ((inside_quotes != 0)
					&& ((buffer[i] == ',')
						|| (buffer[i] == '='))))
			{
				if (buffer[i] == '\"')
				{
					inside_quotes = !inside_quotes;
				}

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
					rc = line_num;
					goto read_config_exit;
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
					rc = line_num;
					goto read_config_exit;
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
				else if (value_start != -1)
				{
					// error on line
					rc = line_num;
					goto read_config_exit;
				}
			}
			else
			{
				// error on line
				rc = line_num;
				goto read_config_exit;
			}
		}

		if (value_end != -1)
		{
			buffer[key_end] = 0;
			buffer[value_end] = 0;

			if (process_config_value(&(buffer[key_start]), &(buffer[value_start])) < 0)
			{
				rc = line_num;
				goto read_config_exit;
			}
		}
	}

read_config_exit:
	fclose(file);

	if (buffer != NULL)
	{
		free(buffer);
	}

	return rc;
}

void free_config(void)
{
	if (mount_dir != NULL)
	{
		free(mount_dir);
		mount_dir = NULL;
	}

	free_mount_opts_array();
}

const char* get_mount_options_for_fs_from_config(const char *fstype)
{
	unsigned int item = 0;

	if (default_mount_opts_array != NULL)
	{
		for ( ; item < default_mount_opts_array_size; ++item)
		{
			if ((default_mount_opts_array[item] != NULL)
				&& (default_mount_opts_array[item]->fs_type != NULL)
				&& (default_mount_opts_array[item]->opts != NULL)
				&& (strcmp(fstype, default_mount_opts_array[item]->fs_type) == 0))
			{
				return default_mount_opts_array[item]->opts;
			}
		}
	}

	return NULL;
}
