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

#if (defined OS_FreeBSD)
#define _WITH_GETLINE
#endif /* (defined OS_FreeBSD) */

#include "daemon/config_file.h"
#include "daemon/filesystem_opts.h"
#include "daemon/return_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef CONFIG_DIR
#error CONFIG_DIR is not defined
#endif /* CONFIG_DIR */

#define config_filename CONFIG_DIR "/dtmd.conf"

int daemonize = 1;
int use_syslog = 1;
int unmount_on_exit = 0;
enum mount_by_value_enum mount_by_value = mount_by_device_name;
char *mount_dir = NULL;
int create_mount_dir_on_startup = 0;
int clear_mount_dir = 1;

struct config_mount_opts
{
	char *fs_type;
	char *opts;
};

static struct config_mount_opts **default_mount_opts_array = NULL;
static size_t default_mount_opts_array_size = 0;

static struct config_mount_opts **mandatory_mount_opts_array = NULL;
static size_t mandatory_mount_opts_array_size = 0;

static const char *config_yes = "yes";
static const char *config_no = "no";

static const char *config_unmount_on_exit = "unmount_on_exit";

static const char *config_mount_by = "mount_by";
static const char *config_mount_by_name = "name";
static const char *config_mount_by_label = "label";

static const char *config_use_syslog = "use_syslog";

static const char *config_mount_dir = "mount_dir";

static const char *config_create_mount_dir_on_startup = "create_mount_dir";

static const char *config_clear_mount_dir = "clear_mount_dir";

static const char *config_default_mount_opts = "default_mount_opts_";

static const char *config_mandatory_mount_opts = "mandatory_mount_opts_";

/* TODO: similar functions for default and mandatory configs. generalize them */
static int insert_default_mount_opts_into_array(char *fs_name, char *fs_opts)
{
	void *tmp;
	struct config_mount_opts *array_item;
	size_t item = 0;

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

		return result_fail;
	}
	else
	{
		array_item = (struct config_mount_opts*) malloc(sizeof(struct config_mount_opts));
		if (array_item == NULL)
		{
			goto insert_default_mount_opts_into_array_error_1;
		}

		tmp = realloc(default_mount_opts_array, (default_mount_opts_array_size + 1) * (sizeof(struct config_mount_opts*)));
		if (tmp == NULL)
		{
			goto insert_default_mount_opts_into_array_error_2;
		}

		default_mount_opts_array = (struct config_mount_opts**) tmp;
		++default_mount_opts_array_size;
		array_item->fs_type = fs_name;
		array_item->opts = fs_opts;
		default_mount_opts_array[default_mount_opts_array_size - 1] = array_item;

		return result_success;
	}

insert_default_mount_opts_into_array_error_2:
	free(array_item);

insert_default_mount_opts_into_array_error_1:
	free(fs_name);
	free(fs_opts);

	return result_fatal_error;
}

static int insert_mandatory_mount_opts_into_array(char *fs_name, char *fs_opts)
{
	void *tmp;
	struct config_mount_opts *array_item;
	size_t item = 0;

	for ( ; item < mandatory_mount_opts_array_size; ++item)
	{
		if (strcmp(fs_name, mandatory_mount_opts_array[item]->fs_type) == 0)
		{
			break;
		}
	}

	if (item < mandatory_mount_opts_array_size)
	{
		free(fs_name);
		free(mandatory_mount_opts_array[item]->opts);
		mandatory_mount_opts_array[item]->opts = fs_opts;

		return result_fail;
	}
	else
	{
		array_item = (struct config_mount_opts*) malloc(sizeof(struct config_mount_opts));
		if (array_item == NULL)
		{
			goto insert_mandatory_mount_opts_into_array_error_1;
		}

		tmp = realloc(mandatory_mount_opts_array, (mandatory_mount_opts_array_size + 1) * (sizeof(struct config_mount_opts*)));
		if (tmp == NULL)
		{
			goto insert_mandatory_mount_opts_into_array_error_2;
		}

		mandatory_mount_opts_array = (struct config_mount_opts**) tmp;
		++mandatory_mount_opts_array_size;
		array_item->fs_type = fs_name;
		array_item->opts = fs_opts;
		mandatory_mount_opts_array[mandatory_mount_opts_array_size - 1] = array_item;

		return result_success;
	}

insert_mandatory_mount_opts_into_array_error_2:
	free(array_item);

insert_mandatory_mount_opts_into_array_error_1:
	free(fs_name);
	free(fs_opts);

	return result_fatal_error;
}

