// ====================================================================
//  Class:  XInterface.Tab_IADeathMatch
//  Parent: XInterface.Tab_InstantActionBaseRules
//
//  <Enter a description here>
// ====================================================================

class Tab_IADeathMatch extends Tab_InstantActionBaseRules;

var config bool	LastAutoAdjustSkill;

var localized string	GoalScoreText;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.Initcomponent(MyController, MyOwner);
	MyGoalScore.MyLabel.Caption = GoalScoreText;

	moCheckBox(Controls[15]).Checked(LastAutoAdjustSkill);
}

function string Play()
{
	LastAutoAdjustSkill = moCheckBox(Controls[15]).IsChecked();
	SaveConfig();

	return Super.Play()$"?AutoAdjust="$LastAutoAdjustSkill;
}

defaultproperties
{
	Begin Object class=GUIImage Name=IARulesBK4
		WinWidth=0.957500
		WinHeight=0.156016
		WinLeft=0.021641
		WinTop=0.78543
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(14)=GUIImage'IARulesBK4'

	Begin Object class=moCheckBox Name=IARulesAutoAdjustSkill
		WinWidth=0.250000
		WinHeight=0.156016
		WinLeft=0.375000
		WinTop=0.858295
		Caption="Auto-Adjust Skill"
		Hint="When enabled, bots will adjust their skill to match yours."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(15)=moCheckBox'IARulesAutoAdjustSkill'

	GoalScoreText="Frag Limit"
}