// ====================================================================
//  Class:  XInterface.Tab_DetailSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_DetailSettings extends UT2K3TabPanel;

var config bool bExpert;
var localized string	DetailLevels[7];
var bool				bPlayedSound;

// bit hacky - but needed to know if we are increasing detail
var int					prevWorldDetail, prevTextureDetail, prevCharDetail;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;
	Super.Initcomponent(MyController, MyOwner);

	for (i=0;i<Controls.Length;i++)
		Controls[i].OnChange=InternalOnChange;

	if(PlayerOwner().Level.IsDemoBuild())
	{
		for(i = 0;i < 4;i++)
			moComboBox(Controls[1]).AddItem(DetailLevels[i]);
		for(i = 0;i < 4;i++)
			moComboBox(Controls[2]).AddItem(DetailLevels[i]);
	}
	else
	{
		for(i = 0;i < ArrayCount(DetailLevels);i++)
			moComboBox(Controls[1]).AddItem(DetailLevels[i]);
		for(i = 0;i < ArrayCount(DetailLevels);i++)
			moComboBox(Controls[2]).AddItem(DetailLevels[i]);
	}
	moComboBox(Controls[1]).ReadOnly(True);
	moComboBox(Controls[2]).ReadOnly(True);

	moComboBox(Controls[3]).AddItem(DetailLevels[3]);
	moComboBox(Controls[3]).AddItem(DetailLevels[4]);
	moComboBox(Controls[3]).AddItem(DetailLevels[6]);
	moComboBox(Controls[3]).ReadOnly(True);

	for(i = 2;i < 5;i++)
		moComboBox(Controls[4]).AddItem(DetailLevels[i]);
	moComboBox(Controls[4]).ReadOnly(True);

	for(i = 2;i < 5;i++)
		moComboBox(Controls[10]).AddItem(DetailLevels[i]);
	moComboBox(Controls[10]).ReadOnly(True);
}

function InternalOnLoadINI(GUIComponent Sender, string s)
{
	local int i;
	local bool a, b;

	if (Sender==Controls[1])
	{
		if(s == "UltraLow")
			prevTextureDetail=0;
		else if(s == "Low")
			prevTextureDetail=1;
		else if(s == "Lower")
			prevTextureDetail=2;
		else if(s == "Normal")
			prevTextureDetail=3;
		else if(s == "Higher")
			prevTextureDetail=4;
		else if(s == "High")
			prevTextureDetail=5;
		else if(s == "UltraHigh")
			prevTextureDetail=6;

		moComboBox(Sender).SetText(DetailLevels[prevTextureDetail]);
	}

	else if (Sender==Controls[2])
	{
		if(s == "UltraLow")
			prevCharDetail=0;
		else if(s == "Low")
			prevCharDetail=1;
		else if(s == "Lower")
			prevCharDetail=2;
		else if(s == "Normal")
			prevCharDetail=3;
		else if(s == "Higher")
			prevCharDetail=4;
		else if(s == "High")
			prevCharDetail=5;
		else if(s == "UltraHigh")
			prevCharDetail=6;

		moComboBox(Sender).SetText(DetailLevels[prevCharDetail]);
	}

	else if (Sender==Controls[3])
	{
		a = bool(PlayerOwner().ConsoleCommand("get ini:Engine.Engine.RenderDevice HighDetailActors"));
		b = bool(PlayerOwner().ConsoleCommand("get ini:Engine.Engine.RenderDevice SuperHighDetailActors"));

		if(b)
			prevWorldDetail=6;
		else if(a)
			prevWorldDetail=4;
		else
			prevWorldDetail=3;

		moComboBox(Sender).SetText(DetailLevels[prevWorldDetail]);
	}

	else if (Sender==Controls[4])
	{
		if(PlayerOwner().Level.default.PhysicsDetailLevel == PDL_Low)
			moComboBox(Sender).SetText(DetailLevels[2]);
		else if(PlayerOwner().Level.default.PhysicsDetailLevel == PDL_Medium)
			moComboBox(Sender).SetText(DetailLevels[3]);
		else
			moComboBox(Sender).SetText(DetailLevels[4]);
	}

	else if (Sender==Controls[5])
		moCheckBox(Sender).Checked(class'UnrealPawn'.default.bPlayerShadows);

	else if (Sender==Controls[7])
		moCheckBox(Sender).Checked(!bool(s));

	else if (Sender==Controls[10])
	{
		i = PlayerOwner().Level.default.DecalStayScale;

		switch (i)
		{
			case 0 : moComboBox(Sender).SetText(DetailLevels[2]);break;
			case 1 : moComboBox(Sender).SetText(DetailLevels[3]);break;
			case 2 : moComboBox(Sender).SetText(DetailLevels[4]);break;
		}
	}

    else if (Sender==Controls[14])
    	moCheckBox(Sender).Checked(class'UnrealPawn'.Default.bBlobShadow);

	else
		moCheckBox(Sender).Checked(bool(s));
}

