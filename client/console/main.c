/*
 * Copyright (C) 2016-2019 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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

#include <dtmd-library.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

volatile int run = 1;

void client_sighandler(int signum)
{
	run = 0;
}

int first = 1;

#define print_first(var) \
	if (var) \
	{ \
		(var) = 0; \
	} \
	else \
	{ \
		fprintf(stdout, "\n"); \
	}

void helper_print_device(const dtmd_removable_media_t *media_ptr)
{
	switch (media_ptr->type)
	{
	case dtmd_removable_media_type_device_partition:
	case dtmd_removable_media_type_stateless_device:
	case dtmd_removable_media_type_stateful_device:
		break;

	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		return;
	}

	fprintf(stdout, "Path: %s\n", media_ptr->path);
	fprintf(stdout, "Type: %s\n", dtmd_device_type_to_string(media_ptr->type));

	switch (media_ptr->type)
	{
	case dtmd_removable_media_type_stateless_device:
		fprintf(stdout, "Subtype: %s\n", dtmd_device_subtype_to_string(media_ptr->subtype));
		break;

	case dtmd_removable_media_type_stateful_device:
		fprintf(stdout, "Subtype: %s\n", dtmd_device_subtype_to_string(media_ptr->subtype));
		fprintf(stdout, "State: %s\n", dtmd_device_state_to_string(media_ptr->state));
		// NOTE: fallthrough

	case dtmd_removable_media_type_device_partition:
		if (media_ptr->fstype != NULL)
		{
			fprintf(stdout, "Filesystem type: %s\n", media_ptr->fstype);
		}

		if (media_ptr->label != NULL)
		{
			fprintf(stdout, "Label: %s\n", media_ptr->label);
		}

		if (media_ptr->mnt_point != NULL)
		{
			fprintf(stdout, "Mount point: %s\n", media_ptr->mnt_point);
		}

		if (media_ptr->mnt_opts != NULL)
		{
			fprintf(stdout, "Mount options: %s\n", media_ptr->mnt_opts);
		}
		break;

	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		break;
	}
}

void helper_callback_print_device(dtmd_t *library, const dt_command_t *cmd)
{
	dtmd_result_t result;
	dtmd_removable_media_t *device;

	result = dtmd_fill_removable_device_from_notification(library, cmd, dtmd_fill_link, &device);
	if (result == dtmd_ok)
	{
		helper_print_device(device);
		dtmd_free_removable_devices(library, device);
	}
}

void client_callback(dtmd_t *library, void *arg, const dt_command_t *cmd)
{
	if (arg == (void*)1)
	{
		if ((strcmp(cmd->cmd, dtmd_notification_removable_device_added) == 0) && (dtmd_is_notification_valid_removable_device(library, cmd)))
		{
			print_first(first);
			fprintf(stdout, "Device added\n");
			helper_callback_print_device(library, cmd);
		}
		else if ((strcmp(cmd->cmd, dtmd_notification_removable_device_removed) == 0) && (cmd->args_count == 1)
			&& (cmd->args[0] != NULL))
		{
			print_first(first);
			fprintf(stdout, "Device removed\nPath: %s\n", cmd->args[0]);
		}
		else if ((strcmp(cmd->cmd, dtmd_notification_removable_device_changed) == 0) && (dtmd_is_notification_valid_removable_device(library, cmd)))
		{
			print_first(first);
			fprintf(stdout, "Device changed\n");
			helper_callback_print_device(library, cmd);
		}
		else if ((strcmp(cmd->cmd, dtmd_notification_removable_device_mounted) == 0) && (cmd->args_count == 3)
			&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL) && (cmd->args[2] != NULL))
		{
			print_first(first);
			fprintf(stdout, "Device mounted\nPath: %s\nMount point: %s\nMount options: %s\n", cmd->args[0], cmd->args[1], cmd->args[2]);
		}
		else if ((strcmp(cmd->cmd, dtmd_notification_removable_device_unmounted) == 0) && (cmd->args_count == 2)
			&& (cmd->args[0] != NULL) && (cmd->args[1] != NULL))
		{
			print_first(first);
			fprintf(stdout, "Device unmounted\nPath: %s\nMount point: %s\n", cmd->args[0], cmd->args[1]);
		}
	}
}

void client_state_callback(dtmd_t *library, void *arg, dtmd_state_t state)
{
	switch (state)
	{
	case dtmd_state_connected:
		fprintf(stdout, "Connected to dtmd daemon\n");
		break;

	case dtmd_state_disconnected:
		fprintf(stdout, "Disconnected from dtmd daemon\n");
		break;

	case dtmd_state_failure:
		fprintf(stdout, "Got exit signal from daemon\n");
		run = 0;
		break;

	default:
		fprintf(stdout, "Got unknown state from daemon\n");
		run = 0;
		break;
	}
}

void printUsage(char *app)
{
	fprintf(stderr, "USAGE: %s command\n"
		"\twhere command is one of following:\n"
		"\t\tlist_all\n"
		"\t\tlist path\n"
		"\t\tmount device [ mount_options ]\n"
		"\t\tunmount device\n"
		"\t\tls_fs\n"
		"\t\tls_fs_opts filesystem\n"
#if (defined OS_Linux)
		"\t\tpoweroff device\n"
#endif /* (defined OS_Linux) */
		"\t\tmonitor\n", app);
}

