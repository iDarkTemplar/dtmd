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

#ifndef DT_TESTS_H
#define DT_TESTS_H

#include <stdio.h>

#define tests_init(); static int tests_result_value = 0; \
static int tests_on_error_quit_value = 0;

#define tests_result() tests_result_value

#define tests_quit_on_error(bool); tests_on_error_quit_value = bool;

#define test_compare(test) \
if (!(test)) \
{ \
	printf("line %d, test failed: %s\n",__LINE__, #test ); \
	if (tests_on_error_quit_value) \
	{ \
		return -1; \
	} \
	else \
	{ \
		tests_result_value = -1; \
	} \
}

#define test_compare_comment(test, comment) \
if (!(test)) \
{ \
	printf("line %d, test failed: %s\n",__LINE__, #test ); \
	printf("Comment: %s\n", comment); \
	if (tests_on_error_quit_value) \
	{ \
		return -1; \
	} \
	else \
	{ \
		tests_result_value = -1; \
	} \
}

#endif /* DT_TESTS_H */
