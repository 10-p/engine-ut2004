class UT2BotConfigPage extends UT2K3GUIPage;

var localized string NoInformation;
var GUIImage BotPortrait;
var GUILabel BotName;

var int ConfigIndex;
var xUtil.PlayerRecord ThisBot;
var bool bIgnoreChange;
var moComboBox Wep;

var array<CacheManager.WeaponRecord> Records;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;
	Super.Initcomponent(MyController, MyOwner);
	BotPortrait=GUIImage(Controls[1]);
	BotName=GUILabel(Controls[3]);

	class'CacheManager'.static.GetWeaponList( Records );
    Wep = moComboBox(Controls[13]);
    Wep.AddItem("None");
    for (i=0;i<Records.Length;i++)
    	Wep.AddItem(Records[i].FriendlyName,,Records[i].ClassName);

    Wep.Onchange=ComboBoxChange;

	moSlider(Controls[6]).MySlider.OnDrawCaption=AggDC;
	moSlider(Controls[7]).MySlider.OnDrawCaption=AccDC;
	moSlider(Controls[8]).MySlider.OnDrawCaption=ComDC;
	moSlider(Controls[9]).MySlider.OnDrawCaption=StrDC;
	moSlider(Controls[10]).MySlider.OnDrawCaption=TacDC;
	moSlider(Controls[11]).MySlider.OnDrawCaption=ReaDC;
}

function SetupBotInfo(Material Portrait, string DecoTextName, xUtil.PlayerRecord PRE)
{

    ThisBot = PRE;

	// Setup the Portrait from here
	BotPortrait.Image = PRE.Portrait;
	// Setup the decotext from here
	BotName.Caption = PRE.DefaultName;

    ConfigIndex = class'CustomBotConfig'.static.IndexFor(PRE.DefaultName);

	bIgnoreChange = true;
    if (ConfigIndex>=0)
    {
    	moSlider(Controls[6]).SetValue(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Aggressiveness);
    	moSlider(Controls[7]).SetValue(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Accuracy);
    	moSlider(Controls[8]).SetValue(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].CombatStyle);
    	moSlider(Controls[9]).SetValue(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].StrafingAbility);
    	moSlider(Controls[10]).SetValue(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Tactics);
        moSlider(Controls[11]).SetValue(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].ReactionTime);
        mocheckBox(Controls[12]).Checked(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Jumpiness > 0.5);

        Wep.Find(class'CustomBotConfig'.default.ConfigArray[ConfigIndex].FavoriteWeapon,,True);

    }
    else
	{
    	moSlider(Controls[6]).SetValue(float(PRE.Aggressiveness));
    	moSlider(Controls[7]).SetValue(float(PRE.Accuracy));
    	moSlider(Controls[8]).SetValue(float(PRE.CombatStyle));
    	moSlider(Controls[9]).SetValue(float(PRE.StrafingAbility));
    	moSlider(Controls[10]).SetValue(float(PRE.Tactics));
    	moSlider(Controls[11]).SetValue(float(PRE.ReactionTime));
        mocheckBox(Controls[12]).Checked(float(PRE.Jumpiness) > 0.5);

        Wep.Find(PRE.FavoriteWeapon,,True);

    }

    bIgnoreChange=false;

}

function bool OkClick(GUIComponent Sender)
{
	Controller.CloseMenu(false);
	class'CustomBotConfig'.static.StaticSaveConfig();
	return true;
}

function bool ResetClick(GUIComponent Sender)
{
	bIgnoreChange = true;

	SetDefaults();

   	moSlider(Controls[6]).SetValue(float(ThisBot.Aggressiveness));
   	moSlider(Controls[7]).SetValue(float(ThisBot.Accuracy));
   	moSlider(Controls[8]).SetValue(float(ThisBot.CombatStyle));
   	moSlider(Controls[9]).SetValue(float(ThisBot.StrafingAbility));
   	moSlider(Controls[10]).SetValue(float(ThisBot.Tactics));
   	moSlider(Controls[11]).SetValue(float(ThisBot.ReactionTime));
   	moCheckBox(Controls[12]).Checked(float(ThisBot.Jumpiness) > 0.5);
	Wep.Find(ThisBot.FavoriteWeapon,false,True);
    bIgnorechange = false;

	return true;
}


function string DoPerc(GUISlider Control)
{
	local float r,v,vmin;

    vmin = Control.MinValue;
    r = Control.MaxValue - vmin;
    v = Control.Value - vmin;

    return string(int(v/r*100));
}



function string AggDC()
{
	return DoPerc(moSlider(Controls[6]).MySlider) $ "%";
}

function string AccDC()
{
	return DoPerc(moSlider(Controls[7]).MySlider) $"%";
}

function string ComDC()
{
	return DoPerc(moSlider(Controls[8]).MySlider) $"%";
}

function string StrDC()
{
	return DoPerc(moSlider(Controls[9]).MySlider) $"%";
}

function string TacDC()
{
	return DoPerc(moSlider(Controls[10]).MySlider) $"%";
}

