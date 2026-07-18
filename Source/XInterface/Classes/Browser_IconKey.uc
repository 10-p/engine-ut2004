class Browser_IconKey extends UT2K3GUIPage;

function bool InternalOnClick(GUIComponent Sender)
{
	Controller.CloseMenu(false);
	return true;
}



function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;

	Super.InitComponent(MyController, MyOwner);

	for(i=0; i<7; i++)
	{
		GUIImage(Controls[3+2*i]).Image = class'Browser_ServersList'.default.Icons[i];
		GUILabel(Controls[4+2*i]).Caption = class'Browser_ServersList'.default.IconDescriptions[i];
	}
}


defaultproperties
{
	Begin Object Class=GUIButton name=DialogBackground
		WinWidth=0.500000
		WinHeight=0.556251
		WinLeft=0.250000
		WinTop=0.256667
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="ListBox"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=GUIButton'DialogBackground'
	
	Begin Object Class=GUIButton Name=OkButton
		Caption="OK"
		WinWidth=0.200000
		WinHeight=0.040000
		WinLeft=0.400000
		WinTop=0.75
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'OkButton'

	Begin Object class=GUILabel Name=DialogText
		Caption="Server Icon Key"
		TextALign=TXTA_Center
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.000000
		WinTop=0.278334
	End Object
	Controls(2)=GUILabel'DialogText'




	Begin Object Class=GUIImage name=Icon1
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16
		WinHeight=16
		WinLeft=0.3
		WinTop=0.35
	End Object
	Controls(3)=GUIImage'Icon1'

	Begin Object Class=GUILabel name=Label1
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.360000
		WinTop=0.340000
		StyleName="TextLabel"
	End Object
	Controls(4)=GUILabel'Label1'


	Begin Object Class=GUIImage name=Icon2
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16.000000
		WinHeight=16.000000
		WinLeft=0.300000
		WinTop=0.41
	End Object
	Controls(5)=GUIImage'Icon2'

	Begin Object Class=GUILabel name=Label2
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.36
		WinTop=0.40
		StyleName="TextLabel"
	End Object
	Controls(6)=GUILabel'Label2'


	Begin Object Class=GUIImage name=Icon3
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16.000000
		WinHeight=16.000000
		WinLeft=0.300000
		WinTop=0.47
	End Object
	Controls(7)=GUIImage'Icon3'

	Begin Object Class=GUILabel name=Label3
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.36
		WinTop=0.46
		StyleName="TextLabel"
	End Object
	Controls(8)=GUILabel'Label3'



	Begin Object Class=GUIImage name=Icon4
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16.000000
		WinHeight=16.000000
		WinLeft=0.300000
		WinTop=0.53
	End Object
	Controls(9)=GUIImage'Icon4'

	Begin Object Class=GUILabel name=Label4
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.36
		WinTop=0.52
		StyleName="TextLabel"
	End Object
	Controls(10)=GUILabel'Label4'



	Begin Object Class=GUIImage name=Icon5
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16.000000
		WinHeight=16.000000
		WinLeft=0.300000
		WinTop=0.59
	End Object
	Controls(11)=GUIImage'Icon5'

	Begin Object Class=GUILabel name=Label5
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.36
		WinTop=0.58
		StyleName="TextLabel"
	End Object
	Controls(12)=GUILabel'Label5'



	Begin Object Class=GUIImage name=Icon6
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16.000000
		WinHeight=16.000000
		WinLeft=0.300000
		WinTop=0.65
	End Object
	Controls(13)=GUIImage'Icon6'

	Begin Object Class=GUILabel name=Label6
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.36
		WinTop=0.64
		StyleName="TextLabel"
	End Object
	Controls(14)=GUILabel'Label6'

	Begin Object Class=GUIImage name=Icon7
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=16.000000
		WinHeight=16.000000
		WinLeft=0.300000
		WinTop=0.71
	End Object
	Controls(15)=GUIImage'Icon7'

	Begin Object Class=GUILabel name=Label7
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.350000
		WinHeight=32.000000
		WinLeft=0.36
		WinTop=0.70
		StyleName="TextLabel"
	End Object
	Controls(16)=GUILabel'Label7'

	WinLeft=0
	WinTop=0
	WinWidth=1
	WinHeight=1
	bRequire640x480=false	
}
