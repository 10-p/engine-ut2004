/*=============================================================================
	PixoDrv.h: Unreal Pixo GL defines support header.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain

=============================================================================*/

// Since I took the GL driver and ported/slashed it over to Pixo I added the
//  following defines so PixoDrv will build without gl.h.

#define GL_ADD                            0x0104
#define GL_TEXTURE                        0x1702
#define GL_REPLACE                        0x1E01
#define GL_MODULATE                       0x2100
#define GL_REPEAT                         0x2901
#define GL_INTERPOLATE                    0x8575
#define GL_CONSTANT                       0x8576
#define GL_PRIMARY_COLOR                  0x8577
#define GL_PREVIOUS                       0x8578
#define GL_SUBTRACT                       0x84E7
#define GL_CLAMP_TO_EDGE                  0x812F

// BlendingFactors
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_COLOR                      0x0306

