// ====================================================================
//  Written by Joe Wilcox
//  (c) 2002, Epic Games, Inc.  All Rights Reserved
// ====================================================================

class JoeTest extends TestPageBase;

#exec OBJ LOAD FILE=InterfaceContent.utx

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.Initcomponent(MyController, MyOwner);
	Controls[2].OnClick=OnClick1;
}

function bool OnClick1(GUIComponent Sender)
{
	Controller.OpenMenu("xinterface.joetestB");
	return true;
}

function bool OnClick2(GUIComponent Sender)
{
//	Controller.CloseMenu(false);
	return true;
}

defaultproperties
{
	Begin Object Class=GUIImage Name=TitleStrip
		Image=Texture'BorderBoxD'
		ImageStyle=ISTY_Stretched
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.9
		WinHeight=0.125
		WinLeft=0.05
		WinTop=0.05
		Name="TitleStrip"
	End Object
	Controls(0)=GUIImage'TitleStrip'
	Begin Object Class=GUILabel Name=TitleText
		Caption="Joe's Amazing Test GUI"
		TextFont="UT2LargeFont"
		TextAlign=TXTA_Center
		TextColor=(R=0,G=255,B=128,A=255)
		bTransparent=true
		WinWidth=0.9
		WinHeight=0.125
		WinLeft=0.05
		WinTop=0.05
		Name="TitleText"
	End Object
	Controls(1)=GUILabel'TitleText'
	Begin Object Class=GUIButton Name=TestButton1
		Caption="Close Window"
		WinWidth=0.5
		WinLeft=0.25
		WinTop=0.25
		Name="TestButton1"
	End Object
	Controls(2)=GUIButton'TestButton1'
	Begin Object class=GUINumericEdit Name=TestNumEdit
		WinWidth=0.125
		WinLeft=0.25
		WinTop=0.4
		MinValue=0
		MaxValue=999
		Name="TestNumEdit"
	End Object
	Controls(3)=GUINumericEdit'TestNumEdit'
	Begin Object class=GUIEditBox Name=TestNumEdit2
		WinWidth=0.5
		WinLeft=0.25
		WinTop=0.6
		IniOption="ini:Engine.Engine.RenderDevice HighDetailActors"
		Name="TestNumEdit2"
	End Object
	Controls(4)=GUIEditBox'TestNumEdit2'
	Background=Material'InterfaceContent.Backgrounds.bg10'
	WinWidth=1.0
	WinHeight=1.0
	WinTop=0.0
	WinLeft=0.0
	bCheckResolution=true		
}
