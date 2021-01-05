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

#include <stdlib.h>
#include <string.h>
#include "2d/gr.h"
#include "gamefont.h"
#include "cfile/Cfile.h"
#include "locale/transl.h"

const char* Gamefont_filenames[] = { "font1-1.fnt",			// Font 0
											"font2-1.fnt",			// Font 1
											"font2-2.fnt",			// Font 2
											"font2-3.fnt",			// Font 3
											"font3-1.fnt",			// Font 4
											"font3-1.fnt",          // Smaller font
											"font3-1.fnt"           // Gauge numerals only font
};

grs_fontstyle* Gamefonts[MAX_FONTS];

int Gamefont_installed = 0;

void gamefont_load_extrafonts(grs_fontstyle* style, int styleNum)
{
	char inputline[41];
	CFILE* fntfile = cfopen("FONTS.LST", "rb");
	if (fntfile)
	{
		while (cfgets(inputline, sizeof(inputline) - 1, fntfile))
		{
			char* p = strchr(inputline, '\n');
			if (p) *p = '\0';
			if (*inputline && *inputline != '#')
			{
				int st, yo; 
				uint32_t fontOffset;
				char fontFilename[30];
				if (sscanf(inputline, "%i %u %i %30s", &st, &fontOffset, &yo, &fontFilename) == 4 && st == styleNum)
				{
					grs_font* font = gr_init_font(fontFilename);
					if (font)
					{
						font->ft_yoffset = yo;
						gr_register_font(style, font, fontOffset);
					}
				}
			}
		}
		cfclose(fntfile);
	}

	fntfile = cfopen("OFFSETS.LST", "rb");
	if (fntfile)
	{
		while (cfgets(inputline, sizeof(inputline) - 1, fntfile))
		{
			char* p = strchr(inputline, '\n');
			if (p) *p = '\0';
			if (*inputline && *inputline != '#')
			{
				int st, yo, dyo, fh;
				if (sscanf(inputline, "%i %i %i %i", &st, &yo, &dyo, &fh) == 4 && st == styleNum)
				{
					style->yoffset = yo;
					style->deffont->ft_yoffset = dyo;
					style->ft_h = fh;
				}
			}
		}
		cfclose(fntfile);
	}

	fntfile = cfopen("KERNS.LST", "rb");
	if (fntfile)
	{
		while (cfgets(inputline, sizeof(inputline) - 1, fntfile))
		{
			char* p = strchr(inputline, '\n');
			if (p) *p = '\0';
			if (*inputline && *inputline != '#')
			{
				uint32_t c1, c2;
				int st;
				short d;
				if (sscanf(inputline, "%i %u %u %hi", &st, &c1, &c2, &d) == 4 && st == styleNum)
				{
					gr_register_kern(style, c1, c2, d);
				}
			}
		}
		cfclose(fntfile);
		gr_prepare_kerns(style);
	}
}

void gamefont_init()
{
	int i;

	if (Gamefont_installed) return;
	Gamefont_installed = 1;

	for (i = 0; i < MAX_FONTS; i++)
	{
		Gamefonts[i] = gr_init_fontstyle(Gamefont_filenames[i]);
		gamefont_load_extrafonts(Gamefonts[i], i);
	}

	atexit(gamefont_close);
}

void gamefont_close(void)
{
	int i;

	if (!Gamefont_installed) return;
	Gamefont_installed = 0;

	for (i = 0; i < MAX_FONTS; i++)
	{
		gr_close_fontstyle(Gamefonts[i]);
		Gamefonts[i] = NULL;
	}
}

