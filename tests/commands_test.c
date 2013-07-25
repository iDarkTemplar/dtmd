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
#include "library/dtmd-commands.h"
#include "tests/dt_tests.h"

void print_command(struct dtmd_command *cmd)
{
	int i;

	printf("command: %s\n", cmd->cmd);

	for (i = 0; i < cmd->args_count; ++i)
	{
		if (cmd->args[i] != NULL)
		{
			printf("arg %d: \"%s\"\n", i+1, cmd->args[i]);
		}
		else
		{
			printf("arg %d: nil\n", i+1);
		}
	}
}

void print_and_free(struct dtmd_command *cmd)
{
	if (cmd != NULL)
	{
		print_command(cmd);
		dtmd_free_command(cmd);
	}
}

int main(int argc, char **argv)
{
	unsigned char *cmd1 = (unsigned char*) "cmd()\n";
	unsigned char *cmd2 = (unsigned char*) "cmd2(\"arg1\", \"arg2\", \"arg3\")\n";
	unsigned char *cmd3 = (unsigned char*) "cmd3(arg1\", \"arg2\", \"arg3\")\n";
	unsigned char *cmd4 = (unsigned char*) "cmd4(\"arg1\" \"arg2\", \"arg3\")\n";
	unsigned char *cmd5 = (unsigned char*) "cmd5(\"arg1\")\n";
	unsigned char *cmd6 = (unsigned char*) "cmd_6(\"arg1\")\n";
	unsigned char *cmd7 = (unsigned char*) "cmd_7(\"\")\n";
	unsigned char *cmd8 = (unsigned char*) "cmd_8(nil)\n";
	unsigned char *cmd9 = (unsigned char*) "cmd_9(\"arg1\", nil, \"arg3\")\n";
	unsigned char *cmd10 = (unsigned char*) "cmd10(nil \"arg3\")\n";
	unsigned char *cmd11 = (unsigned char*) "(\"arg1\")\n";

	struct dtmd_command *cmd1_res;
	struct dtmd_command *cmd2_res;
	struct dtmd_command *cmd3_res;
	struct dtmd_command *cmd4_res;
	struct dtmd_command *cmd5_res;
	struct dtmd_command *cmd6_res;
	struct dtmd_command *cmd7_res;
	struct dtmd_command *cmd8_res;
	struct dtmd_command *cmd9_res;
	struct dtmd_command *cmd10_res;
	struct dtmd_command *cmd11_res;

	tests_init();

	(void)argc;
	(void)argv;

	test_compare((cmd1_res = dtmd_parse_command(cmd1)) != NULL);
	test_compare((cmd2_res = dtmd_parse_command(cmd2)) != NULL);
	test_compare((cmd3_res = dtmd_parse_command(cmd3)) == NULL);
	test_compare((cmd4_res = dtmd_parse_command(cmd4)) == NULL);
	test_compare((cmd5_res = dtmd_parse_command(cmd5)) != NULL);
	test_compare((cmd6_res = dtmd_parse_command(cmd6)) != NULL);
	test_compare((cmd7_res = dtmd_parse_command(cmd7)) != NULL);
	test_compare((cmd8_res = dtmd_parse_command(cmd8)) != NULL);
	test_compare((cmd9_res = dtmd_parse_command(cmd9)) != NULL);
	test_compare((cmd10_res = dtmd_parse_command(cmd10)) == NULL);
	test_compare((cmd11_res = dtmd_parse_command(cmd11)) == NULL);

	print_and_free(cmd1_res);
	print_and_free(cmd2_res);
	print_and_free(cmd3_res);
	print_and_free(cmd4_res);
	print_and_free(cmd5_res);
	print_and_free(cmd6_res);
	print_and_free(cmd7_res);
	print_and_free(cmd8_res);
	print_and_free(cmd9_res);
	print_and_free(cmd10_res);
	print_and_free(cmd11_res);

	return tests_result();
}
