class Browser_ServerListPageBuddy extends Browser_ServerListPageMS;

// Actual list of buddies
var() config array<String> Buddies;

var GUISplitter		  MainSplit;
var GUISplitter		  BudSplit;
var Browser_BuddyList MyBuddyList;
var localized string AddBuddyCaption;
var localized string RemoveBuddyCaption;

var bool BuddyInitialized;

function InitComponent(GUIController C, GUIComponent O)
{
	Super.InitComponent(C, O);

	if( !BuddyInitialized )
	{
		MainSplit = GUISplitter(Controls[0]);

		// Set one half of the buddy splitter to be the servers panel
		BudSplit.Controls[1] = MainSplit.Controls[0];

		// Set the buddy splitter as one half of the main splitter
		MainSplit.Controls[0] = BudSplit;

		// Init the budsplit
		BudSplit.InitComponent(C, MainSplit);
	}

	MyBuddyList = Browser_BuddyList( GUIMultiColumnListBox(GUIPanel(BudSplit.Controls[0]).Controls[0]).Controls[0] );
	MyBuddyList.MyBuddyPage = self;
	MyBuddyList.ItemCount = Buddies.Length;

	// Change server list spacing a bit
	MyServersList.InitColumnPerc[0]=0.1;
	MyServersList.InitColumnPerc[1]=0.25;
	MyServersList.InitColumnPerc[2]=0.15;
	MyServersList.InitColumnPerc[3]=0.125;
	MyServersList.InitColumnPerc[4]=0.125;

	// This page has a REFRESH LIST button
	GUIButton(GUIPanel(Controls[1]).Controls[6]).OnClick=MyRefreshClick;
	GUIButton(GUIPanel(Controls[1]).Controls[6]).Caption=RefreshCaption;
	GUIButton(GUIPanel(Controls[1]).Controls[6]).bVisible=true;

	// take over the "Add Favorite" button for "Add Buddy".
	GUIButton(GUIPanel(Controls[1]).Controls[7]).OnClick=AddBuddyClick;
	GUIButton(GUIPanel(Controls[1]).Controls[7]).Caption=AddBuddyCaption;

	// add "Remove Buddy" button
	GUIButton(GUIPanel(Controls[1]).Controls[4]).OnClick=RemoveBuddyClick;
	GUIButton(GUIPanel(Controls[1]).Controls[4]).Caption=RemoveBuddyCaption;

	StatusBar.WinWidth=0.6;

	BuddyInitialized = True;
}

function bool MyRefreshClick(GUIComponent Sender)
{
	Super.MyRefreshClick(Sender);
	return true;
}

function RefreshList()
{
	local int i;
	local MasterServerClient.QueryData QD;

	MyServersList.Clear();

	// Construct query containing all buddy names
	i = Buddies.Length;
	MSC.Query.Length = i;

	for(i=0; i<Buddies.Length; i++)
	{
		QD.Key			= "buddy";
		QD.Value		= Buddies[i];
		QD.QueryType	= QT_Equals;
		MSC.Query[i] = QD;
	}

	// Run query
	MSC.StartQuery(CTM_Query);

	StatusBar.SetCaption(StartQueryString);
	SetTimer(0, false); // Stop it going back to ready from a previous timer!
}

// Add a new buddy to your list
function bool AddBuddyClick(GUIComponent Sender)
{
	if ( Controller.OpenMenu("xinterface.Browser_AddBuddy") )
		Browser_AddBuddy(Controller.TopPage()).MyBuddyPage = self;

	return true;
}

// Remove current buddy from your list
function bool RemoveBuddyClick(GUIComponent Sender)
{
	if(MyBuddyList.Index < 0 || MyBuddyList.Index >= Buddies.Length)
		return true;

	Buddies.Remove(MyBuddyList.Index, 1);

	MyBuddyList.ItemCount = Buddies.Length;

	SaveConfig();
	return true;
}

defaultproperties
{
	// Buddy list box
	Begin Object Class=Browser_BuddyList Name=TheBuddyList
	End Object

	Begin Object Class=GUIMultiColumnListBox Name=BuddyListBox
		WinLeft=0
		WinTop=0
		WinWidth=1
		WinHeight=1
		bVisibleWhenEmpty=True
		Controls(0)=TheBuddyList
		StyleName="ServerBrowserGrid"
	End Object

	// Buddy panel
	Begin Object Class=GUIPanel Name=BuddyPanel
		Controls(0)=BuddyListBox;
	End Object

	// Splitter to divide buddies and rules horizontally
	Begin Object Class=GUISplitter Name=BuddySplitter
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		Controls(0)=BuddyPanel
		SplitOrientation=SPLIT_Horizontal
		SplitPosition=0.25
		Background=DefaultTexture
		bBoundToParent=true
		bScaleToParent=true
	End Object
	BudSplit=BuddySplitter

	AddBuddyCaption="ADD BUDDY"
	RemoveBuddyCaption="REMOVE BUDDY"
}
