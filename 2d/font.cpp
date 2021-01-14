/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "mem/mem.h"

#include "2d/gr.h"
#include "2d/grdef.h"
#include "misc/error.h"

#include "cfile/cfile.h"
#include "platform/mono.h"

#include "locale/utf8.h"

#define FONT        grd_curcanv->cv_font
#define FG_COLOR    grd_curcanv->cv_font_fg_color
#define BG_COLOR    grd_curcanv->cv_font_bg_color
#define FWIDTH       ((FONT)->ft_w)
#define FHEIGHT      ((FONT)->ft_h)
#define FMAXHEIGHT   ((FONT)->ft_mh)
#define FBASELINE(f) ((f)->ft_baseline)
#define FFLAGS(f)    ((f)->ft_flags)
#define FMINCHAR(f)  ((f)->ft_minchar)
#define FMAXCHAR(f)  ((f)->ft_maxchar)
#define FDATA(f)     ((f)->ft_data)
#define FCHARS(f)    ((f)->ft_chars)
#define FWIDTHS(f)   ((f)->ft_widths)

#define BITS_TO_BYTES(x)    (((x)+7)>>3)

grs_font* resolve_font(grs_fontstyle* style, uint32_t c, int *cout, int *infont)
{
#if FAST_FONT_TABLE
	int co;
	grs_font* font, ** ftmp;
	if (c <= 0x0d)
	{
		if (*infont)
			*infont = 0;
		return style->deffont;
	}
	if (c <= 0x17 && c >= 0x0e) // hack: remap special UI characters
	{
		font = style->deffont;
		co = c + 0x70 - font->ft_minchar;
		if (*cout)
			*cout = co;
		if (*infont)
			*infont = co >= 0 && co <= (font->ft_maxchar - font->ft_minchar);
		return font;
	}
	ftmp = style->fonts[c >> FAST_FONT_TABLE_SHIFT];
	font = ftmp ? ftmp[c & FAST_FONT_TABLE_MASK] : NULL;
	if (!font)
	{
		if (*infont)
			*infont = 0;
		return style->deffont;
	}
	co = c - (font->ft_uoffset + font->ft_minchar);
	if (cout)
		*cout = co;
	if (infont)
		*infont = font && co >= 0 && co <= (font->ft_maxchar - font->ft_minchar);
	return font;
#else
	int inf = 1, co;
	if (c <= 0x0d)
	{
		if (*infont)
			*infont = 0;
		return style->font;
	}
	if (c <= 0x17 && c >= 0x0e) // hack: remap special UI characters
	{
		style->font = style->deffont;
		style->minchar = style->font->ft_minchar;
		style->maxchar = style->font->ft_maxchar;
		co = c + 0x70 - style->font->ft_minchar;
		if (*cout)
			*cout = co;
		if (*infont)
			*infont = co >= 0 && co <= (style->font->ft_maxchar - style->font->ft_minchar);
		return style->font;
	}
	if (c < style->minchar || style->maxchar < c)
	{
		Assert(style->root != NULL);
		grs_fontstyle_node* node = style->root;
		while (node)
		{
			if (c >= node->minchar && c <= node->maxchar)
				break;
			else if (c < node->minchar)
				node = node->left;
			else if (c > node->maxchar)
				node = node->right;
		}
		if (node)
			style->font = node->font, inf = 1;
		else
			style->font = style->root->font, inf = 0;
		style->minchar = node->minchar;
		style->maxchar = node->maxchar;
	}
	co = c - style->minchar;
	if (cout)
		*cout = co;
	if (infont)
		*infont = inf && co >= 0 && co <= (style->font->ft_maxchar - style->font->ft_minchar);
	return style->font;
#endif
}

int gr_internal_string_clipped(int x, int y, const char* s);
int gr_internal_string_clipped_m(int x, int y, const char* s);

int gr_kern_order_ex(const grs_kernex* k1, const grs_kernex* k2)
{
	return ((int)k1->c1 - (int)k2->c1) || ((int)k1->c2 - (int)k2->c2);
}

int gr_kern_order(const void* v1, const void* v2)
{
	grs_kernex* k1 = (grs_kernex*)v1, * k2 = (grs_kernex*)v2;
	return gr_kern_order_ex(k1, k2);
}

int find_full_kern_entry(int* output, grs_fontstyle* style, uint32_t first, uint32_t second)
{
	// binary search time

	grs_kernex tmp = { first, second, 0 };
	grs_kernex* lst = style->kerns;
	int l = 0, r = style->kerncnt - 1, m, ord;
	
	while (l <= r)
	{
		m = (l + r) / 2;
		ord = gr_kern_order_ex(&tmp, &lst[m]);

		if (ord == 0)
		{
			*output = lst[m].spacing;
			return 1;
		}
		else if (l == r)
			break;
		else if (ord > 0)
			l = m + 1;
		else if (ord < 0)
			r = m - 1;
	}

	return 0;
}