function string InternalOnSaveINI(GUIComponent Sender); 		// Do the actual work here

function InternalOnChange(GUIComponent Sender)
{
	local String t,v;
	local bool b, goingUp;
	local int newDetail;

	if (!Controller.bCurMenuInitialized)
		return;

	if (Sender==Controls[1])
	{
		t = "set ini:Engine.Engine.ViewportManager TextureDetail";

		if(moComboBox(Sender).GetText() == DetailLevels[0])
		{
			v = "UltraLow";
			newDetail = 0;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[1])
		{
			v = "Low";
			newDetail = 1;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[2])
		{
			v = "Lower";
			newDetail = 2;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[3])
		{
			v = "Normal";
			newDetail = 3;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[4])
		{
			v = "Higher";
			newDetail = 4;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[5])
		{
			v = "High";
			newDetail = 5;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[6])
		{
			v = "UltraHigh";
			newDetail = 6;
		}

		PlayerOwner().ConsoleCommand(t$"Terrain"@v);
		PlayerOwner().ConsoleCommand(t$"World"@v);
		PlayerOwner().ConsoleCommand(t$"Rendermap"@v);
		PlayerOwner().ConsoleCommand(t$"Lightmap"@v);
		PlayerOwner().ConsoleCommand("flush");

		if(newDetail > prevTextureDetail)
			goingUp = true;

		prevTextureDetail = newDetail;
	}

	else if (Sender==Controls[2])
	{
		t = "set ini:Engine.Engine.ViewportManager TextureDetail";

		if(moComboBox(Sender).GetText() == DetailLevels[0])
		{
			v = "UltraLow";
			newDetail = 0;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[1])
		{
			v = "Low";
			newDetail = 1;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[2])
		{
			v = "Lower";
			newDetail = 2;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[3])
		{
			v = "Normal";
			newDetail = 3;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[4])
		{
			v = "Higher";
			newDetail = 4;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[5])
		{
			v = "High";
			newDetail = 5;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[6])
		{
			v = "UltraHigh";
			newDetail = 6;
		}

		PlayerOwner().ConsoleCommand(t$"WeaponSkin"@v);
		PlayerOwner().ConsoleCommand(t$"PlayerSkin"@v);
		PlayerOwner().ConsoleCommand("flush");

		if(newDetail > prevCharDetail)
			goingUp = true;

		prevCharDetail = newDetail;
	}

	else if (Sender==Controls[3])
	{
		if(moComboBox(Sender).GetText() == DetailLevels[6])
		{
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice HighDetailActors True");
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice SuperHighDetailActors True");
			PlayerOwner().Level.DetailChange(DM_SuperHigh);
			newDetail = 6;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[4])
		{
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice HighDetailActors True");
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice SuperHighDetailActors False");
			PlayerOwner().Level.DetailChange(DM_High);
			newDetail = 4;
		}
		else if(moComboBox(Sender).GetText() == DetailLevels[3])
		{
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice HighDetailActors False");
			PlayerOwner().ConsoleCommand("set ini:Engine.Engine.RenderDevice SuperHighDetailActors False");
			PlayerOwner().Level.DetailChange(DM_Low);
			newDetail = 3;
		}

		if(newDetail > prevWorldDetail)
			goingUp = true;

		prevWorldDetail = newDetail;
	}

	else if (Sender==Controls[4])
	{
		if (moComboBox(Sender).GetText()==DetailLevels[2])
			PlayerOwner().Level.default.PhysicsDetailLevel=PDL_Low;
		else if (moComboBox(Sender).GetText()==DetailLevels[3])
			PlayerOwner().Level.default.PhysicsDetailLevel=PDL_Medium;
		else if (moComboBox(Sender).GetText()==DetailLevels[4])
			PlayerOwner().Level.default.PhysicsDetailLevel=PDL_High;

		PlayerOwner().Level.PhysicsDetailLevel = PlayerOwner().Level.default.PhysicsDetailLevel;
		PlayerOwner().Level.SaveConfig();
	}

	else if (Sender==Controls[5])
	{
		PlayerOwner().ConsoleCommand("set UnrealPawn bPlayerShadows "$moCheckBox(Sender).IsChecked());
		class'UnrealPawn'.default.bPlayerShadows = moCheckBox(Sender).IsChecked();
		class'UnrealPawn'.static.StaticSaveConfig();

		if( moCheckBox(Sender).IsChecked() )
			goingUp = true;
	}

    else if (Sender==Controls[14])
    {
		PlayerOwner().ConsoleCommand("set UnrealPawn bBlobShadow "$moCheckBox(Sender).IsChecked());
		class'UnrealPawn'.default.bBlobShadow = moCheckBox(Sender).IsChecked();
		class'UnrealPawn'.static.StaticSaveConfig();
    }

	else if (Sender==Controls[7])
	{
		b = moCheckBox(Sender).IsChecked();
		b = b!=true;
		PlayerOwner().ConsoleCommand("set"@Sender.INIOption@b);

		if( moCheckBox(Sender).IsChecked() )
			goingUp = true;
	}

	else if (Sender==Controls[10])
	{
		if (moComboBox(Sender).GetText()==DetailLevels[4])
			PlayerOwner().Level.default.DecalStayScale=2;
		else if (moComboBox(Sender).GetText()==DetailLevels[3])
			PlayerOwner().Level.default.DecalStayScale=1;
		else if (moComboBox(Sender).GetText()==DetailLevels[2])
			PlayerOwner().Level.default.DecalStayScale=0;

		PlayerOwner().Level.DecalStayScale=PlayerOwner().Level.default.DecalStayScale;
		PlayerOwner().Level.SaveConfig();
	}

	else
	{
		PlayerOwner().ConsoleCommand("set"@Sender.INIOption@moCheckBox(Sender).IsChecked());

		if( moCheckBox(Sender).IsChecked() )
			goingUp = true;
	}

	// If we have increased the detail level, show paranoid warning
	if (goingUp && !bExpert)
		Controller.OpenMenu("XInterface.UT2PerformWarn");


	// Check if we are maxed out (and mature-enabled)!
	if( !bPlayedSound && !PlayerOwner().bNoMatureLanguage &&
		moComboBox(Controls[1]).GetText() == DetailLevels[6] &&
		moComboBox(Controls[2]).GetText() == DetailLevels[6] &&
		moComboBox(Controls[3]).GetText() == DetailLevels[6] &&
		moComboBox(Controls[4]).GetText() == DetailLevels[4] &&
		moCheckBox(Controls[5]).IsChecked() &&
		moCheckBox(Controls[6]).IsChecked() &&
		moCheckBox(Controls[7]).IsChecked() &&
		moCheckBox(Controls[8]).IsChecked() &&
		moCheckBox(Controls[9]).IsChecked() &&
		moComboBox(Controls[10]).GetText() == DetailLevels[4] &&
		moCheckBox(Controls[11]).IsChecked() &&
		moCheckBox(Controls[12]).IsChecked() )
	{
		PlayerOwner().ClientPlaySound(sound'AnnouncerMale2K4.HolyShit_F');
		bPlayedSound = true;
	}
}

