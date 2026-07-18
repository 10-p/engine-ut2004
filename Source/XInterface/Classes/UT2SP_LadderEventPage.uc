class UT2SP_LadderEventPage extends UT2K3GUIPage;

var GUILabel lblTitle, lblCaption;
var GUIImage gImage;
var GUIButton btnOK, btnMap;
var string TutorialName;

function InitComponent(GUIController pMyController, GUIComponent MyOwner)
{
	Super.Initcomponent(pMyController, MyOwner);
	lblTitle=GUILabel(Controls[0]);
	lblCaption=GUILabel(Controls[1]);
	btnOK=GUIButton(Controls[3]);
	gImage=GUIImage(Controls[2]);
	gImage.Image = Material(DynamicLoadObject("Laddershots.TeamDMShot", class'Material'));
	btnMap=GUIButton(Controls[5]);
	btnOK.OnClick=InternalOnClick;
	btnMap.OnClick=InternalOnClick;
}

function bool InternalOnClick(GUIComponent Sender)
{
	if (Sender==btnOK)
	{
		return Controller.CloseMenu();
	}
	else if (Sender==btnMap)
	{
		PlayerOwner().Level.Game.CurrentGameProfile.bInLadderGame=true;  // so that it'll reload into SP menus
		PlayerOwner().Level.Game.SavePackage(PlayerOwner().Level.Game.CurrentGameProfile.PackageName);
		PlayerOwner().ConsoleCommand ("START"@TutorialName$".ut2?quickstart=true?TeamScreen=false?savegame="$PlayerOwner().Level.Game.CurrentGameProfile.PackageName);
		Controller.CloseAll(false);
		return true;
	}
	return false;
}

function SetTutorialName(string tutname)
{
	TutorialName = tutname;
	if ( tutname == "" )
	{
		btnMap.bVisible=false;
		btnMap.bAcceptsInput=false;
	}
	else
	{
		btnMap.bVisible=true;
		btnMap.bAcceptsInput=true;
		BtnOK.WinLeft=0.2;
	}
}

defaultproperties
{
	Begin Object Class=GUILabel Name=SPLEPtitle
		WinWidth=1.00000
		WinHeight=0.100000
		WinLeft=0.00000
		WinTop=0.068750
		Caption="CONGRATULATIONS!"
		TextAlign=TXTA_Center
		TextFont="UT2LargeFont"
	End Object
	Begin Object Class=GUILabel Name=SPLEPcaption
		WinWidth=1.000000
		WinHeight=0.350000
		WinLeft=0.000000
		WinTop=0.716563
		TextAlign=TXTA_Center
		TextFont="UT2SmallFont"
		bMultiLine=true
	End Object
	Begin Object Class=GUIImage Name=SPLEPimage
		//Image=Material'Laddershots.TeamDMShot'
		ImageStyle=ISTY_Stretched
		WinTop=0.17
		WinLeft=0.25
		WinHeight=0.5
		WinWidth=0.5
		X1=0
		Y1=0
		X2=1023
		Y2=767
	End Object
	Begin Object Class=GUIButton Name=SPLEPOK
	    Caption="CONTINUE"
		WinWidth=0.200000
		WinHeight=0.050000
		WinLeft=0.400000
		WinTop=0.88
	End Object
	Begin Object Class=GUIImage Name=SPLEPimageborder
		Image=Material'InterfaceContent.BorderBoxA1'
		ImageStyle=ISTY_Stretched
		WinWidth=0.505078
		WinHeight=0.507421
		WinLeft=0.247265
		WinTop=0.166354
	End Object
	Begin Object Class=GUIButton Name=SPLEPMap
	    Caption="VIEW TUTORIAL"
		WinWidth=0.20
		WinHeight=0.050000
		WinLeft=0.6
		WinTop=0.88
		bVisible=false
		bAcceptsInput=false
	End Object

	Controls(0)=SPLEPtitle
	Controls(1)=SPLEPcaption
	Controls(2)=SPLEPimage
	Controls(3)=SPLEPOK
	Controls(4)=SPLEPimageborder
	Controls(5)=SPLEPMap

	Background=Material'InterfaceContent.Backgrounds.bg10'
	WinTop=0.0
	WinHeight=1.0
	WinLeft=0.0
	WinWidth=1.0

	//BackgroundColor=(R=64,G=64,B=64,A=255)
	//BackgroundRStyle=MSTY_Alpha
}