int find_kern_entry(int* output, grs_font* font1, unsigned char first, grs_font* font2, unsigned char second)
{
	if (font1 == font2)
	{
		uint8_t* p = font1->ft_kerndata;

		while (*p != 255)
			if (p[0] == first && p[1] == second)
			{
				*output = *(p + 2);
				return 1;
			}
			else p += 3;
	}
	return 0;
}

//takes the character BEFORE being offset into current font
void get_char_width(uint32_t c, uint32_t c2, int* width, int* spacing)
{
	int letter, infont;
	grs_font* fnt = resolve_font(FONT, c, &letter, &infont);

	if (!infont) //not in font, draw as space
	{
		*width = 0;
		if (FFLAGS(fnt) & FT_PROPORTIONAL)
			* spacing = FWIDTH / 2;
		else
			*spacing = FWIDTH;
		return;
	}

	if (FFLAGS(fnt) & FT_PROPORTIONAL)
		*width = FWIDTHS(fnt)[letter];
	else
		*width = FWIDTH;

	*spacing = *width;

	if (FFLAGS(fnt) & FT_KERNED)
	{
		uint8_t* p;

		if (!(c2 == 0 || c2 == '\n')) 
		{
			int letter2;
			grs_font* fnt2 = resolve_font(FONT, c2, &letter2, &infont);

			if (infont)
				if (!find_kern_entry(spacing, fnt, letter, fnt2, letter2))
					find_full_kern_entry(spacing, FONT, c, c2);
		}
	}
}

int get_string_width(unsigned char* s)
{
	int w = 0, cw, cs;
	uint32_t u, lu = 0;
	while ((u = utf8_read(&s)) && u != '\n')
	{
		if (lu)
		{
			get_char_width(lu, u, &cw, &cs);
			w += cs;
		}
		lu = u;
	}

	get_char_width(lu, u, &cw, &cs);
	w += cs;

	return w;
}

int get_centered_x(unsigned char* s)
{
	return ((grd_curcanv->cv_bitmap.bm_w - get_string_width(s)) / 2);
}



int gr_internal_string0(int x, int y, unsigned char* s)
{
	unsigned char* fp;
	unsigned char* text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline, inf, yo;
	grs_font* fnt;
	uint32_t u;

	unsigned int VideoOffset, VideoOffset1;

	VideoOffset1 = y * ROWSIZE + x;

	next_row = s;

	while (next_row != NULL)
	{
		text_ptr1 = next_row;
		next_row = NULL;

		if (x == 0x8000) //centered
		{
			int xx = get_centered_x((unsigned char*)text_ptr1);
			VideoOffset1 = y * ROWSIZE + xx;
		}

		for (r = 0; r < FMAXHEIGHT; r++)
		{
			text_ptr = text_ptr1;

			VideoOffset = VideoOffset1;

			while (u = utf8_read(&text_ptr))
			{
				if (u == '\n')
				{
					next_row = text_ptr;
					break;
				}

				underline = 0;
				fnt = resolve_font(FONT, u, &letter, &inf);

				if (u == '&')
				{
					u = utf8_read(&text_ptr);
					fnt = resolve_font(FONT, u, &letter, &inf);
					if ((r == FBASELINE(fnt) + 2 + fnt->ft_yoffset) || (r == FBASELINE(fnt) + 3 + fnt->ft_yoffset))
						underline = 1;
				}

				yo = fnt->ft_yoffset;
				get_char_width(u, utf8_read_noseek(text_ptr), &width, &spacing);

				if (!inf) // not in font, draw as space
				{	
					VideoOffset += spacing;
					continue;
				}

				if (FFLAGS(fnt) & FT_PROPORTIONAL)
					fp = FCHARS(fnt)[letter];
				else
					fp = FDATA(fnt) + letter * BITS_TO_BYTES(width) * fnt->ft_h;

				if (underline)
					for (i = 0; i < width; i++)
						DATA[VideoOffset++] = (unsigned char)FG_COLOR;
				else if (r >= yo && r < (fnt->ft_h + yo))
				{
					fp += BITS_TO_BYTES(width) * (r - yo);

					BitMask = 0;

					for (i = 0; i < width; i++)
					{
						if (BitMask == 0)
						{
							bits = *fp++;
							BitMask = 0x80;
						}

						if (bits & BitMask)
							DATA[VideoOffset++] = (unsigned char)FG_COLOR;
						else
							DATA[VideoOffset++] = (unsigned char)BG_COLOR;
						BitMask >>= 1;
					}
				}
				else
					VideoOffset += width;

				VideoOffset += spacing - width;		//for kerning
			}

			VideoOffset1 += ROWSIZE; y++;
		}
	}
	return 0;
}

