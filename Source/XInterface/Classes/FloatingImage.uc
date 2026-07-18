//==============================================================================
//	Created on: 10/10/2003
//	This is a GUIImage that is not full screen, but should render beneath everything else
//
//	Written by Ron Prestenback
//	© 2003, Epic Games, Inc.  All Rights Reserved
//==============================================================================
class FloatingImage extends GUIImage;

DefaultProperties
{
	RenderWeight=0.000001

	WinTop=0.375
	WinHeight=0.35
	WinWidth=0.5
	WinLeft=0.25

	ImageStyle=ISTY_PartialScaled

	DropShadow=Material'2K4Menus.Floating.FloatingShadow'
	DropShadowX=6
	DropShadowY=6

	bBoundToParent=True
	bScaleToParent=True
}
