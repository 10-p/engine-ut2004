// ====================================================================
//  Class:  XInterface.Tab_SPLadderTeam
//  Parent: XInterface.Tab_SPLadderBase
//  Single player menu for for selecting which match to play.
//  author:  capps 8/24/02
// ====================================================================

class Tab_SPLadderTeam extends Tab_SPLadderBase;

#exec OBJ LOAD FILE=InterfaceContent.utx

var array<LadderButton> TDMButtons;
var array<LadderButton> DOMButtons;
var array<LadderButton> BRButtons;
var array<LadderButton> CTFButtons;
var array<int>			LockIndexes;
var GUIGfxButton		ChampButton;
var GUIImage			ChampBorder;

function InitPanel()
{
local int i;

	ScrollInfo=GUIScrollTextBox(Controls[4]);
	MapPicHolder=GUIImage(Controls[1]);
	MapNameLabel=GUILabel(Controls[3]);
	ChampButton=GUIGfxButton(Controls[29]);
	ChampBorder=GUIImage(Controls[30]);

	// Create TDMButtons Array
	for (i=0; i<6; i++)
		TDMButtons[TDMButtons.Length] = NewLadderButton(1, i, 0.050195, 0.799215 - 0.128333 * i);

	for (i=0; i<5; i++)
		DOMButtons[DOMButtons.Length] = NewLadderButton(2, i, 0.176172, 0.799215 - 0.128333 * i);

	for (i=0; i<5; i++)
		BRButtons[BRButtons.Length] = NewLadderButton(4, i, 0.733789, 0.799215 - 0.128333 * i);

	for (i=0; i<6; i++)
		CTFButtons[CTFButtons.Length] = NewLadderButton(3, i, 0.860742, 0.799215 - 0.128333 * i);

	ScrollInfo.SetContent("");

	OnProfileUpdated();
}

function OnProfileUpdated()
{
local int i;

	for (i=0; i<6; i++)
	{
		UpdateLadderButton(TDMButtons[i], 1, i);
		UpdateLadderButton(CTFButtons[i], 3, i);
	}
		
	for (i=0; i<5; i++)
	{
		UpdateLadderButton(DOMButtons[i], 2, i);
		UpdateLadderButton(BRButtons[i], 4, i);
	}

	ChampBorder.bVisible=false;

	// Set the Yellow Bars and the Lock picture for completed ladders
	SetYellowBar(1, 12, TDMButtons);
	SetYellowBar(2, 20, DOMButtons); 	
	SetYellowBar(4, 24, BRButtons); 	
	SetYellowBar(3, 16, CTFButtons);
	SetChampionship(false);

	// Figure out which map info to display, always display lowest uncompleted ladder's next map
	if (GetProfile() != None)
	{
		GetProfile().ChampBorderObject = ChampBorder;

		if (!SetActiveMatch(1, TDMButtons) && !SetActiveMatch(2, DOMButtons) && !SetActiveMatch(3, CTFButtons) && !SetActiveMatch(4, BRButtons))
		{
			if (GetProfile().LadderRung[5] >= 0) {
				Log ( "SINGLEPLAYER Opening Championship." );
				SetChampionship(true);		// turn on button
				ChampMatch(ChampButton);	// and select it
			}
		}
	}
	// Else Display a No Picture avail (default picture)
}

function int SetYellowBar(int ladder, int index, out array<LadderButton> Buttons)
{
local int Rung, lockindex;
local string HiStr;

	Super.SetYellowBar(ladder, index, Buttons);

	if (GetProfile() != None)
	{
		Rung = GetProfile().LadderRung[ladder];
		lockindex = ladder;
		// Reset Lock pictures with the bars going to them

		HiStr = "";
		if (Rung == Buttons.Length)
			HiStr = "Hi";


		GUIImage(Controls[4+lockindex]).Image = DLOMaterial("InterfaceContent.SPMenu.Lock"$lockindex$HiStr);
		if (ladder < 3)
			GUIImage(Controls[index-2]).Image = DLOMaterial("InterfaceContent.SPMenu.BarCornerLeft"$HiStr);
		else
			GUIImage(Controls[index-2]).Image = DLOMaterial("InterfaceContent.SPMenu.BarCornerRight"$HiStr);
		GUIImage(Controls[index-3]).Image = DLOMaterial("InterfaceContent.SPMenu.BarHorizontal"$HiStr);
	}

	if (HiStr == "Hi")
		return 1;
	else 
		return 0;
}

