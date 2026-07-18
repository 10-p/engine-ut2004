// ====================================================================
//  Class:  XInterface.Tab_AudioSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_AudioSettings extends UT2K3TabPanel;

#exec OBJ LOAD FILE=PickupSounds.uax
#exec OBJ LOAD FILE=AnnouncerMale2K4.uax


var localized string	AudioModes[4],
						VoiceModes[4],
						AnnounceModes[3];



var moComboBox APack;
var config APackInfo BonusPackInfo[4];    // Mod authors, do put anything in here since it
										  // will be overwritten by the bonus pack.  Use PackInfo instead

var config array<APackInfo> PackInfo;

var bool binitialized;
function InitComponent(GUIController MyController, GUIComponent MyOwner)
{

	local int i;
    local string DefPack;

	Super.Initcomponent(MyController, MyOwner);

    bInitialized = false;

	for (i=0;i<Controls.Length;i++)
		Controls[i].OnChange=InternalOnChange;

	for(i = 0;i < ArrayCount(AudioModes);i++)
		moComboBox(Controls[8]).AddItem(AudioModes[i]);

	for(i = 0;i < ArrayCount(VoiceModes);i++)
		moComboBox(Controls[14]).AddItem(VoiceModes[i]);


	for(i = 0;i < ArrayCount(AnnounceModes);i++)
		moComboBox(Controls[15]).AddItem(AnnounceModes[i]);

	Controls[3].FriendlyLabel = GUILabel(Controls[2]);
	Controls[5].FriendlyLabel = GUILabel(Controls[4]);

    APack = moComboBox(Controls[16]);

    DefPack = class'UnrealPlayer'.Default.CustomizedAnnouncerPack;

    for (i=0;i<4;i++)
    {
    	if (BonusPackInfo[i].Description != "")
        {
	        APack.AddItem(BonusPackInfo[i].Description,none,BonusPackInfo[i].PackageName);
	        if (BonusPackInfo[i].PackageName ~= DefPack)
	            APack.SetText(BonusPackInfo[i].Description);
        }
    }

    // Add user announcer packs

    for (i=0;i<PackInfo.Length;i++)
    {
    	APack.AddItem(PackInfo[i].Description,none,PackInfo[i].PackageName);
        if (PackInfo[i].PackageName ~= DefPack)
        	APack.SetText(PackInfo[i].Description);
    }

	bInitialized = true;

}

function InternalOnLoadINI(GUIComponent Sender, string s)
{
	local bool b1, b2, b3;

    if ( Sender==Controls[7] )
     	GUISlider(Controls[7]).SetValue(PlayerOwner().AnnouncerVolume);

	else if ( Sender==Controls[8] )
	{
	    B1 = bool( PlayerOwner().ConsoleCommand("get ini:Engine.Engine.AudioDevice UseEAX" ) );
	    B2 = bool( PlayerOwner().ConsoleCommand("get ini:Engine.Engine.AudioDevice CompatibilityMode" ) );
	    B3 = bool( PlayerOwner().ConsoleCommand("get ini:Engine.Engine.AudioDevice Use3DSound" ) );

	    if( B2 )
	        moComboBox(Controls[8]).SetText(AudioModes[3]);
	    else if( B1 )
			moComboBox(Controls[8]).SetText(AudioModes[2]);
		else if( B3 )
			moComboBox(Controls[8]).SetText(AudioModes[1]);
		else
			moComboBox(Controls[8]).SetText(AudioModes[0]);
	}

	else if (Sender==Controls[9])
	{
		moCheckBox(Sender).Checked(bool(s));
	}

	else if (Sender==Controls[14])
	{
	    if( !PlayerOwner().bNoVoiceMessages && !PlayerOwner().bNoVoiceTaunts && !PlayerOwner().bNoAutoTaunts )
			moComboBox(Controls[14]).SetText(VoiceModes[0]);
	    else if( !PlayerOwner().bNoVoiceMessages && !PlayerOwner().bNoVoiceTaunts && PlayerOwner().bNoAutoTaunts )
			moComboBox(Controls[14]).SetText(VoiceModes[1]);
	    else if( !PlayerOwner().bNoVoiceMessages && PlayerOwner().bNoVoiceTaunts && PlayerOwner().bNoAutoTaunts )
			moComboBox(Controls[14]).SetText(VoiceModes[2]);
	    else
		 	moComboBox(Controls[14]).SetText(VoiceModes[3]);
	}

	else if (Sender==Controls[10])
		moCheckBox(Sender).Checked(class'Hud'.default.bMessageBeep);

	else if (Sender==Controls[11])
		moCheckBox(Sender).Checked(PlayerOwner().bAutoTaunt);

	else if (Sender==Controls[12])
		moCheckBox(Sender).Checked(!PlayerOwner().bNoMatureLanguage);

	else if (Sender==Controls[13])
	{
	    B1 = PlayerOwner().Level.bLowSoundDetail;
		B2 = bool( PlayerOwner().ConsoleCommand("get ini:Engine.Engine.AudioDevice LowQualitySound" ) );

		// Make sure both are the same - LevelInfo.bLowSoundDetail take priority
		if(B1 != B2)
		{
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice LowQualitySound"@B1);
			PlayerOwner().ConsoleCommand("SOUND_REBOOT");

			// Restart music.
			if( PlayerOwner().Level.Song != "" && PlayerOwner().Level.Song != "None" )
				PlayerOwner().ClientSetMusic( PlayerOwner().Level.Song, MTRAN_Instant );
		}

		moCheckBox(Sender).Checked(B1);
	}

	else if (Sender==Controls[15])
		moComboBox(Sender).setIndex(PlayerOwner().AnnouncerLevel);


}

