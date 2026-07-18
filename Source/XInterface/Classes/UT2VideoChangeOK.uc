// ====================================================================
//  Class:  XInterface.UT2VideoChangeOK
//  Parent: XInterface.GUIPage
//
//  <Enter a description here>
// ====================================================================

class UT2VideoChangeOK extends UT2K3GUIPage;

var		Int 		Count;
var		string		OrigRes;

var localized string	RestoreTextPre,
						RestoreTextPost,
						RestoreTextSingular;

event Timer()
{
	Count--;
	if (Count>1)
		GUILabel(Controls[4]).Caption = RestoreTextPre$Count$RestoreTextPost;
	else
		GUILabel(Controls[4]).Caption = RestoreTextSingular;

	if (Count<=0)
	{
		SetTimer(0);

		// Reset resolution here

		PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice Use16bit"@(InStr(OrigRes,"x16")!=-1));
		PlayerOwner().ConsoleCommand("setres"@OrigRes);

		Controller.CloseMenu(True);
	}
}

function Execute(string DesiredRes)
{
	local string res,bit,x,y;
	local int i;

	if (DesiredRes=="")
		return;

	res	= Controller.GetCurrentRes();
	bit = PlayerOwner().ConsoleCommand("get ini:Engine.Engine.RenderDevice Use16bit");

	if (bit=="true")
		OrigRes=res$"x16";
	else
		OrigRes=res$"x32";

	if(bool(PlayerOwner().ConsoleCommand("ISFULLSCREEN")))
		OrigRes=OrigRes$"f";
	else
		OrigRes=OrigRes$"w";

	PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice Use16bit"@(InStr(DesiredRes,"x16") != -1));
	PlayerOwner().ConsoleCommand("setres"@DesiredRes);

	i = Instr(DesiredRes,"x");
	x = left(DesiredRes,i);
	y = mid(DesiredRes,i+1);

	if( (int(x)<640) || (int(y)<480) )
	{
		PlayerOwner().ConsoleCommand("tempsetres 640x480");
		SetTimer(0,false);
		Controller.ReplaceMenu("xinterface.UT2DeferChangeRes");
		Controller.GameResolution = Left(DesiredRes,Len(DesiredRes) - 4);
	}
	else
		Controller.GameResolution = "";
}


function StartTimer()
{
	Count=15;
	SetTimer(1.0,true);
}

function bool InternalOnClick(GUIComponent Sender)
{
	SetTimer(0);
	if (Sender==Controls[2])
	{
		PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice Use16bit"@(InStr(OrigRes,"x16")!=-1));
		PlayerOwner().ConsoleCommand("setres"@OrigRes);
	}

	GUILabel(Controls[3]).Caption="Accept these settings?";
	Controller.CloseMenu(Sender == Controls[2]);

	return true;
}


defaultproperties
{

	Begin Object Class=GUIButton name=VidOKBackground
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="SquareBar"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=GUIButton'VidOKBackground'

	Begin Object Class=GUIButton Name=AcceptButton
		Caption="Keep Settings"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.125
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'AcceptButton'

	Begin Object Class=GUIButton Name=BackButton
		Caption="Restore Settings"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.65
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(2)=GUIButton'BackButton'

	Begin Object class=GUILabel Name=VideoOKDesc
		Caption="Accept these settings?"
		TextALign=TXTA_Center
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.4
		WinHeight=32
	End Object
	Controls(3)=GUILabel'VideoOKDesc'

	Begin Object class=GUILabel Name=VideoOkTimerDesc
		Caption="(Original settings will be restored in 15 seconds)"
		TextALign=TXTA_Center
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2MenuFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.46
		WinHeight=32
	End Object
	Controls(4)=GUILabel'VideoOKTimerDesc'

	WinLeft=0
	WinTop=0.375
	WinWidth=1
	WinHeight=0.25
	OnActivate=StartTimer

	RestoreTextPre="(Original settings will be restored in "
	RestoreTextPost=" seconds)"
	RestoreTextSingular="(Original settings will be restored in 1 second)"
	InactiveFadeColor=(R=128,G=128,B=128,A=255)
}