static void free_default_mount_opts_array(void)
{
	size_t item = 0;

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

static void free_mandatory_mount_opts_array(void)
{
	size_t item = 0;

	if (mandatory_mount_opts_array != NULL)
	{
		for ( ; item < mandatory_mount_opts_array_size; ++item)
		{
			if (mandatory_mount_opts_array[item] != NULL)
			{
				if (mandatory_mount_opts_array[item]->fs_type != NULL)
				{
					free(mandatory_mount_opts_array[item]->fs_type);
				}

				if (mandatory_mount_opts_array[item]->opts != NULL)
				{
					free(mandatory_mount_opts_array[item]->opts);
				}

				free(mandatory_mount_opts_array[item]);
			}
		}

		free(mandatory_mount_opts_array);
		mandatory_mount_opts_array = NULL;
		mandatory_mount_opts_array_size = 0;
	}
}

static int process_config_value(const char *key, const char *value)
{
	char *fs_name;
	char *fs_opts;
	int result;
	const struct dtmd_filesystem_options *fsopts_type;
	dtmd_fsopts_list_t fsopts_list;

	if (strcmp(key, config_unmount_on_exit) == 0)
	{
		if (strcmp(value, config_yes) == 0)
		{
			unmount_on_exit = 1;
			return result_success;
		}
		else if (strcmp(value, config_no) == 0)
		{
			unmount_on_exit = 0;
			return result_success;
		}
	}
	else if (strcmp(key, config_mount_by) == 0)
	{
		if (strcmp(value, config_mount_by_name) == 0)
		{
			mount_by_value = mount_by_device_name;
			return result_success;
		}
		else if (strcmp(value, config_mount_by_label) == 0)
		{
			mount_by_value = mount_by_device_label;
			return result_success;
		}
	}
	if (strcmp(key, config_use_syslog) == 0)
	{
		if (strcmp(value, config_yes) == 0)
		{
			use_syslog = 1;
			return result_success;
		}
		else if (strcmp(value, config_no) == 0)
		{
			use_syslog = 0;
			return result_success;
		}
	}
	else if (strcmp(key, config_mount_dir) == 0)
	{
		if ((strlen(value) > 1)
			&& (value[0] == '\"')
			&& (value[strlen(value) - 1] == '\"'))
		{
			if (mount_dir != NULL)
			{
				free(mount_dir);
			}

			mount_dir = (char*) malloc(strlen(value) - 1);
			if (mount_dir != NULL)
			{
				memcpy(mount_dir, &(value[1]), strlen(value) - 2);
				mount_dir[strlen(value) - 2] = 0;

				return result_success;
			}

			return result_fatal_error;
		}
	}
	else if (strcmp(key, config_create_mount_dir_on_startup) == 0)
	{
		if (strcmp(value, config_yes) == 0)
		{
			create_mount_dir_on_startup = 1;
			return result_success;
		}
		else if (strcmp(value, config_no) == 0)
		{
			create_mount_dir_on_startup = 0;
			return result_success;
		}
	}
	else if (strcmp(key, config_clear_mount_dir) == 0)
	{
		if (strcmp(value, config_yes) == 0)
		{
			clear_mount_dir = 1;
			return result_success;
		}
		else if (strcmp(value, config_no) == 0)
		{
			clear_mount_dir = 0;
			return result_success;
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
				result = result_fatal_error;

				fs_name = strdup(&(key[strlen(config_default_mount_opts)]));

				if (fs_name != NULL)
				{
					fs_opts = (char*) malloc(strlen(value) - 1);
					if (fs_opts != NULL)
					{
						memcpy(fs_opts, &(value[1]), strlen(value) - 2);
						fs_opts[strlen(value) - 2] = 0;

						fsopts_type = get_fsopts_for_fs(fs_name);
						if (fsopts_type != NULL)
						{
							init_options_list(&fsopts_list);
							result = convert_options_to_list(fs_opts, fsopts_type, NULL, NULL, &fsopts_list);
							free_options_list(&fsopts_list);

							if (is_result_successful(result))
							{
								return insert_default_mount_opts_into_array(fs_name, fs_opts);
							}
						}

						free(fs_opts);
					}

					free(fs_name);
				}

				return result;
			}
		}
	}
	else if (strncmp(key, config_mandatory_mount_opts, strlen(config_mandatory_mount_opts)) == 0)
	{
		if (strlen(key) > strlen(config_mandatory_mount_opts))
		{
			if ((strlen(value) > 1)
				&& (value[0] == '\"')
				&& (value[strlen(value) - 1] == '\"'))
			{
				result = result_fatal_error;

				fs_name = strdup(&(key[strlen(config_mandatory_mount_opts)]));

				if (fs_name != NULL)
				{
					fs_opts = (char*) malloc(strlen(value) - 1);
					if (fs_opts != NULL)
					{
						memcpy(fs_opts, &(value[1]), strlen(value) - 2);
						fs_opts[strlen(value) - 2] = 0;

						fsopts_type = get_fsopts_for_fs(fs_name);
						if (fsopts_type != NULL)
						{
							init_options_list(&fsopts_list);
							result = convert_options_to_list(fs_opts, fsopts_type, NULL, NULL, &fsopts_list);
							free_options_list(&fsopts_list);

							if (is_result_successful(result))
							{
								return insert_mandatory_mount_opts_into_array(fs_name, fs_opts);
							}
						}

						free(fs_opts);
					}

					free(fs_name);
				}

				return result;
			}
		}
	}

	return result_fail;
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

			if (is_result_failure(process_config_value(&(buffer[key_start]), &(buffer[value_start]))))
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

	free_default_mount_opts_array();
	free_mandatory_mount_opts_array();
}

static const char* generic_get_mount_options_for_fs_from_config(const char *fstype, struct config_mount_opts **mount_opts_array, size_t mount_opts_array_size)
{
	size_t item = 0;

	if (mount_opts_array != NULL)
	{
		for ( ; item < mount_opts_array_size; ++item)
		{
			if ((mount_opts_array[item] != NULL)
				&& (mount_opts_array[item]->fs_type != NULL)
				&& (mount_opts_array[item]->opts != NULL)
				&& (strcmp(fstype, mount_opts_array[item]->fs_type) == 0))
			{
				return mount_opts_array[item]->opts;
			}
		}
	}

	return NULL;
}

const char* get_default_mount_options_for_fs_from_config(const char *fstype)
{
	return generic_get_mount_options_for_fs_from_config(fstype, default_mount_opts_array, default_mount_opts_array_size);
}

const char* get_mandatory_mount_options_for_fs_from_config(const char *fstype)
{
	return generic_get_mount_options_for_fs_from_config(fstype, mandatory_mount_opts_array, mandatory_mount_opts_array_size);
}