void print_device_recursive(dtmd_removable_media_t *media_ptr, int *printing_first_device)
{
	dtmd_removable_media_t *iter_media_ptr;

	print_first(*printing_first_device);

	helper_print_device(media_ptr);

	for (iter_media_ptr = media_ptr->children_list; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		print_device_recursive(iter_media_ptr, printing_first_device);
	}
}

void print_devices(dtmd_removable_media_t *media_ptr, int *printing_first_device)
{
	dtmd_removable_media_t *iter_media_ptr;

	for (iter_media_ptr = media_ptr; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		print_device_recursive(iter_media_ptr, printing_first_device);
	}
}

size_t count_devices_recursive(dtmd_removable_media_t *media_ptr)
{
	size_t result = 0;
	dtmd_removable_media_t *iter_media_ptr;

	switch (media_ptr->type)
	{
	case dtmd_removable_media_type_stateless_device:
	case dtmd_removable_media_type_stateful_device:
		++result;
		break;

	case dtmd_removable_media_type_device_partition:
	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		break;
	}

	for (iter_media_ptr = media_ptr->children_list; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		result += count_devices_recursive(iter_media_ptr);
	}

	return result;
}

size_t count_devices(dtmd_removable_media_t *media_ptr)
{
	size_t result = 0;
	dtmd_removable_media_t *iter_media_ptr;

	for (iter_media_ptr = media_ptr; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		result += count_devices_recursive(iter_media_ptr);
	}

	return result;
}

int client_list_all_removable_devices(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	dtmd_removable_media_t *devices;
	size_t devices_count;
	int printing_first_device = 1;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_all_removable_devices(lib, dtmd_library_timeout_infinite, &devices);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't enumerate devices, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	devices_count = count_devices(devices);

	fprintf(stdout, "Found %zu devices\n\n", devices_count);

	print_devices(devices, &printing_first_device);

	dtmd_free_removable_devices(lib, devices);
	dtmd_deinit(lib);

	return 0;
}

int client_list_removable_device(const char *path)
{
	dtmd_t *lib;
	dtmd_result_t result;
	dtmd_removable_media_t *devices;
	int printing_first_device = 1;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_removable_device(lib, dtmd_library_timeout_infinite, path, &devices);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list device, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	print_devices(devices, &printing_first_device);

	dtmd_free_removable_devices(lib, devices);
	dtmd_deinit(lib);

	return 0;
}

