// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class GUIFloatEdit extends GUIMultiComponent
	Native;


cpptext
{
		void PreDraw(UCanvas* Canvas);
}

var Automated GUIEditBox MyEditBox;
var Automated GUISpinnerButton MySpinner;

var()	string				Value;
var()	bool				bLeftJustified;
var()	float				MinValue;
var()	float				MaxValue;
var()	float				Step;

var()   bool                bReadOnly;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	if ( MinValue < 0 )
		MyEditBox.bIncludeSign = True;


	Super.Initcomponent(MyController, MyOwner);

	MyEditBox.OnChange = EditOnChange;
	MyEditBox.SetText(Value);
	MyEditBox.OnKeyEvent = EditKeyEvent;
	MyEditBox.OnDeActivate = CheckValue;

	MyEditBox.INIOption  = INIOption;
	MyEditBox.INIDefault = INIDefault;

	CalcMaxLen();

	MySpinner.bNeverFocus = true;
	MySpinner.FocusInstead = MyEditBox;
	MySpinner.OnPlusClick    = SpinnerPlusClick;
	MySpinner.OnMinusClick   = SpinnerMinusClick;

	SetReadOnly(bReadOnly);

    SetHint(Hint);

}

function SetHint(string NewHint)
{
	local int i;
	Super.SetHint(NewHint);

    for (i=0;i<Controls.Length;i++)
    	Controls[i].SetHint(NewHint);
}

function CalcMaxLen()
{
	local int digitcount,x;

	digitcount=1;
	x=10;
	while (x<MaxValue)
	{
		digitcount++;
		x*=10;
	}

	MyEditBox.MaxWidth = DigitCount+4;
}
function SetValue(float V)
{
	MyEditBox.SetText( string(FClamp(V, MinValue, MaxValue)) );
}

function bool SpinnerPlusClick(GUIComponent Sender)
{
    SetValue(float(Value) + Step);
	return true;
}

function bool SpinnerMinusClick(GUIComponent Sender)
{
	SetValue(float(Value) - Step);
	return true;
}

function bool EditKeyEvent(out byte Key, out byte State, float delta)
{
	if ( (key==0xEC) && (State==3) )
	{
		SpinnerPlusClick(none);
		return true;
	}

	if ( (key==0xED) && (State==3) )
	{
		SpinnerMinusClick(none);
		return true;
	}

	return MyEditBox.InternalOnKeyEvent(Key,State,Delta);


}

function CheckValue()
{
	SetValue(float(Value));
}

function EditOnChange(GUIComponent Sender)
{
	Value = string( FClamp(float(MyEditBox.TextStr), MinValue, MaxValue) );
    OnChange(Sender);
}

function SetReadOnly(bool b)
{
	bReadOnly = b;
	MyEditBox.bReadOnly = b;
	if ( b )
	{
		DisableComponent(MySpinner);
	}
	else
	{
		EnableComponent(MySpinner);
	}
}

function SetFriendlyLabel( GUILabel NewLabel )
{
	Super.SetFriendlyLabel(NewLabel);

	if ( MyEditBox != None )
		MyEditbox.SetFriendlyLabel(NewLabel);

	if ( MySpinner != None )
		MySpinner.SetFriendlyLabel(NewLabel);
}

function ValidateValue()
{
	local float f;

	f = float(MyEditBox.TextStr);
	MyEditBox.TextStr = string(FClamp(f, MinValue, MaxValue));
	MyEditBox.bHasFocus = False;
}

defaultproperties
{
	OnDeactivate=ValidateValue
	Begin Object Class=GUIEditBox Name=cMyEditBox
		bNeverScale=True
		TextStr=""
		bFloatOnly=true
	End Object

	Begin Object Class=GUISpinnerButton Name=cMySpinner
		bTabStop=false
		bNeverScale=True
	End Object

	Begin object Class=GUIToolTip Name=GUIFloatEditToolTip
	End Object
	ToolTip=GUIFloatEditToolTip


	MyEditBox=cMyEditBox
	MySpinner=cMySpinner

	Value="0.0"
	Step=0.1
	MinValue=-9999
	MaxValue=9999
	bAcceptsInput=true
	bLeftJustified=false
	WinHeight=0.06

	PropagateVisibility=true

}