function string InternalOnSaveINI(GUIComponent Sender); 		// Do the actual work here



function InternalOnChange(GUIComponent Sender)
{
	local String t;
	local bool b1,b2,b3;

	if (!Controller.bCurMenuInitialized)
		return;

	if (Sender==Controls[3])
	{
   	    PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice MusicVolume "$GUISlider(Sender).Value);
	}

	else if (Sender==Controls[5])
	{
   	    PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice SoundVolume "$GUISlider(Sender).Value);
   	    PlayerOwner().ConsoleCommand("stopsounds");
		PlayerOwner().PlaySound(sound'PickupSounds.AdrenelinPickup');
	}

	else if (Sender==Controls[7])
	{
   	    PlayerOwner().AnnouncerVolume=GUISlider(Controls[7]).Value;
        PlayerOwner().SaveConfig();
        PlayerOwner().PlayRewardAnnouncement('Adrenalin', 0, true);
	}

	else if (Sender==Controls[8])
	{
		t = moComboBox(Sender).GetText();

		if (t==AudioModes[3])
		{
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice UseEAX false" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice Use3DSound false" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice CompatibilityMode true" );
		}
		else if (t==AudioModes[0])
		{
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice UseEAX false" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice Use3DSound false" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice CompatibilityMode false" );
		}
		else if (t==AudioModes[1])
		{
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice UseEAX false" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice Use3DSound true" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice CompatibilityMode false" );
			Controller.OpenMenu("XInterface.UT2PerformWarn");
		}

		else if (t==AudioModes[2])
		{
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice UseEAX true" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice Use3DSound true" );
            PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice CompatibilityMode false" );
			Controller.OpenMenu("XInterface.UT2PerformWarn");
		}

		PlayerOwner().ConsoleCommand("SOUND_REBOOT");

		// Restart music.
		if( PlayerOwner().Level.Song != "" && PlayerOwner().Level.Song != "None" )
			PlayerOwner().ClientSetMusic( PlayerOwner().Level.Song, MTRAN_Instant );

    }

	else if (Sender==Controls[9])
		PlayerOwner().ConsoleCommand("set"@Sender.INIOption@moCheckBox(Sender).IsChecked());

	else if (Sender==Controls[10])
	{
		if( PlayerOwner().MyHud != None )
			PlayerOwner().MyHud.bMessageBeep = moCheckBox(Sender).IsChecked();

		class'Hud'.default.bMessageBeep = moCheckBox(Sender).IsChecked();
		class'Hud'.static.StaticSaveConfig();
	}

	else if (Sender==Controls[11])
		PlayerOwner().SetAutoTaunt(moCheckBox(Sender).IsChecked());

	else if (Sender==Controls[12])
	{
		PlayerOwner().bNoMatureLanguage = !moCheckBox(Sender).IsChecked();
		PlayerOwner().SaveConfig();
	}

	else if (Sender==Controls[14])
	{
		t = moComboBox(Sender).GetText();

		if (t==VoiceModes[0])
		{
			b1 = false;
			b2 = false;
			b3 = false;
		}
		else if (t==VoiceModes[1])
		{
			b1 = true;
			b2 = false;
			b3 = false;
		}
		else if (t==VoiceModes[2])
		{
			b1 = true;
			b2 = true;
			b3 = false;
		}
		else
		{
			b1 = true;
			b2 = true;
			b3 = true;
		}

		PlayerOwner().bNoAutoTaunts=b1;
		PlayerOwner().bNoVoiceTaunts=b2;
		PlayerOwner().bNoVoiceMessages=b3;
		PlayerOwner().SaveConfig();
	}

	else if (Sender==Controls[13])
	{

		b1 = moCheckBox(Sender).IsChecked();

		PlayerOwner().Level.bLowSoundDetail = b1;
        PlayerOwner().Level.SaveConfig();

		PlayerOwner().ConsoleCommand("set ini:Engine.Engine.AudioDevice LowQualitySound"@B1);
		PlayerOwner().ConsoleCommand("SOUND_REBOOT");

		// Restart music.
		if( PlayerOwner().Level.Song != "" && PlayerOwner().Level.Song != "None" )
			PlayerOwner().ClientSetMusic( PlayerOwner().Level.Song, MTRAN_Instant );
	}

	else if (Sender==Controls[15])
	{
		PlayerOwner().AnnouncerLevel = moComboBox(Sender).GetIndex();
		PlayerOwner().SaveConfig();
	}

    else if (Sender==APack)
    {
        class'UnrealPlayer'.default.CustomizedAnnouncerPack = APack.GetExtra();
        class'UnrealPlayer'.static.StaticSaveConfig();

        if (UnrealPlayer(PlayerOwner())!=None)
	    	UnrealPlayer(PlayerOwner()).CustomizedAnnouncerPack = APack.GetExtra();
    }

}