int gr_internal_string0m(int x, int y, unsigned char* s)
{
	unsigned char* fp;
	unsigned char* text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline, inf, yo;
	grs_font* fnt;
	uint32_t u;

	unsigned int VideoOffset, VideoOffset1;

	VideoOffset1 = y * ROWSIZE + x;

	next_row = s;

	while (next_row != NULL)
	{
		text_ptr1 = next_row;
		next_row = NULL;

		if (x == 0x8000) //centered
		{
			int xx = get_centered_x((unsigned char*)text_ptr1);
			VideoOffset1 = y * ROWSIZE + xx;
		}

		for (r = 0; r < FMAXHEIGHT; r++)
		{
			text_ptr = text_ptr1;

			VideoOffset = VideoOffset1;

			while (u = utf8_read(&text_ptr))
			{
				if (u == '\n')
				{
					next_row = text_ptr;
					break;
				}

				underline = 0;
				fnt = resolve_font(FONT, u, &letter, &inf);

				if (u == '&')
				{
					u = utf8_read(&text_ptr);
					fnt = resolve_font(FONT, u, &letter, &inf);
					if ((r == FBASELINE(fnt) + 2 + fnt->ft_yoffset) || (r == FBASELINE(fnt) + 3 + fnt->ft_yoffset))
						underline = 1;
				}

				yo = fnt->ft_yoffset;
				get_char_width(u, utf8_read_noseek(text_ptr), &width, &spacing);

				if (!inf) //not in font, draw as space
				{	
					VideoOffset += spacing;
					continue;
				}

				if (FFLAGS(fnt) & FT_PROPORTIONAL)
					fp = FCHARS(fnt)[letter];
				else
					fp = FDATA(fnt) + letter * BITS_TO_BYTES(width) * fnt->ft_h;

				if (underline)
					for (i = 0; i < width; i++)
						DATA[VideoOffset++] = (unsigned char)FG_COLOR;
				else if (r >= yo && r < (fnt->ft_h + yo))
				{
					fp += BITS_TO_BYTES(width) * (r - yo);

					BitMask = 0;

					for (i = 0; i < width; i++)
					{
						if (BitMask == 0)
						{
							bits = *fp++;
							BitMask = 0x80;
						}

						if (bits & BitMask)
							DATA[VideoOffset++] = (unsigned char)FG_COLOR;
						else
							VideoOffset++;
						BitMask >>= 1;
					}
				}
				else
					VideoOffset += width;

				VideoOffset += spacing - width;
			}

			VideoOffset1 += ROWSIZE; y++;
		}
	}
	return 0;
}


int gr_internal_string2(int x, int y, char* s)
{
	Error("gr_internal_string2: STUB\n");
	return 0;
}

int gr_internal_string2m(int x, int y, char* s)
{
	Error("gr_internal_string2m: STUB\n");
	return 0;
}

//a bitmap for the character
grs_bitmap char_bm = {
				0,0,0,0,						//x,y,w,h
				BM_LINEAR,					//type
				BM_FLAG_TRANSPARENT,		//flags
				0,								//rowsize
				NULL,							//data
				0								//selector
};

int gr_internal_color_string(int x, int y, const char* s)
{
	unsigned char* fp;
	unsigned char* text_ptr, * next_row, * text_ptr1;
	int width, spacing, letter, inf, yo;
	int xx, yy;
	grs_font* fnt;
	uint32_t u;

	char_bm.bm_h = FMAXHEIGHT;		//set height for chars of this font

	next_row = (unsigned char* )s;

	yy = y;

	while (next_row != NULL)
	{
		text_ptr1 = next_row;
		next_row = NULL;

		text_ptr = text_ptr1;

		xx = x;

		if (xx == 0x8000)			//centered
			xx = get_centered_x(text_ptr);

		while (u = utf8_read(&text_ptr))
		{
			if (u == '\n')
			{
				next_row = text_ptr;
				yy += FHEIGHT;
				break;
			}

			fnt = resolve_font(FONT, u, &letter, &inf);

			yo = fnt->ft_yoffset;
			get_char_width(u, utf8_read_noseek(text_ptr), &width, &spacing);

			if (!inf) //not in font, draw as space
			{
				xx += spacing;
				continue;
			}

			if (FFLAGS(fnt) & FT_PROPORTIONAL)
				fp = FCHARS(fnt)[letter];
			else
				fp = FDATA(fnt) + letter * width * fnt->ft_h;

			char_bm.bm_w = char_bm.bm_rowsize = width;
			char_bm.bm_h = fnt->ft_h;

			char_bm.bm_data = fp;
			gr_bitmapm(xx, yy + yo, &char_bm);

			xx += spacing;
		}

	}
	return 0;
}

