// ====================================================================
//  Class:  XInterface.Tab_InstantActionMapList
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_InstantActionMapList extends UT2K3TabPanel;

var GUIListBox 			MyFullMapList;
var GUIListBox 			MyCurMapList;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.Initcomponent(MyController, MyOwner);

	MyFullMapList  			= GUIListBox(Controls[0]);
	MyCurMapList 	  		= GUIListBox(Controls[1]);
}

function string Play()
{
/*
	local TAB_InstantActionMain pMain;
	local MapList ML;
	local class<MapList> MLClass;
	local int i;

	if (UT2InstantActionPage(Controller.ActivePage)==None)
		return "";

	pMain = UT2InstantActionPage(Controller.ActivePage).pMain;

	MLClass = class<MapList>(DynamicLoadObject(pMain.GetMapListClass(), class'class'));
	if (MLClass!=None)
	{
		ML = PlayerOwner().spawn(MLClass);
		if (ML!=None)
		{
			ML.Maps.Remove(0,ML.Maps.Length);
			for (i=0;i<MyCurMapList.ItemCount();i++)
				ML.Maps[i]=MyCurMapList.List.GetItemAtIndex(i);

			ML.SaveConfig();
			ML.Destroy();
		}
	}
*/
	return "";
}


function ReadMapList(string MapPreFix, string MapListClass)
{
/*
	local MapList ML;
	local class<MapList> MLClass;
	local int i,j;

	MyFullMapList.List.Clear();
	MyCurMapList.List.Clear();

	Controller.GetMapList(MapPrefix,MyFullMapList.List);
	MyFullMapList.List.SetIndex(0);

	MLClass = class<MapList>(DynamicLoadObject(MapListClass, class'class'));
	if (MLClass!=None)
	{
		ML = PlayerOwner().spawn(MLClass);
		if (ML!=None)
		{
			for (i=0;i<ML.Maps.Length;i++)
			{
				for (j=0;j<MyFullMapList.ItemCount();j++)
				{
					if (MyFullMapList.List.GetItemAtIndex(j) ~= ML.Maps[i])
					{
						MyCurMapList.List.Add(ML.Maps[i]);
						MyFullmapList.List.Remove(j,1);
						break;
					}
				}
			}
			ML.Destroy();
		}
	}
*/
}

function bool MapAdd(GUIComponent Sender)
{
	if ( (MyFullMapList.ItemCount()==0) || (MyFullMapList.List.Index<0) )
		return true;

	MyCurMapList.List.Add(MyFullMapList.List.Get());
	MyFullMapList.List.Remove(MyFullMapList.List.Index,1);

	return true;
}

function bool MapRemove(GUIComponent Sender)
{
	if ( (MyCurMapList.ItemCount()==0) || (MyCurMapList.List.Index<0) )
		return true;

	MyFullMapList.List.Add(MyCurMapList.List.Get());
	MyCurMapList.List.Remove(MyCurMapList.List.Index,1);

	return true;
}

function bool MapAll(GUIComponent Sender)
{
	if (MyFullMapList.ItemCount()==0)
		return true;

	MyCurMapList.List.LoadFrom(MyFullMapList.List,false);
	MyFullMapList.List.Clear();

	return true;
}

function bool MapClear(GUIComponent Sender)
{
	if (MyCurMapList.ItemCount()==0)
		return true;

	MyFullMapList.List.LoadFrom(MyCurMapList.List,false);
	MyCurMapList.List.Clear();

	return true;
}

function bool MapUp(GUIComponent Sender)
{
	local int index;
	if ( (MyCurMapList.ItemCount()==0) || (MyCurMapList.List.Index<0) )
		return true;

	index = MyCurMapList.List.Index;
	if (index>0)
	{
		MyCurMapList.List.Swap(index,index-1);
		MyCurMapList.List.Index = index-1;
	}

	return true;
}