defaultproperties
{

	Begin Object class=GUIImage Name=DetailBK
		WinWidth=0.957500
		WinHeight=0.521250
		WinLeft=0.021641
		WinTop=0.320000
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'DetailBK'

	Begin Object class=moComboBox Name=DetailWorldDetail
		WinWidth=0.4
		WinHeight=0.050000
		WinLeft=0.05
		WinTop=0.1
		Caption="Texture Detail"
		INIOption="ini:Engine.Engine.ViewportManager TextureDetailWorld"
		INIDefault="High"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Changes how much world detail will be rendered."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(1)=moComboBox'DetailWorldDetail'

	Begin Object class=moComboBox Name=DetailCharacterDetail
		WinWidth=0.4
		WinHeight=0.050000
		WinLeft=0.55
		WinTop=0.1
		Caption="Character Detail"
		INIOption="ini:Engine.Engine.ViewportManager TextureDetailPlayerSkin"
		INIDefault="High"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Changes how much character detail will be rendered."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(2)=moComboBox'DetailCharacterDetail'

	Begin Object class=moComboBox Name=DetailActorDetail
		WinWidth=0.4
		WinHeight=0.050000
		WinLeft=0.05
		WinTop=0.2
		Caption="World Detail"
		INIOption="@Internal"
		INIDefault="High"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Changes the level of detail used for optional geometry and effects."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(3)=moComboBox'DetailActorDetail'

	Begin Object class=moComboBox Name=DetailPhysics
		WinWidth=0.4
		WinHeight=0.050000
		WinLeft=0.55
		WinTop=0.2
		Caption="Physics Detail"
		INIOption="@Internal"
		INIDefault="High"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Changes the physics simulation level of detail."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(4)=moComboBox'DetailPhysics'

	Begin Object class=moCheckBox Name=DetailCharacterShadows
		WinWidth=0.3
		WinHeight=0.050000
		WinLeft=0.1
		WinTop=0.36
		Caption="Character Shadows"
		INIOption="@Internal"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables character shadows."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(5)=moCheckBox'DetailCharacterShadows'

	Begin Object class=moCheckBox Name=DetailDecals
		WinWidth=0.3
		WinHeight=0.050000
		WinLeft=0.6
		WinTop=0.36
		Caption="Decals"
		INIOption="ini:Engine.Engine.ViewportManager Decals"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables weapon scarring effects."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(6)=moCheckBox'DetailDecals'

	Begin Object class=moCheckBox Name=DetailDynamicLighting
		WinWidth=0.3
		WinHeight=0.050000
		WinLeft=0.1
		WinTop=0.46
		Caption="Dynamic Lighting"
		INIOption="ini:Engine.Engine.ViewportManager NoDynamicLights"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables dynamic lights."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(7)=moCheckBox'DetailDynamicLighting'

	Begin Object class=moCheckBox Name=DetailCoronas
		WinWidth=0.3
		WinHeight=0.050000
		WinLeft=0.6
		WinTop=0.46
		Caption="Coronas"
		INIOption="ini:Engine.Engine.ViewportManager Coronas"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables coronas."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(8)=moCheckBox'DetailCoronas'

	Begin Object class=moCheckBox Name=DetailDetailTextures
		WinWidth=0.3
		WinHeight=0.050000
		WinLeft=0.1
		WinTop=0.56
		Caption="Detail Textures"
		INIOption="ini:Engine.Engine.RenderDevice DetailTextures"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables detail textures."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(9)=moCheckBox'DetailDetailTextures'

	Begin Object class=moComboBox Name=DetailDecalStay
		WinWidth=0.350000
		WinHeight=0.060000
		WinLeft=0.598750
		WinTop=0.550000
		Caption="Decal Stay"
		INIOption="@Internal"
		INIDefault="Normal"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Changes how long weapon scarring effects stay around."
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(10)=moComboBox'DetailDecalStay'

	Begin Object class=moCheckBox Name=DetailProjectors
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.100000
		WinTop=0.656251
		Caption="Projectors"
		INIOption="ini:Engine.Engine.ViewportManager Projectors"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables Projectors."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(11)=moCheckBox'DetailProjectors'

	Begin Object class=moCheckBox Name=DetailDecoLayers
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.598750
		WinTop=0.656251
		Caption="Foliage"
		INIOption="ini:Engine.Engine.ViewportManager DecoLayers"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enables grass and other decorative foliage."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(12)=moCheckBox'DetailDecoLayers'

	Begin Object class=moCheckBox Name=DetailTrilinear
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.100000
		WinTop=0.75
		Caption="Trilinear Filtering"
		INIOption="ini:Engine.Engine.RenderDevice UseTrilinear"
		INIDefault="False"
		OnLoadINI=InternalOnLoadINI
		OnSaveINI=InternalOnSaveINI
		Hint="Enable trilinear filtering, recommended for high-performance PCs."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(13)=moCheckBox'DetailTrilinear'

	Begin Object class=moCheckBox Name=DetailBlob
		WinWidth=0.300000
		WinHeight=0.040000
		WinLeft=0.598750
		WinTop=0.75
		Caption="Use Blob Shadows"
		INIOption="@Internal"
		INIDefault="False"
		OnLoadINI=InternalOnLoadINI
		Hint="Enable blob shadows.  Recommended for low-performance PCs."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(14)=moCheckBox'DetailBlob'

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.74
	bAcceptsInput=false

	DetailLevels(0)="Lowest"
	DetailLevels(1)="Lower"
	DetailLevels(2)="Low"
	DetailLevels(3)="Normal"
	DetailLevels(4)="High"
	DetailLevels(5)="Higher"
	DetailLevels(6)="Highest"
    bExpert=false;

}