function string ReaDC()
{
	return DoPerc(moSlider(Controls[11]).MySlider) $"%";
}

function SetDefaults()
{
	class'CustomBotConfig'.default.ConfigArray[ConfigIndex].CharacterName = ThisBot.DefaultName;
	class'CustomBotConfig'.default.ConfigArray[ConfigIndex].PlayerName = ThisBot.DefaultName;
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].FavoriteWeapon = ThisBot.FavoriteWeapon;
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Aggressiveness = float(ThisBot.Aggressiveness);
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Accuracy = float(ThisBot.Accuracy);
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].CombatStyle = float(ThisBot.CombatStyle);
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].StrafingAbility = float(ThisBot.StrafingAbility);
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Tactics = float(ThisBot.Tactics);
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].ReactionTime = float(ThisBot.ReactionTime);
    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Jumpiness = float(ThisBot.Jumpiness);
}

function SliderChange(GUIComponent Sender)
{
	local GUISlider S;

	if ( moSlider(Sender) != None )
		S = moSlider(Sender).MySlider;

    if ( bIgnoreChange || S == None )
    	return;

	// Look to see if this is a new entry

    if (ConfigIndex==-1)
    {
    	ConfigIndex = class'CustomBotConfig'.Default.ConfigArray.Length;
		class'CustomBotConfig'.Default.ConfigArray.Length = ConfigIndex+1;
        SetDefaults();
    }


	if (S == Controls[6])
      class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Aggressiveness = S.Value;

	else if (S == Controls[7])
      class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Accuracy = S.Value;

	else if (S == Controls[8])
      class'CustomBotConfig'.default.ConfigArray[ConfigIndex].CombatStyle = S.Value;

	else if (S == Controls[9])
      class'CustomBotConfig'.default.ConfigArray[ConfigIndex].StrafingAbility = S.Value;

	else if (S == Controls[10])
      class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Tactics = S.Value;

	else if (S == Controls[11])
      class'CustomBotConfig'.default.ConfigArray[ConfigIndex].ReactionTime = S.Value;
}

function CheckBoxChange(GUIComponent Sender)
{
	if (bIgnorechange || Sender!=Controls[18])
    	return;

	// Look to see if this is a new entry

    if (ConfigIndex==-1)
    {
    	ConfigIndex = class'CustomBotConfig'.Default.ConfigArray.Length;
		class'CustomBotConfig'.Default.ConfigArray.Length = ConfigIndex+1;
        SetDefaults();
    }
    if ( moCheckBox(Controls[18]).IsChecked() )
		class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Jumpiness = 1;
	else 
		class'CustomBotConfig'.default.ConfigArray[ConfigIndex].Jumpiness = 0;
}

function ComboBoxChange(GUIComponent Sender)
{
	if (bIgnorechange || Sender!=Controls[13])
    	return;

	// Look to see if this is a new entry
    if (ConfigIndex==-1)
    {
    	ConfigIndex = class'CustomBotConfig'.Default.ConfigArray.Length;
		class'CustomBotConfig'.Default.ConfigArray.Length = ConfigIndex+1;
        SetDefaults();
    }

    class'CustomBotConfig'.default.ConfigArray[ConfigIndex].FavoriteWeapon = moComboBox(Sender).GetExtra();
}

