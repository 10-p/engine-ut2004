class TestImagePage extends TestPageBase;

#exec OBJ LOAD FILE=InterfaceContent.utx

var automated GUIImage i_Test;
var automated moComboBox co_Style, co_Align, co_Select, co_Render;
var automated GUINumericEdit nu_Width, nu_Height, nu_PortionX1, nu_PortionX2, nu_PortionY1, nu_PortionY2;
var automated GUIButton b_Add;
var automated GUILabel l_ImageSize;

var config array<string> Images;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController, MyOwner);

	Background = Controller.DefaultPens[1];
}

function bool InternalOnClick( GUIComponent Sender )
{
	local string S;
	local Material M;

	if ( Sender == b_Add )
	{
		S = co_Select.GetText();
		if ( co_Select.FindIndex(s) == -1 )
		{
			M = LoadTexture(S);
			if ( M != None )
			{
				co_Select.AddItem(S);
				Images[Images.Length] = S;
				SaveConfig();

				SetNewImage(S);
			}
		}
	}
	return true;
}

function InternalOnOpen()
{
	local int i;

	// Prepare the ComboBoxes
	co_Style.AddItem("Normal");
	co_Style.AddItem("Stretched");
	co_Style.AddItem("Scaled");
	co_Style.AddItem("Bound");
	co_Style.AddItem("Justified");
	co_Style.AddItem("PartialScaled");

	co_Align.AddItem("TopLeft");
	co_Align.AddItem("Center");
	co_Align.AddItem("BottomRight");

	co_Render.AddItem("None");
	co_Render.AddItem("Normal");
	co_Render.AddItem("Masked");
	co_Render.AddItem("Translucent");
	co_Render.AddItem("Modulated");
	co_Render.AddItem("Alpha");
	co_Render.AddItem("Additive");
	co_Render.AddItem("Subtractive");
	co_Render.AddItem("Particle");
	co_Render.AddItem("AlphaZ");

	for ( i = 0; i < Images.Length; i++ )
		co_Select.AddItem(Images[i]);

	SetNewImage(co_Select.MyComboBox.List.Get(True));
	co_Render.SetIndex(1);
	co_Style.SetIndex(0);
	co_Align.SetIndex(0);
	nu_PortionX1.SetValue(-1);
	nu_PortionY1.SetValue(-1);
	nu_PortionX2.SetValue(-1);
	nu_PortionY2.SetValue(-1);
}

function InternalOnRendered(Canvas C)
{
	if ( i_Test.bPositioned )
	{
		nu_Width.Value = string(i_Test.ActualWidth());
		nu_Height.Value = string(i_Test.ActualHeight());

		OnRendered=None;
	}
}

function OnNewImgStyle(GUIComponent Sender)
{
local string NewImgStyle;

	NewImgStyle = co_Style.GetText();
	switch ( NewImgStyle )
	{
	case "Normal":
		i_Test.ImageStyle = ISTY_Normal;
		break;

	case "Stretched":
		i_Test.ImageStyle = ISTY_Stretched;
		break;

	case "Scaled":
		i_Test.ImageStyle = ISTY_Scaled;
		break;

	case "Bound":
		i_Test.ImageStyle = ISTY_Bound;
		break;

	case "Justified":
		i_Test.ImageStyle = ISTY_Justified;
		break;

	case "PartialScaled":
		i_Test.ImageStyle = ISTY_PartialScaled;
		break;
	}
}

