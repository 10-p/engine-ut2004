class GUIMultiColumnListHeader extends GUIComponent
	native;

cpptext
{
	UBOOL MousePressed(UBOOL IsRepeat);
	UBOOL MouseReleased();
	UBOOL MouseMove(INT XDelta, INT YDelta);
	UBOOL MouseHover();
	void Draw(UCanvas* Canvas);
	void PreDraw(UCanvas* Canvas);
}

var() GUIMultiColumnList MyList;
var() editconst const int SizingCol;
var() editconst const int ClickingCol;

var	GUIStyles	BarStyle;
var string		BarStyleName;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local eFontScale x;
	Super.InitComponent(MyController, MyOwner);

    if (BarStyleName!="")
    	BarStyle = Controller.GetStyle(BarStyleName,x);

}


defaultproperties
{
	StyleName="SectionHeaderTop"
    BarStyleName="SectionHeaderBar"
	bCaptureTabs=False
	bNeverFocus=False
	bTabStop=False
	bAcceptsInput=True
	SizingCol=-1
	ClickingCol=-1
	bRequiresStyle=True
}
