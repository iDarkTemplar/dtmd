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
#include "daemon/label.h"
#include "tests/dt_tests.h"

void print_and_free(const char *orig, char *label, const char *expected)
{
	const char *cur;

	printf("Orig:  %s\n", orig);
	printf("Encoded: %s\n\n", label);

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
		cur = label;
		printf("%d\n", *cur);

		while (*cur != 0)
		{
			++cur;
			printf("%d\n", *cur);
		}

		printf("\n");

		free(label);
	}
	else
	{
		printf("Decoded: NULL\n\n");
	}
}

int main(int argc, char **argv)
{
	const char *label_test1 = "test\\a\\b\\f\\n\\r\\t\\\\\\'\\\"\\123\\x20";
	const char *label_test2 = "test\\x20\\040label\\z";
	const char *label_test3 = "label\\0a1fail";
	const char *label_test4 = "label\\xfzfail";
	const char *label_test5 = "test\\x00\\000label\\z";
	const char *label_test6 = "..";
	const char *label_test7 = "$(echo lol)";
	const char *label_test8 = "`echo lol`";
	const char *label_test9 = "../";

	char *label_result1;
	char *label_result2;
	char *label_result3;
	char *label_result4;
	char *label_result5;
	char *label_result6;
	char *label_result7;
	char *label_result8;
	char *label_result9;

	const char *label_expected_result1 = "test\\007\\010\\014\\012\\015\\011\\134\\047\\042S ";
	const char *label_expected_result2 = "test  label\\z";
	const char *label_expected_result3 = NULL;
	const char *label_expected_result4 = NULL;
	const char *label_expected_result5 = "test\\000\\000label\\z";
	const char *label_expected_result6 = "\\056\\056";
	const char *label_expected_result7 = "\\044\\050echo lol\\051";
	const char *label_expected_result8 = "\\140echo lol\\140";
	const char *label_expected_result9 = "\\056\\056\\057";

	tests_init();

	(void)argc;
	(void)argv;

	test_compare(((label_result1 = decode_label(label_test1)) != NULL) && (strcmp(label_result1, label_expected_result1) == 0));
	test_compare(((label_result2 = decode_label(label_test2)) != NULL) && (strcmp(label_result2, label_expected_result2) == 0));
	test_compare((label_result3 = decode_label(label_test3)) == NULL);
	test_compare((label_result4 = decode_label(label_test4)) == NULL);
	test_compare(((label_result5 = decode_label(label_test5)) != NULL) && (strcmp(label_result5, label_expected_result5) == 0));
	test_compare(((label_result6 = decode_label(label_test6)) != NULL) && (strcmp(label_result6, label_expected_result6) == 0));
	test_compare(((label_result7 = decode_label(label_test7)) != NULL) && (strcmp(label_result7, label_expected_result7) == 0));
	test_compare(((label_result8 = decode_label(label_test8)) != NULL) && (strcmp(label_result8, label_expected_result8) == 0));
	test_compare(((label_result9 = decode_label(label_test9)) != NULL) && (strcmp(label_result9, label_expected_result9) == 0));

	print_and_free(label_test1, label_result1, label_expected_result1);
	print_and_free(label_test2, label_result2, label_expected_result2);
	print_and_free(label_test3, label_result3, label_expected_result3);
	print_and_free(label_test4, label_result4, label_expected_result4);
	print_and_free(label_test5, label_result5, label_expected_result5);
	print_and_free(label_test6, label_result6, label_expected_result6);
	print_and_free(label_test7, label_result7, label_expected_result7);
	print_and_free(label_test8, label_result8, label_expected_result8);
	print_and_free(label_test9, label_result9, label_expected_result9);

	return tests_result();
}
