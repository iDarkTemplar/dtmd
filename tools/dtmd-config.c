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

#include <dtmd.h>

#include <stdio.h>
#include <string.h>

enum app_action {
	action_unknown,
	action_print_help,
	action_print_pidfile,
	action_print_socket
};

void print_help(const char *appname, FILE *output)
{
	fprintf(output,
	"USAGE: %s option\n"
	"\twhere option is one of following:\n"
	"\t--help (-h) - print this help\n"
	"\t--pidfile (-p) - print path to pid file\n"
	"\t--socket (-s) - print path to socket\n",
	appname);
}

int main(int argc, char **argv)
{
	enum app_action act = action_unknown;

	if (argc == 2)
	{
		if ((strcmp(argv[1], "--help") == 0)
			|| (strcmp(argv[1], "-h") == 0))
		{
			act = action_print_help;
		}
		else if ((strcmp(argv[1], "--pidfile") == 0)
			|| (strcmp(argv[1], "-p") == 0))
		{
			act = action_print_pidfile;
		}
		else if ((strcmp(argv[1], "--socket") == 0)
			|| (strcmp(argv[1], "-s") == 0))
		{
			act = action_print_socket;
		}
	}

	switch (act)
	{
	case action_print_pidfile:
		fprintf(stdout, "%s\n", dtmd_daemon_lock);
		break;

	case action_print_socket:
		fprintf(stdout, "%s\n", dtmd_daemon_socket_addr);
		break;

	case action_print_help:
		print_help(argv[0], stdout);
		break;

	case action_unknown:
	default:
		print_help(argv[0], stderr);
		return -1;
		break;
	}

	return 0;
}
