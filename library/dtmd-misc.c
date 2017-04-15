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

#include <string.h>

const char* dtmd_device_type_to_string(dtmd_removable_media_type_t type)
{
	switch (type)
	{
	case dtmd_removable_media_type_device_partition:
		return dtmd_string_device_type_device_partition;

	case dtmd_removable_media_type_stateless_device:
		return dtmd_string_device_type_stateless_device;

	case dtmd_removable_media_type_stateful_device:
		return dtmd_string_device_type_stateful_device;

	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		return dtmd_string_device_type_unknown_or_persistent;
	}
}

dtmd_removable_media_type_t dtmd_string_to_device_type(const char *string)
{
	if (string != NULL)
	{
		if (strcmp(string, dtmd_string_device_type_device_partition) == 0)
		{
			return dtmd_removable_media_type_device_partition;
		}
		else if (strcmp(string, dtmd_string_device_type_stateless_device) == 0)
		{
			return dtmd_removable_media_type_stateless_device;
		}
		else if (strcmp(string, dtmd_string_device_type_stateful_device) == 0)
		{
			return dtmd_removable_media_type_stateful_device;
		}
	}

	return dtmd_removable_media_type_unknown_or_persistent;
}

const char* dtmd_device_subtype_to_string(dtmd_removable_media_subtype_t subtype)
{
	switch (subtype)
	{
	case dtmd_removable_media_subtype_cdrom:
		return dtmd_string_device_subtype_cdrom;

	case dtmd_removable_media_subtype_removable_disk:
		return dtmd_string_device_subtype_removable_disk;

	case dtmd_removable_media_subtype_sd_card:
		return dtmd_string_device_subtype_sd_card;

	case dtmd_removable_media_subtype_unknown_or_persistent:
	default:
		return dtmd_string_device_subtype_unknown_or_persistent;
	}
}

dtmd_removable_media_subtype_t dtmd_string_to_device_subtype(const char *string)
{
	if (string != NULL)
	{
		if (strcmp(string, dtmd_string_device_subtype_cdrom) == 0)
		{
			return dtmd_removable_media_subtype_cdrom;
		}
		else if (strcmp(string, dtmd_string_device_subtype_removable_disk) == 0)
		{
			return dtmd_removable_media_subtype_removable_disk;
		}
		else if (strcmp(string, dtmd_string_device_subtype_sd_card) == 0)
		{
			return dtmd_removable_media_subtype_sd_card;
		}
	}

	return dtmd_removable_media_subtype_unknown_or_persistent;
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

	case dtmd_error_code_no_such_removable_device:
		return dtmd_string_error_code_no_such_removable_device;

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
		return dtmd_string_error_code_mount_point_busy;

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
		else if (strcmp(string, dtmd_string_error_code_no_such_removable_device) == 0)
		{
			return dtmd_error_code_no_such_removable_device;
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

dtmd_removable_media_t* dtmd_find_media(const char *path, dtmd_removable_media_t *root_node)
{
	dtmd_removable_media_t *result = NULL;
	dtmd_removable_media_t *iter_media_ptr = NULL;

	for (iter_media_ptr = root_node; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		if (strcmp(path, iter_media_ptr->path) == 0)
		{
			return iter_media_ptr;
		}

		result = dtmd_find_media(path, iter_media_ptr->first_child);
		if (result != NULL)
		{
			return result;
		}
	}

	return NULL;
}