function bool MapDown(GUIComponent Sender)
{
	local int index;
	if ( (MyCurMapList.ItemCount()==0) || (MyCurMapList.List.Index<0) )
		return true;

	index = MyCurMapList.List.Index;
	if (index<MyCurMapList.ItemCount()-1)
	{
		MyCurMapList.List.Swap(index,index+1);
		MyCurMapList.List.Index = index+1;
	}

	return true;
}

defaultproperties
{
	Begin Object Class=GUIListBox Name=IAMapListFullMapList
		WinWidth=0.368359
		WinHeight=0.783942
		WinLeft=0.041406
		WinTop=0.083281
		bVisibleWhenEmpty=true
		StyleName="SquareButton"
		Hint="Available maps"
	End Object
	Controls(0)=GUIListBox'IAMapListFullMapList'

	Begin Object Class=GUIListBox Name=IAMapListCurMapList
		WinWidth=0.368359
		WinHeight=0.783942
		WinLeft=0.584376
		WinTop=0.083281
		bVisibleWhenEmpty=true
		StyleName="SquareButton"
		Hint="Selected maps"
	End Object
	Controls(1)=GUIListBox'IAMapListCurMapList'

	Begin Object Class=GUIButton Name=IAMapListAdd
		Caption="Add"
		Hint="Add this map to your map list"
		WinWidth=0.145
		WinHeight=0.05
		WinLeft=0.425
		WinTop=0.323801
		OnClick=MapAdd
		OnClickSound=CS_Up
	End Object
	Controls(2)=GUIButton'IAMapListAdd'

	Begin Object Class=GUIButton Name=IAMapListRemove
		Caption="Remove"
		Hint="Remove this map from your map list"
		WinWidth=0.145
		WinHeight=0.05
		WinLeft=0.425
		WinTop=0.493072
		OnClick=MapRemove
		OnClickSound=CS_Down
	End Object
	Controls(3)=GUIButton'IAMapListRemove'

	Begin Object Class=GUIButton Name=IAMapListAll
		Caption="Add All"
		Hint="Add all maps to your map list"
		WinWidth=0.145
		WinHeight=0.05
		WinLeft=0.425
		WinTop=0.388905
		OnClick=MapAll
		OnClickSound=CS_Up
	End Object
	Controls(4)=GUIButton'IAMapListAll'

	Begin Object Class=GUIButton Name=IAMapListClear
		Caption="Remove All"
		Hint="Remove all maps from your map list"
		WinWidth=0.145
		WinHeight=0.05
		WinLeft=0.425
		WinTop=0.558176
		OnClick=MapClear
		OnClickSound=CS_Down
	End Object
	Controls(5)=GUIButton'IAMapListClear'

	Begin Object Class=GUIButton Name=IAMapListUp
		Caption="Up"
		Hint="Move this map higher up in the list"
		WinWidth=0.145
		WinHeight=0.05
		WinLeft=0.425
		WinTop=0.121978
		OnClick=MapUp
		OnClickSound=CS_Up
	End Object
	Controls(6)=GUIButton'IAMapListUp'

	Begin Object Class=GUIButton Name=IAMapListDown
		Caption="Down"
		Hint="Move this map lower down in the list"
		WinWidth=0.145
		WinHeight=0.05
		WinLeft=0.425
		WinTop=0.779531
		OnClick=MapDown
		OnClickSound=CS_Down
	End Object
	Controls(7)=GUIButton'IAMapListDown'


	Begin Object class=GUILabel Name=IAMapListAvilLabel
		Caption="Available Map List"
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.500000
		WinHeight=32.000000
		WinLeft=0.049022
		WinTop=0.015885
	End Object
	Controls(8)=GUILabel'IAMapListAvilLabel'

	Begin Object class=GUILabel Name=IAMapListSelectedLabel
		Caption="Selected Map List"
		TextALign=TXTA_Left
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.500000
		WinHeight=32.000000
		WinLeft=0.592383
		WinTop=0.015885
	End Object
	Controls(9)=GUILabel'IAMapListSelectedLabel'

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false
}
