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

#include <stdlib.h>
#include <string.h>
#include "common/commands.h"
#include "tests/dt_tests.h"

void print_command(struct command *cmd)
{
	int i;

	printf("command: %s\n", cmd->cmd);

	for (i = 0; i < cmd->args_count; ++i)
	{
		printf("arg %d: \"%s\"\n", i+1, cmd->args[i]);
	}
}

int main(int argc, char **argv)
{
	tests_init();

	unsigned char *cmd1 = (unsigned char*) "cmd()\n";
	unsigned char *cmd2 = (unsigned char*) "cmd2(\"arg1\", \"arg2\", \"arg3\")\n";
	unsigned char *cmd3 = (unsigned char*) "cmd3(arg1\", \"arg2\", \"arg3\")\n";
	unsigned char *cmd4 = (unsigned char*) "cmd4(\"arg1\" \"arg2\", \"arg3\")\n";
	unsigned char *cmd5 = (unsigned char*) "cmd5(\"arg1\")\n";
	unsigned char *cmd6 = (unsigned char*) "cmd_6(\"arg1\")\n";
	unsigned char *cmd7 = (unsigned char*) "cmd_7(\"\")\n";

	struct command *cmd1_res;
	struct command *cmd2_res;
	struct command *cmd3_res;
	struct command *cmd4_res;
	struct command *cmd5_res;
	struct command *cmd6_res;
	struct command *cmd7_res;

	(void)argc;
	(void)argv;

	test_compare((cmd1_res = parse_command(cmd1)) != NULL);
	test_compare((cmd2_res = parse_command(cmd2)) != NULL);
	test_compare((cmd3_res = parse_command(cmd3)) == NULL);
	test_compare((cmd4_res = parse_command(cmd4)) == NULL);
	test_compare((cmd5_res = parse_command(cmd5)) != NULL);
	test_compare((cmd6_res = parse_command(cmd6)) != NULL);
	test_compare((cmd7_res = parse_command(cmd7)) != NULL);

	if (cmd1_res)
	{
		print_command(cmd1_res);
		free_command(cmd1_res);
	}

	if (cmd2_res)
	{
		print_command(cmd2_res);
		free_command(cmd2_res);
	}

	if (cmd3_res)
	{
		print_command(cmd3_res);
		free_command(cmd3_res);
	}

	if (cmd4_res)
	{
		print_command(cmd4_res);
		free_command(cmd4_res);
	}

	if (cmd5_res)
	{
		print_command(cmd5_res);
		free_command(cmd5_res);
	}

	if (cmd6_res)
	{
		print_command(cmd6_res);
		free_command(cmd6_res);
	}

	if (cmd7_res)
	{
		print_command(cmd7_res);
		free_command(cmd7_res);
	}

	return tests_result();
}
