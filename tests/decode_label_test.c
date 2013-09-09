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
#include <dtmd-misc.h>
#include "tests/dt_tests.h"

void print_and_free(char *orig, char *label, char *expected)
{
	char *cur;

	printf("Orig:  %s\n\n", orig);

	if (expected != NULL)
	{
		printf("Expected:\n");
		cur = expected;
		printf("%d\n", *cur);

		while (*cur != 0)
		{
			++cur;
			printf("%d\n", *cur);
		}

		printf("\n");
	}
	else
	{
		printf("Expected: NULL\n\n");
	}

	if (label != NULL)
	{
		printf("Decoded:\n");
		cur = expected;
		printf("%d\n", *cur);

		while (*cur != 0)
		{
			++cur;
			printf("%d\n", *cur);
		}

		printf("\n");

		dtmd_free_decoded_label(label);
	}
	else
	{
		printf("Decoded: NULL\n\n");
	}
}

int main(int argc, char **argv)
{
	char *label_test1 = "test\\a\\b\\f\\n\\r\\t\\\\\\'\\\"\\123\\x20";
	char *label_test2 = "test\\x20\\040label";
	char *label_test3 = "label\\0a1fail";
	char *label_test4 = "label\\xfzfail";

	char *label_result1;
	char *label_result2;
	char *label_result3;
	char *label_result4;

#ifdef DTMD_MISC_DECODE_CONTROL_CHARS
	char *label_expected_result1 = "test\a\b\f\n\r\t\\'\"\123\x20";
#else /* DTMD_MISC_DECODE_CONTROL_CHARS */
	char *label_expected_result1 = NULL;
#endif /* DTMD_MISC_DECODE_CONTROL_CHARS */
	char *label_expected_result2 = "test  label";
	char *label_expected_result3 = NULL;
	char *label_expected_result4 = NULL;

	tests_init();

	(void)argc;
	(void)argv;

	test_compare((label_result1 = dtmd_decode_label(label_test1)) == NULL);
	test_compare(((label_result2 = dtmd_decode_label(label_test2)) != NULL) && (strcmp(label_result2, label_expected_result2) == 0));
	test_compare((label_result3 = dtmd_decode_label(label_test3)) == NULL);
	test_compare((label_result4 = dtmd_decode_label(label_test4)) == NULL);

	print_and_free(label_test1, label_result1, label_expected_result1);
	print_and_free(label_test2, label_result2, label_expected_result2);
	print_and_free(label_test3, label_result3, label_expected_result3);
	print_and_free(label_test4, label_result4, label_expected_result4);

	return tests_result();
}
