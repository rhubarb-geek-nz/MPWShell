/************************************
 * Copyright (c) 2024 Roger Brown.
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mbcs.h"

int mbcsLen(const unsigned char* p, size_t avail)
{
	if (avail)
	{
		size_t len = 1;
		unsigned char c = *p;

		if (c < 0x80)
		{
			return 1;
		}

		if (c < 0xC0)
		{
			return -1;
		}

		if (c < 0xE0)
		{
			len = 2;
		}
		else
		{
			if (len < 0xF0)
			{
				len = 3;
			}
			else
			{
				len = 4;
			}
		}

		if (avail >= len)
		{
			size_t i = len;

			while (--i)
			{
				if (0x80 != (0xC0 & *++p))
				{
					return -(int)len;
				}
			}
		}

		return (int)len;
	}

	return 0;
}

wchar_t mbcsToChar(const unsigned char* p, int len)
{
	const unsigned char c = p[0];

	if (c < 0x80) return c;

	if (c < 0xC0) return ~0;
	if (c < 0xE0) return ((c & 0x1f) << 6) | (p[1] & 0x3F);
#ifdef __WATCOMC__
	return ((c & 0xf) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
#else
	if (c < 0xF0) return ((c & 0xf) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
	return ((c & 0x7) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
#endif
}

int mbcsFromChar(wchar_t ch, unsigned char* p)
{
	if (ch < 0x80)
	{
		*p = (char)ch;
		return 1;
	}

	if (ch < 0x800)
	{
		p[0] = 0xC0 | (ch >> 6);
		p[1] = 0x80 | (ch & 0x3F);

		return 2;
	}

#ifdef __WATCOMC__
	p[0] = 0xE0 | (ch >> 12);
	p[1] = 0x80 | ((ch >> 6) & 0x3F);
	p[2] = 0x80 | (ch & 0x3F);

	return 3;
#else
	if (ch < 0x40000)
	{
		p[0] = 0xE0 | (ch >> 12);
		p[1] = 0x80 | ((ch >> 6) & 0x3F);
		p[2] = 0x80 | (ch & 0x3F);

		return 3;
	}

	p[0] = 0xF0 | (((unsigned int)ch) >> 18);
	p[1] = 0x80 | ((ch >> 12) & 0x3F);
	p[2] = 0x80 | ((ch >> 6) & 0x3F);
	p[3] = 0x80 | (ch & 0x3F);

	return 4;
#endif
}