int gr_ustring(int x, int y, const char* s)
{
	if (FONT->flags & FT_COLOR)
	{
		return gr_internal_color_string(x, y, s);
	}
	else
		/*switch (TYPE)
		{
		case BM_LINEAR:*/
			if (BG_COLOR == -1)
				return gr_internal_string0m(x, y, (unsigned char*)s);
			else
				return gr_internal_string0(x, y, (unsigned char*)s);
		/*case BM_SVGA:
			if (BG_COLOR == -1)
				return gr_internal_string2m(x, y, (char*)s);
			else
				return gr_internal_string2(x, y, (char*)s);
		}*/

	return 0;
}

int gr_string(int x, int y, const char* s)
{
	int w, h, aw;
	int clipped = 0;
	y -= FONT->yoffset;

	if (x == 0x8000)
	{
		if (y < 0) clipped |= 1;
		gr_get_string_size(s, &w, &h, &aw);
		// for x, since this will be centered, only look at
		// width.
		if (w > grd_curcanv->cv_bitmap.bm_w) clipped |= 1;
		if ((y + h) > grd_curcanv->cv_bitmap.bm_h) clipped |= 1;

		if ((y + h) < 0) clipped |= 2;
		if (y > grd_curcanv->cv_bitmap.bm_h) clipped |= 2;

	}
	else
	{
		if ((x < 0) || (y < 0)) clipped |= 1;
		gr_get_string_size(s, &w, &h, &aw);
		if ((x + w) > grd_curcanv->cv_bitmap.bm_w) clipped |= 1;
		if ((y + h) > grd_curcanv->cv_bitmap.bm_h) clipped |= 1;
		if ((x + w) < 0) clipped |= 2;
		if ((y + h) < 0) clipped |= 2;
		if (x > grd_curcanv->cv_bitmap.bm_w) clipped |= 2;
		if (y > grd_curcanv->cv_bitmap.bm_h) clipped |= 2;
	}

	if (!clipped)
		return gr_ustring(x, y, s);

	if (clipped & 2)
	{
		// Completely clipped...
		mprintf((1, "Text '%s' at (%d,%d) is off screen!\n", s, x, y));
		return 0;
	}

	if (clipped & 1)
	{
		// Partially clipped...
		//mprintf( (0, "Text '%s' at (%d,%d) is getting clipped!\n", s, x, y ));
	}

	// Partially clipped...

	if (FONT->flags & FT_COLOR)
		return gr_internal_color_string(x, y, s);

	if (BG_COLOR == -1)
		return gr_internal_string_clipped_m(x, y, s);

	return gr_internal_string_clipped(x, y, s);
}


void gr_get_string_size(const char* ss, int* string_width, int* string_height, int* average_width)
{
	const uint8_t* s = (const uint8_t*)ss;
	int i = 0, longest_width = 0;
	int width, spacing;
	uint32_t u, nu;

	*string_height = FHEIGHT;
	*string_width = 0;
	*average_width = FWIDTH;

	if (s != NULL)
	{
		*string_width = 0;
		u = utf8_read((unsigned char**)&s);
		while (u)
		{
			//			if (*s == '&')
			//				s++;
			while (u == '\n')
			{
				s++;
				*string_height += FHEIGHT;
				*string_width = 0;
				u = utf8_read((unsigned char**)&s);
			}

			if (!u) break;
			nu = utf8_read((unsigned char**)&s);

			get_char_width(u, nu, &width, &spacing);

			*string_width += spacing;

			if (*string_width > longest_width)
				longest_width = *string_width;

			++i;
			u = nu;
		}
	}
	*string_width = longest_width;
}


int gr_uprintf(int x, int y, const char* format, ...)
{
	char buffer[1000];
	va_list args;

	va_start(args, format);
	vsnprintf(buffer, 1000, format, args);
	return gr_ustring(x, y, buffer);
}

int gr_printf(int x, int y, const char* format, ...)
{
	char buffer[1000];
	va_list args;

	va_start(args, format);
	vsnprintf(buffer, 1000, format, args);
	return gr_string(x, y, buffer);
}

