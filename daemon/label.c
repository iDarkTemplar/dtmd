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

#include "daemon/label.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/*
	\a – Bell (beep)
	\b – Backspace
	\f – Formfeed
	\n – New line
	\r – Carriage return
	\t – Horizontal tab
	\\ – Backslash
	\' – Single quotation mark
	\" – Double quotation mark
	\ooo – Octal representation
	\xdd – Hexadecimal representation
*/

char* decode_label(const char *label)
{
	char *result;
	char *cur_result;
	int i;
	int k;

	result = malloc(strlen(label)+1);
	if (result == NULL)
	{
		return NULL;
	}

	cur_result = result;

	while (*label)
	{
		if ((*label) == '\\')
		{
			++label;

			if ((*label) == 0)
			{
				free(result);
				return NULL;
			}

			switch (*label)
			{
#ifdef DTMD_MISC_DECODE_CONTROL_CHARS
			case 'a':
				*cur_result = '\a';
				break;

			case 'b':
				*cur_result = '\b';
				break;

			case 'f':
				*cur_result = '\f';
				break;

			case 'n':
				*cur_result = '\n';
				break;

			case 'r':
				*cur_result = '\r';
				break;

			case 't':
				*cur_result = '\t';
				break;

			case '\\':
				*cur_result = '\\';
				break;

			case '\'':
				*cur_result = '\'';
				break;

			case '\"':
				*cur_result = '\"';
				break;
#endif /* DTMD_MISC_DECODE_CONTROL_CHARS */

			case 'x':
				k = 0;

				for (i = 1; i < 3; ++i)
				{
					if (!isxdigit(label[i]))
					{
						free(result);
						return NULL;
					}

					k *= 16;

					if ((label[i] >= '0') && (label[i] <= '9'))
					{
						k += label[i] - '0';
					}
					else
					{
						k += tolower(label[i]) - 'a' + 10;
					}
				}

				*cur_result = k;
				label += 2;
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				k = 0;

				for (i = 0; i < 3; ++i)
				{
					if ((label[i] < '0') || (label[i] > '7'))
					{
						free(result);
						return NULL;
					}

					k *= 8;
					k += label[i] - '0';
				}

				*cur_result = k;
				label += 2;
				break;

			default:
#ifdef DTMD_MISC_DECODE_CONTROL_CHARS
				*cur_result = '\\';
				++cur_result;
				*cur_result = *label;
#else /* DTMD_MISC_DECODE_CONTROL_CHARS */
				free(result);
				return NULL;
#endif /* DTMD_MISC_DECODE_CONTROL_CHARS */
			}
		}
		else
		{
			*cur_result = *label;
		}

		++cur_result;
		++label;
	}

	*cur_result = 0;

	return result;
}
