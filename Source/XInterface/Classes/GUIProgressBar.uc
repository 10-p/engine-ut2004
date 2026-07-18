class GUIProgressBar extends GUIComponent
	Native;

cpptext
{
	void Draw(UCanvas* Canvas);
}

var Material	BarBack;		// The unselected portion of the bar
var Material	BarTop;			// The selected portion of the bar
var Color		BarColor;		// The Color of the Bar
var float		Low;			// The minimum value we should see
var float		High;			// The maximum value we should see
var float		Value;			// The current value (not clamped)

var float		CaptionWidth;	// The space reserved to the Caption
var eTextAlign	CaptionAlign;	// How align the text
var eTextAlign	ValueRightAlign;	//
var localized string Caption;	// The Caption itself
var string		FontName;		// Which font to use for display
var string		ValueFontName;	// Font to use for displaying values, use FontName if ValueFontName==""

var float	GraphicMargin;		// How Much margin to trim from graphic (X Margin only)
var float	ValueRightWidth;	// Space to leave free on right side
var bool	bShowLow;			// Show Low(Minimum) left of Bar
var bool	bShowHigh;			// Show High (Maximum) right of Bar
var bool	bShowValue;			// Show the value right of the Bar (like 75 or 75/100)
var int		NumDecimals;		// Number of decimals to display

var eDrawDirection BarDirection;

defaultproperties
{
	BarBack=Material'InterfaceContent.Menu.BorderBoxD'
	BarTop=Material'InterfaceContent.Menu.StatusBarInner'
	BarColor=(R=255,G=203,B=0,A=255)
	Low=0.0
	High=100.0
	Value=0.0
	bShowLow=false
	bShowHigh=false
	bShowValue=true
	CaptionWidth=0.45
	ValueRightWidth=0.2
	ValueRightAlign=TXTA_Right
	NumDecimals=0
	FontName="UT2MenuFont"
	BarDirection=DRD_LeftToRight
}