void gr_close_font(grs_font* font)
{
	if (font)
	{
		free(font->ft_datablock);
		if (font->ft_chars)
			free(font->ft_chars);
		free(font);
	}
}

void build_colormap_good(uint8_t* palette, uint8_t* colormap, int* freq);

extern void decode_data_asm(uint8_t* data, int num_pixels, uint8_t* colormap, int* count); //[ISB] fucking hack

//[ISB] saner font handling
//implying that there's anything sane about this font format, sadly...
void GR_ReadFont(grs_font* font, CFILE* fp, int len)
{
	int dataPtr, charPtr, widthPtr, kernPtr;
	int nchars;
	int i;
	uint8_t* ptr;

	font->ft_datablock = (uint8_t*)malloc(len);

	font->ft_w = CF_ReadShort(fp);
	font->ft_h = CF_ReadShort(fp);
	font->ft_flags = CF_ReadShort(fp);
	font->ft_baseline = CF_ReadShort(fp);
	font->ft_minchar = CF_ReadByte(fp);
	font->ft_maxchar = CF_ReadByte(fp);
	font->ft_bytewidth = CF_ReadShort(fp);
	dataPtr = CF_ReadInt(fp);
	charPtr = CF_ReadInt(fp);
	widthPtr = CF_ReadInt(fp);
	kernPtr = CF_ReadInt(fp);

	cfseek(fp, 8, SEEK_SET);
	cfread(font->ft_datablock, len, 1, fp);

	nchars = font->ft_maxchar - font->ft_minchar + 1;

	//printf(" dimensions: %d x %d\n", font->ft_w, font->ft_h);
	//printf(" flags: %d\n", font->ft_flags);
	//printf(" baseline: %d\n", font->ft_baseline);
	//printf(" range: %d - %d (%d chars)\n", font->ft_minchar, font->ft_maxchar, nchars);

	if (font->ft_flags & FT_PROPORTIONAL)
	{
		//font->ft_widths = (short*)(((int)font->ft_widths) + ((uint8_t*)font));
		font->ft_widths = (short*)&font->ft_datablock[widthPtr];
		font->ft_data = &font->ft_datablock[dataPtr];
		font->ft_chars = (unsigned char**)malloc(nchars * sizeof(unsigned char*));
		ptr = font->ft_data;

		for (i = 0; i < nchars; i++)
		{
			font->ft_chars[i] = ptr;
			if (font->ft_flags & FT_COLOR)
				ptr += font->ft_widths[i] * font->ft_h;
			else
				ptr += BITS_TO_BYTES(font->ft_widths[i]) * font->ft_h;
		}
	}
	else
	{
		//font->ft_data = ((unsigned char*)font) + sizeof(*font);
		font->ft_data = &font->ft_datablock[dataPtr];
		font->ft_chars = NULL;
		font->ft_widths = NULL;

		ptr = font->ft_data + (nchars * font->ft_w * font->ft_h);
	}

	if (font->ft_flags & FT_KERNED)
		font->ft_kerndata = &font->ft_datablock[kernPtr];

	if (font->ft_flags & FT_COLOR) //remap palette
	{
		uint8_t palette[256 * 3];
		uint8_t colormap[256];
		int freq[256];

		cfread(palette, 3, 256, fp);		//read the palette

		build_colormap_good(&palette[0], &colormap[0], freq);

		colormap[255] = 255;

		decode_data_asm(font->ft_data, ptr - font->ft_data, colormap, freq);
	}

	font->ft_yoffset = 0;
#if FAST_FONT_TABLE
	font->ft_uoffset = 0;
#endif
}

#if FAST_FONT_TABLE

grs_fontstyle* gr_init_fontstyle(const char* name)
{
	int i;
	grs_fontstyle* fontstyle = (grs_fontstyle*)malloc(sizeof(grs_fontstyle));
	fontstyle->ft_w = fontstyle->ft_h = fontstyle->ft_mh = 1;
	for (i = 0; i < FAST_FONT_TABLE_SIZE; ++i)
		fontstyle->fonts[i] = NULL;
	fontstyle->deffont = NULL;
	Assert(name != NULL);
	gr_register_font(fontstyle, gr_init_font(name), 0);
	fontstyle->kerncnt = 0;
	fontstyle->kerns = (grs_kernex*)malloc(sizeof(grs_kernex) * (fontstyle->kernsize = 64));
	fontstyle->yoffset = 0;
	FONT = fontstyle;
	return fontstyle;
}

