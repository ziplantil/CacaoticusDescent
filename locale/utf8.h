/* Cacaoticus Descent */
/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License,
as described in copying.txt
*/

#pragma once

inline uint32_t utf8_read(unsigned char** cptr)
{
	uint32_t u;
	unsigned char* p = *cptr, c = *p++, c2, c3, c4;
	if (!(c & 0x80))
		u = c;
	else if ((c & 0xE0) == 0xC0)
	{
		c2 = *p++;
		u = ((c & 0x1F) << 6) | (c2 & 0x3F);
	}
	else if ((c & 0xF0) == 0xE0)
	{
		c2 = *p++; c3 = *p++; 
		u = (((c & 0x0F) << 12) | ((c2 & 0x3F) << 6)) | (c3 & 0x3F);
	}
	else if ((c & 0xF8) == 0xF0)
	{
		c2 = *p++; c3 = *p++; c4 = *p++;
		u = ((((c & 0x07) << 18) | ((c2 & 0x3F) << 12)) | ((c3 & 0x3F) << 6)) | (c4 & 0x3F);
	}
	else
		u = '?';
	*cptr = p;
	return u;
}

inline uint32_t utf8_read_noseek(unsigned char* p)
{
	unsigned char* copy = p;
	return utf8_read(&copy);
}

inline uint32_t utf8_read_buf(unsigned char** cptr, char* buf)
{
	uint32_t u;
	unsigned char* p = *cptr, c = *buf++ = *p++, c2, c3, c4;
	if (!(c & 0x80))
		u = c;
	else if ((c & 0xE0) == 0xC0)
	{
		*buf++ = c2 = *p++;
		u = ((c & 0x1F) << 6) | (c2 & 0x3F);
	}
	else if ((c & 0xF0) == 0xE0)
	{
		*buf++ = c2 = *p++; *buf++ = c3 = *p++;
		u = (((c & 0x0F) << 12) | ((c2 & 0x3F) << 6)) | (c3 & 0x3F);
	}
	else if ((c & 0xF8) == 0xF0)
	{
		*buf++ = c2 = *p++; *buf++ = c3 = *p++; *buf++ = c4 = *p++;
		u = ((((c & 0x07) << 18) | ((c2 & 0x3F) << 12)) | ((c3 & 0x3F) << 6)) | (c4 & 0x3F);
	}
	else
		u = '?';
	*buf++ = 0;
	*cptr = p;
	return u;
}