int client_mount(char *device, char *mount_opts)
{
	dtmd_t *lib;
	dtmd_result_t result;
	int func_result = 0;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_mount(lib, dtmd_library_timeout_infinite, device, mount_opts);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Mount successful!\n");
	}
	else
	{
		fprintf(stdout, "Mount failed, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}

int client_unmount(char *device)
{
	dtmd_t *lib;
	dtmd_result_t result;
	int func_result = 0;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_unmount(lib, dtmd_library_timeout_infinite, device);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Unmount successful!\n");
	}
	else
	{
		fprintf(stdout, "Unmount failed, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}

int client_list_supported_filesystems(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	size_t count, i;
	const char **filesystems;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_supported_filesystems(lib, dtmd_library_timeout_infinite, &count, &filesystems);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list supported filesystems, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Got %zu supported filesystems:\n", count);

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "\t%s\n", filesystems[i]);
	}

	dtmd_free_supported_filesystems_list(lib, count, filesystems);
	dtmd_deinit(lib);

	return 0;
}

int client_list_supported_filesystem_options(const char *filesystem)
{
	dtmd_t *lib;
	dtmd_result_t result;
	size_t count, i;
	const char **options_list;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_list_supported_filesystem_options(lib, dtmd_library_timeout_infinite,filesystem, &count, &options_list);
	if (result != dtmd_ok)
	{
		fprintf(stderr, "Couldn't list supported filesystems, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		dtmd_deinit(lib);
		return -1;
	}

	fprintf(stdout, "Got %zu supported filesystem options for %s:\n", count, filesystem);

	for (i = 0; i < count; ++i)
	{
		fprintf(stdout, "\t%s\n", options_list[i]);
	}

	dtmd_free_supported_filesystem_options_list(lib, count, options_list);
	dtmd_deinit(lib);

	return 0;
}

#if (defined OS_Linux)
int client_poweroff(const char *device)
{
	dtmd_t *lib;
	dtmd_result_t result;
	int func_result = 0;

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)0, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	result = dtmd_poweroff(lib, dtmd_library_timeout_infinite, device);
	if (result == dtmd_ok)
	{
		fprintf(stdout, "Poweroff successful!\n");
	}
	else
	{
		fprintf(stdout, "Poweroff failed, error code %d, details: %s\n", result, dtmd_error_code_to_string(dtmd_get_code_of_command_fail(lib)));
		func_result = -1;
	}

	dtmd_deinit(lib);

	return func_result;
}
#endif /* (defined OS_Linux) */

int client_monitor(void)
{
	dtmd_t *lib;
	dtmd_result_t result;
	struct sigaction act;

	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = &client_sighandler;

	if ((sigaction(SIGINT, &act, NULL) != 0) || (sigaction(SIGTERM, &act, NULL) != 0))
	{
		fprintf(stderr, "Couldn't set signal handler\n");
		return -1;
	}

	lib = dtmd_init(&client_callback, &client_state_callback, (void*)1, &result);
	if (lib == NULL)
	{
		fprintf(stderr, "Couldn't initialize dtmd-library, error code: %d\n", result);
		return -1;
	}

	while (run)
	{
		sleep(1);
	}

	dtmd_deinit(lib);

	return 0;
}

int main(int argc, char **argv)
{
	if ((argc == 2) && (strcmp(argv[1], "list_all") == 0))
	{
		return client_list_all_removable_devices();
	}
	else if ((argc == 3) && (strcmp(argv[1], "list") == 0))
	{
		return client_list_removable_device(argv[2]);
	}
	else if (((argc == 3) || (argc == 4)) && (strcmp(argv[1], "mount") == 0))
	{
		return client_mount(argv[2], (argc == 4) ? (argv[3]) : (NULL));
	}
	else if ((argc == 3) && (strcmp(argv[1], "unmount") == 0))
	{
		return client_unmount(argv[2]);
	}
	else if ((argc == 2) && (strcmp(argv[1], "ls_fs") == 0))
	{
		return client_list_supported_filesystems();
	}
	else if ((argc == 3) && (strcmp(argv[1], "ls_fs_opts") == 0))
	{
		return client_list_supported_filesystem_options(argv[2]);
	}
#if (defined OS_Linux)
	else if ((argc == 3) && (strcmp(argv[1], "poweroff") == 0))
	{
		return client_poweroff(argv[2]);
	}
#endif /* (defined OS_Linux) */
	else if ((argc == 2) && (strcmp(argv[1], "monitor") == 0))
	{
		return client_monitor();
	}
	else
	{
		printUsage(argv[0]);
		return -1;
	}
}
