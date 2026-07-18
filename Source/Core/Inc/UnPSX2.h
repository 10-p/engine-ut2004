/*=============================================================================
	UnPSX2.h: PSX2 specific declarations.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#if __PSX2_EE__

/*-----------------------------------------------------------------------------
	Shift-JIS2 conversion functions
-----------------------------------------------------------------------------*/

char appSjis2Ascii(const unsigned char *);
void appSjis2AsciiString(const unsigned char *, char *);
int appIsSjis(unsigned char *);
long appIsAscii(char *);
u_short appAscii2Sjis(unsigned char);
void appAsciiString2Sjis(const u_char *, u_short *);
short appSwapShort(u_short);

/*-----------------------------------------------------------------------------
	Evil Music/CD read syncronization stuff.
-----------------------------------------------------------------------------*/

// these are implemented in PSX2Audio
void appPauseMusic();
void appResumeMusic();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