// received click from the championship button
function bool ChampMatch (GUIComponent Sender) 
{
	local LadderButton LButton;
	local GameProfile GP;

	GP = GetProfile();
	if (GP != None ) 
	{
		if ( GP.ChampBorderObject != none ) 
			GUIImage(GP.ChampBorderObject).bVisible=true;
		GP.CurrentMenuRung = GP.LadderRung[5];
		GP.CurrentLadder = 5;
		LButton=LadderButton(GP.NextMatchObject);
		if ( LButton != none ) 
		{
			LButton.SetState(GP.LadderRung[LButton.LadderIndex]);
			LButton = none;
		}
		ShowMatchInfo( GP.GetMatchInfo ( GP.CurrentLadder, GP.CurrentMenuRung ) );
		MatchUpdated(GP.CurrentLadder, GP.CurrentMenuRung);
	}
	return true;
}

// 
function SetChampionship(bool bEnable) 
{
	local MatchInfo MI;
	local GameProfile GP;

	GP = GetProfile();
	if (GP != None ) 
	{
		ChampButton.bVisible=bEnable;
		ChampButton.bAcceptsInput=bEnable;
		ChampButton.bNeverFocus=!bEnable;

		if ( bEnable ) 
		{
			// completed or not?
			if ( GP.LadderRung[5] >= GP.GameLadder.default.ChampionshipMatches.Length ) 
			{
				ChampButton.Graphic = DLOMaterial("PlayerPictures.aDOM");
				// show trophy
			} 
			else 
			{
				// show thumb
				MI = GP.GetMatchInfo( 5, GP.LadderRung[5] );
				ChampButton.Graphic = DLOMaterial("SinglePlayerThumbs."$MI.LevelName);		
			}
		}
	}
}

