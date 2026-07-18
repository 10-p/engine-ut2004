// ====================================================================
//  Class:  XInterface.Tab_VideoSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_VideoSettings extends UT2K3TabPanel;

#exec OBJ LOAD FILE=InterfaceContent.utx

var		bool ShowSShot;

struct DisplayMode
{
	var int	Width,
			Height;
};

var DisplayMode DisplayModes[23];

var localized string	BitDepthText[2];

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;
	Super.Initcomponent(MyController, MyOwner);

	for (i=0;i<Controls.Length;i++)
		Controls[i].OnChange=InternalOnChange;

	moComboBox(Controls[1]).AddItem(BitDepthText[0]);
	moComboBox(Controls[1]).AddItem(BitDepthText[1]);
	moComboBox(Controls[1]).ReadOnly(true);
	
	CheckSupportedResolutions();
	
	Controls[6].FriendlyLabel  = GUILabel(Controls[5]);
	Controls[8].FriendlyLabel  = GUILabel(Controls[7]);
	Controls[10].FriendlyLabel = GUILabel(Controls[9]);
	
}

function CheckSupportedResolutions()
{
	local int		HighestRes;
	local int		Index;
	local int		BitDepth;
	local string	CurrentSelection;

	CurrentSelection = moComboBox(Controls[0]).MyComboBox.Edit.GetText();
	if(moComboBox(Controls[0]).ItemCount() > 0)
	moComboBox(Controls[0]).RemoveItem(0,moComboBox(Controls[0]).ItemCount());

	if(moComboBox(Controls[1]).GetText()==BitDepthText[0])
		BitDepth = 16;
	else
		BitDepth = 32;

	// Don't let user create non-fullscreen window bigger than highest
	//  supported resolution, or MacOS X client crashes. --ryan.
	if(!moCheckBox(Controls[2]).IsChecked()) // Controls[2] == fs toggle.
	{
		HighestRes = 0;
		for(Index = 0;Index < ArrayCount(DisplayModes);Index++)
		{
			if (PlayerOwner().ConsoleCommand(
					"SupportedResolution"$
					" WIDTH="$DisplayModes[Index].Width$
					" HEIGHT="$DisplayModes[Index].Height$
					" BITDEPTH="$BitDepth) == "1")
			{
				HighestRes = Index;   // biggest resolution hardware supports.
			}
		}

		for(Index = 0;Index <= HighestRes;Index++)
		{
			moComboBox(Controls[0]).AddItem(DisplayModes[Index].Width$"x"$DisplayModes[Index].Height);
		}
	}

	else  // Set dropdown for fullscreen modes...
	{
		for(Index = 0;Index < ArrayCount(DisplayModes);Index++)
		{
			if (PlayerOwner().ConsoleCommand(
				"SupportedResolution"$
				" WIDTH="$DisplayModes[Index].Width$
				" HEIGHT="$DisplayModes[Index].Height$
				" BITDEPTH="$BitDepth) == "1")
			{
				moComboBox(Controls[0]).AddItem(DisplayModes[Index].Width$"x"$DisplayModes[Index].Height);
			}
		}
    }

	moComboBox(Controls[0]).SetText(CurrentSelection);
}

function Refresh()
{
	InternalOnLoadINI(Controls[0],"");
	InternalOnLoadINI(Controls[1],"");
	InternalOnLoadINI(Controls[2],"");
}
	
function InternalOnLoadINI(GUIComponent Sender, string s)
{
	local string temp;

	if (Sender==Controls[0])
	{
		// Resolution
		if(Controller.GameResolution != "")
			moComboBox(Controls[0]).SetText(Controller.GameResolution);
		else
			moComboBox(Controls[0]).SetText(Controller.GetCurrentRes());
	}
	if (Sender==Controls[1])
	{
		if (PlayerOwner().ConsoleCommand("get ini:Engine.Engine.RenderDevice Use16bit") ~= "true")
			moComboBox(Sender).SetText(BitDepthText[0]);
		else
			moComboBox(Sender).SetText(BitDepthText[1]);

		CheckSupportedResolutions();
	}
	else if (Sender==Controls[2])
	{
		Temp = Sender.PlayerOwner().ConsoleCommand("ISFULLSCREEN");
		moCheckBox(Sender).Checked(bool(Temp));	
		CheckSupportedResolutions();
	}			
}		
		 
function string InternalOnSaveINI(GUIComponent Sender); 		// Do the actual work here