defaultproperties
{

	Begin Object class=GUIImage Name=AudioBK1
		WinWidth=0.957500
		WinHeight=0.166289
		WinLeft=0.021641
		WinTop=0.083281
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'AudioBK1'

	Begin Object class=GUIImage Name=AudioBK2
		WinWidth=0.957500
		WinHeight=0.625897
		WinLeft=0.021641
		WinTop=0.362446
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(1)=GUIImage'AudioBK2'

	Begin Object class=GUILabel Name=AudioMusicVolumeLabel
		Caption="Music Volume"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.055664
		WinTop=0.1
	End Object
	Controls(2)=GUILabel'AudioMusicVolumeLabel'

	Begin Object class=GUISlider Name=AudioMusicVolumeSlider
		WinWidth=0.25
		WinLeft=0.062500
		WinTop=0.156146
		MinValue=0
		MaxValue=1
		INIOption="ini:Engine.Engine.AudioDevice MusicVolume"
		INIDefault="0.5"
		Hint="Changes the volume of the background music."
	End Object
	Controls(3)=GUISlider'AudioMusicVolumeSlider'

	Begin Object class=GUILabel Name=AudioEffectsVolumeLabel
		Caption="Effects Volume"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.685547
		WinTop=0.1
	End Object
	Controls(4)=GUILabel'AudioEffectsVolumeLabel'

	Begin Object class=GUISlider Name=AudioEffectsVolumeSlider
		WinWidth=0.25
		WinLeft=0.6875
		WinTop=0.156146
		MinValue=0
		MaxValue=1
		INIOption="ini:Engine.Engine.AudioDevice SoundVolume"
		INIDefault="0.9"
		Hint="Changes the volume of all in game sound effects."
	End Object
	Controls(5)=GUISlider'AudioEffectsVolumeSlider'

	Begin Object class=GUILabel Name=AudioVoiceVolumeLabel
		Caption="Announcer Volume"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.369141
		WinTop=0.1
	End Object
	Controls(6)=GUILabel'AudioVoiceVolumeLabel'

	Begin Object class=GUISlider Name=AudioVoiceVolumeSlider
		WinWidth=0.25
		WinLeft=0.375
		WinTop=0.156146
		MinValue=1
		MaxValue=4
        bIntSlider=true
		INIOption="@Internal"
		OnLoadINI=InternalOnLoadINI
		Hint="Changes the volume of all in game voice messages."
	End Object
	Controls(7)=GUISlider'AudioVoiceVolumeSlider'

	Begin Object class=moComboBox Name=AudioMode
		WinWidth=0.700000
		WinHeight=0.060000
		WinLeft=0.15
		WinTop=0.278646
		Caption="Audio Mode"
		INIOption="@Internal"
		INIDefault="Software 3D Audio"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Changes the audio system mode."
		CaptionWidth=0.3
		ComponentJustification=TXTA_Left
		bReadOnly=true
	End Object
	Controls(8)=moComboBox'AudioMode'

	Begin Object class=moCheckBox Name=AudioReverseStereo
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.100000
		WinTop=0.433333
		Caption="Reverse Stereo"
		INIOption="ini:Engine.Engine.AudioDevice ReverseStereo"
		INIDefault="False"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Reverses the left and right audio channels."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(9)=moCheckbox'AudioReverseStereo'

	Begin Object class=moCheckBox Name=AudioMessageBeep
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.600000
		WinTop=0.433333
		Caption="Message Beep"
		INIOption="@Internal"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables a beep when receiving a text message from other players."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(10)=mocheckbox'AudioMessageBeep'

	Begin Object class=moCheckBox Name=AudioAutoTaunt
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.100000
		WinTop=0.533333
		Caption="Auto-Taunt"
		INIOption="@Internal"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables your in-game player to automatically taunt opponents."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(11)=moCheckBox'AudioAutoTaunt'

	Begin Object class=moCheckBox Name=AudioMatureTaunts
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.600000
		WinTop=0.53333
		Caption="Mature Taunts"
		INIOption="@Internal"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables off-color commentary."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(12)=moCheckBox'AudioMatureTaunts'

	Begin Object class=moCheckBox Name=AudioLowDetail
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.100000
		WinTop=0.63333
		Caption="Low Sound Detail"
		INIOption="@Internal"
		INIDefault="False"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Lowers quality of sound."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(13)=moCheckBox'AudioLowDetail'

	Begin Object class=moComboBox Name=AudioPlayVoices
		WinWidth=0.468750
		WinHeight=0.060000
		WinLeft=0.260352
		WinTop=0.733291
		Caption="Play Voices"
		INIOption="@Internal"
		INIDefault="All"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Defines the types of voice messages to play."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
		bReadOnly=true
	End Object
	Controls(14)=moComboBox'AudioPlayVoices'

	Begin Object class=moComboBox Name=AudioAnnounce
		WinWidth=0.468750
		WinHeight=0.060000
		WinLeft=0.260352
		WinTop=0.816597
		Caption="Announcements"
		INIOption="@Internal"
		INIDefault="All"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Adjusts the amount of in-game announcements."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
		bReadOnly=true
	End Object
	Controls(15)=moComboBox'AudioAnnounce'

	Begin Object class=moComboBox Name=AudioAnnouncerPack
		WinWidth=0.468750
		WinHeight=0.060000
		WinLeft=0.260352
		WinTop=0.900069
		Caption="Announcer Voice"
		INIOption="@Internal"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Select the Announcer for tonight's game."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
		bReadOnly=true
	End Object
	Controls(16)=moComboBox'AudioAnnouncerPack'

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.74
	bAcceptsInput=false

	AudioModes[0]="Software 3D Audio"
	AudioModes[1]="Hardware 3D Audio"
	AudioModes[2]="Hardware 3D Audio + EAX"
	AudioModes[3]="Safe Mode"

	VoiceModes[0]="All"
	VoiceModes[1]="No auto-taunts"
	VoiceModes[2]="No taunts"
	VoiceModes[3]="None"

	AnnounceModes[0]="None"
	AnnounceModes[1]="Minimal"
	AnnounceModes[2]="All"
}