void gr_register_font(grs_fontstyle* style, grs_font* font, uint32_t offset)
{
	int c;
	grs_font** ftmp;
	Assert(offset + font->ft_maxchar < UNICODE_CODEPOINT_COUNT);
	for (c = offset + font->ft_minchar; c <= offset + font->ft_maxchar; ++c)
	{
		ftmp = style->fonts[c >> FAST_FONT_TABLE_SHIFT];
		if (!ftmp)
			ftmp = style->fonts[c >> FAST_FONT_TABLE_SHIFT] = (grs_font**)calloc(FAST_FONT_TABLE_SUBTABLE_SIZE, sizeof(grs_font*));
		ftmp[c & FAST_FONT_TABLE_MASK] = font;
	}
	if (style->ft_w < font->ft_w)
		style->ft_w = font->ft_w;
	if (style->ft_h < font->ft_h)
		style->ft_h = font->ft_h;
	if (style->ft_mh < font->ft_h)
		style->ft_mh = font->ft_h;
	if (!style->deffont)
	{
		style->deffont = font;
		style->flags = font->ft_flags;
	}
#if FAST_FONT_TABLE
	font->ft_uoffset = offset;
#endif
}

void gr_close_fontstyle(grs_fontstyle* style)
{
	int i, j;
	grs_font* last_font = NULL, * font, ** ftable;
	for (i = 0; i < FAST_FONT_TABLE_SIZE; ++i)
	{
		if (ftable = style->fonts[i])
		{
			for (j = 0; j < FAST_FONT_TABLE_SUBTABLE_SIZE; ++j)
			{
				font = ftable[j];
				if (last_font != font)
				{
					last_font = font;
					if (font)
						free(font);
				}
			}
			free(ftable);
		}
	}
	free(style->kerns);
	free(style);
}

#else

grs_fontstyle* gr_init_fontstyle(const char* name)
{
	grs_fontstyle* fontstyle = (grs_fontstyle*)malloc(sizeof(grs_fontstyle));
	fontstyle->ft_w = fontstyle->ft_h = 1;
	fontstyle->font = NULL;
	fontstyle->minchar = fontstyle->maxchar = 0;
	fontstyle->root = (grs_fontstyle_node*)malloc(sizeof(grs_fontstyle_node));;
	fontstyle->root->font = NULL;
	fontstyle->root->balance = 0;
	fontstyle->kerncnt = 0;
	fontstyle->kerns = (grs_kernex*)malloc(sizeof(grs_kernex) * (fontstyle->kernsize = 64));
	fontstyle->yoffset = 0;
	Assert(name != NULL);
	gr_register_font(fontstyle, gr_init_font(name), 0);
	FONT = fontstyle;
	return fontstyle;
}

grs_fontstyle_node* gr_font_rotate_left(grs_fontstyle_node* node)
{
	grs_fontstyle_node* root = node->right;
	grs_fontstyle_node* subtree = root->left;
	root->left = node;
	node->right = subtree;
	root->parent = node->parent;
	node->parent = root;
	if (subtree) subtree->parent = node;
	node->balance = node->balance - 1 - (root->balance > 0 ? root->balance : 0);
	root->balance = root->balance - 1 + (node->balance < 0 ? node->balance : 0);
	return root;
}

grs_fontstyle_node* gr_font_rotate_right(grs_fontstyle_node* node)
{
	grs_fontstyle_node* root = node->left;
	grs_fontstyle_node* subtree = root->right;
	root->right = node;
	node->left = subtree;
	root->parent = node->parent;
	node->parent = root;
	if (subtree) subtree->parent = node;
	node->balance = node->balance + 1 - (root->balance < 0 ? root->balance : 0);
	root->balance = root->balance + 1 + (node->balance > 0 ? node->balance : 0);
	return root;
}

#define _MAX(x, y) ((x) > (y) ? (x) : (y))

