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
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfile/cfile.h"
#include "mem/mem.h"
#include "misc/error.h"

#include "inferno.h"
#include "text.h"
#include "misc/args.h"
#include "compbit.h"

char *text;

char *Text_string_orig[N_TEXT_STRINGS];

void free_text()
{
	free(text);
}

// rotates a byte left one bit, preserving the bit falling off the right
void encode_rotate_left(char *c)
{
	int found;

	found = 0;
	if (*c & 0x80)
		found = 1;
	*c = *c << 1;
	if (found)
		*c |= 0x01;
}

//decode and encoded line of text
void decode_text_line(char *p)
{
	for (;*p;p++) {
		encode_rotate_left(p);
		*p = *p ^ BITMAP_TBL_XOR;
		encode_rotate_left(p);
	}
}

//load all the text strings for Descent
void load_text()
{
	CFILE  *tfile;
	CFILE *ifile;
	int len,i, have_binary = 0;
	char *tptr;
	const char *filename="descent.tex";

	if ((i=FindArg("-text"))!=0)
		filename = Args[i+1];

	if ((tfile = cfopen(filename,"rb")) == NULL) {
		filename="descent.txb";
		if ((ifile = cfopen(filename, "rb")) == NULL)
			Error("Cannot open file DESCENT.TEX or DESCENT.TXB");
		have_binary = 1;

		len = cfilelength(ifile);

		MALLOC(text,char,len);

		atexit(free_text);

		cfread(text,1,len,ifile);

		cfclose(ifile);

	} else {
		int c;
		char * p;

		len = cfilelength(tfile);

		MALLOC(text,char,len);

		atexit(free_text);

		//fread(text,1,len,tfile);
		p = text;
		do {
			c = cfgetc( tfile );
			if ( c != 13 )
				*p++ = c;
		} while ( c!=EOF );

		cfclose(tfile);
	}

	for (i=0,tptr=text;i<N_TEXT_STRINGS;i++) {
		char *p;

		Text_string_orig[i] = tptr;

		tptr = strchr(tptr,'\n');

		#ifdef MACINTOSH			// total hack for mac patch since they don't want to patch data.
		if (!tptr && (i == 644) )
			break;
		#else
		if (!tptr)
			Error("Not enough strings in text file - expecting %d, found %d",N_TEXT_STRINGS,i);
		#endif

		if ( tptr ) *tptr++ = 0;

		if (have_binary)
			decode_text_line(Text_string_orig[i]);

		//scan for special chars (like \n)
		for (p=Text_string_orig[i];p=strchr(p,'\\');) {
			char newchar;

			if (p[1] == 'n') newchar = '\n';
			else if (p[1] == 't') newchar = '\t';
			else if (p[1] == '\\') newchar = '\\';
			else
				Error("Unsupported key sequence <\\%c> on line %d of file <%s>",p[1],i+1,filename); 

			p[0] = newchar;
			strcpy(p+1,p+2);
			p++;
		}
 
	}

//	Assert(tptr==text+len || tptr==text+len-2);
	
}

void strcpyn(char* dst, const char* src, size_t n)
{
	if (!n) return;
	while (--n && (*dst++ = *src++));
	*dst++ = '\0';
}

