/*=============================================================================
	BrowserGroup : Browser window for actor groups
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

extern HWND GhwndEditorFrame;

// --------------------------------------------------------------
//
// NEW/RENAME GROUP Dialog
//
// --------------------------------------------------------------

class WDlgGroup : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgGroup,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WEdit NameEdit;

	FString defName, Name;
	UBOOL bNew;

	// Constructor.
	WDlgGroup( UObject* InContext, WBrowser* InOwnerWindow )
	:	WDialog			( TEXT("Group"), IDDIALOG_GROUP, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	NameEdit		( this, IDEC_NAME )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgGroup::OnInitDialog);
		WDialog::OnInitDialog();

		NameEdit.SetText( *defName );
		::SetFocus( NameEdit.hWnd );

		if( bNew )
			SetText(TEXT("New Group"));
		else
			SetText(TEXT("Rename Group"));

		NameEdit.SetSelection(0, -1);

		unguard;
	}
	virtual INT DoModal( UBOOL InbNew, FString _defName )
	{
		guard(WDlgGroup::DoModal);

		bNew = InbNew;
		defName = _defName;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgGroup::OnOk);
		Name = NameEdit.GetText();
		EndDialog(TRUE);
		unguard;
	}
};

// --------------------------------------------------------------
//
// WBrowserGroup
//
// --------------------------------------------------------------

#define ID_BG_TOOLBAR	29050
TBBUTTON tbBGButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_GB_NEW_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_GB_DELETE_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 3, IDMN_GB_ADD_TO_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 4, IDMN_GB_DELETE_FROM_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 5, IDMN_GB_REFRESH, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 6, IDMN_GB_SELECT, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 7, IDMN_GB_DESELECT, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_BG[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("New Group"), IDMN_GB_NEW_GROUP,
	TEXT("Delete"), IDMN_GB_DELETE_GROUP,
	TEXT("Add Selected Actors to Group(s)"), IDMN_GB_ADD_TO_GROUP,
	TEXT("Delete Select Actors from Group(s)"), IDMN_GB_DELETE_FROM_GROUP,
	TEXT("Refresh Group List"), IDMN_GB_REFRESH,
	TEXT("Select Actors in Group(s)"), IDMN_GB_SELECT,
	TEXT("Deselect Actors in Group(s)"), IDMN_GB_DESELECT,
	NULL, 0
};

class WBrowserGroup : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserGroup,WBrowser,Window)

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WCheckListBox *ListGroups;
	HWND hWndToolBar;
	WToolTip *ToolTipCtrl;

	// Structors.
	WBrowserGroup( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
	{
		Container = NULL;
		ListGroups = NULL;
		MenuID = IDMENU_BrowserGroup;
		BrowserID = eBROWSER_GROUP;
		Description = TEXT("Groups");
	}

	// WBrowser interface.
	void OpenWindow( UBOOL bChild )
	{
		guard(WBrowserGroup::OpenWindow);
		WBrowser::OpenWindow( bChild );
		SetCaption();
		unguard;
	}
	virtual void UpdateMenu()
	{
		guard(WBrowserGroup::UpdateMenu);
		HMENU menu = IsDocked() ? GetMenu( OwnerWindow->hWnd ) : GetMenu( hWnd );
		CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );
		unguard;
	}
	void OnCreate()
	{
		guard(WBrowserGroup::OnCreate);
		WBrowser::OnCreate();

		SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserGroup) );

		Container = new FContainer();

		ListGroups = new WCheckListBox( this, IDLB_GROUPS );
		ListGroups->OpenWindow( 1, 0, 1, 1 );

		hWndToolBar = CreateToolbarEx( 
			hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
			IDB_BrowserGroup_TOOLBAR,
			8,
			hInstance,
			IDB_BrowserGroup_TOOLBAR,
			(LPCTBBUTTON)&tbBGButtons,
			11,
			16,16,
			16,16,
			sizeof(TBBUTTON));
		check(hWndToolBar);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_BG[tooltip].ID > 0 ; ++tooltip )
		{
			// Figure out the rectangle for the toolbar button.
			INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_BG[tooltip].ID, 0 );
			RECT rect;
			SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_BG[tooltip].ToolTip, tooltip, &rect );
		}

		INT Top = 0;
		Anchors.Set( (DWORD)hWndToolBar,		FWindowAnchor( hWnd, hWndToolBar,		ANCHOR_TL, 0, 0,	ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		Anchors.Set( (DWORD)ListGroups->hWnd,	FWindowAnchor( hWnd, ListGroups->hWnd,	ANCHOR_TL, 4, Top,	ANCHOR_BR, -4, -4 ) );

		Container->SetAnchors( &Anchors );

		RefreshGroupList();
		PositionChildControls();

		unguard;
	}
	virtual void RefreshAll()
	{
		guard(WBrowserGroup::RefreshAll);
		RefreshGroupList();
		unguard;
	}
	void OnDestroy()
	{
		guard(WBrowserGroup::OnDestroy);

		delete Container;
		delete ListGroups;

		::DestroyWindow( hWndToolBar );
		delete ToolTipCtrl;

		WBrowser::OnDestroy();
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WBrowserGroup::OnSize);
		WBrowser::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		UpdateMenu();
		unguard;
	}
	// Updates the check status of all the groups, based on the contents of the VisibleGroups
	// variable in the LevelInfo.
	void GetFromVisibleGroups()
	{
		guard(WBrowserGroup::GetFromVisibleGroups);

		// First set all groups to "off"
		for( INT x = 0 ; x < ListGroups->GetCount() ; ++x )
			ListGroups->SetItemData( x, 0 );

		// Now turn "on" the ones we need to.
		TArray<FString> Array;
		GUnrealEd->Level->GetLevelInfo()->VisibleGroups.ParseIntoArray( TEXT(","), &Array );

		for( INT x = 0 ; x < Array.Num() ; ++x )
		{
			INT Index = ListGroups->FindStringExact( *Array(x) );
			if( Index != LB_ERR )
				ListGroups->SetItemData( Index, 1 );
		}

		UpdateVisibility();
		unguard;
	}
	// Updates the VisibleGroups variable in the LevelInfp
	void SendToVisibleGroups()
	{
		guard(WBrowserGroup::SendToVisibleGroups);

		FString NewVisibleGroups;

		for( INT x = 0 ; x < ListGroups->GetCount() ; ++x )
		{
			if( (int)ListGroups->GetItemData( x ) )
			{
				if( NewVisibleGroups.Len() )
					NewVisibleGroups += TEXT(",");
				NewVisibleGroups += ListGroups->GetString(x);
			}
		}

		GUnrealEd->Level->GetLevelInfo()->VisibleGroups = NewVisibleGroups;

		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
		unguard;
	}
	void RefreshGroupList()
	{
		guard(WBrowserGroup::RefreshGroupList);

		// Loop through all the actors in the world and put together a list of unique group names.
		// Actors can belong to multiple groups by seperating the group names with semi-colons ("group1;group2")
		TArray<FString> Groups;

		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if(	Actor 
				&& !Cast<ACamera>(Actor )
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				TArray<FString> Array;
				FString S = *Actor->Group;
				S.ParseIntoArray( TEXT(","), &Array );

				for( INT x = 0 ; x < Array.Num() ; ++x )
				{
					// Only add the group name if it doesn't already exist.
					UBOOL bExists = FALSE;
					for( INT z = 0 ; z < Groups.Num() ; ++z )
						if( Groups(z) == Array(x) )
						{
							bExists = 1;
							break;
						}

					if( !bExists )
						new(Groups)FString( Array(x) );
				}
			}
		}

		// Add the list of unique group names to the group listbox
		ListGroups->Empty();

		for( INT x = 0 ; x < Groups.Num() ; ++x )
		{
			ListGroups->AddString( *Groups(x) );
			ListGroups->SetItemData( ListGroups->FindStringExact( *Groups(x) ), 1 );
		}

		GetFromVisibleGroups();

		ListGroups->SetCurrent( 0, 1 );
		unguard;
	}
	void PositionChildControls( void )
	{
		guard(WBrowserGroup::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}
	// Loops through all actors in the world and updates their visibility based on which groups are checked.
	void UpdateVisibility()
	{
		guard(WBrowserGroup::UpdateVisibility);

		FString NewVisibleGroups;

		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);

			if(	Actor 
				&& !Cast<ACamera>(Actor )
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				Actor->Modify();
				Actor->bHiddenEdGroup = 0;

				TArray<FString> Array;
				FString S = *Actor->Group;
				S.ParseIntoArray( TEXT(","), &Array );

				for( INT x = 0 ; x < Array.Num() ; ++x )
				{
					INT Index = ListGroups->FindStringExact( *Array(x) );
					if( Index != LB_ERR && !(int)ListGroups->GetItemData( Index ) )
					{
						Actor->bHiddenEdGroup = 1;
						break;
					}
				}
			}
		}

		PostMessageX( GhwndEditorFrame, WM_COMMAND, WM_REDRAWALLVIEWPORTS, 0 );
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WBrowserGroup::OnCommand);
		switch( Command ) {

			case IDMN_GB_NEW_GROUP:
				NewGroup();
				break;

			case IDMN_GB_DELETE_GROUP:
				DeleteGroup();
				break;

			case IDMN_GB_ADD_TO_GROUP:
				{
					INT SelCount = ListGroups->GetSelectedCount();
					if( SelCount == LB_ERR )	return;
					int* Buffer = new int[SelCount];
					ListGroups->GetSelectedItems(SelCount, Buffer);
					for( INT s = 0 ; s < SelCount ; ++s )
						AddToGroup(ListGroups->GetString(Buffer[s]));
					delete [] Buffer;
				}
				break;

			case IDMN_GB_DELETE_FROM_GROUP:
				DeleteFromGroup();
				break;

			case IDMN_GB_RENAME_GROUP:
				RenameGroup();
				break;

			case IDMN_GB_REFRESH:
				OnRefreshGroups();
				break;

			case IDMN_GB_SELECT:
				SelectActorsInGroups(1);
				break;

			case IDMN_GB_DESELECT:
				SelectActorsInGroups(0);
				break;

			case WM_WCLB_UPDATE_VISIBILITY:
				UpdateVisibility();
				SendToVisibleGroups();
				break;

			default:
				WBrowser::OnCommand(Command);
				break;
		}
		unguard;
	}
	void SelectActorsInGroups( UBOOL Select )
	{
		guard(WBrowserGroup::SelectActorsInGroups);

		INT SelCount = ListGroups->GetSelectedCount();
		if( SelCount == LB_ERR )	return;
		int* Buffer = new int[SelCount];
		ListGroups->GetSelectedItems(SelCount, Buffer);

		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if(	Actor 
				&& !Cast<ACamera>(Actor )
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				FString GroupName = *Actor->Group;
				TArray<FString> Array;
				GroupName.ParseIntoArray( TEXT(","), &Array );
				for( INT x = 0 ; x < Array.Num() ; ++x )
				{
					INT idx = ListGroups->FindStringExact( *Array(x) );

					for( INT s = 0 ; s < SelCount ; ++s )
						if( idx == Buffer[s] )
							GUnrealEd->SelectActor( GUnrealEd->Level, Actor, Select, 0 );
				}
			}
		}

		delete [] Buffer;
		PostMessageX( GhwndEditorFrame, WM_COMMAND, WM_REDRAWALLVIEWPORTS, 0 );
		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );

		unguard;
	}
	void NewGroup()
	{
		guard(WBrowserGroup::NewGroup);

		if( !NumActorsSelected() )
		{
			appMsgf(0, TEXT("You must have some actors selected to create a new group."));
			return;
		}

		// Generate a suggested group name to use as a default.
		INT x = 1;
		FString DefaultName;
		while(1)
		{
			DefaultName = *FString::Printf(TEXT("Group%d"), x);
			if( ListGroups->FindStringExact( *DefaultName ) == LB_ERR )
				break;
			x++;
		}

		WDlgGroup dlg( NULL, this );
		if( dlg.DoModal( 1, DefaultName ) )
		{
			if( GUnrealEd->Level->GetLevelInfo()->VisibleGroups.Len() )
				GUnrealEd->Level->GetLevelInfo()->VisibleGroups += TEXT(",");
			GUnrealEd->Level->GetLevelInfo()->VisibleGroups += dlg.Name;

			AddToGroup( dlg.Name );
			RefreshGroupList();
		}

		unguard;
	}
	void DeleteGroup()
	{
		guard(WBrowserGroup::DeleteGroup);
		INT SelCount = ListGroups->GetSelectedCount();
		if( SelCount == LB_ERR )	return;
		int* Buffer = new int[SelCount];
		ListGroups->GetSelectedItems(SelCount, Buffer);
		for( INT s = 0 ; s < SelCount ; ++s )
		{
			FString DeletedGroup = ListGroups->GetString(Buffer[s]);
			for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
			{
				AActor* Actor = GUnrealEd->Level->Actors(i);
				if(	Actor 
					&& !Cast<ACamera>(Actor )
					&& Actor!=GUnrealEd->Level->Brush()
					&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
				{
					FString GroupName = *Actor->Group;
					TArray<FString> Array;
					GroupName.ParseIntoArray( TEXT(","), &Array );
					FString NewGroup;
					for( INT x = 0 ; x < Array.Num() ; ++x )
					{
						if( Array(x) != DeletedGroup )
						{
							if( NewGroup.Len() )
								NewGroup += TEXT(",");
							NewGroup += Array(x);
						}
					}
					if( NewGroup != *Actor->Group )
					{
						Actor->Modify();
						Actor->Group = *NewGroup;
					}
				}
			}
		}
		delete [] Buffer;

		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
		RefreshGroupList();
		unguard;
	}
	void AddToGroup( FString InGroupName)
	{
		guard(WBrowserGroup::AddToGroup);
		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if(	Actor 
				&& Actor->bSelected
				&& !Cast<ACamera>(Actor )
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				FString GroupName = *Actor->Group, NewGroupName;

				// Make sure this actor is not already in this group.  If so, don't add it again.
				TArray<FString> Array;
				GroupName.ParseIntoArray( TEXT(","), &Array );
				INT x;
				for( x = 0 ; x < Array.Num() ; ++x )
				{
					if( Array(x) == InGroupName )
						break;
				}

				if( x == Array.Num() )
				{
					// Add the group to the actors group list
					NewGroupName = *FString::Printf(TEXT("%s%s%s"), *GroupName, (GroupName.Len()?TEXT(","):TEXT("")), *InGroupName );
					Actor->Modify();
					Actor->Group = *NewGroupName;
				}
			}
		}

		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
		unguard;
	}
	void DeleteFromGroup()
	{
		guard(WBrowserGroup::DeleteFromGroup);
		INT SelCount = ListGroups->GetSelectedCount();
		if( SelCount == LB_ERR )	return;
		int* Buffer = new int[SelCount];
		ListGroups->GetSelectedItems(SelCount, Buffer);
		for( INT s = 0 ; s < SelCount ; ++s )
		{
			FString DeletedGroup = ListGroups->GetString(Buffer[s]);

			for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
			{
				AActor* Actor = GUnrealEd->Level->Actors(i);
				if(	Actor 
					&& Actor->bSelected
					&& !Cast<ACamera>(Actor )
					&& Actor!=GUnrealEd->Level->Brush()
					&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
				{
					FString GroupName = *Actor->Group;
					TArray<FString> Array;
					GroupName.ParseIntoArray( TEXT(","), &Array );
					FString NewGroup;
					for( INT x = 0 ; x < Array.Num() ; ++x )
						if( Array(x) != DeletedGroup )
						{
							if( NewGroup.Len() )
								NewGroup += TEXT(",");
							NewGroup += Array(x);
						}
					if( NewGroup != *Actor->Group )
					{
						Actor->Modify();
						Actor->Group = *NewGroup;
					}
				}
			}
		}

		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
		RefreshGroupList();
		unguard;
	}
	void RenameGroup()
	{
		guard(WBrowserGroup::RenameGroup);

		WDlgGroup dlg( NULL, this );
		INT SelCount = ListGroups->GetSelectedCount();
		if( SelCount == LB_ERR )	return;
		int* Buffer = new int[SelCount];
		ListGroups->GetSelectedItems(SelCount, Buffer);
		for( INT s = 0 ; s < SelCount ; ++s )
		{
			FString Src = ListGroups->GetString(Buffer[s]);
			if( dlg.DoModal( 0, Src ) )
				SwapGroupNames( Src, dlg.Name );
		}
		delete [] Buffer;

		RefreshGroupList();
		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );

		unguard;
	}
	void SwapGroupNames( FString Src, FString Dst )
	{
		guard(WBrowserGroup::SwapGroupNames);

		if( Src == Dst ) return;
		check(Src.Len());
		check(Dst.Len());

		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if(	Actor 
				&& !Cast<ACamera>(Actor )
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				FString GroupName = *Actor->Group, NewGroup;
				TArray<FString> Array;
				GroupName.ParseIntoArray( TEXT(","), &Array );
				for( INT x = 0 ; x < Array.Num() ; ++x )
				{
					FString AddName;
					AddName = Array(x);
					if( Array(x) == Src )
						AddName = Dst;

					if( NewGroup.Len() )
						NewGroup += TEXT(",");
					NewGroup += AddName;
				}

				if( NewGroup != *Actor->Group )
				{
					Actor->Modify();
					Actor->Group = *NewGroup;
				}
			}
		}
		unguard;
	}
	INT NumActorsSelected()
	{
		guard(WBrowserGroup::NumActorsSelected);
		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("ACTOR"), TEXT("NUMSELECTED"), GetPropResult );
		return appAtoi(*GetPropResult);
		unguard;
	}

	// Notification delegates for child controls.
	//
	void OnNewGroup()
	{
		guard(WBrowserGroup::OnNewGroupClick);
		NewGroup();
		unguard;
	}
	void OnDeleteGroup()
	{
		guard(WBrowserGroup::OnDeleteGroupClick);
		DeleteGroup();
		unguard;
	}
	void OnAddToGroup()
	{
		guard(WBrowserGroup::OnAddToGroupClick);
		INT SelCount = ListGroups->GetSelectedCount();
		if( SelCount == LB_ERR )	return;
		int* Buffer = new int[SelCount];
		ListGroups->GetSelectedItems(SelCount, Buffer);
		for( INT s = 0 ; s < SelCount ; ++s )
			AddToGroup(ListGroups->GetString(Buffer[s]));
		delete [] Buffer;
		unguard;
	}
	void OnDeleteFromGroup()
	{
		guard(WBrowserGroup::OnDeleteFromGroupClick);
		DeleteFromGroup();
		unguard;
	}
	void OnRefreshGroups()
	{
		guard(WBrowserGroup::OnRefreshGroupsClick);
		RefreshGroupList();
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

