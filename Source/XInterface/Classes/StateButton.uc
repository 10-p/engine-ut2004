//-----------------------------------------------------------
// This is used for the MP3 player and the demo menu.  Lot's
// of native stuff going on under the hood so consider it special case.
//-----------------------------------------------------------
class StateButton extends GUIButton
	native;

cpptext
{
	void Draw(UCanvas* Canvas);
}

var() material 			Images[5];
var() eImgStyle			ImageStyle;			// How should we display this image

DefaultProperties
{
	ImageStyle=ISTY_Scaled
}