function InternalOnChange(GUIComponent Sender)
{
	if (!Controller.bCurMenuInitialized)
		return;

	if (Sender==Controls[0] || Sender==Controls[1] || Sender==Controls[2] )
	{
		Controls[3].bVisible=true;
		
		if(Sender != Controls[0])
			CheckSupportedResolutions();
	}

	else if (Sender==Controls[6])
		PlayerOwner().ConsoleCommand("GAMMA"@GUISlider(Controls[6]).Value);

	else if (Sender==Controls[8])
		PlayerOwner().ConsoleCommand("BRIGHTNESS"@GUISlider(Controls[8]).Value);
		
	else if (Sender==Controls[10])
		PlayerOwner().ConsoleCommand("CONTRAST"@GUISlider(Controls[10]).Value);
}			

function bool ApplyChanges(GUIComponent Sender)
{
	local string DesiredRes;

	DesiredRes = moComboBox(Controls[0]).MyComboBox.Edit.GetText();
	
	if (moComboBox(Controls[1]).GetText()==BitDepthText[0])
		DesiredRes=DesiredRes$"x16";
	else
		DesiredRes=DesiredRes$"x32";

	if (moCheckBox(Controls[2]).IsChecked())
		DesiredRes=DesiredRes$"f";
	else
		DesiredRes=DesiredRes$"w";

	Controls[3].bVisible=false;
		
	if ( Controller.OpenMenu("xinterface.UT2VideoChangeOK") )
		UT2VideoChangeOK(Controller.TopPage()).Execute(DesiredRes);

	return true;
}

function bool InternalOnClick(GUIComponent Sender)
{
	showsshot = !showsshot;
	
	Controls[12].bVisible = !ShowSShot;
	Controls[13].bVisible = !ShowSShot;
	
	if (showsshot)
		GUIImage(Controls[11]).Image = material'GammaSet1';
	else
		GUIImage(Controls[11]).Image = material'GammaSet0';
		
	return true;
}						