function InternalOnChange( GUIComponent Sender )
{
	if ( Sender == co_Render )
	{
		switch (co_Render.GetText())
		{
		case "None":
			i_Test.ImageRenderStyle = MSTY_None;
			break;

		case "Normal":
			i_Test.ImageRenderStyle = MSTY_Normal;
			break;

		case "Masked":
			i_Test.ImageRenderStyle = MSTY_Masked;
			break;

		case "Translucent":
			i_Test.ImageRenderStyle = MSTY_Translucent;
			break;

		case "Modulated":
			i_Test.ImageRenderStyle = MSTY_Modulated;
			break;

		case "Alpha":
			i_Test.ImageRenderStyle = MSTY_Alpha;
			break;

		case "Additive":
			i_Test.ImageRenderStyle = MSTY_Additive;
			break;

		case "Subtractive":
			i_Test.ImageRenderStyle = MSTY_Subtractive;
			break;

		case "Particle":
			i_Test.ImageRenderStyle = MSTY_Particle;
			break;

		case "AlphaZ":
			i_Test.ImageRenderStyle = MSTY_AlphaZ;
			break;
		}
	}
}

function OnNewImgAlign(GUIComponent Sender)
{
local string NewImgAlign;

	NewImgAlign = co_Align.GetText();
	if (NewImgAlign == "TopLeft")
		i_Test.ImageAlign = IMGA_TopLeft;
	else if (NewImgAlign == "Center")
		i_Test.ImageAlign = IMGA_Center;
	else if (NewImgAlign == "BottomRight")
		i_Test.ImageAlign = IMGA_BottomRight;
}

function OnNewImgSelect(GUIComponent Sender)
{
	local string S;

	s = co_Select.GetText();
	if ( co_Select.FindIndex(s) != -1 )
		SetNewImage(s);
}

function SetNewImage(string ImageName)
{
	i_Test.Image = LoadTexture(ImageName);
	if ( i_Test.Image != None )
		l_ImageSize.Caption =  i_Test.Image.MaterialUSize() $ "x" $ i_Test.Image.MaterialVSize();
}

function Material LoadTexture(string TextureFullName)
{
	return Material(DynamicLoadObject(TextureFullName, class'Material'));
}

function ResizeImage(GUIComponent Sender)
{
	if ( Sender == nu_Width )
		i_Test.WinWidth = i_Test.RelativeWidth(int(nu_Width.Value));

	else i_Test.WinHeight = i_Test.RelativeHeight(int(nu_Height.Value));
}

function ChangePortion(GUIComponent Sender)
{
	switch ( Sender )
	{
	case nu_PortionX1:
		i_Test.X1 = int(GUINumericEdit(Sender).Value);
		break;
	case nu_PortionX2:
		i_Test.X2 = int(GUINumericEdit(Sender).Value);
		break;
	case nu_PortionY1:
		i_Test.Y1 = int(GUINumericEdit(Sender).Value);
		break;
	case nu_PortionY2:
		i_Test.Y2 = int(GUINumericEdit(Sender).Value);
		break;
	}
}

