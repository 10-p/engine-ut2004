// ====================================================================
//  Class:  XInterface.Tab_InstantActionMutators
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_InstantActionMutators extends UT2K3TabPanel;

var config 	string 	LastActiveMutators;

var GUIListBox			MyAvailMutators;
var GUIListBox			MySelectedMutators;
var GUIScrollTextBox	MyDescBox;

var	xMutatorList 		MutList;

var string				MutConfigMenu;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;
	local string t,m;
	
	Super.Initcomponent(MyController, MyOwner);

	MyAvailMutators=GUIListBox(Controls[1]);
	MyAvailMutators.List.OnDblClick=AvailDblClick;

	MySelectedMutators=GUIListBox(Controls[2]);
	MySelectedMutators.List.OnDblClick=SelectedDblClick;
	MyDescBox=GUIScrollTextBox(Controls[3]);

    MutList = PlayerOwner().Spawn(class'xMutatorList');
    MutList.Init();

	if( PlayerOwner().Level.IsDemoBuild())
	{
		i = 0;	
		while (i<MutList.MutatorList.Length)
		{
			if (MutList.MutatorList[i].ClassName ~= "xgame.mutzoominstagib" || MutList.MutatorList[i].ClassName ~= "unrealgame.mutlowgrav")
				i++;
			else
				MutList.MutatorList.Remove(i,1);
		}
			
	
	}

	for (i=0;i<MutList.MutatorList.Length;i++)
		MyAvailMutators.List.Add(MutList.MutatorList[i].FriendlyName,,MutList.MutatorList[i].Description);
	
	if (MyAvailMutators.List.ItemCount > 0)
	{
		MyDescBox.SetContent(MyAvailMutators.List.GetExtra());
		MutConfigMenu = GetMutatorConfigMenu( MyAvailMutators.List.Get() );
	}
	else if (MySelectedMutators.List.ItemCount > 0)
	{
		MyDescBox.SetContent(MySelectedMutators.List.GetExtra());
		MutConfigMenu = GetMutatorConfigMenu( MySelectedMutators.List.Get() );
	}
	else
		MyDescBox.SetContent("None");
	
	if(MutConfigMenu == "")
		Controls[4].bVisible = false;
	else
		Controls[4].bVisible = true;

 	m = LastActiveMutators;		
	t = NextMutatorInString(m);
	while (t!="")
	{
		SelectMutator(t);
		t = NextMutatorInString(m);
	}			
			
}

// Play is called when the play button is pressed.  It saves any releavent data and then
// returns any additions to the URL
function string Play()
{
	local string url,t;
	local int i;
	
	for (i=0;i<MySelectedMutators.ItemCount();i++)
	{
		t = ResolveMutator(MySelectedMutators.List.GetItemAtIndex(i));
		if (t!="")
		{
			if (url!="")
				url=url$","$t;
			else
				url = t;
		}
	} 

	if (url!="")
	{
		LastActiveMutators=url;
		url="?Mutator="$url;
	}
	else
		LastActiveMutators="";
		
	SaveConfig();
	return url;
}

function string ResolveMutator(string FriendlyName)
{
	local int i;
	
	for (i=0;i<MutList.MutatorList.Length;i++)
		if (MutList.MutatorList[i].FriendlyName~=FriendlyName)
			return MutList.MutatorList[i].ClassName;
	
	return "";
}

function string GetMutatorConfigMenu(string FriendlyName)
{
	local int i;
	
	for (i=0;i<MutList.MutatorList.Length;i++)
		if (MutList.MutatorList[i].FriendlyName~=FriendlyName)
			return MutList.MutatorList[i].ConfigMenuClassName;
	
	return "";
}

function string NextMutatorInString(out string mut)
{
	local string t;
	local int p;
	
	if (Mut=="")
		return "";
	
	p = Instr(Mut,",");
	if (p<0)
	{
		t = Mut;
		Mut = "";
	}
	else
	{
	 	t   = Left(Mut,p);
		Mut = Right(Mut,len(Mut)-P-1);
	}
	return t;
}
	