void gr_register_font(grs_fontstyle* style, grs_font* font, uint32_t offset)
{
	if (style->ft_w < font->ft_w)
		style->ft_w = font->ft_w;
	if (style->ft_h < font->ft_h)
		style->ft_h = font->ft_h;
	if (!style->root->font)
	{
		style->root->font = font;
		style->root->balance = 0;
		style->root->parent = style->root->left = style->root->right = NULL;
		style->minchar = style->root->minchar = font->ft_minchar + offset;
		style->maxchar = style->root->maxchar = font->ft_maxchar + offset;
		style->root->maxchar = _MAX(style->root->maxchar, 0x7e);
		style->font = font;
		style->flags = font->ft_flags;
		style->deffont = font;
		return;
	}

	grs_fontstyle_node* sentinel = style->root, * parent;
	grs_fontstyle_node** insert_to = &sentinel;
	uint32_t pos = offset + font->ft_minchar, cmp;
	while (*insert_to)
	{
		cmp = (*insert_to)->minchar;
		Assert(pos != cmp);
		parent = *insert_to;
		if (pos < cmp)
		{
			--(*insert_to)->balance;
			insert_to = &(*insert_to)->left;
		}
		else
		{
			++(*insert_to)->balance;
			insert_to = &(*insert_to)->right;
		}
	}

	grs_fontstyle_node* newnode = (grs_fontstyle_node*)malloc(sizeof(grs_fontstyle_node));
	newnode->font = font;
	newnode->minchar = font->ft_minchar + offset;
	newnode->maxchar = font->ft_maxchar + offset;
	newnode->balance = 0;
	newnode->left = newnode->right = NULL;
	newnode->parent = parent;
	*insert_to = newnode;

	// tree might now be unbalanced. go back up
	while (parent)
	{
		if (parent->balance > 1)
		{
			if (parent->left && parent->left->balance < 0)
				parent->left = gr_font_rotate_left(parent->left);
			parent = gr_font_rotate_right(parent);
		}
		else if (parent->balance < -1)
		{
			if (parent->right && parent->right->balance > 0)
				parent->right = gr_font_rotate_right(parent->right);
			parent = gr_font_rotate_left(parent);
		}
		else
			break;
	}
}

void gr_free_fontnode(grs_fontstyle_node* root, grs_font* except)
{
	if (!root) return;
	if (root->left) gr_free_fontnode(root->left, except);
	if (root->right) gr_free_fontnode(root->right, except);
	if (root->font != except) free(root->font);
	free(root);
}

void gr_close_custom_fonts(grs_fontstyle* style)
{
	if (style->root->font != style->deffont)
	{
		free(style->root->font);
		style->root->font = style->deffont;
		style->minchar = style->deffont->ft_minchar;
		style->maxchar = style->deffont->ft_maxchar;
	}

	gr_free_fontnode(style->root->left, style->deffont);
	gr_free_fontnode(style->root->right, style->deffont);
	style->root->left = style->root->right = NULL;
	style->ft_w = style->deffont->ft_w;
	style->ft_h = style->deffont->ft_h;
	style->root->balance = 0;
}

void gr_close_fontstyle(grs_fontstyle* style)
{
	if (style->root)
	{
		gr_close_custom_fonts(style);
		gr_close_font(style->root->font);
	}
	free(style->kerns);
	free(style);
}

#endif

void gr_prepare_kerns(grs_fontstyle* style)
{
	qsort(style->kerns, style->kerncnt, sizeof(grs_kernex), &gr_kern_order);
}

void gr_register_kern(grs_fontstyle* style, uint32_t c1, uint32_t c2, short spacing)
{
	int newpos = 0, ord;
	Assert(c1 <= UNICODE_CODEPOINT_COUNT && c2 <= UNICODE_CODEPOINT_COUNT);
	if (style->kerncnt == style->kernsize)
		style->kerns = (grs_kernex*)realloc(style->kerns, sizeof(grs_kernex) * (style->kernsize *= 2));
	style->kerns[style->kerncnt++] = { c1, c2, spacing };
}

grs_font * gr_init_font(const char* fontname)
{
	grs_font* font;
	CFILE* fontfile;
	int file_id;
	int datasize;		//size up to (but not including) palette

	fontfile = cfopen(fontname, "rb");

	if (!fontfile)
		Error("Can't open font file %s", fontname);

	cfread(&file_id, sizeof(file_id), 1, fontfile);
	datasize = CF_ReadInt(fontfile);

	if (file_id != 'NFSP')
		Error("File %s is not a font file", fontname);

	font = (grs_font*)malloc(datasize);

	//printf("loading font %s\n", fontname);
	GR_ReadFont(font, fontfile, datasize);

	cfclose(fontfile);

	//set curcanv vars

	/* FONT = font; */
	FG_COLOR = 0;
	BG_COLOR = 0;

	return font;
}

void gr_set_fontcolor(int fg, int bg)
{
	FG_COLOR = fg;
	BG_COLOR = bg;
}

void gr_set_curfont(grs_fontstyle* newfont)
{
	FONT = newfont;
}


