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

	result = malloc((strlen(label)*4)+1);
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
			case 'a':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\a' & 0700) >> 6);
				cur_result[2] = '0' + (('\a' &  070) >> 3);
				cur_result[3] = '0' + ( '\a' &   07);
				cur_result += 3;
				break;

			case 'b':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\b' & 0700) >> 6);
				cur_result[2] = '0' + (('\b' &  070) >> 3);
				cur_result[3] = '0' + ( '\b' &   07);
				cur_result += 3;
				break;

			case 'f':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\f' & 0700) >> 6);
				cur_result[2] = '0' + (('\f' &  070) >> 3);
				cur_result[3] = '0' + ( '\f' &   07);
				cur_result += 3;
				break;

			case 'n':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\n' & 0700) >> 6);
				cur_result[2] = '0' + (('\n' &  070) >> 3);
				cur_result[3] = '0' + ( '\n' &   07);
				cur_result += 3;
				break;

			case 'r':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\r' & 0700) >> 6);
				cur_result[2] = '0' + (('\r' &  070) >> 3);
				cur_result[3] = '0' + ( '\r' &   07);
				cur_result += 3;
				break;

			case 't':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\t' & 0700) >> 6);
				cur_result[2] = '0' + (('\t' &  070) >> 3);
				cur_result[3] = '0' + ( '\t' &   07);
				cur_result += 3;
				break;

			case '\\':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\\' & 0700) >> 6);
				cur_result[2] = '0' + (('\\' &  070) >> 3);
				cur_result[3] = '0' + ( '\\' &   07);
				cur_result += 3;
				break;

			case '\'':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\'' & 0700) >> 6);
				cur_result[2] = '0' + (('\'' &  070) >> 3);
				cur_result[3] = '0' + ( '\'' &   07);
				cur_result += 3;
				break;

			case '\"':
				cur_result[0] = '\\';
				cur_result[1] = '0' + (('\"' & 0700) >> 6);
				cur_result[2] = '0' + (('\"' &  070) >> 3);
				cur_result[3] = '0' + ( '\"' &   07);
				cur_result += 3;
				break;

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

				if ((!iscntrl(k)) && (!ispunct(k)))
				{
					*cur_result = k;
				}
				else
				{
					cur_result[0] = '\\';
					cur_result[1] = '0' + ((k & 0700) >> 6);
					cur_result[2] = '0' + ((k &  070) >> 3);
					cur_result[3] = '0' + ( k &   07);
					cur_result += 3;
				}

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

				if ((!iscntrl(k)) && (!ispunct(k)))
				{
					*cur_result = k;
				}
				else
				{
					cur_result[0] = '\\';
					cur_result[1] = '0' + ((k & 0700) >> 6);
					cur_result[2] = '0' + ((k &  070) >> 3);
					cur_result[3] = '0' + ( k &   07);
					cur_result += 3;
				}

				label += 2;
				break;

			default:
				*cur_result = '\\';
				++cur_result;
				*cur_result = *label;
			}
		}
		else
		{
			if ((!iscntrl(*label)) && (!ispunct(*label)))
			{
				*cur_result = *label;
			}
			else
			{
				cur_result[0] = '\\';
				cur_result[1] = '0' + (((*label) & 0700) >> 6);
				cur_result[2] = '0' + (((*label) &  070) >> 3);
				cur_result[3] = '0' + ( (*label) &   07);
				cur_result += 3;
			}
		}

		++cur_result;
		++label;
	}

	*cur_result = 0;

	return result;
}