const char* _PRIMARY_WEAPON_NAMES[10] = {
	"TXT_W_LASER",
	"TXT_W_VULCAN",
	"TXT_W_SPREADFIRE",
	"TXT_W_PLASMA",
	"TXT_W_FUSION",
	"TXT_W_SLASER",
	"TXT_W_SVULCAN",
	"TXT_W_SSPREADFIRE",
	"TXT_W_SPLASMA",
	"TXT_W_SFUSION",
};
const char* _SECONDARY_WEAPON_NAMES[10] = {
	"TXT_W_C_MISSILE",
	"TXT_W_H_MISSILE",
	"TXT_W_P_BOMB",
	"TXT_W_S_MISSILE",
	"TXT_W_M_MISSILE",
	"TXT_W_SMISSILE1",
	"TXT_W_SMISSILE2",
	"TXT_W_SMISSILE3",
	"TXT_W_SMISSILE4",
	"TXT_W_SMISSILE5",
};
const char* _PRIMARY_WEAPON_NAMES_SHORT[10] = {
	"TXT_W_LASER_S",
	"TXT_W_VULCAN_S",
	"TXT_W_SPREADFIRE_S",
	"TXT_W_PLASMA_S",
	"TXT_W_FUSION_S",
	"TXT_W_SLASER_S",
	"TXT_W_SVULCAN_S",
	"TXT_W_SSPREADFIRE_S",
	"TXT_W_SPLASMA_S",
	"TXT_W_SFUSION_S",
};
const char* _SECONDARY_WEAPON_NAMES_SHORT[10] = {
	"TXT_W_C_MISSILE_S",
	"TXT_W_H_MISSILE_S",
	"TXT_W_P_BOMB_S",
	"TXT_W_S_MISSILE_S",
	"TXT_W_M_MISSILE_S",
	"TXT_W_SMISSILE1_S",
	"TXT_W_SMISSILE2_S",
	"TXT_W_SMISSILE3_S",
	"TXT_W_SMISSILE4_S",
	"TXT_W_SMISSILE5_S",
};
const char* _CONTROL_TEXT[7] = {
	"TXT_CONTROL_KEYBOARD",
	"TXT_CONTROL_JOYSTICK",
	"TXT_CONTROL_FSTICKPRO",
	"TXT_CONTROL_THRUSTFCS",
	"TXT_CONTROL_GGAMEPAD",
	"TXT_CONTROL_MOUSE",
	"TXT_CONTROL_CYBERMAN",
};
const char* _CONNECT_STRINGS[7] = {
	"TXT_NET_DISCONNECTED",
	"TXT_NET_PLAYING",
	"TXT_NET_ESCAPED",
	"TXT_NET_DIED",
	"TXT_NET_FOUND_SECRET",
	"TXT_NET_ESCAPE_TUNNEL",
	"TXT_NET_RESERVED",
};
const char* _NET_DUMP_STRINGS[7] = {
	"TXT_NET_GAME_CLOSED",
	"TXT_NET_GAME_FULL",
	"TXT_NET_GAME_BETWEEN",
	"TXT_NET_GAME_NSELECT",
	"TXT_NET_GAME_NSTART",
	"TXT_NET_GAME_CONNECT",
	"TXT_NET_GAME_WRONGLEV",
};
const char* _MODE_NAMES[4] = {
	"TXT_ANARCHY",
	"TXT_TEAM_ANARCHY",
	"TXT_ANARCHY_W_ROBOTS",
	"TXT_COOPERATIVE",
};
const char* _MODEM_ERROR_MESS[6] = {
	"TXT_NO_DIAL_TONE",
	"TXT_BUSY",
	"TXT_NO_ANSWER",
	"TXT_NO_CARRIER",
	"TXT_VOICE",
	"TXT_ERR_MODEM_RETURN",
};
const char* _MENU_DIFFICULTY_TEXT[5] = {
	"TXT_DIFFICULTY_1",
	"TXT_DIFFICULTY_2",
	"TXT_DIFFICULTY_3",
	"TXT_DIFFICULTY_4",
	"TXT_DIFFICULTY_5",
};
const char* _MENU_DETAIL_TEXT[6] = {
	"TXT_DETAIL_1",
	"TXT_DETAIL_2",
	"TXT_DETAIL_3",
	"TXT_DETAIL_4",
	"TXT_DETAIL_5",
	"TXT_DETAIL_CUSTOM_",
};
const char* _ORDINAL_TEXT[10] = {
	"TXT_1ST",
	"TXT_2ND",
	"TXT_3RD",
	"TXT_4TH",
	"TXT_5TH",
	"TXT_6TH",
	"TXT_7TH",
	"TXT_8TH",
	"TXT_9TH",
	"TXT_10TH",
};

const char* _SECONDARY_WEAPON_NAMES_VERY_SHORT[10] = {
	"TXT_CONCUSSION",
	"TXT_HOMING",
	"TXT_PROXBOMB",
	"TXT_SMART",
	"TXT_MEGA",
	"TXT_SMISSILE1_VS",
	"TXT_SMISSILE2_VS",
	"TXT_SMISSILE3_VS",
	"TXT_SMISSILE4_VS",
	"TXT_SMISSILE5_VS",
};

const char* _TRANSLATED_LEVEL_NAMES[40] = { };