defaultproperties
{
	Begin Object class=GUIImage Name=PageBack
		Image=Material'InterfaceContent.Menu.EditBoxDown'
		ImageStyle=ISTY_Stretched
		WinWidth=0.890625
		WinHeight=1.000000
		WinLeft=0.062500
		WinTop=0.000000
		bScaleToParent=true
		bBoundToParent=true
	End Object
	Controls(0)=PageBack

	Begin Object class=GUIImage Name=imgBotPic
		WinWidth=0.246875
		WinHeight=0.658008
		WinLeft=0.078125
		WinTop=0.193982
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Justified
		RenderWeight=0.1001
	End Object
	Controls(1)=imgBotPic

	Begin Object class=GUIImage Name=BotPortraitBorder
		WinWidth=0.253125
		WinHeight=0.664258
		WinLeft=0.076563
		WinTop=0.188427
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
		DropShadow=Material'2K4Menus.Controls.shadow'
		DropShadowX=8
		DropShadowY=8
		RenderWeight=0.1
	End Object
	Controls(2)=BotPortraitBorder

	Begin Object class=GUILabel Name=BotCfgName
		WinWidth=0.598437
		WinHeight=0.052539
		WinLeft=0.084744
		WinTop=0.119068
		TextAlign=TXTA_Left
		Caption="Unknown"
		StyleName="TextLabel"
		FontScale=FNS_Large
	End Object
	Controls(3)=BotCfgName

	Begin Object class=GUIButton Name=ResetButton
		WinWidth=0.167187
		WinHeight=0.045313
		WinLeft=0.585938
		WinTop=0.825001
		Caption="Reset"
		OnClick=ResetClick
	End Object
	Controls(4)=ResetButton

	Begin Object class=GUIButton Name=OkButton
		WinWidth=0.167187
		WinHeight=0.045313
		WinLeft=0.765625
		WinTop=0.825001
		Caption="OK"
		OnClick=OkClick
	End Object
	Controls(5)=OkButton

/*
	Begin Object class=GUILabel Name=BotAggrText
		TextAlign=TXTA_Left
		StyleName="TextLabel"
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.344727
		WinTop=0.189583
	End Object

	Begin Object class=GUILabel Name=BotAccuracyText
		TextALign=TXTA_Left
		StyleName="TextLabel"
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.344727
		WinTop=0.262499
		TabOrder=1
	End Object

	Begin Object class=GUILabel Name=BotCStyleText
		TextAlign=TXTA_Left
		StyleName="TextLabel"
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.344727
		WinTop=0.335415
	End Object

	Begin Object class=GUILabel Name=BotStrafeText
		TextALign=TXTA_Left
		StyleName="TextLabel"
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.344727
		WinTop=0.408333
	End Object

	Begin Object class=GUILabel Name=BotTacticsText
		TextAlign=TXTA_Left
		StyleName="TextLabel"
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.344727
		WinTop=0.481249
	End Object

	Begin Object class=GUILabel Name=BotReactionText
		TextALign=TXTA_Left
		StyleName="TextLabel"
		WinWidth=0.250000
		WinHeight=32.000000
		WinLeft=0.344727
		WinTop=0.574999
	End Object
*/

	Begin Object class=moSlider Name=BotAggrSlider
		Caption="Aggressiveness"
		WinWidth=0.598438
		WinLeft=0.345313
		WinTop=0.208229
		WinHeight=0.03
		MinValue=0
		MaxValue=1
		Hint="Configures the aggressiveness rating of this bot."
        OnChange=SliderChange
        TabOrder=0
        bHeightFromComponent=False
	End Object

	Begin Object class=moSlider Name=BotAccuracySlider
		Caption="Accuracy"
		WinWidth=0.598438
		WinHeight=0.030000
		WinLeft=0.345313
		WinTop=0.281145
		MinValue=-1
		MaxValue=1
		Hint="Configures the accuracy rating of this bot."
        OnChange=SliderChange;
        bHeightFromComponent=False
	End Object

	Begin Object class=moSlider Name=BotCStyleSlider
		Caption="Combat Style"
		WinWidth=0.598438
		WinHeight=0.030000
		WinLeft=0.345313
		WinTop=0.354062
		MinValue=0
		MaxValue=1
		Hint="Adjusts the combat style of this bot."
        OnChange=SliderChange;
        bHeightFromComponent=False
	End Object

	Begin Object class=moSlider Name=BotStrafeSlider
		Caption="Strafing Ability"
		WinWidth=0.598438
		WinHeight=0.030000
		WinLeft=0.345313
		WinTop=0.426979
		MinValue=0
		MaxValue=1
		Hint="Adjusts the strafing ability of this bot."
        OnChange=SliderChange;
        bHeightFromComponent=False
	End Object

	Begin Object class=moSlider Name=BotTacticsSlider
		Caption="Tactics"
		WinWidth=0.598438
		WinHeight=0.030000
		WinLeft=0.345313
		WinTop=0.499895
		MinValue=-1
		MaxValue=1
		Hint="Adjusts the team-play awareness ability of this bot."
        OnChange=SliderChange;
        bHeightFromComponent=False
	End Object

	Begin Object class=moSlider Name=BotReactionSlider
		Caption="Reaction Time"
		WinWidth=0.598438
		WinHeight=0.030000
		WinLeft=0.345313
		WinTop=0.593645
		MinValue=-4
		MaxValue=4
		Hint="Adjusts the reaction speed of this bot."
        OnChange=SliderChange;
        bHeightFromComponent=False
	End Object

	Begin Object class=moCheckBox Name=BotJumpy
		WinWidth=0.598438
		WinHeight=0.040000
		WinLeft=0.345313
		WinTop=0.666562
		Caption="Jump Happy"
		Hint="Controls whether this bot jumps a lot during the game."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Right
        OnChange=CheckBoxChange
        bHeightFromComponent=False
	End Object

	Begin Object class=moComboBox Name=BotWeapon
		WinWidth=0.598438
		WinHeight=0.044375
		WinLeft=0.345313
		WinTop=0.729062
		Caption="Preferred Weapon"
		Hint="Select which weapon this bot should prefer."
		CaptionWidth=0.45
        bReadOnly=true
		ComponentJustification=TXTA_Left
        bHeightFromComponent=False
	End Object

    Controls(6)=BotAggrSlider
    Controls(7)=BotAccuracySlider
    Controls(8)=BotCStyleSlider
    Controls(9)=BotStrafeSlider
    Controls(10)=BotTacticsSlider
    Controls(11)=BotReactionSlider
    Controls(12)=BotJumpy
    Controls(13)=BotWeapon

	WinWidth=1.0
	WinHeight=0.8
	WinTop=0.1
	WinLeft=0.0

	NoInformation="No Information Available!"
}