defaultproperties
{
	Begin Object Class=GUIImage Name=MapPicBack
		Image=Material'InterfaceContent.Menu.EditBox'
		WinWidth=0.413281
		WinHeight=0.294141
		WinLeft=0.293164
		WinTop=0.247917
		ImageStyle=ISTY_Stretched
	End Object

	Begin Object Class=GUIImage Name=MapPic
		Image=Material'InterfaceContent.Menu.NoLevelPreview'
		WinWidth=0.401563
		WinHeight=0.279493
		WinLeft=0.299219
		WinTop=0.255313
		ImageStyle=ISTY_Justified
		ImageAlign=IMGA_Center
	End Object

	Begin Object Class=GUIImage Name=MapInfoBack
		Image=Material'InterfaceContent.Menu.EditBox'
		WinWidth=0.412305
		WinHeight=0.361524
		WinLeft=0.294141
		WinTop=0.551823
		ImageStyle=ISTY_Stretched
	End Object

	Begin Object Class=GUILabel Name=MapInfoName
		Caption="No Match Selected"
		WinWidth=0.396680
		WinHeight=0.051406
		WinLeft=0.301172
		WinTop=0.562240
		TextAlign=TXTA_Center
		TextFont="UT2HeaderFont"
		TextColor=(R=220,G=180,B=0,A=255)	
	End Object

	Begin Object Class=GUIScrollTextBox Name=MapInfoScroll
		WinWidth=0.402539
		WinHeight=0.290234
		WinLeft=0.299023
		WinTop=0.616407
		CharDelay=0.0025
		EOLDelay=0.5
		bNeverFocus=true
		StyleName="NoBackground"
	End Object

	Begin Object Class=GUIImage Name=ImgLock1
		Image=Material'InterfaceContent.SPMenu.Lock1'
		WinWidth=0.062891
		WinHeight=0.087305
		WinLeft=0.437695
		WinTop=0.064322
		ImageStyle=ISTY_Scaled
		ImageAlign=IMGA_Center
	End Object

	Begin Object Class=GUIImage Name=ImgLock2
		Image=Material'InterfaceContent.SPMenu.Lock2'
		WinWidth=0.062891
		WinHeight=0.087305
		WinLeft=0.437695
		WinTop=0.150260
		ImageStyle=ISTY_Scaled
		ImageAlign=IMGA_Center
	End Object

	Begin Object Class=GUIImage Name=ImgLock3
		Image=Material'InterfaceContent.SPMenu.Lock3'
		WinWidth=0.062891
		WinHeight=0.087305
		WinLeft=0.500195
		WinTop=0.064322
		ImageStyle=ISTY_Scaled
		ImageAlign=IMGA_Center
	End Object

	Begin Object Class=GUIImage Name=ImgLock4
		Image=Material'InterfaceContent.SPMenu.Lock4'
		WinWidth=0.062891
		WinHeight=0.087305
		WinLeft=0.500195
		WinTop=0.150260
		ImageStyle=ISTY_Scaled
		ImageAlign=IMGA_Center
	End Object

	// BAR LEADING TO TDM LADDER

	Begin Object Class=GUIImage Name=TopLeftBar
		Image=Material'InterfaceContent.SPMenu.BarHorizontal'
		WinWidth=0.338086
		WinHeight=0.003906
		WinLeft=0.099218
		WinTop=0.105729
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=TopLeftCorner
		Image=Material'InterfaceContent.SPMenu.BarCornerLeft'
		WinWidth=0.003906
		WinHeight=0.003906
		WinLeft=0.095312
		WinTop=0.105729
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=TDMBar1
		Image=Material'InterfaceContent.SPMenu.BarVertical'
		WinWidth=0.003906
		WinLeft=0.095312
		WinTop=0.109896
		WinHeight=0.682227
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=TDMBar2
		Image=Material'InterfaceContent.SPMenu.BarVerticalHi'
		WinWidth=0.003906
		WinLeft=0.095312
		WinTop=0.109896
		WinHeight=0.682227
		ImageStyle=ISTY_Scaled
		bVisible=false
	End Object

	// BAR LEADING TO CTF LADDER

	Begin Object Class=GUIImage Name=TopRightBar
		Image=Material'InterfaceContent.SPMenu.BarHorizontal'
		WinWidth=0.338086
		WinHeight=0.003906
		WinLeft=0.563085
		WinTop=0.105729
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=TopRightCorner
		Image=Material'InterfaceContent.SPMenu.BarCornerRight'
		WinWidth=0.003906
		WinHeight=0.003906
		WinLeft=0.901171
		WinTop=0.105729
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=CTFBar1
		Image=Material'InterfaceContent.SPMenu.BarVertical'
		WinWidth=0.003906
		WinLeft=0.901171
		WinTop=0.109896
		WinHeight=0.682227
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=CTFBar2
		Image=Material'InterfaceContent.SPMenu.BarVerticalHi'
		WinWidth=0.003906
		WinLeft=0.901171
		WinTop=0.109896
		WinHeight=0.682227
		ImageStyle=ISTY_Scaled
		bVisible=false
	End Object

	// BAR LEADING TO DOM LADDER

	Begin Object Class=GUIImage Name=MidLeftBar
		Image=Material'InterfaceContent.SPMenu.BarHorizontal'
		WinWidth=0.215039
		WinHeight=0.003906
		WinLeft=0.222266
		WinTop=0.194218
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=MidLeftCorner
		Image=Material'InterfaceContent.SPMenu.BarCornerLeft'
		WinWidth=0.003906
		WinHeight=0.003906
		WinLeft=0.219336
		WinTop=0.194218
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=DOMBar1
		Image=Material'InterfaceContent.SPMenu.BarVertical'
		WinWidth=0.003906
		WinHeight=0.594688
		WinLeft=0.219336
		WinTop=0.198125
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=DOMBar2
		Image=Material'InterfaceContent.SPMenu.BarVerticalHi'
		WinWidth=0.003906
		WinHeight=0.594688
		WinLeft=0.219336
		WinTop=0.198125
		ImageStyle=ISTY_Scaled
		bVisible=false
	End Object

	// BAR LEADING TO BR LADDER

	Begin Object Class=GUIImage Name=MidRightBar
		Image=Material'InterfaceContent.SPMenu.BarHorizontal'
		WinWidth=0.213086
		WinHeight=0.003906
		WinLeft=0.563085
		WinTop=0.194270
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=MidRightCorner
		Image=Material'InterfaceContent.SPMenu.BarCornerRight'
		WinWidth=0.003906
		WinHeight=0.003906
		WinLeft=0.776171
		WinTop=0.194271
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=BRBar1
		Image=Material'InterfaceContent.SPMenu.BarVertical'
		WinWidth=0.003906
		WinHeight=0.594687
		WinLeft=0.775976
		WinTop=0.197970
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=BRBar2
		Image=Material'InterfaceContent.SPMenu.BarVerticalHi'
		WinWidth=0.003906
		WinHeight=0.594687
		WinLeft=0.775976
		WinTop=0.197970
		ImageStyle=ISTY_Scaled
		bVisible=false
	End Object

	// Ladder Labels

	Begin Object Class=GUILabel Name=TDMLabel
		Caption="TEAM DEATHMATCH"
		TextColor=(R=192,B=192,G=192,A=255)
		bTransparent=true
		BackColor=(R=255,B=255,G=255,A=255)
		TextAlign=TXTA_Center
		WinWidth=0.347500
		WinHeight=0.043125
		WinLeft=0.118437
		WinTop=0.060833
	End Object

	Begin Object Class=GUILabel Name=CTFLabel
		Caption="CAPTURE THE FLAG"
		TextColor=(R=192,B=192,G=192,A=255)
		bTransparent=true
		BackColor=(R=255,B=255,G=255,A=255)
		TextAlign=TXTA_Center
		WinWidth=0.346250
		WinHeight=0.043125
		WinLeft=0.544064
		WinTop=0.060833
	End Object

	Begin Object Class=GUILabel Name=DOMLabel
		Caption="DOMINATION"
		TextColor=(R=192,B=192,G=192,A=255)
		bTransparent=true
		BackColor=(R=255,B=255,G=255,A=255)
		TextAlign=TXTA_Center
		WinWidth=0.223122
		WinHeight=0.043125
		WinLeft=0.242813
		WinTop=0.149167
	End Object

	Begin Object Class=GUILabel Name=BRLabel
		Caption="BOMBING RUN"
		TextColor=(R=192,B=192,G=192,A=255)
		bTransparent=true
		BackColor=(R=255,B=255,G=255,A=255)
		TextAlign=TXTA_Center
		WinWidth=0.221874
		WinHeight=0.043125
		WinLeft=0.537818
		WinTop=0.149166
	End Object

	// championship image
	Begin Object Class=GUIGfxButton Name=SPLChampButton
		Graphic=Material'InterfaceContent.SPMenu.Lock1'
		WinWidth=0.083925
		WinHeight=0.113688
		WinLeft=0.458633
		WinTop=0.094322
	    Position=ICP_Scaled
		bVisible=false
		bAcceptsInput=false
		OnClick=ChampMatch
		OnDblClick=MatchDoubleClick
		bRepeatClick=false
	End Object

	// championship border
	Begin Object Class=GUIImage Name=SPLChampBorder
		Image=Material'InterfaceContent.SP_FinalButton'
		WinWidth=0.125722
		WinHeight=0.172673
		WinLeft=0.437930
		WinTop=0.064895
		bVisible=false
		bAcceptsInput=false
		ImageStyle=ISTY_Scaled
	End Object



	Controls(0)=GUIImage'MapPicBack'
	Controls(1)=GUIImage'MapPic'
	Controls(2)=GUIImage'MapInfoBack'
	Controls(3)=GUILabel'MapInfoName'
	Controls(4)=GUIScrollTextBox'MapInfoScroll'
	Controls(5)=GUIImage'ImgLock1'
	Controls(6)=GUIImage'ImgLock2'
	Controls(7)=GUIImage'ImgLock3'
	Controls(8)=GUIImage'ImgLock4'
	Controls(9)=GUIImage'TopLeftBar'
	Controls(10)=GUIImage'TopLeftCorner'
	Controls(11)=GUIImage'TDMBar1'
	Controls(12)=GUIImage'TDMBar2'
	Controls(13)=GUIImage'TopRightBar'
	Controls(14)=GUIImage'TopRightCorner'
	Controls(15)=GUIImage'CTFBar1'
	Controls(16)=GUIImage'CTFBar2'
	Controls(17)=GUIImage'MidLeftBar'
	Controls(18)=GUIImage'MidLeftCorner'
	Controls(19)=GUIImage'DOMBar1'
	Controls(20)=GUIImage'DOMBar2'
	Controls(21)=GUIImage'MidRightBar'
	Controls(22)=GUIImage'MidRightCorner'
	Controls(23)=GUIImage'BRBar1'
	Controls(24)=GUIImage'BRBar2'
	Controls(25)=GUILabel'TDMLabel'
	Controls(26)=GUILabel'CTFLabel'
	Controls(27)=GUILabel'DOMLabel'
	Controls(28)=GUILabel'BRLabel'
	Controls(29)=GUIGfxButton'SPLChampButton'
	Controls(30)=GUIImage'SPLChampBorder'

	LockIndexes(0)=0
	LockIndexes(1)=1
	LockIndexes(2)=3
	LockIndexes(3)=2
	LockIndexes(4)=4

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false		
	bFillHeight=true;
}