int gr_internal_string_clipped(int x, int y, const char* s)
{
	unsigned char* fp;
	unsigned char* text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline, inf, yo;
	int x1 = x, last_x;
	grs_font* fnt;
	uint32_t u;

	next_row = (unsigned char*)s;

	while (next_row != NULL)
	{
		text_ptr1 = next_row;
		next_row = NULL;

		x = x1;
		if (x == 0x8000)			//centered
			x = get_centered_x(text_ptr1);

		last_x = x;

		for (r = 0; r < FMAXHEIGHT; r++)
		{
			text_ptr = text_ptr1;
			x = last_x;

			while (u = utf8_read(&text_ptr)) 
			{
				if (u == '\n') 
				{
					next_row = text_ptr;
					break;
				}
				underline = 0;
				fnt = resolve_font(FONT, u, &letter, &inf);

				if (u == '&')
				{
					u = utf8_read(&text_ptr);
					fnt = resolve_font(FONT, u, &letter, &inf);
					if ((r == FBASELINE(fnt) + 2 + fnt->ft_yoffset) || (r == FBASELINE(fnt) + 3 + fnt->ft_yoffset))
						underline = 1;
				}

				yo = fnt->ft_yoffset;
				get_char_width(u, utf8_read_noseek(text_ptr), &width, &spacing);

				if (!inf) //not in font, draw as space
				{
					x += spacing;
					continue;
				}

				if (FFLAGS(fnt) & FT_PROPORTIONAL)
					fp = FCHARS(fnt)[letter];
				else
					fp = FDATA(fnt) + letter * BITS_TO_BYTES(width) * fnt->ft_h;

				if (underline) 
				{
					for (i = 0; i < width; i++) 
					{
						gr_setcolor(FG_COLOR);
						gr_pixel(x++, y);
					}
				}
				else if (r >= yo && r < (fnt->ft_h + yo)) 
				{
					fp += BITS_TO_BYTES(width) * (r - yo);

					BitMask = 0;

					for (i = 0; i < width; i++) 
					{
						if (BitMask == 0)
						{
							bits = *fp++;
							BitMask = 0x80;
						}
						if (bits & BitMask)
							gr_setcolor(FG_COLOR);
						else
							gr_setcolor(BG_COLOR);
						gr_pixel(x++, y);
						BitMask >>= 1;
					}
				}
				else
					x += width;

				x += spacing - width;		//for kerning
			}
			y++;
		}
	}
	return 0;
}

int gr_internal_string_clipped_m(int x, int y, const char* s)
{
	unsigned char* fp;
	unsigned char* text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline, inf, yo;
	int x1 = x, last_x;
	uint32_t u;
	grs_font* fnt;

	next_row = (unsigned char*)s;

	while (next_row != NULL)
	{
		text_ptr1 = next_row;
		next_row = NULL;

		x = x1;
		if (x == 0x8000)			//centered
			x = get_centered_x(text_ptr1);

		last_x = x;

		for (r = 0; r < FMAXHEIGHT; r++)
		{
			x = last_x;

			text_ptr = text_ptr1;

			while (u = utf8_read(&text_ptr))
			{
				if (u == '\n')
				{
					next_row = text_ptr;
					break;
				}
				underline = 0;
				fnt = resolve_font(FONT, u, &letter, &inf);

				if (u == '&')
				{
					u = utf8_read(&text_ptr);
					fnt = resolve_font(FONT, u, &letter, &inf);
					if ((r == FBASELINE(fnt) + 2 + fnt->ft_yoffset) || (r == FBASELINE(fnt) + 3 + fnt->ft_yoffset))
						underline = 1;
				}

				yo = fnt->ft_yoffset;
				get_char_width(u, utf8_read_noseek(text_ptr), &width, &spacing);

				if (!inf) //not in font, draw as space
				{
					x += spacing;
					continue;
				}

				if (FFLAGS(fnt) & FT_PROPORTIONAL)
					fp = FCHARS(fnt)[letter];
				else
					fp = FDATA(fnt) + letter * BITS_TO_BYTES(width) * fnt->ft_h;

				if (underline) 
				{
					for (i = 0; i < width; i++) 
					{
						gr_setcolor(FG_COLOR);
						gr_pixel(x++, y);
					}
				}
				else if (r >= yo && r < (fnt->ft_h + yo))
				{
					fp += BITS_TO_BYTES(width) * (r - yo);

					BitMask = 0;

					for (i = 0; i < width; i++) 
					{
						if (BitMask == 0)
						{
							bits = *fp++;
							BitMask = 0x80;
						}
						if (bits & BitMask) 
						{
							gr_setcolor(FG_COLOR);
							gr_pixel(x++, y);
						}
						else 
						{
							x++;
						}
						BitMask >>= 1;
					}
				}
				else
					x += width;

				x += spacing - width;		//for kerning
			}
			y++;
		}
	}
	return 0;
}