defaultproperties
{
	Begin Object class=moComboBox Name=VideoResolution
		WinWidth=0.390000
		WinHeight=0.050000
		WinLeft=0.124258
		WinTop=0.047396
		Caption="Resolution"
		INIOption="@INTERNAL"
		INIDefault="640x480"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Select the video resolution at which you wish to play."
		bReadOnly=true
		CaptionWidth=0.375
	End Object
	Controls(0)=moComboBox'VideoResolution'		

	Begin Object class=moComboBox Name=VideoColorDepth
		WinWidth=0.390000
		WinHeight=0.050000
		WinLeft=0.121484
		WinTop=0.152345
		Caption="Color Depth"
		INIOption="@Internal"
		INIDefault="false"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Select the maximum number of colors to display at one time."
		CaptionWidth=0.375		
	End Object
	Controls(1)=moComboBox'VideoColorDepth'		

	Begin Object class=moCheckBox Name=VideoFullScreen
		WinWidth=0.350000
		WinHeight=0.040000
		WinLeft=0.667226
		WinTop=0.047396
		Caption="Full Screen"
		INIOption="@Internal"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Check this box to run the game full screen."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.375		
	End Object
	Controls(2)=moCheckBox'VideoFullScreen'		

	Begin Object class=GUIButton Name=VideoApply
		WinWidth=0.25000
		WinHeight=0.050000
		WinLeft=0.667226
		WinTop=0.152345
		Caption="Apply Changes"
		Hint="Apply all changes to your video settings."
		bVisible=false;
		OnClick=ApplyChanges
	End Object
	Controls(3)=GUIButton'VideoApply'		

	
	Begin Object class=GUIImage Name=GammaBK
		WinWidth=0.957500
		WinHeight=0.697273
		WinLeft=0.021641
		WinTop=0.280365
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(4)=GUIImage'GammaBK'
	
	
	Begin Object class=GUILabel Name=GammaLabel
		Caption="Gamma"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.060547
		WinTop=0.341146
		StyleName="TextLabel"
	End Object
	Controls(5)=GUILabel'GammaLabel'

	Begin Object class=GUISlider Name=GammaSlider
		WinWidth=0.250000
		WinLeft=0.062500
		WinTop=0.402345
		MinValue=0.5
		MaxValue=2.5
		INIOption="ini:Engine.Engine.ViewportManager Gamma"
		INIDefault="0.8"
		Hint="Use the slider to adjust the Gamma to suit your monitor."
	End Object
	Controls(6)=GUISlider'GammaSlider'

	
	Begin Object class=GUILabel Name=BrightnessLabel
		Caption="Brightness"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.061524
		WinTop=0.555990
		StyleName="TextLabel"
	End Object
	Controls(7)=GUILabel'BrightnessLabel'
	
	Begin Object class=GUISlider Name=BrightnessSlider
		WinWidth=0.250000
		WinLeft=0.062500
		WinTop=0.623699
		MinValue=0
		MaxValue=1
		INIOption="ini:Engine.Engine.ViewportManager Brightness"
		INIDefault="0.8"
		Hint="Use the slider to adjust the Brightness to suit your monitor."
	End Object
	Controls(8)=GUISlider'BrightnessSlider'

	Begin Object class=GUILabel Name=ContrastLabel
		Caption="Contrast"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.059570
		WinTop=0.790365
		StyleName="TextLabel"
	End Object
	Controls(9)=GUILabel'ContrastLabel'
	
	Begin Object class=GUISlider Name=ContrastSlider
		WinWidth=0.250000
		WinLeft=0.062500
		WinTop=0.851565
		MinValue=0
		MaxValue=1
		INIOption="ini:Engine.Engine.ViewportManager Contrast"
		INIDEfault="0.8"
		Hint="Use the slider to adjust the Contrast to suit your monitor."
	End Object
	Controls(10)=GUISlider'ContrastSlider'

	Begin Object class=GUIImage Name=GammaBar
		WinWidth=0.400000
		WinHeight=0.500000
		WinLeft=0.454102
		WinTop=0.377604
		Image=material'GammaSet1'
		ImageColor=(R=255,G=255,B=255,A=255);
	End Object
	Controls(11)=GUIImage'GammaBar'

	Begin Object class=GUILabel Name=BrightDesc1
		Caption="Adjust the Gamma Setting so that the"
		TextALign=TXTA_Center
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2MenuFont"
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.151367
		WinTop=0.8
		bVisible=false
	End Object
	Controls(12)=GUILabel'BrightDesc1'

	Begin Object class=GUILabel Name=BrightDesc2
		Caption="Square is completely black."
		TextALign=TXTA_Center
		TextFont="UT2MenuFont"
		TextColor=(R=230,G=200,B=0,A=255)
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.136719
		WinTop=0.870000
		bVisible=false
	End Object
	Controls(13)=GUILabel'BrightDesc2'
	
	Begin Object class=GUIButton Name=VideoLeft
		WinWidth=0.043555
		WinHeight=0.084414
		WinLeft=0.397656
		WinTop=0.573959
		bNeverFocus=true
		bRepeatClick=true
		OnClick=InternalOnClick
		bVisible=false
		StyleName="ArrowLeft"		
	End Object
	Controls(14)=GUIButton'VideoLeft'	

	Begin Object class=GUIButton Name=VideoRight
		WinWidth=0.043555
		WinHeight=0.084414
		WinLeft=0.864063
		WinTop=0.573959
		StyleName="ArrowRight"
		bNeverFocus=true
		bRepeatClick=true
		OnClick=InternalOnClick
		bVisible=false
	End Object
	Controls(15)=GUIButton'VideoRight'	
	
	
	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.74
	bAcceptsInput=false
	
	DisplayModes(0)=(Width=320,Height=240)
	DisplayModes(1)=(Width=512,Height=384)
	DisplayModes(2)=(Width=640,Height=480)
	DisplayModes(3)=(Width=720,Height=480)  // Some mac powerbooks
	DisplayModes(4)=(Width=800,Height=500)
	DisplayModes(5)=(Width=800,Height=600)
	DisplayModes(6)=(Width=840,Height=524)  // iMac g5 20"
	DisplayModes(7)=(Width=896,Height=600)  // Some mac powerbooks
	DisplayModes(8)=(Width=1024,Height=640)
	DisplayModes(9)=(Width=1024,Height=768)
	DisplayModes(10)=(Width=1152,Height=720)
	DisplayModes(11)=(Width=1152,Height=864)
	DisplayModes(12)=(Width=1280,Height=800)
	DisplayModes(13)=(Width=1280,Height=854) // Some mac powerbooks
	DisplayModes(14)=(Width=1280,Height=960)
	DisplayModes(15)=(Width=1280,Height=1024)
	DisplayModes(16)=(Width=1344,Height=840)  // imac G5 20"
	DisplayModes(17)=(Width=1440,Height=900)  // Some mac powerbooks
	DisplayModes(18)=(Width=1600,Height=1200)
	DisplayModes(19)=(Width=1680,Height=1050)
	DisplayModes(20)=(Width=1920,Height=1200)  // Apple 23" display
	DisplayModes(21)=(Width=2048,Height=1280)  // Apple 30" display
	DisplayModes(22)=(Width=2560,Height=1600)  // Apple 30" display

	BitDepthText(0)="16-bit Color"
	BitDepthText(1)="32-bit Color"
}