defaultproperties
{
	OnRendered=InternalOnRendered
	Begin Object Class=GUIImage Name=TheImage
		WinWidth=0.600000
		WinHeight=0.976563
		WinLeft=0.088281
		WinTop=0.115365
		RenderWeight=0.4
		ImageStyle=ISTY_Normal
		ImageRenderStyle=MSTY_Normal
		ImageAlign=IMGA_TopLeft
	End Object
	i_Test=TheImage

	Begin Object Class=moComboBox Name=SelectImage
		Caption="Select Image"
		WinWidth=0.538086
		WinHeight=0.040000
		WinLeft=0.000000
		WinTop=0.000000
		OnChange=OnNewImgSelect
		CaptionWidth=0.1
		ComponentWidth=-1
		bAutoSizeCaption=True
		TabOrder=0
	End Object
	co_Select=SelectImage

	Begin Object Class=GUIbutton Name=AddImage
		OnClick=InternalOnClick
		Caption="Add"
		WinWidth=0.050273
		WinHeight=0.040000
		WinLeft=0.539258
		WinTop=0.000000
		TabOrder=1
	End Object
	b_Add=AddImage

	Begin Object Class=moComboBox Name=ImageStyle
		WinWidth=0.250000
		WinHeight=0.040000
		WinLeft=0.000
		WinTop=0.045
		OnChange=OnNewImgStyle
		Caption="Image Style"
		CaptionWidth=0.1
		ComponentWidth=-1
		bAutoSizeCaption=True
		bReadOnly=True
		TabOrder=4
	End Object
	co_Style=ImageStyle

	Begin Object Class=moComboBox Name=ImageAlign
		WinWidth=0.208984
		WinHeight=0.040000
		WinLeft=0.265625
		WinTop=0.045
		OnChange=OnNewImgAlign
		Caption="Align"
		CaptionWidth=0.1
		ComponentWidth=-1
		bAutoSizeCaption=True
		bReadOnly=True
		TabOrder=5
	End Object
	co_Align=ImageAlign

	Begin Object Class=moComboBox Name=ImageRenderStyle
		WinWidth=0.326172
		WinHeight=0.040000
		WinLeft=0.480469
		WinTop=0.0450
		Caption="Render Style"
		OnChange=InternalOnChange
		CaptionWidth=0.1
		ComponentWidth=-1
		bAutoSizeCaption=True
		bReadOnly=True
		TabOrder=6
	End Object
	co_Render=ImageRenderStyle

	Begin Object Class=GUINumericEdit Name=ImageWidth
		WinWidth=0.107617
		WinHeight=0.040000
		WinLeft=0.731055
		WinTop=0.000000
		OnChange=ResizeImage
		MinValue=10
		MaxValue=1014
		Step=10
		TabOrder=2
	End Object
	nu_Width=ImageWidth

	Begin Object Class=GUINumericEdit Name=ImageHeight
		WinWidth=0.069531
		WinHeight=0.040000
		WinLeft=0.838672
		WinTop=0.000000
		OnChange=ResizeImage
		MinValue=10
		MaxValue=758
		Step=10
		TabOrder=3
	End Object
	nu_Height=ImageHeight

	Begin Object Class=GUILabel Name=ImageDims
		WinWidth=0.121289
		WinHeight=0.040000
		WinLeft=0.594336
		WinTop=0.000000
		StyleName="TextLabel"
		FontScale=FNS_Small
	End Object
	l_ImageSize=ImageDims

	Begin Object Class=GUINumericEdit Name=ImagePortionX1
		OnChange=ChangePortion
		WinWidth=0.083203
		WinHeight=0.040000
		WinLeft=0.003516
		WinTop=0.221354
		MinValue=-1
		MaxValue=1024
		TabOrder=7
		Step=10
	End Object
	nu_PortionX1=ImagePortionX1

	Begin Object Class=GUINumericEdit Name=ImagePortionY1
		OnChange=ChangePortion
		WinWidth=0.083203
		WinHeight=0.040000
		WinLeft=0.003516
		WinTop=0.261354
		MinValue=-1
		MaxValue=1024
		Step=10
		TabOrder=8
	End Object
	nu_PortionY1=ImagePortionY1

	Begin Object Class=GUINumericEdit Name=ImagePortionX2
		OnChange=ChangePortion
		WinWidth=0.083203
		WinHeight=0.040000
		WinLeft=0.003516
		WinTop=0.301354
		MinValue=-1
		MaxValue=1024
		Step=10
		TabOrder=9
	End Object
	nu_PortionX2=ImagePortionX2

	Begin Object Class=GUINumericEdit Name=ImagePortionY2
		OnChange=ChangePortion
		WinWidth=0.083203
		WinHeight=0.040000
		WinLeft=0.003516
		WinTop=0.341354
		MinValue=-1
		MaxValue=1024
		Step=10
		TabOrder=10
	End Object
	nu_PortionY2=ImagePortionY2

	OnOpen=InternalOnOpen
	bRenderWorld=False
	bRequire640x480=True
	bCaptureInput=True

	Images(0)="PlayerPictures.cEgyptFemaleBA"
	Images(1)="InterfaceContent.Menu.bg07"
	Images(2)="2K4Menus.Controls.menuBackground2"
	Images(3)="2K4Menus.Controls.popupBorder_b"
	Images(4)="2K4Menus.Controls.editbox2_b"
}
