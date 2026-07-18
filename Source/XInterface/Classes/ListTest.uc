class ListTest extends TestPageBase;

//var GUIList	TheList;
//var GUIStringCollection Col1, Col2;
var GUIScrollText	tScroll;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
local string newText;

	Super.InitComponent(MyController, MyOwner);
	tScroll=GUIScrollTextBox(Controls[0]).MyScrollText;

	newText = "This is a simple test of multiline splitting that should be happening very easily but i will debug to be sure that whatever the length is set at it wont bug."$Chr(10)$Chr(10);
	newText = newText$"But more importantly i must also handle having more than just 1 line of text and see how this is dealt with."$Chr(10);
	newText = newText$"I wonder how this would be dealt with if i wasnt taking care of it and if Joe wasnt going to do it."$Chr(10)$Chr(10);
	newText = newText$"Bah, We'll see later since no one seems to reply to my messages.";

	tScroll.SetContent(newText);
//	TheList=GUIList(Controls[0]);
//	Col1=GUIStringCollection(GUIListCollection(Controls[1]).MyCollection);
//	Col2=GUIStringCollection(GUIListCollection(Controls[2]).MyCollection);
//	Col2.bMultiSelect=true;
	Controller.bDesignMode = false;
}

function bool AddClick(GUIComponent Sender)
{
//	TheList.Add("Simple Item Text");
//	Col1.AddItem("Simple Item Text"@Col1.Count());
//	Col2.AddItem("Simple Item Text"@Col1.Count());
	return true;
}

defaultproperties
{
	Begin Object Class=GUIScrollTextBox Name=Scroller
		WinTop=0.05
		WinLeft=0.05
		WinWidth=0.35
		WinHeight=0.2
		CharDelay=0.05
		EOLDelay=0.6
//		bRepeat=true
	End Object

	// First a normal GUIList
//	Begin Object Class=GUIList Name=lbTheList
//		WinTop=0.05
//		WinLeft=0.05
//		WinWidth=0.4
//		WinHeight=0.4
//	End Object

	// Then, a single selection list
//	Begin Object Class=GUIListCollection Name=lcSingleList
//		WinTop=0.05
//		WinLeft=0.55
//		WinWidth=0.4
//		WinHeight=0.4
//	End Object

	// Then a multi-selection List
//	Begin Object Class=GUIListCollection Name=lcMultiList
//		WinTop=0.55
//		WinLeft=0.55
//		WinWidth=0.4
//		WinHeight=0.4
//	End Object

	Begin Object Class=GUIButton Name=btnAddItems
		WinLeft=0.05
		WinTop=0.55
		WinWidth=0.4
		WinHeight=0.06
		OnClick=AddClick
	End Object

	Controls(0)=GUIScrollTextBox'Scroller'
	Controls(1)=GUIButton'btnAddItems'

//	Controls(0)=GUIList'lbTheList'
//	Controls(1)=GUIListCollection'lcSingleList'
//	Controls(2)=GUIListCollection'lcMultiList'
//	Controls(3)=GUIButton'btnAddItems'
}