function SelectMutator(string mutclass)
{
	local int i;
	
	for (i=0;i<MutList.MutatorList.Length;i++)
		if (MutList.MutatorList[i].ClassName~=mutclass)
		{
			MyAvailMutators.List.Find(MutList.MutatorList[i].FriendlyName);
			AddMutator(none);
			return;
		}
}
		 

function bool AvailDBLClick(GUIComponent Sender)
{

	AddMutator(Sender);
	return true;
}

function bool SelectedDBLClick(GUIComponent Sender)
{

	RemoveMutator(Sender);
	return true;
}

function bool MutConfigClick(GUIComponent Sender)
{
	if(MutConfigMenu == "")
		return true;

	Log("Launch: "$MutConfigMenu);
	Controller.OpenMenu(MutConfigMenu);

	return true;
}

function ListChange(GUIComponent Sender)
{
	local string NewDesc;
	
	
	if (Sender==MyAvailMutators)
	{
		NewDesc = MyAvailMutators.List.GetExtra();
		MutConfigMenu = GetMutatorConfigMenu( MyAvailMutators.List.Get() );
	}
	else if (Sender==MySelectedMutators)
	{
		NewDesc = MySelectedMutators.List.GetExtra();
		MutConfigMenu = GetMutatorConfigMenu( MySelectedMutators.List.Get() );
	}
	else
		NewDesc = "";
		
	if (NewDesc!="")
		MyDescBox.SetContent(NewDesc);

	// Turn on mutator config menu if desired.
	if(MutConfigMenu == "")
		Controls[4].bVisible = false;
	else
		Controls[4].bVisible = true;
}		

function string GetGroupFor(string FriendlyName)
{
	local int i;
	for (i=0;i<MutList.MutatorList.Length;i++)
		if (MutList.MutatorList[i].FriendlyName ~= FriendlyName)
			return MutList.MutatorList[i].GroupName;
	
	return "";
}

function bool AddMutator(GUIComponent Sender)
{
	local int index,i;
	local string gname;
	
	Index = MyAvailMutators.List.Index;
	if (Index<0)
		return true;

	// Make sure one 1 mutator for each group is in there
	
	gname = GetGroupFor(MyAvailMutators.List.Get());
	if (gname!="")
	{
		for (i=0;i<MySelectedMutators.List.ItemCount;i++)
		{
			if (GetGroupFor(MySelectedMutators.List.GetItemAtIndex(i)) ~= gname)
			{
			
				MyAvailMutators.List.Add(MySelectedMutators.List.GetItemAtIndex(i),,MySelectedMutators.List.GetExtraAtIndex(i));
				MySelectedMutators.List.Remove(i,1);
				break;
			}
		}
	}
		
	MySelectedMutators.List.Add(MyAvailMutators.List.Get(),,MyAvailMutators.List.GetExtra());
	MyAvailMutators.List.Remove(Index,1);

	
	MyAvailMutators.List.SortList();
	MySelectedMutators.List.SortList();
	
	return true;	
	
}

function bool RemoveMutator(GUIComponent Sender)
{
	local int index;
	
	Index = MySelectedMutators.List.Index;
	if (Index<0)
		return true;

	MyAvailMutators.List.Add(MySelectedMutators.List.Get(),,MySelectedMutators.List.GetExtra());
	MySelectedMutators.List.Remove(Index,1);
	
	MyAvailMutators.List.SortList();
	MySelectedMutators.List.SortList();
	
	return true;	
}	

function bool AddAllMutators(GUIComponent Sender)
{
	if (MyAvailMutators.ItemCount()<=0)
		return true;
		
	MySelectedMutators.List.LoadFrom(MyAvailMutators.List,false);
	MyAvailMutators.List.Clear();

//	MySelectedMutators.List.Sort();

}	

function bool RemoveAllMutators(GUIComponent Sender)
{
	if (MySelectedMutators.ItemCount()<=0)
		return true;
		
	MyAvailMutators.List.LoadFrom(MySelectedMutators.List,false);
	MySelectedMutators.List.Clear();
//	MyAvailMutators.List.Sort();
}	

