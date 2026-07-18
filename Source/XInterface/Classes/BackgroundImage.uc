//==============================================================================
//	Created on: 10/10/2003
//	This class has been setup as a template for GUIImages that should render beneath
//  all other components and cover the entire screen
//
//	Written by Ron Prestenback
//	© 2003, Epic Games, Inc.  All Rights Reserved
//==============================================================================
class BackgroundImage extends GUIImage;

DefaultProperties
{
	RenderWeight=0
	ImageStyle=ISTY_Stretched
	ImageRenderStyle=MSTY_Normal

	WinLeft=0.0
	WinTop=0.0
	WinWidth=1.0
	WinHeight=1.0

	bScaleToParent=False
	bBoundToParent=False
}