defaultproperties  
{
	Begin Object class=GUIImage Name=IAMutatorBK1
		WinWidth=0.962383
		WinHeight=0.370860
		WinLeft=0.016758
		WinTop=0.630156
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'IAMutatorBK1'

	Begin Object Class=GUIListBox Name=IAMutatorAvailList
		WinWidth=0.368359
		WinLeft=0.041406
		WinHeight=0.502696
		WinTop=0.083281
		bVisibleWhenEmpty=true
		OnChange=ListChange
		StyleName="SquareButton"
		Hint="These are the available mutators."
	End Object
	Controls(1)=GUIListBox'IAMutatorAvailList'

	Begin Object Class=GUIListBox Name=IAMutatorSelectedList
		WinWidth=0.368359
		WinLeft=0.584376
		WinHeight=0.502696
		WinTop=0.083281
		bVisibleWhenEmpty=true
		StyleName="SquareButton"
		OnChange=ListChange
		Hint="These are the mutators you will play with."
	End Object
	Controls(2)=GUIListBox'IAMutatorSelectedList'
			
	Begin Object Class=GUIScrollTextBox Name=IAMutatorScroll
		WinWidth=0.942969
		WinHeight=0.283007
		WinLeft=0.025976
		WinTop=0.645834
		CharDelay=0.0025
		EOLDelay=0.5
		bNeverFocus=true
		StyleName="NoBackground"
	End Object
	Controls(3)=GUIScrollTextBox'IAMutatorScroll'		

	Begin Object Class=GUIButton Name=IAMutatorConfig
		Caption="Configure Mutator"
		Hint="Configure the selected mutator"
		WinWidth=0.239063
		WinHeight=0.054648
		WinLeft=0.729492
		WinTop=0.933490
		bVisible=false
		OnClick=MutConfigClick
	End Object
	Controls(4)=GUIButton'IAMutatorConfig'

	
	Begin Object Class=GUIButton Name=IAMutatorAdd
		Caption="Add"
		Hint="Adds the selection to the list of mutators to play with."
		WinWidth=0.145000
		WinHeight=0.050000
		WinLeft=0.425
		WinTop=0.194114
		OnClickSound=CS_Up
		OnClick=AddMutator
	End Object
	Controls(5)=GUIButton'IAMutatorAdd'

	Begin Object Class=GUIButton Name=IAMutatorRemove
		Caption="Remove"
		Hint="Removes the selection from the list of mutators to play with."
		WinWidth=0.145000
		WinHeight=0.050000
		WinLeft=0.425
		WinTop=0.424322
		OnClickSound=CS_Down
		OnClick=RemoveMutator
	End Object
	Controls(6)=GUIButton'IAMutatorRemove'

	Begin Object Class=GUIButton Name=IAMutatorAll
		Caption="Add All"
		Hint="Adds all mutators to the list of mutators to play with."
		WinWidth=0.145000
		WinHeight=0.050000
		WinLeft=0.425
		WinTop=0.259218
		OnClickSound=CS_Up
		OnClick=AddAllMutators
	End Object
	Controls(7)=GUIButton'IAMutatorAll'

	Begin Object Class=GUIButton Name=IAMutatorClear
		Caption="Remove All"
		Hint="Removes all mutators from the list of mutators to play with."
		WinWidth=0.145000
		WinHeight=0.050000
		WinLeft=0.425
		WinTop=0.360259
		OnClick=RemoveAllMutators
		OnClickSound=CS_Down
	End Object
	Controls(8)=GUIButton'IAMutatorClear'

	Begin Object class=GUILabel Name=IAMutatorAvilLabel
		Caption="Available Mutators"
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.500000
		WinHeight=32.000000
		WinLeft=0.049022
		WinTop=0.015885
	End Object
	Controls(9)=GUILabel'IAMutatorAvilLabel'
			
	Begin Object class=GUILabel Name=IAMutatorSelectedLabel
		Caption="Active Mutators"
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.500000
		WinHeight=32.000000
		WinLeft=0.592383
		WinTop=0.015885
	End Object
	Controls(10)=GUILabel'IAMutatorSelectedLabel'
			
	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false		
}
