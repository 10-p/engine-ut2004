/*=============================================================================
	BrowserLIPSinc.cpp : Browser window for LIPSinc Animations
	
	  Revision history:
		* Created by Jamie Redmond
		* UI Tweaks by John Briggs

    Work-in-progress TODO's:
		
=============================================================================*/

#include "UnrealEd.h"
#include <mmsystem.h>

#ifndef PlaySound
#ifdef UNICODE
	#define PlaySound  PlaySoundW
#else
	#define PlaySound  PlaySoundA
#endif // !UNICODE
#endif

// LIPSinc Browser import/export/interface preferences.
#define IMPERSONATOR_VERSION_STRING TEXT("LIPSinc Impersonator for Unreal - 2110")

#ifdef WITH_LIPSINC

	void ULIPSincControllerProps::PostEditChange()
	{
		guard(ULIPSincControllerProps::PostEditChange);
		WBrowserLIPSinc* Browser = (WBrowserLIPSinc*)WBrowserLIPSincPtr;
		Browser->SaveLIPSincControllerProperties();
		unguard;
	}

	IMPLEMENT_CLASS(ULIPSincControllerProps);

	void ULIPSincAnimProps::PostEditChange()
	{
		guard(ULIPSincAnimProps::PostEditChange);
		WBrowserLIPSinc* Browser = (WBrowserLIPSinc*)WBrowserLIPSincPtr;
		Browser->SaveLIPSincAnimProperties();
		unguard;
	}

	IMPLEMENT_CLASS(ULIPSincAnimProps);

	void ULIPSincPrefsProps::PostEditChange()
	{
		guard(ULIPSincPrefsProps::PostEditChange);
		WBrowserLIPSinc* Browser = (WBrowserLIPSinc*)WBrowserLIPSincPtr;
		Browser->SaveLIPSincPrefsProperties();
		unguard;
	}

	IMPLEMENT_CLASS(ULIPSincPrefsProps);

#else

	void ULIPSincControllerProps::PostEditChange()
	{
		guard(ULIPSincControllerProps::PostEditChange);
		unguard;
	}

	IMPLEMENT_CLASS(ULIPSincControllerProps);

	void ULIPSincAnimProps::PostEditChange()
	{
		guard(ULIPSincAnimProps::PostEditChange);
		unguard;
	}

	IMPLEMENT_CLASS(ULIPSincAnimProps);

	void ULIPSincPrefsProps::PostEditChange()
	{
		guard(ULIPSincPrefsProps::PostEditChange);
		unguard;
	}

	IMPLEMENT_CLASS(ULIPSincPrefsProps);

#endif

#ifdef WITH_LIPSINC

#pragma comment(lib, "winmm.lib") // PlaySound
#pragma comment(lib, "..\\..\\ImpersonatorLib\\ReleaseDynamic\\ImpersonatorLib_rd.lib")

#define LIPSINC_SHOWFLAGS SHOW_StandardView | SHOW_ChildWindow | SHOW_Frame | SHOW_Actors | SHOW_StaticMeshes | SHOW_Particles

void NoControllerMessage( void )
{
	appMsgf(0, TEXT("The current mesh does not have a LIPSinc controller.  Please import a .LBP file."));
}

class TLIPSincQuickLoadList
{

public:

	TLIPSincQuickLoadList()
	{
		guard(TLIPSincQuickLoadList::TLIPSincQuickLoadList);
		unguard;
	}

	~TLIPSincQuickLoadList()
	{
		guard(TLIPSincQuickLoadList::~TLIPSincQuickLoadList);
		Clear();
		unguard;
	}

	UBOOL FindLastLTF( TLIPSincController &InController, INT InAnimIndex, FString &OutLTFPath )
	{
		guard(TLIPSincQuickLoadList::FindLastLTF);
		return _FindLastLTF( InController.Name(), InController.GetAnimation(InAnimIndex)->Name(), OutLTFPath );
		unguard;
	}

	void UpdateList( TLIPSincController &InController, INT InAnimIndex, FString InLTFPath )
	{
		guard(TLIPSincQuickLoadList::UpdateList);
		_UpdateList( InController.Name(), InController.GetAnimation(InAnimIndex)->Name(), InLTFPath );
		unguard;
	}

	void Clear( void )
	{
		guard(TLIPSincQuickLoadList::Clear);
		QuickLoadList.Empty();
		unguard;
	}

	// Debugging Only.
	void PrintList( void )
	{
		guard(TLIPSincQuickLoadList::PrintList);

		debugf(TEXT("[LIPSinc]: Printing Quick Load List..."));

		INT NumEntries = QuickLoadList.Num();

        for( INT i = 0; i < NumEntries; ++i )
		{
			debugf(TEXT("[LIPSinc]:\t %s -> %s -> %s"),
				*QuickLoadList(i).ControllerName,
				*QuickLoadList(i).AnimName,
				*QuickLoadList(i).PathToLTF);
		}

		debugf(TEXT("[LIPSinc]: End of List."));

		unguard;
	}

protected:

	UBOOL _FindLastLTF( FString InControllerName, FString InAnimName, FString &OutLTFPath )
	{
		guard(TLIPSincQuickLoadList::FindLastLTF);

        INT NumEntries = QuickLoadList.Num();

		for( INT i = 0; i < NumEntries; ++i )
		{
			if( QuickLoadList(i).ControllerName == InControllerName &&
				QuickLoadList(i).AnimName       == InAnimName )
			{
				OutLTFPath = QuickLoadList(i).PathToLTF;
				return 1;
			}
		}

		return 0;

		unguard;
	}

	void _UpdateList( FString InControllerName, FString InAnimName, FString InLTFPath )
	{
		guard(TLIPSincQuickLoadList::UpdateList);

		INT NumEntries = QuickLoadList.Num();

		for( INT i = 0; i < NumEntries; ++i )
		{
			if( QuickLoadList(i).ControllerName == InControllerName &&
				QuickLoadList(i).AnimName       == InAnimName )
			{
				QuickLoadList(i).PathToLTF = InLTFPath;
				return;
			}
		}

		TLIPSincQuickLoadEntry NewEntry( InControllerName, InAnimName, InLTFPath );

		TLIPSincQuickLoadEntry *pNewEntry = new(QuickLoadList)TLIPSincQuickLoadEntry();
		*pNewEntry = NewEntry;

		unguard;
	}

	class TLIPSincQuickLoadEntry
	{
		
	public:

		TLIPSincQuickLoadEntry()
		{
			guard(TLIPSincQuickLoadEntry::TLIPSincQuickLoadEntry);
			unguard;
		}

		TLIPSincQuickLoadEntry( FString InControllerName, FString InAnimName, FString InLTFPath )
		{
			guard(TLIPSincQuickLoadEntry::TLIPSincQuickLoadEntry);
			ControllerName = InControllerName;
			AnimName       = InAnimName;
			PathToLTF	   = InLTFPath;
			unguard;
		}

		~TLIPSincQuickLoadEntry()
		{
			guard(TLIPSincQuickLoadEntry::~TLIPSincQuickLoadEntry);
			unguard;
		}

		FString ControllerName;
		FString AnimName;
		FString PathToLTF;
	};

	TArray<TLIPSincQuickLoadEntry> QuickLoadList;
};

TLIPSincQuickLoadList GQuickLoadList;

// --------------------------------------------------------------
//
// Name LIPSinc Animation dialog.
//
// --------------------------------------------------------------

class WDlgNameLIPSincAnimation : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNameLIPSincAnimation,WDialog,UnrealEd)

	// Variables
	WEdit   NameEdit;

	WButton OkButton;
	WButton CancelButton;

	FString	Name;

	// Constructor.
	WDlgNameLIPSincAnimation( UObject* InContext, WBrowser* InOwnerWindow )
		: WDialog		( TEXT("Name LIPSinc Animation"), IDDIALOG_NAME_LIPSINC_ANIMATION, InOwnerWindow )
	, NameEdit			( this, IDC_NEW_LIPSINC_ANIM_NAME )
	, OkButton          ( this, IDOK, FDelegate(this,(TDelegate)OnOk) )
	, CancelButton		( this, IDCANCEL, FDelegate(this,(TDelegate)OnCancel) )
	{
	}
	
	// WDialog Interface.
	void OnInitDialog()
	{
		guard(WDlgNameLIPSincAnimation::OnInitDialog);
		WDialog::OnInitDialog();
		NameEdit.SetText( *Name );
		unguard;
	}

	virtual INT DoModal( FString InName )
	{
		guard(WDlgCreateLIPSincAnim::DoModal);
		Name = InName;
		return WDialog::DoModal( hInstance );
		unguard;
	}

	// Save the name.
	void OnOk()
	{		
		guard(WDlgNameLIPSincAnimation::OnOk);
		if( GetDataFromUser() )
		{				
			EndDialog(TRUE);
		}
		unguard;
	}

	// Don't save the name.
	void OnCancel()
	{
		guard(WDlgNameLIPSincAnimation::OnCancel);
		EndDialog(FALSE);
		unguard;
	}

	BOOL GetDataFromUser( void )
	{
		guard(WDlgNameLIPSincAnimation::GetDataFromUser);
		Name = NameEdit.GetText();
		return TRUE;
		unguard;
	}
};

// --------------------------------------------------------------
//
// Delete LIPSinc Expression dialog.
//
// --------------------------------------------------------------

class WDlgDeleteLIPSincExpression : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgDeleteLIPSincExpression,WDialog,UnrealEd)

	// Variables
	WListBox ExpressionList;
	WButton OkButton;
	WButton DeleteButton;

	TLIPSincController *pLIPSincController;
	USkeletalMeshInstance *pMeshInstance;

	// Constructor.
	WDlgDeleteLIPSincExpression( UObject* InContext, WBrowser* InOwnerWindow )
		:	WDialog		  ( TEXT("Delete Expression"), IDDIALOG_LIPSINC_DELETE_EXPRESSION, InOwnerWindow )
	,	OkButton		  ( this, IDOK,	FDelegate(this,(TDelegate)OnOk) )
	,	DeleteButton	  ( this, ID_LIPSINC_DELETE_EXPRESSION,	FDelegate(this,(TDelegate)OnDeleteExpression) )
	,	ExpressionList	  ( this, ID_LIPSINC_EXPRESSION_LIST )
	,   pLIPSincController( NULL )
	,   pMeshInstance     ( NULL )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgDeleteLIPSincExpression::OnInitDialog);
		WDialog::OnInitDialog();

		// Fill out the list box with expression names.
		if( pLIPSincController )
		{
			for( INT i = 0; i < pLIPSincController->NumExpressions(); ++i )
			{
				ExpressionList.AddString( *FString::Printf(TEXT("%s"), *(pLIPSincController->GetExpression(i)->GetName())) );
			}

			ExpressionList.SetCurrent(0);
			SetFocus(ExpressionList.hWnd);
		}

		unguard;
	}

	virtual INT DoModal( TLIPSincController *pInLIPSincController, USkeletalMeshInstance *pInMeshInstance )
	{
		guard(WDlgDeleteLIPSincExpression::DoModal);

		pLIPSincController = pInLIPSincController;
		pMeshInstance      = pInMeshInstance;

		return WDialog::DoModal( hInstance );
		unguard;
	}

	void OnOk()
	{		
		guard(WDlgDeleteLIPSincExpression::OnOk);

		ExpressionList.Empty();
		
		EndDialog(TRUE);
		
		unguard;
	}

	void OnDeleteExpression()
	{
		guard(WDlgDeleteLIPSincExpression::OnDeleteExpression);

		// Delete the expression.
		if( pLIPSincController )
		{
			pLIPSincController->DeleteExpression( ExpressionList.GetString( ExpressionList.GetCurrent() ) );

			// Force the bones to be initialized.
			if( pMeshInstance )
			{
				pLIPSincController->InitializeBones( pMeshInstance->CachedLIPSincBones );
			}

			pLIPSincController->SetDirty();
			ExpressionList.Empty();
			
			for( INT i = 0; i < pLIPSincController->NumExpressions(); ++i )
			{
				ExpressionList.AddString( *FString::Printf(TEXT("%s"), *(pLIPSincController->GetExpression(i)->GetName())) );
			}

			ExpressionList.SetCurrent(0);
			SetFocus(ExpressionList.hWnd);
		}

		unguard;
	}

};


// --------------------------------------------------------------
//
// New LIPSinc Animation dialog..
//
// --------------------------------------------------------------

class WDlgCreateLIPSincAnim : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgCreateLIPSincAnim,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WButton LoadTextButton;
	WButton SaveTextButton;
	WButton PlayButton;
		
	WEdit NameEdit;

	WLabel FilenameLabel;

	WComboBox SoundPackageCombo;
	
	WCheckBox TextCheck;
	WCheckBox ImportAudioCheck;

	WEdit TextToProcess;
	
	FString defPackage, defGroup, defName;
	TArray<FString>* Filenames;

	FString Package, Group, Name;
	
	UBOOL DoText;
	UBOOL DoImportAudio;

	FString Text;

	HBITMAP OpenBitmap;
	HBITMAP SaveBitmap;
	HBITMAP PlayBitmap;
	HBITMAP StopBitmap;

	UBOOL isPlaying;
	
	// Constructor.
	WDlgCreateLIPSincAnim( UObject* InContext, WBrowser* InOwnerWindow )
		:	WDialog		  ( TEXT("Create New LIPSinc Animation"), IDDIALOG_CREATE_LIPSINC_ANIM, InOwnerWindow )
	,	OkButton		  ( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	  ( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	SoundPackageCombo ( this, IDC_SOUNDPACKAGE )
	,	NameEdit		  ( this, IDEC_NAME )
	,   TextCheck         ( this, IDCK_USETEXT )
	,   ImportAudioCheck  ( this, IDCK_IMPORTAUDIO )
	,	TextToProcess	  ( this, IDC_TEXTTOPROCESS )
	,	LoadTextButton    ( this, IDC_LOADTEXT,	FDelegate(this,(TDelegate)OnLoadText) )
	,	SaveTextButton	  ( this, IDC_SAVETEXT, FDelegate(this,(TDelegate)OnSaveText) )
	,   FilenameLabel	  ( this, IDS_FILENAME )
	,	PlayButton		  ( this, IDB_LIPSINC_PLAY, FDelegate(this,(TDelegate)OnPlayButton) )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgCreateLIPSincAnim::OnInitDialog);
		WDialog::OnInitDialog();

		OpenBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDB_LIPSINCFILEOPEN), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );
		LoadTextButton.SetBitmap(OpenBitmap);

		SaveBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDB_LIPSINCFILESAVE), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );
		SaveTextButton.SetBitmap(SaveBitmap);

		PlayBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_PLAY), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );
		PlayButton.SetBitmap(PlayBitmap);
		isPlaying = 0;
		StopBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_STOP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );

		FString audioName = GetFilenameOnly( defName );
		FString txtFilename = defName.Left( defName.InStr( TEXT("."), 1 ) );
		txtFilename += TEXT(".txt");

		NameEdit.SetText( *audioName );

		// If they have text, use it.
		FString Data;
		if ( appLoadFileToString( Data, *txtFilename ) )
		{
			TextToProcess.SetText( *Data );
			TextCheck.SetCheck(1);  // default is to use text
		}
		else
		{
			TextCheck.SetCheck(0);
		}

		FilenameLabel.SetText( *defName );

		ImportAudioCheck.SetCheck(1);	// Default to importing audio
		
		RefreshSoundPackages();

		FString currPackage;
		GConfig->GetString(TEXT("LIPSinc Browser"), TEXT("LastSoundPackage"), currPackage, TEXT("UnrealEd.ini"));

		if( currPackage.Len() )
		{
			SoundPackageCombo.SetCurrent(SoundPackageCombo.FindString(*currPackage));
		}
		
		::SetFocus( SoundPackageCombo.hWnd );
		
		unguard;
	}
	
	void OnDestroy()
	{
		guard(WDlgCreateLIPSincAnim::OnDestroy);
		WDialog::OnDestroy();

		::DeleteObject(OpenBitmap);
		::DeleteObject(SaveBitmap);

		unguard;
	}

	void RefreshSoundPackages()
	{
		SoundPackageCombo.Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Sound"), GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
		{
			SoundPackageCombo.AddString( *(StringArray(x)) );
		}

		SoundPackageCombo.SetCurrent( 0 );
	}
	
	virtual INT DoModal( FString InDefPackage, FString InDefGroup, FString InDefName )
	{
		guard(WDlgCreateLIPSincAnim::DoModal);

		defPackage = InDefPackage;
		defGroup = InDefGroup;
		defName = InDefName;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	
	void OnOk()
	{		
		guard(WDlgCreateLIPSincAnim::OnOk);
		if( GetDataFromUser() )
		{	
			// Persistance
			GConfig->SetString(TEXT("LIPSinc Browser"), TEXT("LastSoundPackage"), *Package, TEXT("UnrealEd.ini"));
			PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
			EndDialog(TRUE);
		}
		unguard;
	}
	
	BOOL GetDataFromUser( void )
	{
		guard(WDlgCreateLIPSincAnim::GetDataFromUser);

		Package = SoundPackageCombo.GetString( SoundPackageCombo.GetCurrent() );
				
		Name = NameEdit.GetText();

		DoText		  = TextCheck.IsChecked();
		DoImportAudio = ImportAudioCheck.IsChecked();
		Text		  = TextToProcess.GetText();

		if( !Package.Len() || !Name.Len() )
		{
			appMsgf( 0, TEXT("Invalid input.") );
			return FALSE;
		}

		// UnrealEd will crash when the Name contains a lot of characters (~>450) due to the cmd buffer being 512 characters.
		// Having a name with >450 characters is insane, so this artificial limit / check is here merely to clear the bug from the QA records.
		if( Name.Len() > 200 )
		{
			appMsgf( 0, TEXT("The name you entered is greater than 200 characters.  Please limit your name to 200 characters or less.") );
			return FALSE;
		}

		return TRUE;
		unguard;
	}

	void OnLoadText()
	{
		guard (WDlgCreateLIPSincAnim::OnLoadText);

		OPENFILENAMEA ofn;
		char File[8192] = "\0";
		
		ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFile = File;
		ofn.nMaxFile = sizeof(char) * 8192;
		
		ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files\0*.*\0\0";
		ofn.lpstrDefExt = "txt";
		
		ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GLastDir[eLASTDIR_WAV]) );								
		
		ofn.lpstrTitle = "Select text file to load";
		ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER;
		
		if( GetOpenFileNameA(&ofn) )
		{
			FString Data;
			if ( appLoadFileToString( Data, ANSI_TO_TCHAR(File) ) )
			{
				TextToProcess.SetText( *Data );	

				if( !TextCheck.bChecked )
				{
					TextCheck.SetCheck(1);
				}
			}
		}

		unguard;
	}

	void OnSaveText()
	{
		guard (WDlgCreateLIPSincAnim::OnSaveText);

		OPENFILENAMEA ofn;
		FString filename = defName.Left( defName.InStr( TEXT("."), 1 ) );
		filename += TEXT(".txt");
		char File[8192] = "\0";
		strcpy(File, TCHAR_TO_ANSI(*(filename)));
		
		ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFile = File;
		ofn.nMaxFile = sizeof(char) * 8192;
		
		ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files\0*.*\0\0";
		ofn.lpstrDefExt = "txt";
				
		ofn.lpstrTitle = "Save text file as";
		ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_OVERWRITEPROMPT	;


		if( GetSaveFileNameA(&ofn) )
		{
			FString Data;
			Data = TextToProcess.GetText();
			appSaveStringToFile( Data, ANSI_TO_TCHAR(File) );
		}

		unguard;
	}

	void OnPlayButton()
	{
		guard(WDlgCreateLIPSincAnim::OnPlayButton);

		if(!isPlaying)
		{
			PlayButton.SetBitmap(StopBitmap);
			PlaySound(*FilenameLabel.GetText(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
			isPlaying = !isPlaying;
		}
		else
		{
			PlayButton.SetBitmap(PlayBitmap);
			PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
			isPlaying = !isPlaying;
		}

		unguard;
	}
};

// --------------------------------------------------------------
//
// New LIPSinc Animation Batch dialog..
//
// --------------------------------------------------------------

class WDlgCreateLIPSincAnimBatch : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgCreateLIPSincAnimBatch,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
		
	WComboBox SoundPackageCombo;
	
	WCheckBox TextCheck;
	WCheckBox ImportAudioCheck;

	WLabel	  FilenameLabel;
	
	TArray<FString> Filenames;

	FString Package;
	
	UBOOL DoText;
	UBOOL DoImportAudio;
	
	// Constructor.
	WDlgCreateLIPSincAnimBatch( UObject* InContext, WBrowser* InOwnerWindow )
		:	WDialog		  ( TEXT("Batch Create New LIPSinc Animations"), IDDIALOG_CREATE_LIPSINC_ANIM_BATCH, InOwnerWindow )
	,	OkButton		  ( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	  ( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	SoundPackageCombo ( this, IDC_SOUNDPACKAGE )
	,   TextCheck         ( this, IDCK_USETEXT )
	,   ImportAudioCheck  ( this, IDCK_IMPORTAUDIO )
	,	FilenameLabel	  ( this, IDS_FILENAME )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgCreateLIPSincAnimBatch::OnInitDialog);
		WDialog::OnInitDialog();

		FString temp = TEXT("");
		for (int i = 1; i < Filenames.Num(); ++i)
		{
			temp += *Filenames(i);
			temp += TEXT("; ");
		}

		FilenameLabel.SetText(*temp);
		ImportAudioCheck.SetCheck(1);

		RefreshSoundPackages();
		
		::SetFocus( SoundPackageCombo.hWnd );

		TextCheck.SetCheck(1);  // default is to use text

		FString currPackage;
		GConfig->GetString(TEXT("LIPSinc Browser"), TEXT("LastSoundPackage"), currPackage, TEXT("UnrealEd.ini"));

		if( currPackage.Len() )
		{
			SoundPackageCombo.SetCurrent(SoundPackageCombo.FindString(*currPackage));
		}
		
		unguard;
	}
	
	void OnDestroy()
	{
		guard(WDlgCreateLIPSincAnimBatch::OnDestroy);
		WDialog::OnDestroy();

		unguard;
	}

	void RefreshSoundPackages()
	{
		SoundPackageCombo.Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Sound"), GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
		{
			SoundPackageCombo.AddString( *(StringArray(x)) );
		}

		SoundPackageCombo.SetCurrent( 0 );
	}
	
	virtual INT DoModal(TArray<FString> files)
	{
		guard(WDlgCreateLIPSincAnimBatch::DoModal);

		Filenames = files;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	
	void OnOk()
	{		
		guard(WDlgCreateLIPSincAnimBatch::OnOk);
		if( GetDataFromUser() )
		{
			GConfig->SetString(TEXT("LIPSinc Browser"), TEXT("LastSoundPackage"), *Package, TEXT("UnrealEd.ini"));
			EndDialog(TRUE);
		}
		unguard;
	}
	
	BOOL GetDataFromUser( void )
	{
		guard(WDlgCreateLIPSincAnimBatch::GetDataFromUser);

		Package = SoundPackageCombo.GetString( SoundPackageCombo.GetCurrent() );
		
		DoText		  = TextCheck.IsChecked();
		DoImportAudio = ImportAudioCheck.IsChecked();

		if( !Package.Len() )
		{
			appMsgf( 0, TEXT("Invalid input.") );
			return FALSE;
		}

		return TRUE;
		unguard;
	}
};

// --------------------------------------------------------------
//
// LIPSinc Controller Information Dialog
//
// --------------------------------------------------------------

class WDlgLIPSincControllerInfo : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgLIPSincControllerInfo,WDialog,UnrealEd)

	WLabel ControllerMemorySize;
	WLabel ControllerDiskSize;
	WLabel ControllerNumAnimations;
	WLabel ControllerTotalDuration;

	WLabel AnimName;
	WLabel AnimMemorySize;
	WLabel AnimDiskSize;
	WLabel AnimDuration;

	WLabel BonePoseNumPoses;
	WLabel BonePoseBonesPerPose;
	WLabel BonePoseSize;

	WLabel NumExpressions;
	WLabel ExpressionsSize;

	WLabel VersionInfo;

	WButton OkButton;

	TLIPSincController* LIPSincController;

	FString CurrentAnimationName;

	WDlgLIPSincControllerInfo( UObject* InContext, WBrowser* InOwnerWindow, TLIPSincController* controller, FString name )
		: WDialog				  ( TEXT("LIPSinc Controller Information"), IDDIALOG_LIPSINC_CONTROLLER_INFO, InOwnerWindow )
		, ControllerMemorySize	  ( this, ID_LIPS_CONTROLLER_MEM_SIZE )
		, ControllerDiskSize	  ( this, ID_LIPS_CONTROLLER_DISK_SIZE )
		, ControllerNumAnimations ( this, ID_LIPS_CONTROLLER_NUMANIMS )
		, ControllerTotalDuration ( this, ID_LIPS_CONTROLLER_DURATION )
		, AnimName				  ( this, ID_LIPS_ANIM_NAME )
		, AnimMemorySize		  ( this, ID_LIPS_ANIM_MEM_SIZE )
		, AnimDiskSize			  ( this, ID_LIPS_ANIM_DISK_SIZE )
		, AnimDuration			  ( this, ID_LIPS_ANIM_DURATION )
		, BonePoseNumPoses		  ( this, ID_LIPS_NUM_POSES )
		, BonePoseBonesPerPose	  ( this, ID_LIPS_BONES_PER_POSE )
		, BonePoseSize			  ( this, ID_LIPS_BONE_POSE_SIZE )
		, NumExpressions		  ( this, ID_LIPS_NUM_EXPRESSIONS )
		, ExpressionsSize		  ( this, ID_LIPS_EXPRESSIONS_SIZE )
		, OkButton			      ( this, IDOK, FDelegate(this,(TDelegate)OnOkButton) )
		, VersionInfo			  ( this, ID_VERSIONINFO )
	{
		LIPSincController = controller;
		CurrentAnimationName = name;
	}

	void OnInitDialog()
	{
		guard(WDlgLIPSincControllerInfo::OnInitDialog);
		WDialog::OnInitDialog();

		ControllerMemorySize.SetText(*FString::Printf(TEXT("%d bytes"), LIPSincController->MemFootprint()));
		ControllerDiskSize.SetText(*FString::Printf(TEXT("%d bytes"), LIPSincController->DiskFootprint()));
		ControllerNumAnimations.SetText(appItoa(LIPSincController->NumAnimations()));
		ControllerTotalDuration.SetText(*FString::Printf(TEXT("%f minutes"), LIPSincController->TotalAnimTime() / 1000.f / 60.f));

		INT AnimIndex = LIPSincController->FindAnimIndex(CurrentAnimationName);		
		if (AnimIndex != -1)
		{
			AnimName.SetText(*CurrentAnimationName);
			AnimMemorySize.SetText(*FString::Printf(TEXT("%d bytes"), LIPSincController->GetAnimation(AnimIndex)->MemFootprint()));
			AnimDiskSize.SetText(*FString::Printf(TEXT("%d bytes"), LIPSincController->GetAnimation(AnimIndex)->DiskFootprint()));
			AnimDuration.SetText(*FString::Printf(TEXT("%f seconds"), LIPSincController->GetAnimation(AnimIndex)->EndTime() / 1000.f));
		}

		BonePoseNumPoses.SetText(appItoa(LIPSincController->BonePoseInfo()->NumBonePoses()));
		BonePoseBonesPerPose.SetText(appItoa(LIPSincController->BonePoseInfo()->NumBonesPerPose()));
		BonePoseSize.SetText(*FString::Printf(TEXT("%d bytes"), LIPSincController->BonePoseInfo()->MemFootprint()));

		NumExpressions.SetText(appItoa(LIPSincController->NumExpressions()));
		
		INT Size = 0;

		for( INT i = 0; i < LIPSincController->NumExpressions(); ++i )
		{
			Size += LIPSincController->GetExpression(i)->MemFootprint();
		}

		ExpressionsSize.SetText(*FString::Printf(TEXT("%d bytes"), Size));

		VersionInfo.SetText(IMPERSONATOR_VERSION_STRING);

		unguard;
	}

	virtual INT DoModal()
	{
		guard(WDlgLIPSincControllerInfo::DoModal);
		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOkButton()
	{			
		guard(WDlgLIPSincControllerInfo::OnOk);			
		WDialog::EndDialog(TRUE);
		unguard;
	}
};

// --------------------------------------------------------------
//
// WBrowserLIPSinc
//
// --------------------------------------------------------------

// "protecting" the IP at the request of management.
#define LIPSincCall( x ) x
#define LIPSincType( x ) IMPERSONATORLIB_##x
#define FINAL_PARAM( x ) x, reinterpret_cast<unsigned long>(x)*13/11+7-5

TBBUTTON tbLIPSincButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	
	, { 0, 0, TBSTATE_ENABLED,		TBSTYLE_SEP, 0L, 0}

	, { 1, IDMN_FileSave, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}

	, { 0, 0, TBSTATE_ENABLED,		TBSTYLE_SEP, 0L, 0}

	, { 3, IDMN_LIPSINCIMPORTLBP,	TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	, { 2, IDMN_LIPSINCGENERATEANIMATION, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}

	, { 0, 0, TBSTATE_ENABLED,		TBSTYLE_SEP, 0L, 0}

    , { 4, IDMN_VIEW_INFO,			TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	, { 5, IDMN_VIEW_BOUNDS,		TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 6, IDMN_VIEW_BONES,			TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 7, IDMN_VIEW_BONENAMES,		TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 9, IDMN_VIEW_INFLUENCES,	TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, {11, IDMN_VIEW_WIRE,			TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}	
};

LIPSincToolTipStrings ToolTips_LIPSinc[] =
{
	TEXT("Dock/undock from browser window"), IDMN_MB_DOCK,

	TEXT("Save the LIPSinc controller on this mesh"), IDMN_FileSave,

	TEXT("Import bone poses"), IDMN_LIPSINCIMPORTLBP,
	TEXT("Generate LIPSinc animation"), IDMN_LIPSINCGENERATEANIMATION,

	TEXT("Display information about the controller"), IDMN_VIEW_INFO,
	TEXT("View bones"), IDMN_VIEW_BONES,
	TEXT("View bounds"), IDMN_VIEW_BOUNDS,
	TEXT("View influences"), IDMN_VIEW_INFLUENCES,
	TEXT("View wireframe"), IDMN_VIEW_WIRE,
	TEXT("View bone names"), IDMN_VIEW_BONENAMES,
	NULL, 0
};

// Serialize function: needed to associate the custom objects with THIS window so they don't get GC'd at the wrong time; when a level is loaded etc.
void WBrowserLIPSinc::Serialize( FArchive& Ar )
{
	guard(WBrowserLIPSinc::Serialize);
	WBrowser::Serialize( Ar );		
	Ar << LIPSincBrowserLevel << LIPSincControllerProps << LIPSincAnimProps << LIPSincPrefsProps;
	unguard;
}

// Structors.
WBrowserLIPSinc::WBrowserLIPSinc( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
{
	PackageCombo                    = NULL;
	MeshCombo                       = NULL;

	LIPSincControllerPropertyWindow = NULL;
	LIPSincAnimPropertyWindow       = NULL;
	LIPSincPrefsPropertyWindow      = NULL;

	LIPSincControllerProps          = NULL;
	LIPSincAnimProps                = NULL;
	LIPSincPrefsProps               = NULL;
	
	LIPSincAnimList                 = NULL;
		
	AnimListLabel                   = NULL;
	UnrealAnimListLabel             = NULL;

	ScrubBar						= NULL;
	
	Viewport                        = NULL;
	
	bRefPose                        = TRUE;
	bPlaying = bPlayingLIPSinc = bPausedLIPSinc = bWireframe  = bPrintBones = bBackface = bShowTargetStats = FALSE;
	
	MenuID      = IDMENU_BrowserLIPSinc;
	BrowserID   = eBROWSER_LIPSINC;
	Description = TEXT("Impersonator");

	PlayBitmap  = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_PLAY), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	PauseBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_PAUSE), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	StopBitmap  = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_STOP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	
	OldFrameTime = FrameTime = 0.0f;
}

// WBrowser interface.
void WBrowserLIPSinc::OpenWindow( UBOOL bChild )
{
	guard(WBrowserLIPSinc::OpenWindow);
	
	WBrowser::OpenWindow( bChild );
	
	SetCaption();
	Show(1);
	
	unguard;
}

void WBrowserLIPSinc::UpdateMenu()
{
	guard(WBrowserLIPSinc::UpdateMenu);
	
	HMENU menu = IsDocked() ? GetMenu( OwnerWindow->hWnd ) : GetMenu( hWnd );
	CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );
	
	unguard;
}

void WBrowserLIPSinc::OnCreate()
{
	guard(WBrowserLIPSinc::OnCreate);
	WBrowser::OnCreate();
	
	SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserLIPSinc) );
	
	// Create the toolbar.
	hWndToolBar = CreateToolbarEx(
		hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
		IDB_BrowserLIPSinc_TOOLBAR,
		3,
		hInstance,
		IDB_BrowserLIPSinc_TOOLBAR,
		(LPCTBBUTTON)&tbLIPSincButtons,
		13, // Total number of buttons and dividers in toolbar.
		16,16,
		16,16,
		sizeof(TBBUTTON));
	check(hWndToolBar);
	
	ToolTipCtrl = new WToolTip(this);
	ToolTipCtrl->OpenWindow();
	for( INT tooltip = 0 ; ToolTips_LIPSinc[tooltip].ID > 0 ; ++tooltip )
	{
		// Figure out the rectangle for the toolbar button.
		INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_LIPSinc[tooltip].ID, 0 );
		RECT rect;
		SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);
		
		ToolTipCtrl->AddTool( hWndToolBar, ToolTips_LIPSinc[tooltip].ToolTip, tooltip, &rect );
	}
	
	// Package selection
	//
	PackageCombo = new WComboBox( this, IDCB_PACKAGE );
	PackageCombo->OpenWindow( 1, 1 );
	PackageCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnPackageSelectionChange);
	
	// Mesh selection
	//
	MeshCombo = new WComboBox( this, IDCB_MESH );
	MeshCombo->OpenWindow( 1, 1 );  // Sorted listbox
	MeshCombo->SelectionChangeDelegate    = FDelegate(this,(TDelegate)OnMeshSelectionChange );
	
	LIPSincAnimList = new WListBox( this, IDLB_ANIMATIONS );
	LIPSincAnimList->OpenWindow( 1, 0, 0, 0, 0, WS_VSCROLL );
	LIPSincAnimList->DoubleClickDelegate     = FDelegate(this,(TDelegate)OnLIPSincAnimListDoubleClick);		
	LIPSincAnimList->RightClickDelegate      = FDelegate(this,(TDelegate)OnLIPSincAnimListRightClick);	
	LIPSincAnimList->SelectionChangeDelegate = FDelegate(this,(TDelegate)OnLIPSincAnimListSelectionChange);
	SendMessageX( LIPSincAnimList->hWnd, LB_SETCOLUMNWIDTH, 96, 0 );
		
	SplitterContainer = new WSplitterContainer( this );
	SplitterContainer->OpenWindow( 1 );
	SplitterContainer->ParentContainer = &Container;
	SplitterContainer->SetPct( 70 );

	BlendWithCheck = new WCheckBox( this, IDC_BLENDWITH, FDelegate(this, (TDelegate)OnBlendWithChange));
	BlendWithCheck->OpenWindow(1, 0, 0, 0, 0, TEXT("Blend with:"));

	UnrealAnimPackageCombo = new WComboBox(this, IDCB_BLENDWITHPACKAGE);
	UnrealAnimPackageCombo->OpenWindow(1, 0);
	UnrealAnimPackageCombo->SelectionChangeDelegate = FDelegate(this,(TDelegate)OnUnrealAnimPackageSelectionChange);

	UnrealAnimCombo = new WComboBox(this, IDCB_BLENDWITH);
	UnrealAnimCombo->OpenWindow(1, 0);
	UnrealAnimCombo->SelectionChangeDelegate = FDelegate(this,(TDelegate)OnUnrealAnimListSelectionChange);

	// Associate bitmaps with buttons
	PlayButton = new WButton(this, IDB_PLAYLIPSINC, FDelegate(this, (TDelegate)OnPlayButton) );
	PlayButton->OpenWindow( 1, 0, 0, 16, 16, NULL, 0, BS_OWNERDRAW );
	PlayButton->SetBitmap( PlayBitmap );

	StopButton = new WButton(this, IDB_STOPLIPSINC, FDelegate(this, (TDelegate)OnStopButton) );
	StopButton->OpenWindow( 1, 0, 0, 16, 16, NULL, 0, BS_OWNERDRAW );
	StopButton->SetBitmap( StopBitmap );

	// LABELS
	AnimListLabel = new WLabel( this, IDAL_LABEL1 );
	AnimListLabel->OpenWindow( 1, 0 );
	AnimListLabel->SetText( TEXT(" Impersonator Animations") );
	
	UnrealAnimListLabel = new WLabel( this, IDAL_LABEL2 );
	UnrealAnimListLabel->OpenWindow( 1, 0 );
	UnrealAnimListLabel->SetText( TEXT(" Unreal Animations") );

	RegisteredToLabel = new WLabel( SplitterContainer->Pane1, IDS_REGISTEREDTO );
	RegisteredToLabel->OpenWindow( 1, 0 );
	char info[1024];
	LIPSincCall(Function5)(900, FINAL_PARAM(info));
	FString regInfo = FString::Printf(TEXT("This copy of Impersonator is licensed to %s"), appFromAnsi(info));
	RegisteredToLabel->SetText( *regInfo );

	// Scrub bar
	ScrubBar = new WTrackBar( SplitterContainer->Pane1, IDTB_SCRUBFRAMES );
	ScrubBar->ManualTicks = 1;
	ScrubBar->OpenWindow( 1, 0 );
	ScrubBar->SetRange( 0, 10000 );
	ScrubBar->ThumbPositionDelegate = FDelegate(this, (TDelegate)OnSliderMove );
	ScrubBar->ThumbTrackDelegate = FDelegate(this, (TDelegate)OnSliderMove );

	ViewportLabel = new WLabel( SplitterContainer->Pane1, IDCS_LIPSVIEWPORT );
	ViewportLabel->OpenWindow(1, 0);
	
	// Create the mesh viewport
	LIPSincBrowserLevel = new( UObject::GetTransientPackage(), TEXT("LIPSincBrowserLevel") )ULevel( GUnrealEd, 0 );
	Viewport = GUnrealEd->Client->NewViewport( TEXT("LIPSinc") );
	check(Viewport);
	InitViewActor();
	Viewport->Input->Init(Viewport);
	Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 256, 256, 0, 0);

	// Property Editors
	//if( !LIPSincControllerProps ) LIPSincControllerProps = ConstructObject<ULIPSincControllerProps>( ULIPSincControllerProps::StaticClass() );
	if( !LIPSincAnimProps       ) LIPSincAnimProps       = ConstructObject<ULIPSincAnimProps>      ( ULIPSincAnimProps::StaticClass()       );
	//if( !LIPSincPrefsProps      ) LIPSincPrefsProps      = ConstructObject<ULIPSincPrefsProps>     ( ULIPSincPrefsProps::StaticClass()      );

	// Tab control
	PropSheet = new WPropertySheet( SplitterContainer->Pane2, 0 );
	PropSheet->bMultiLine = 0;	// Jbriggs: Put this back to 1 when you add more tabs [6/30/2002]
	PropSheet->bResizable = 1;
	PropSheet->OpenWindow( 1, 1, 0 );
	PropSheet->ParentContainer = &Container;

	//LIPSincControllerPage = new WPropertyPage( PropSheet->Tabs );
	//LIPSincControllerPage->OpenWindow( 0, 0 );
	//LIPSincControllerPage->Caption = TEXT("Controller");
	//PropSheet->AddPage( LIPSincControllerPage );

	LIPSincAnimPage = new WPropertyPage( PropSheet->Tabs );
	LIPSincAnimPage->OpenWindow( 0, 0 );
	LIPSincAnimPage->Caption = TEXT("Animation");
	PropSheet->AddPage( LIPSincAnimPage );

	//LIPSincPrefsPage = new WPropertyPage( PropSheet->Tabs );
	//LIPSincPrefsPage->OpenWindow( 0, 0 );
	//LIPSincPrefsPage->Caption = TEXT("Prefs");
	//PropSheet->AddPage( LIPSincPrefsPage );

	//LIPSincControllerPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), LIPSincControllerPage, 1 );
	//LIPSincControllerPropertyWindow->ShowTreeLines = 0;
	//LIPSincControllerPropertyWindow->SetNotifyHook( GUnrealEd );
	//LIPSincControllerPropertyWindow->OpenChildWindow(0);
	
	LIPSincAnimPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), LIPSincAnimPage, 1 );
	LIPSincAnimPropertyWindow->ShowTreeLines = 0;
	LIPSincAnimPropertyWindow->SetNotifyHook( GUnrealEd );
	LIPSincAnimPropertyWindow->OpenChildWindow(0);

	//LIPSincPrefsPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), LIPSincPrefsPage, 1 );
	//LIPSincPrefsPropertyWindow->ShowTreeLines = 0;
	//LIPSincPrefsPropertyWindow->SetNotifyHook( GUnrealEd );
	//LIPSincPrefsPropertyWindow->OpenChildWindow(0);

	INT top = 30;
	const INT leftPaneWidth = 225;

	// Animation Selection Pane
	Anchors.Set( (DWORD)PackageCombo->hWnd,		FWindowAnchor(hWnd, PackageCombo->hWnd, ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_HEIGHT, leftPaneWidth, STANDARD_CTRL_HEIGHT) );
	top += STANDARD_CTRL_HEIGHT;
	Anchors.Set( (DWORD)MeshCombo->hWnd,		FWindowAnchor(hWnd, MeshCombo->hWnd,    ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_HEIGHT, leftPaneWidth, STANDARD_CTRL_HEIGHT) );
	top += STANDARD_CTRL_HEIGHT + 5;

	Anchors.Set( (DWORD)AnimListLabel->hWnd,	FWindowAnchor(hWnd, AnimListLabel->hWnd, ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_HEIGHT, leftPaneWidth, STANDARD_CTRL_HEIGHT) );
	top += STANDARD_CTRL_HEIGHT;
	Anchors.Set( (DWORD)BlendWithCheck->hWnd,	FWindowAnchor(hWnd, BlendWithCheck->hWnd, ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_HEIGHT, leftPaneWidth, STANDARD_CTRL_HEIGHT));
	top += STANDARD_CTRL_HEIGHT + 2;
	Anchors.Set( (DWORD)UnrealAnimPackageCombo->hWnd,	FWindowAnchor(hWnd, UnrealAnimPackageCombo->hWnd, ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_HEIGHT, leftPaneWidth, STANDARD_CTRL_HEIGHT));
	top += STANDARD_CTRL_HEIGHT + 2;
	Anchors.Set( (DWORD)UnrealAnimCombo->hWnd,	FWindowAnchor(hWnd, UnrealAnimCombo->hWnd, ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_HEIGHT, leftPaneWidth, STANDARD_CTRL_HEIGHT));
	top += STANDARD_CTRL_HEIGHT + 2;

	// Parent the Anim List into the Anim Group Box
	Anchors.Set( (DWORD)LIPSincAnimList->hWnd,	FWindowAnchor(hWnd, LIPSincAnimList->hWnd, ANCHOR_TL, 0, top, ANCHOR_LEFT | ANCHOR_BOTTOM, leftPaneWidth, -STANDARD_CTRL_HEIGHT));
	Anchors.Set( (DWORD)PlayButton->hWnd,		FWindowAnchor(hWnd, PlayButton->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 0, -16, ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16) );
	Anchors.Set( (DWORD)StopButton->hWnd,		FWindowAnchor(hWnd, StopButton->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 20, -16, ANCHOR_WIDTH|ANCHOR_HEIGHT, 16,16) );

	// Anchor the Render window in Pane1
	Anchors.Set( (DWORD)ViewportLabel->hWnd, FWindowAnchor(SplitterContainer->Pane1->hWnd, ViewportLabel->hWnd, ANCHOR_TL, 0, 0, ANCHOR_BR, 0, -STANDARD_CTRL_HEIGHT - 35));
	Anchors.Set( (DWORD)Viewport->GetWindow(), FWindowAnchor(ViewportLabel->hWnd, (HWND)Viewport->GetWindow(), ANCHOR_TL, 0, 0, ANCHOR_BR, 0, 0));
	Anchors.Set( (DWORD)RegisteredToLabel->hWnd, FWindowAnchor(SplitterContainer->Pane1->hWnd, RegisteredToLabel->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 0, -STANDARD_CTRL_HEIGHT, ANCHOR_BR, 0, 0) );

	// Anchor the Scrub bar
	Anchors.Set( (DWORD)ScrubBar->hWnd,			FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubBar->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 0, -STANDARD_CTRL_HEIGHT - 30, ANCHOR_BR, 0, -STANDARD_CTRL_HEIGHT - 3 ) );

	// Anchor the Properties in Pane2
	Anchors.Set( (DWORD)PropSheet->hWnd,		FWindowAnchor( SplitterContainer->Pane2->hWnd, PropSheet->hWnd, ANCHOR_TL, 0, 0,					ANCHOR_BR, 0, 0 ) );

	//Anchors.Set( (DWORD)LIPSincControllerPropertyWindow->hWnd,FWindowAnchor( LIPSincControllerPage->hWnd, LIPSincControllerPropertyWindow->hWnd, ANCHOR_TL, 0, 0, ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)LIPSincAnimPropertyWindow->hWnd,FWindowAnchor( LIPSincAnimPage->hWnd, LIPSincAnimPropertyWindow->hWnd, ANCHOR_TL, 0, 0, ANCHOR_BR, 0, 0 ) );
	//Anchors.Set( (DWORD)LIPSincPrefsPropertyWindow->hWnd,FWindowAnchor( LIPSincPrefsPage->hWnd, LIPSincPrefsPropertyWindow->hWnd, ANCHOR_TL, 0, 0, ANCHOR_BR, 0, 0 ) );
	
	// Anchor the Splitter in the Parent
	Anchors.Set( (DWORD)SplitterContainer->hWnd, FWindowAnchor(hWnd, SplitterContainer->hWnd, ANCHOR_TL, leftPaneWidth + 5, 30, ANCHOR_BR, 0, 0));
	
	Container.SetAnchors(&Anchors);
	PositionChildControls();
	
	SetCaption();

	// #skel - still a bit risky - active meshes from the regular mesh browser can confuse the skeletal-only animation browser.
	WorkMesh = Cast<USkeletalMesh>(UObject::StaticFindObject(USkeletalMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));
	
	RefreshAll();
	
	OnPackageSelectionChange();

	GUnrealEd->Exec( TEXT("AUDIO FINDVIEWPORT") );
	
	unguard;
}

void WBrowserLIPSinc::InitViewActor()
{
	guard(WBrowserLIPSinc::InitViewActor);
	LIPSincBrowserLevel->SpawnViewActor( Viewport );
	check(Viewport->Actor);
	Viewport->Actor->XLevel = LIPSincBrowserLevel;
	Viewport->Actor->ShowFlags = LIPSINC_SHOWFLAGS;
	Viewport->Actor->RendMap   = REN_LIPSinc;
	Viewport->Group = NAME_None;
	Viewport->Actor->Misc1 = 0;
	Viewport->MiscRes = NULL;
	WorkMesh = NULL;
	MeshActor = LIPSincBrowserLevel->SpawnActor( AAnimBrowserMesh::StaticClass() );  // Use AAnimBrowserMesh class
	unguard;
}

//
// Clean up any notify-spawned effects
//
void WBrowserLIPSinc::CleanupLevel()
{
	guard(WBrowserLIPSinc::CleanupLevel);
	UBOOL PassedMeshActor = 0;
	for( INT i=LIPSincBrowserLevel->Actors.Num()-1;i>=0;i-- )
	{		
		if( LIPSincBrowserLevel->Actors(i) == MeshActor )
			PassedMeshActor = 1;
		if( LIPSincBrowserLevel->Actors(i) && !PassedMeshActor && LIPSincBrowserLevel->Actors(i) != Viewport->Actor )
			LIPSincBrowserLevel->DestroyActor( LIPSincBrowserLevel->Actors(i) );
	}
	unguard;
}

void WBrowserLIPSinc::RefreshAll()
{
	guard(WBrowserLIPSinc::RefreshAll);
	OnPackageSelectionChange();
	RefreshPackages();
	RefreshMeshList();
	RefreshLIPSincAnimList();
	RefreshUnrealAnimPackageList();
	RefreshViewport();
	unguard;
}

void WBrowserLIPSinc::RefreshLIPSincControllerProperties()
{
	guard(WBrowserLIPSinc::RefreshLIPSincControllerProperties);

	if( /*Mesh Has LIPSinc Controller &&*/ LIPSincControllerProps )
	{			
		LIPSincControllerPropertyWindow->Root.SetObjects( NULL, 0 );
		LIPSincControllerProps->WBrowserLIPSincPtr = (INT)(this);
		LIPSincControllerPropertyWindow->Root.SetObjects( (UObject**)&LIPSincControllerProps, 1 );
	}

	unguard;
}

void WBrowserLIPSinc::SaveLIPSincControllerProperties()
{
	guard(WBrowserLIPSinc::SaveLIPSincControllerProperties);

	unguard;
}

void WBrowserLIPSinc::RefreshLIPSincAnimProperties()
{
	guard(WBrowserLIPSinc::RefreshLIPSincAnimProperties);

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController && LIPSincAnimProps )
	{			
		LIPSincAnimPropertyWindow->Root.SetObjects( NULL, 0 );

		INT index = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString( LIPSincAnimList->GetCurrent() ) );

		if( index > -1 )
		{
			LIPSincAnimProps->bInterruptible = WorkMesh->LIPSincController->GetAnimation(index)->IsInterruptible();
			LIPSincAnimProps->BlendInTime    = WorkMesh->LIPSincController->GetAnimation(index)->GetBlendInTime();
			LIPSincAnimProps->BlendOutTime   = WorkMesh->LIPSincController->GetAnimation(index)->GetBlendOutTime();
			
			USound* sound = WorkMesh->LIPSincController->GetAnimation(index)->GetUSound();

			if( sound )
			{
				LIPSincAnimProps->Sound = sound;
			}
			else
			{
				LIPSincAnimProps->Sound = NULL;
			}
		}

		LIPSincAnimProps->WBrowserLIPSincPtr = (INT)(this);
		LIPSincAnimPropertyWindow->Root.SetObjects( (UObject**)&LIPSincAnimProps, 1 );
	}

	unguard;
}

void WBrowserLIPSinc::SaveLIPSincAnimProperties()
{
	guard(WBrowserLIPSinc::SaveLIPSincAnimProperties);

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController && LIPSincAnimProps )
	{			
		// Get the index of the current LIPSinc animation
		INT index = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString( LIPSincAnimList->GetCurrent() ) );
		
		if( index > -1 )
		{
			FString FullPkgName = LIPSincAnimProps->Sound ? LIPSincAnimProps->Sound->GetPathName() : TEXT("LIPSinc_NoSound");

			WorkMesh->LIPSincController->GetAnimation(index)->SetFullPkgName( FullPkgName );
			
			WorkMesh->LIPSincController->GetAnimation(index)->SetBlendInTime( LIPSincAnimProps->BlendInTime );
			WorkMesh->LIPSincController->GetAnimation(index)->SetBlendOutTime( LIPSincAnimProps->BlendOutTime );

			WorkMesh->LIPSincController->GetAnimation(index)->SetInterruptible( LIPSincAnimProps->bInterruptible );

			WorkMesh->LIPSincController->SetDirty();

			RefreshLIPSincAnimList();

			INT indexOfCurrent = LIPSincAnimList->FindString(*(WorkMesh->LIPSincController->GetAnimation(index)->Name()));
			LIPSincAnimList->SetCurrent( (indexOfCurrent != INDEX_NONE) ? indexOfCurrent : 0);
		}
	}

	unguard;
}

void WBrowserLIPSinc::RefreshLIPSincPrefsProperties()
{
	guard(WBrowserLIPSinc::RefreshLIPSincPrefsProperties);

	if( LIPSincPrefsProps )
	{			
		LIPSincPrefsPropertyWindow->Root.SetObjects( NULL, 0 );
		LIPSincPrefsProps->WBrowserLIPSincPtr = (INT)(this);
		LIPSincPrefsPropertyWindow->Root.SetObjects( (UObject**)&LIPSincPrefsProps, 1 );
	}

	unguard;
}

void WBrowserLIPSinc::SaveLIPSincPrefsProperties()
{
	guard(WBrowserLIPSinc::SaveLIPSincPrefsProperties);

	unguard;
}

void WBrowserLIPSinc::OnSliderMove()
{
	guard(WBrowserLIPSinc::OnSliderMove);

	if( WorkMesh && WorkMesh->LIPSincController && (WorkMesh->LIPSincController->NumAnimations() > 0) )
	{
		// Figure out what time we're on.
		INT CurPos = ScrubBar->GetPos();

		INT index = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString( LIPSincAnimList->GetCurrent() ) );

		INT EndTime = WorkMesh->LIPSincController->GetAnimation( index )->EndTime();

		INT oldIndex = CurrentMeshInstance()->m_nActiveLIPSincAnim;
		CurrentMeshInstance()->m_nActiveLIPSincAnim = index;

		CurrentMeshInstance()->LIPSincBlendInfo.LastOffset = ((FLOAT)CurPos / 10000.0f) * (FLOAT)EndTime;
		CurrentMeshInstance()->LIPSincBlendInfo.bForceFrame = 1;
		ELIPSincBlendMode OldBlendMode = CurrentMeshInstance()->LIPSincBlendInfo.BlendMode;
		CurrentMeshInstance()->LIPSincBlendInfo.BlendMode = ELBM_NormalBlend;
		RefreshViewport();
		CurrentMeshInstance()->LIPSincBlendInfo.bForceFrame = 0;
		CurrentMeshInstance()->LIPSincBlendInfo.BlendMode = OldBlendMode;
		CurrentMeshInstance()->m_nActiveLIPSincAnim = oldIndex;
	}

	unguard;
}

void WBrowserLIPSinc::RefreshPackages( void )
{
	guard(WBrowserLIPSinc::RefreshPackages);
	
	INT Current = PackageCombo->GetCurrent();
	Current = (Current != CB_ERR) ? Current : 0;
	
	// PACKAGES
	//
	PackageCombo->Empty();
	
	FStringOutputDevice GetPropResult = FStringOutputDevice();
	GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Mesh"), GetPropResult );
	
	TArray<FString> StringArray;
	GetPropResult.ParseIntoArray( TEXT(","), &StringArray );
	
	for( INT x = 0 ; x < StringArray.Num() ; ++x )
		PackageCombo->AddString( *(StringArray(x)) );
	
	PackageCombo->SetCurrent( Current );
	StringArray.Empty();
	
	unguard;
}

void WBrowserLIPSinc::RefreshMeshList()
{
	guard(WBrowserLIPSinc::RefreshMeshList);

	// Save the current mesh
	INT current = MeshCombo->GetCurrent();
	current = (current != CB_ERR) ? current : 0;
	
	FStringOutputDevice GetPropResult = FStringOutputDevice();
	
	// Only get skeletal meshes from the selected package..
	FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
	
	if( Package.Len() < 1) return;
	
	TCHAR l_ch[256];		
	appSprintf( l_ch, TEXT("QUERY TYPE=SkeletalMesh PACKAGE=\"%s\""), *Package );
	
	GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );
	
	MeshCombo->Empty();
	
	TArray<FString> StringArray;
	GetPropResult.ParseIntoArray( TEXT(" "), &StringArray );
	
	for( INT x = 0 ; x < StringArray.Num() ; ++x )
		MeshCombo->AddString( *(StringArray(x)) );
	
	MeshCombo->SetCurrent(current);
	StringArray.Empty();		
	
	unguard;
}

void WBrowserLIPSinc::RefreshLIPSincAnimList()
{
	guard(WBrowserLIPSinc::RefreshLIPSincAnimList);

	// Need to save the string name rather than the index
	FString current = LIPSincAnimList->GetString(LIPSincAnimList->GetCurrent());
	
	LIPSincAnimList->Empty();
	if( !Viewport || !Viewport->Actor )
		return; // Called too soon..

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController )
	{
		INT NumAnims = WorkMesh->LIPSincController->NumAnimations();
		
		for( INT anim = 0; anim < NumAnims; ++anim )
		{
			LIPSincAnimList->AddString( *FString::Printf(TEXT("%s"), *(WorkMesh->LIPSincController->GetAnimation(anim)->Name())) );
		}
	}
	
	INT indexOfCurrent = LIPSincAnimList->FindString(*current);
	LIPSincAnimList->SetCurrent( (indexOfCurrent != INDEX_NONE) ? indexOfCurrent : 0);

	unguard;
}

void WBrowserLIPSinc::RefreshUnrealAnimPackageList( )
{
	guard(WBrowserLIPSinc::RefreshUnrealAnimPackageList);

	FStringOutputDevice GetPropResult = FStringOutputDevice();

	// Only get animations from the selected package..
	FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
	if( Package.Len() < 1) return;
	TCHAR l_ch[512];		
	appSprintf( l_ch, TEXT("QUERY TYPE=MeshAnimation PACKAGE=\"%s\""), *Package );
	GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

	UnrealAnimPackageCombo->Empty();
	TArray<FString> StringArray;
	GetPropResult.ParseIntoArray( TEXT(" "), &StringArray );

	for( INT x = 0 ; x < StringArray.Num(); x++ )
	{
		UnrealAnimPackageCombo->AddString( *(StringArray(x)) );
	}
	UBOOL FoundAnim = false;
	FString DefAnimName;

	// Attempt to make current mesh's DefaultAnim the current one in the list.

	if( WorkMesh && WorkMesh->DefaultAnim )
	{
		DefAnimName = WorkMesh->DefaultAnim->GetName();
		// Find CurrentMeshAnim in the list.
		for(INT x = 0 ; x < StringArray.Num(); x++ )
		{
			if( StringArray(x) == DefAnimName )
			{
				FoundAnim = true;
				UnrealAnimPackageCombo->SetCurrent(x); 					
				break;
			}
		}
	}		
	
	// Not found: set current anim object to be the 0th element...
	if( !FoundAnim )
	{
		UnrealAnimPackageCombo->SetCurrent(0);
	}

	StringArray.Empty();		
	RefreshUnrealAnimList();

	unguard;
}



void WBrowserLIPSinc::RefreshUnrealAnimList()
{
	guard(WBrowserLIPSinc::RefreshUnrealAnimList);

	FString current = UnrealAnimCombo->GetString(UnrealAnimCombo->GetCurrent());
	
	UnrealAnimCombo->Empty();
	if( ! Viewport || !Viewport->Actor )
		return; // Called too soon..
	
	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) )
	{
		USkeletalMeshInstance* Inst = CurrentMeshInstance();

		// Get anims directly from current mesh.			
		if( Inst )
		{
			INT NumAnims = Inst->GetAnimCount();
			
			for( INT anim = 0 ; anim < NumAnims ; ++anim )
			{				
				HMeshAnim hAnim = Inst->GetAnimIndexed(anim);
				INT NumFrames;
				NumFrames = Inst->AnimGetFrameCount(hAnim);
				FName AnimFName = Inst->AnimGetName(hAnim);
				FString Name = *AnimFName;
				UnrealAnimCombo->AddString(*FString::Printf(TEXT("%s"), *Name));
			}
		}

		INT currentIndex = UnrealAnimCombo->FindString(*current);
		UnrealAnimCombo->SetCurrent( (currentIndex != INDEX_NONE) ? currentIndex : 0 );
	}
	
	unguard;
}

void WBrowserLIPSinc::RefreshViewport()
{
	guard(WBrowserLIPSinc::RefreshViewport);
	
	if( Viewport )
	{
		// Visible mesh and animation set in motion here.
		FString MeshName = MeshCombo->GetString(MeshCombo->GetCurrent());
		
		FStringOutputDevice GetPropResult = FStringOutputDevice();
			
		GUnrealEd->Get( TEXT("MESH"), *FString::Printf(TEXT("ANIMSEQ NAME=\"%s\" NUM=%d"), *MeshName, (UnrealAnimCombo->GetCurrent() >= 0) ? UnrealAnimCombo->GetCurrent() : 0), GetPropResult );
			
		GUnrealEd->Exec( *FString::Printf(TEXT("CAMERA UPDATE NAME=LIPSinc MESH=\"%s\" FLAGS=%d REN=%d MISC1=%d MISC2=%d"),
			  *MeshName,
			(bPlaying || bPlayingLIPSinc) ? LIPSINC_SHOWFLAGS | SHOW_RealTime
							              : LIPSINC_SHOWFLAGS,
			REN_LIPSinc,
			bRefPose ? -1 : appAtoi(*(GetPropResult.Right(7).Left(3)))
			));			
	}
	
	unguard;
}

void WBrowserLIPSinc::Draw( UViewport* Viewport )
{
	guard(WBrowserLIPSinc::Draw);

	APlayerController* CameraActor = Viewport->Actor;

	UMesh* Mesh = WorkMesh;
	if( !Mesh || ! Mesh->IsA(USkeletalMesh::StaticClass()) )
		return;	

	USkeletalMeshInstance* MeshInstance = CurrentMeshInstance();
	if( !MeshInstance )
		return;

	GUnrealEd->CurrentMesh = WorkMesh;

	FCameraSceneNode SceneNode(Viewport,&Viewport->RenderTarget,Viewport->Actor,Viewport->Actor->Location,Viewport->Actor->Rotation,Viewport->Actor->FovAngle);
	
	MeshActor->Mesh = Mesh;	
	MeshActor->SetDrawType(DT_Mesh);	
	MeshActor->Location = FVector(0,0,0);
	MeshActor->Rotation = FRotator(0,0,0);
	// Render meshes unlit.
	MeshActor->AmbientGlow	= 255;
	MeshActor->bUnlit = true;

	// Draw the wire grid.
	GUnrealEd->DrawWireBackground(&SceneNode);
	Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f);

	FName DemoAnimSequence;		

	// Update the animation...
	OldFrameTime = FrameTime;
	if( MeshInstance && ( CameraActor->Misc1 < MeshInstance->GetAnimCount()) && ( CameraActor->Misc1 >= 0 ) )
	{
		HMeshAnim ShowAnim = CurrentMeshInstance()->GetAnimIndexed( CameraActor->Misc1 );
		DemoAnimSequence = CurrentMeshInstance()->AnimGetName(ShowAnim);
	}
	else
	{
		// Force the reference pose for skeletal meshes.
		MeshInstance->bForceRefpose = true;
		DemoAnimSequence = NAME_None;
	}

	HMeshAnim Seq = CurrentMeshInstance()->GetAnimNamed( DemoAnimSequence );
	FLOAT NumFrames = Seq ? CurrentMeshInstance()->AnimGetFrameCount(Seq) : 1.0;

	if( bPlaying || bPlayingLIPSinc || CurrentMeshInstance()->LIPSincBlendInfo.bForceFrame )
	{
		// We're playing

		// Calculate deltatime
		static INT LastUpdateCycles;
		INT   CurrentCycles = appCycles();
		FLOAT DeltaTime = bPlayJustStarted ? 0.f : (CurrentCycles - LastUpdateCycles)*GSecondsPerCycle;

		// Tick the animation browser level	
		GTicks++;
		LIPSincBrowserLevel->Tick( LEVELTICK_All, DeltaTime );

		LastUpdateCycles = CurrentCycles;
		bPlayJustStarted = 0;

		// If we're playing, update FrameTime.
		FLOAT DeltaFrame = 0.f;
		FLOAT Rate = Seq ? CurrentMeshInstance()->AnimGetRate(Seq) / NumFrames : 1.0;
		DeltaFrame = Clamp<FLOAT>( (FLOAT)Rate*DeltaTime, 0.f, 1.f);
		FrameTime += DeltaFrame;

		// Always loop
		if( FrameTime >= 1.f )
			CleanupLevel(); // remove effects
		FrameTime -= appFloor(FrameTime); // Loop
		
		MeshInstance->AnimForcePose( DemoAnimSequence, FrameTime, DeltaFrame, 0 );

		// Update the Scrub bar.
		if( !CurrentMeshInstance()->LIPSincBlendInfo.bForceFrame && (MeshInstance->CurrentLIPSincAnim() != FString(TEXT("LIPSinc_NONE"))) )
		{
			INT EndTime = WorkMesh->LIPSincController->GetAnimation( WorkMesh->LIPSincController->FindAnimIndex( MeshInstance->CurrentLIPSincAnim() ) )->EndTime();
			INT ScrubPos = (INT)((MeshInstance->LIPSincBlendInfo.LastOffset / (FLOAT)EndTime) * 10000.0f);
			ScrubBar->SetPos( ScrubPos );
		}
	}

	// Draw the level.
	SceneNode.Render(Viewport->RI);

	// Draw the axis indicator.
	if( GUnrealEd->UseAxisIndicator )
		GUnrealEd->edDrawAxisIndicator(&SceneNode);

	// Print the name of the skeletal mesh at the top of the viewport.
	Viewport->Canvas->CurX = 0;
	Viewport->Canvas->CurY = 0;
	Viewport->Canvas->Color = FColor(255,255,255);
	FString Text = Mesh ? Mesh->GetPathName() : TEXT("No Animating Mesh");
	Viewport->Canvas->WrappedPrintf
	(
		Viewport->Canvas->SmallFont,
		1,
 		*Text
	);

	// Print the current animation frame.
	Viewport->Canvas->CurX = 0;
	Viewport->Canvas->CurY = Viewport->Canvas->CurY+10;
	Viewport->Canvas->Color = FColor(255,255,255);
	
	FString FrameText = Mesh ? Mesh->GetName() : TEXT(" NONE ");
	if( CameraActor->Misc1 < 0) FrameText = TEXT("REFPOSE");

	// Retrieve Displayed LOD index..
	INT DisplayedLodIndex = CurrentMeshInstance()->CurrentLODLevel;

	FString LODMessage;
	if( CurrentMeshInstance()->ForcedLodModel )
		LODMessage = FString::Printf( TEXT("LOD [%i]"),DisplayedLodIndex );
	else
		LODMessage = FString::Printf( TEXT("lod %i (%.2f)"),DisplayedLodIndex, CurrentMeshInstance()->LastLodFactor );

	Viewport->Canvas->WrappedPrintf
	(
		Viewport->Canvas->SmallFont,
		1,
		TEXT("[%s], Seq %i,  Frame %5.2f Max %d  %s"),
		*FrameText,
		CameraActor->Misc1 >=0 ? CameraActor->Misc1 : 0,
		FrameTime * NumFrames,
		(INT)(NumFrames)-1,
		*LODMessage
	);

	FString LIPSincMessage;
	if( CurrentMeshInstance()->IsPlayingLIPSincAnim() )
	{
		LIPSincMessage = FString::Printf( TEXT("Playing %s"), CurrentMeshInstance()->CurrentLIPSincAnim() );
	}
	else
	{
		StopLIPSincAnimation();
		LIPSincMessage = FString::Printf( TEXT("Not Playing") );
	}

	Viewport->Canvas->WrappedPrintf
	(
		Viewport->Canvas->SmallFont,
		1,
		TEXT("[LIPSINC] %s"),
		*LIPSincMessage
	);

	// Make sure the "Bone names in yellow..." string is printed before the pose weights
	// Print out bone names.
	if( MeshInstance->bPrintBoneNames && Viewport->bShowBones )
	{
		FColor curColor = Viewport->Canvas->Color;
		Viewport->Canvas->Color = FColor(255,255,0);
		Viewport->Canvas->WrappedPrintf(Viewport->Canvas->SmallFont, 1, TEXT("Bone names in yellow are included in the LIPSinc poses"));
		Viewport->Canvas->Color = curColor;
	}

	USkeletalMesh* SkelMesh = (USkeletalMesh*)Mesh;
	if( SkelMesh->LIPSincController && bShowTargetStats )
	{
		INT bDefaultTargets = 0;
		
		FColor savedColor = Viewport->Canvas->Color;
			
		if( SkelMesh->LIPSincController->BonePoseInfo()->NumBonePoses() == 20 )
		{
			bDefaultTargets = 1;
		}

		Viewport->Canvas->CurX = 0;
		Viewport->Canvas->CurY += 10;

		Viewport->Canvas->Color = FColor(255,255,255);
		Viewport->Canvas->WrappedPrintf(Viewport->Canvas->SmallFont, 0, TEXT("- Base Targets -"));

		Viewport->Canvas->CurX = 0;
		Viewport->Canvas->CurY += 10;

		for( INT p=0; p<SkelMesh->LIPSincController->BonePoseInfo()->NumBonePoses(); ++p )
		{
			FString TempStr;

			if( bDefaultTargets == 1 )
			{
				switch( p )
				{
				case 0:
					TempStr = FString::Printf( TEXT("Neutral          : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 1:
					TempStr = FString::Printf( TEXT("Eat              : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 2:
					TempStr = FString::Printf( TEXT("Earth            : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 3:
					TempStr = FString::Printf( TEXT("If               : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 4:
					TempStr = FString::Printf( TEXT("Ox               : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 5:
					TempStr = FString::Printf( TEXT("Oat              : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 6:
					TempStr = FString::Printf( TEXT("Wet              : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 7:
					TempStr = FString::Printf( TEXT("Size             : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 8:
					TempStr = FString::Printf( TEXT("Church           : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 9:
					TempStr = FString::Printf( TEXT("Fave             : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 10:
					TempStr = FString::Printf( TEXT("Though           : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 11:
					TempStr = FString::Printf( TEXT("Told             : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 12:
					TempStr = FString::Printf( TEXT("Bump             : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 13:
					TempStr = FString::Printf( TEXT("New              : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 14:
					TempStr = FString::Printf( TEXT("Roar             : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 15:
					TempStr = FString::Printf( TEXT("Cage             : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 16:
					TempStr = FString::Printf( TEXT("Eyebrow_Up_Left  : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 17:
					TempStr = FString::Printf( TEXT("Eyebrow_Up_Right : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 18:
					TempStr = FString::Printf( TEXT("Blink_Left       : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				case 19:
					TempStr = FString::Printf( TEXT("Blink_Right      : %f\n"), MeshInstance->CachedLIPSincValues(p));
					break;
				default:
					TempStr = FString::Printf( TEXT("Unknown Pose Index!\n"));
					break;
				}
			}
			else
			{
				// Nice formatting
				if( p > 9 )
				{
					TempStr = FString::Printf( TEXT("Pose %i : %f\n"), p, MeshInstance->CachedLIPSincValues(p));
				}
				else
				{
					TempStr = FString::Printf( TEXT("Pose %i  : %f\n"), p, MeshInstance->CachedLIPSincValues(p));
				}
			}

			if( MeshInstance->CachedLIPSincValues(p) > 0.f )
			{
				Viewport->Canvas->Color = FColor(255,255,255);
			}
			else
			{
				Viewport->Canvas->Color = FColor(0,255,0);
			}

			Viewport->Canvas->WrappedPrintf
				(
				Viewport->Canvas->SmallFont,
				0,
				TEXT("%s"),
				*TempStr
				);
		}

		Viewport->Canvas->CurX = 0;
		Viewport->Canvas->CurY += 10;

		Viewport->Canvas->Color = FColor(255,255,255);
		Viewport->Canvas->WrappedPrintf(Viewport->Canvas->SmallFont, 0, TEXT("- Expressions -"));

		Viewport->Canvas->CurX = 0;
		Viewport->Canvas->CurY += 10;
		
		for( INT e = 0; e < SkelMesh->LIPSincController->NumExpressions(); ++e )
		{
			FString TempStr;

			TLIPSincExpressionInfo *pExpression = SkelMesh->LIPSincController->GetExpression(e);

			DOUBLE fWeight = pExpression->m_fWeight;

			TempStr = FString::Printf( TEXT("%s : %f"), *(pExpression->GetName()), fWeight );
			
			if( fWeight > 0.f )
			{
				Viewport->Canvas->Color = FColor(255,255,255);
			}
			else
			{
				Viewport->Canvas->Color = FColor(0,255,0);
			}

			Viewport->Canvas->WrappedPrintf
				(
				Viewport->Canvas->SmallFont,
				0,
				TEXT("%s"),
				*TempStr
				);
		}

		Viewport->Canvas->Color = savedColor;
	}
	
	// Print out bone names.
	if( MeshInstance->bPrintBoneNames && Viewport->bShowBones )
	{
		FCanvasUtil	CanvasUtil( &Viewport->RenderTarget,Viewport->RI);
		UCanvas*	Canvas = Viewport->Canvas;

		FMatrix MeshToWorldMatrix = MeshInstance->MeshToWorld();
		for(INT b=0; b< MeshInstance->DebugPivots.Num(); b++)
		{
			INT	XL,YL;
			FName BoneName = SkelMesh->RefSkeleton(b).Name;
			Canvas->WrappedStrLenf( Canvas->SmallFont, XL, YL, *BoneName );
			FString BoneString = *BoneName;
			FMatrix JointMatrix =  MeshInstance->SpaceBases(b).Matrix();
			JointMatrix = MeshToWorldMatrix * JointMatrix;
			FVector B1 = MeshInstance->DebugPivots(b);
			B1 = MeshToWorldMatrix.TransformFVector( B1 );
			FPlane P = SceneNode.Project( B1);
			if( P.Z < 1 ) // Z < 1 : Behind the camera.
			{
				// FString CoordString = FString::Printf(TEXT("[%6.4f]"),P.Z);
				FVector C = CanvasUtil.ScreenToCanvas.TransformFVector(P);
				Canvas->SetClip(C.X,C.Y,XL+50,YL);   //?? Why the XL/YL ? XL+??
				
				// If this bone is contained in the LIPSinc poses, print it yellow
				if( SkelMesh->LIPSincController )
				{
					UBOOL found = 0;

					for( INT lbi=0; lbi<SkelMesh->LIPSincController->BonePoseInfo()->NumBonesPerPose(); ++lbi )
					{
						if( !appStrcmp(*(BoneString), *(SkelMesh->LIPSincController->BonePoseInfo()->GetBoneName( lbi ))) )
						{
							found = 1;
							FColor savedColor = Canvas->Color;
							Canvas->Color = FColor(255,255,0);
							Canvas->WrappedPrintf(Canvas->SmallFont, 0, *BoneString );
							Canvas->Color = savedColor;
							break;
						}
					}
					
					if( !found )
					{
						Canvas->WrappedPrintf(Canvas->SmallFont, 0, *BoneString );
					}

				}
				else
				{
					Canvas->WrappedPrintf(Canvas->SmallFont, 0, *BoneString );
				}
			}
		}
	}

	DemoAnimSequence = NAME_None;

	// Misc. cleanup.
	if( Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		CurrentMeshInstance()->bForceRefpose = false;
	}

	unguard;
}

void WBrowserLIPSinc::PositionChildControls()
{
	guard(WBrowserLIPSinc::PositionChildControls);

	Container.RefreshControls();
	
	unguard;
}

void WBrowserLIPSinc::OnDestroy()
{
	guard(WBrowserLIPSinc::OnDestroy);
	
	delete PackageCombo;
	delete MeshCombo;
	
	delete LIPSincAnimList;
		
	delete AnimListLabel;
	delete UnrealAnimListLabel;
	delete RegisteredToLabel;

	delete ScrubBar;
	
	delete Viewport;
	delete ViewportLabel;

	delete PlayButton;
	delete StopButton;
	
	delete UnrealAnimCombo;

	delete BlendWithCheck;

	delete SplitterContainer;

	::DeleteObject(PlayBitmap);
	::DeleteObject(PauseBitmap);
	::DeleteObject(StopBitmap);
	
	::DestroyWindow( hWndToolBar );
	delete ToolTipCtrl;

	//::DestroyWindow( LIPSincControllerPropertyWindow->hWnd );
	//LIPSincControllerPropertyWindow->Root.SetObjects( NULL, 0 );
	//delete LIPSincControllerPropertyWindow;

	::DestroyWindow( LIPSincAnimPropertyWindow->hWnd );
	LIPSincAnimPropertyWindow->Root.SetObjects( NULL, 0 );
	delete LIPSincAnimPropertyWindow;

	//::DestroyWindow( LIPSincPrefsPropertyWindow->hWnd );
	//LIPSincPrefsPropertyWindow->Root.SetObjects( NULL, 0 );
	//delete LIPSincPrefsPropertyWindow;
	
	WBrowser::OnDestroy();
	unguard;
}

void WBrowserLIPSinc::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WBrowserLIPSinc::OnSize);
	WBrowser::OnSize(Flags, NewX, NewY);
	PositionChildControls();
	InvalidateRect( hWnd, NULL, FALSE );
	
	if( MeshCombo && LIPSincAnimList && WorkMesh ) // Indicates entire window set initialized...
	{
		RefreshAll();
	}	
	
	UpdateMenu();
	unguard;
}

USkeletalMesh* WBrowserLIPSinc::CurrentSkelMesh()
{
	guard(WBrowserLIPSinc::CurrentMesh);
	
	// Ensure mesh current with WorkMesh.  Only returns a valid mesh if it's skeletal.
	if( MeshActor && WorkMesh && MeshActor->Mesh != WorkMesh )
		MeshActor->Mesh = WorkMesh;

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass() ) )
		return (USkeletalMesh*) WorkMesh;
	else
		return NULL;
	unguard;
}

USkeletalMeshInstance* WBrowserLIPSinc::CurrentMeshInstance()
{
	guard(WBrowserLIPSinc::CurrentMeshInstance);
	USkeletalMesh* SkelMesh = CurrentSkelMesh();
	if( SkelMesh )
	{
		if( MeshActor )
			return (USkeletalMeshInstance*)SkelMesh->MeshGetInstance( MeshActor );
		else
			return (USkeletalMeshInstance*)SkelMesh->MeshGetInstance(NULL);
	}
	else
		return NULL;
	unguard;
}

void WBrowserLIPSinc::OnCommand( INT Command )
{
	guard(WBrowserLIPSinc::OnCommand);

	switch( Command ) {
		
	// Load a package containing animation / meshes / associated anim-notifies etc.
	case IDMN_FileSave:
		{
			if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController )
			{
				debugf(TEXT("[LIPSinc]: Saving Controller..."));
				// No relative paths - changing the CWD could break them.
				FString ControllerFilename = FString( appBaseDir() ) + FString( TEXT("LIPSincData/Controllers/") );
				ControllerFilename += WorkMesh->GetName();
				ControllerFilename += TEXT(".lad");
				WorkMesh->LIPSincController->SaveToDisk( ControllerFilename );
				debugf(TEXT("[LIPSinc]: Saved."));
			}
			
			RefreshAll();
		}	
		break;
		
	case IDMN_VIEW_BONES:	
		{
			static UBOOL bToggleHideSkin = false;
			
			if(!Viewport)
				return;
			
			Viewport->bShowBones = !Viewport->bShowBones;
			
			if( Viewport->bShowBones )
			{
				bToggleHideSkin = !bToggleHideSkin;
				Viewport->bHideSkin = bToggleHideSkin; //!Viewport->bHideSkin;
			}
			else
			{
				// Make sure the object is never completely invisible...
				Viewport->bHideSkin=false;
			}
			
			RefreshViewport();				
		}
		break;
		
	case IDMN_VIEW_BONENAMES:
		{
			// Showing bones: a viewport-thing or a MESH-instance thing ?
			bPrintBones = !bPrintBones;
			USkeletalMeshInstance* MInst = CurrentMeshInstance();
			if(  MInst )
			{										
				MInst->bPrintBoneNames = bPrintBones;
			}
			RefreshViewport();
		}
		break;
		
	case IDMN_VIEW_INFLUENCES:
		{
			Viewport->bShowNormals = !Viewport->bShowNormals;
			RefreshViewport();
		}
		break;
		
	case IDMN_VIEW_BOUNDS:
		{
			Viewport->bShowBounds = !Viewport->bShowBounds;
			RefreshViewport();
		}
		break;
		
	case IDMN_VIEW_WIRE:				
		{
			bWireframe = !bWireframe;
			USkeletalMeshInstance* MInst = CurrentMeshInstance();
			if( MInst )				
			{										
				MInst->bForceWireframe = bWireframe;
			}
			RefreshViewport();
		}
		break;

	case IDMN_VIEW_TARGETSTATS:
		{
			bShowTargetStats = !bShowTargetStats;
			RefreshViewport();
		}
		break;
		
	case IDMN_LIPSINCGENERATEANIMATION:
		{
			OnLIPSincGenerateAnimation();
		}
		break;

	case IDMN_LIPSINCIMPORTNEWANIMATION:
		{
			// Create a new animation
			OnLIPSincImportLTF( 0 );
		}
		break;

	case IDMN_LIPSINCIMPORTLBP:
		{
			OnLIPSincImportLBP();
			RefreshViewport();
		}
		break;

	case IDMN_LIPSINC_UPDATEFROMLTF:
		{
			OnLIPSincImportLTF();
		}
		break;

	case IDMN_LIPSINC_QUICKLOADLTF:
		{
			OnLIPSincQuickLoadLTF();
		}
		break;

	case IDMN_LIPSINC_IMPORT_EXPRESSION:
		{
			OnLIPSincImportExpression();
			RefreshViewport();
		}
		break;

	case IDMN_LIPSINC_DELETE_EXPRESSION:
		{
			OnLIPSincDeleteExpression();
			RefreshViewport();
		}
		break;

	case IDMN_LIPSINC_EXPORT_EXPRESSIONS:
		{
			OnLIPSincExportExpressions();
		}
		break;

	case IDMN_LIPSINC_RENAME_ANIMATION:
		{
			OnLIPSincRenameAnimation();
		}
		break;

	case IDMN_LIPSINCREMOVE:
		{
			if( WorkMesh && WorkMesh->LIPSincController )
			{
				INT toDelete = LIPSincAnimList->GetCurrent();

				USkeletalMeshInstance* meshInstance;
				meshInstance = CurrentMeshInstance();

				if (toDelete >= 0)
				{
					StopLIPSincAnimation();

					INT indexInController = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString(toDelete) );
					if ( -1 != indexInController )
					{
						WorkMesh->LIPSincController->DeleteAnimation( indexInController );
						RefreshAll();
					}
				}
			}
			else
			{
				NoControllerMessage();
			}
		}
		break;

	case IDMN_LIPSINCPLAY:
		{
			if( WorkMesh && WorkMesh->LIPSincController )
			{
				bPausedLIPSinc  = 0;
				bPlayingLIPSinc = 0;
				OnPlayButton();
			}
			else
			{
				NoControllerMessage();
			}
		}
		break;

	case IDMN_VIEW_INFO:
		{
			if ( WorkMesh && WorkMesh->LIPSincController )
			{
				WDlgLIPSincControllerInfo l_dlg(NULL, this, WorkMesh->LIPSincController, LIPSincAnimList->GetString(LIPSincAnimList->GetCurrent()));
				l_dlg.DoModal();
			}
			else
			{
				NoControllerMessage();
			}
		}
		break;

	default:
		WBrowser::OnCommand(Command);
		break;
	}
	unguard;
}

// Notification delegates for child controls.
//
void WBrowserLIPSinc::OnPackageSelectionChange()
{
	guard(WBrowserLIPSinc::OnPackageSelectionChange);
	
	RefreshMeshList(); // Sets current mesh to 0th entry...
	OnMeshSelectionChange();
		
	unguard;
}

void WBrowserLIPSinc::OnMeshSelectionChange()
{
	guard(WBrowserLIPSinc::OnMeshSelectionChange);
	
	WorkMesh = Cast<USkeletalMesh>(UObject::StaticFindObject(USkeletalMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));								
		
	RefreshLIPSincAnimList();
	RefreshUnrealAnimPackageList();
	//RefreshLIPSincControllerProperties();
	RefreshLIPSincAnimProperties();
	//RefreshLIPSincPrefsProperties();
	RefreshViewport();
	SetCaption();
	
	unguard;
}

void WBrowserLIPSinc::OnUnrealAnimPackageSelectionChange( )
{
	guard(WBrowserLIPSinc::OnUnrealAnimPackageSelectionChange);

	// Defaultanimation assigned ?
	if(  CurrentSkelMesh() )
	{					
		USkeletalMesh* SkelMesh = CurrentSkelMesh();
		// Since an animation was explicitly selected, make it current AND active now for the mesh...	
		UMeshAnimation* CurrentMeshAnim = Cast<UMeshAnimation>(
			UObject::StaticFindObject(UMeshAnimation::StaticClass(), ANY_PACKAGE,
			*( UnrealAnimPackageCombo->GetString(UnrealAnimPackageCombo->GetCurrent()))));

		CurrentMeshInstance()->ClearSkelAnims();
		if(! CurrentMeshInstance()->SetSkelAnim( CurrentMeshAnim, NULL ) )
					debugf(TEXT("SetSkelAnim failed - Mesh: %s  Animation: %s "),SkelMesh->GetName(),CurrentMeshAnim->GetName());			

	}
	
	RefreshUnrealAnimList();
	RefreshViewport();
	SetCaption();

	unguard;
}


void WBrowserLIPSinc::OnUnrealAnimListSelectionChange()
{
	guard(WBrowserLIPSinc::OnUnrealAnimListSelectionChange);
	
	if( !Viewport || !Viewport->Actor )
		return; // Called too soon..
	
	CleanupLevel();  // remove effects
	
	RefreshViewport();
	
	unguard;
}

void WBrowserLIPSinc::OnLIPSincAnimListDoubleClick()
{
	guard(WBrowserLIPSinc::OnLIPSincAnimListDoubleClick);
	
	if( !Viewport || !Viewport->Actor )
		return; // Called too soon..
	
	StopLIPSincAnimation();
	OnPlayButton();					
				
	RefreshViewport();
	
	unguard;
}

void WBrowserLIPSinc::OnUnrealAnimListDoubleClick()
{
	guard(WBrowserLIPSinc::OnUnrealAnimListDoubleClick);
	
	if( !Viewport || !Viewport->Actor )
		return; // Called too soon..
	
	bRefPose = !bRefPose;

	bPlayJustStarted = bPlaying = 1;
	
	RefreshViewport();
	
	unguard;
}

void WBrowserLIPSinc::OnUnrealAnimSelectionChange()
{
	guard(WBrowserLIPSinc::OnUnrealAnimSelectionChange);

	if( !Viewport || !Viewport->Actor )
		return;		// Called too soon.

	bPlayJustStarted = bPlaying = bRefPose = BlendWithCheck->bChecked ? 1 : 0;

	RefreshViewport();

	unguard;
}

int WBrowserLIPSinc::RunAnalysis( FString WavFilename, FString AnimName, FString PkgName, UBOOL bText, FString Text )
{
	guard(WBrowserLIPSinc::RunAnalysis);

	const TCHAR theAppName[] = TEXT("Impersonator");
	
	// This needs to correctly reset itself to the current directory.
	FString ImpersonatorDataDirectory = FString(appBaseDir()) + FString(TEXT("LIPSincData"));
	LIPSincType(ERR) err = LIPSincCall(Function1)(FINAL_PARAM(TCHAR_TO_ANSI(*ImpersonatorDataDirectory)));
	if( err != LIPSincType(NOERR) )
	{
		debugf(TEXT("[LIPSinc]: Couldn't find the LIPSinc data folder at %s"), *ImpersonatorDataDirectory);
		appMsgf(0, *FString::Printf(TEXT("Error: %s could not find the LIPSinc data folder"), theAppName));
		LIPSincCall(Function2)();
		return -1;
	}
	
	debugf(TEXT("[LIPSinc]: Impersonator Started."));
	
	LIPSincType(ANALYSIS) *pAnalysis = NULL;

	FString MappingFilename = FString(appBaseDir()) + FString(TEXT("LIPSincData/Mappings/")) + WorkMesh->GetName() +
							  FString(TEXT(".map"));

	char mapFilename[8192]; mapFilename[0] = '\0';

	debugf(TEXT("[LIPSinc]: Looking for map %s"), *MappingFilename);

	FArchive* Ar = GFileManager->CreateFileReader(*(MappingFilename));
	
	if( Ar )
	{
		debugf(TEXT("[LIPSinc]: Found map!"));
		strcpy(mapFilename, TCHAR_TO_ANSI(*(MappingFilename)));
		delete Ar;
	}

	FString TextFilename = WavFilename.Left( WavFilename.InStr( TEXT("."), 1 ) );
	TextFilename += TEXT(".txt");
	
	char wavFilename[8192]; wavFilename[0] = '\0';
	char txtFilename[8192]; txtFilename[0] = '\0';	
	
	strcpy(wavFilename, TCHAR_TO_ANSI(*(WavFilename)));
	strcpy(txtFilename, TCHAR_TO_ANSI(*(TextFilename)));
	
	char *pTextInputData   = NULL;
	char *pTextForAnalysis = NULL;
	
	if( bText )
	{
		FString Data;
		UBOOL goodText;
		if (Text.Len() > 0)
		{
			Data = Text;
			goodText = 1;
		}
		else
		{
			goodText = appLoadFileToString( Data, ANSI_TO_TCHAR(txtFilename) );
		}
		
		if( goodText )
		{
			long textFileSize = Data.Len()+1;
			
			debugf(TEXT("[LIPSinc]: Using text %s size %i"), *(TextFilename), textFileSize);
			
			pTextInputData = new char[textFileSize+1];
			pTextForAnalysis = new char[textFileSize+1];
			strcpy(pTextInputData, TCHAR_TO_ANSI(*(Data)));
						
			pTextInputData[textFileSize] = '\0';
			
			if( LIPSincCall(Function7)(pTextInputData, textFileSize, FINAL_PARAM(pTextForAnalysis)) != LIPSincType(NOERR) )
			{
				// We should never be here.
				LIPSincCall(Function2)();
				return -1;
			}
		}
	}
	
	if( LIPSincCall(Function8)(&pAnalysis, wavFilename, pTextForAnalysis, FINAL_PARAM(mapFilename)) != LIPSincType(NOERR) )
	{
		appMsgf(0, *FString::Printf(TEXT("Error: %s could not analyze the specified file. Please ensure it is a PCM wave file."), theAppName));
		LIPSincCall(Function2)();
		return -1;
	}

	if(pTextInputData)
	{
		delete [] pTextInputData;
		pTextInputData = NULL;
	}
	
	if(pTextForAnalysis)
	{
		delete [] pTextForAnalysis;
		pTextForAnalysis = NULL;
	}
	
	TLIPSincAnimation myAnim;
	
	myAnim.SetName( AnimName );
	
	FString myFull = *(PkgName);
	myFull += FString(ANSI_TO_TCHAR("."));
	myFull += *(AnimName);
	
	myAnim.SetFullPkgName(myFull);
	
	long numSpeechTargetTracks = 0;
	long numGestureTracks      = 0;
	
	if( LIPSincCall(Function10)(pAnalysis, FINAL_PARAM(&numSpeechTargetTracks)) != LIPSincType(NOERR) )
	{
		appMsgf(0, *FString::Printf(TEXT("Error: %s could not get the number of speech tracks."), theAppName));
		LIPSincCall(Function2)();
		return -1;
	}
		
	if( LIPSincCall(Function13)(pAnalysis, FINAL_PARAM(&numGestureTracks)) != LIPSincType(NOERR) )
	{
		appMsgf(0, *FString::Printf(TEXT("Error: %s could not get the number of gesture tracks."), theAppName));
		LIPSincCall(Function2)();
		return -1;
	}
		
	long keyCount   = 0;
	
	double time     = 0.0f;
	double value    = 0.0f;
	double derivIn  = 0.0f;
	double derivOut = 0.0f;

	for( int stTrack = 0; stTrack < numSpeechTargetTracks; ++stTrack )
	{
		if( LIPSincCall(Function11)(pAnalysis, stTrack, FINAL_PARAM(&keyCount)) != LIPSincType(NOERR) )
		{
			appMsgf(0, *FString::Printf(TEXT("Error: %s could not get the number of keys in a speech track."), theAppName));
			LIPSincCall(Function2)();
			return -1;
		}
		
		TLIPSincAnimationTrack thisTrack;

		for( int stKey = 0; stKey < keyCount; ++stKey )
		{
			if( LIPSincCall(Function12)(
				pAnalysis,
				stTrack,
				stKey,
				&time,
				&value,
				&derivIn,
				FINAL_PARAM(&derivOut)) != LIPSincType(NOERR) )
			{
				appMsgf(0, *FString::Printf(TEXT("Error: %s could not retrieve a key in a speech track."), theAppName));
				LIPSincCall(Function2)();
				return -1;
			}
			
			int nTime = (int)(time * 1000.0f);  // convert to msecs
			
			TLIPSincAnimationKey thisKey(nTime, value);
			
			thisTrack.AddKey(thisKey);
		}
		
		myAnim.AddTrack(thisTrack);
	}
	
	for( int sgTrack = 0; sgTrack < numGestureTracks; ++sgTrack )
	{
		if( LIPSincCall(Function14)(pAnalysis, sgTrack, FINAL_PARAM(&keyCount)) != LIPSincType(NOERR) )
		{
			appMsgf(0, *FString::Printf(TEXT("Error: %s could not get the number of keys in a gesture track."), theAppName));
			LIPSincCall(Function2)();
			return -1;
		}
		
		TLIPSincAnimationTrack thisTrack;

		for( int sgKey = 0; sgKey < keyCount; ++sgKey )
		{
			if( LIPSincCall(Function15)(
				pAnalysis,
				sgTrack,
				sgKey,
				&time,
				&value,
				&derivIn,
				FINAL_PARAM(&derivOut)) != LIPSincType(NOERR) )
			{
				appMsgf(0, *FString::Printf(TEXT("Error: %s could not retrieve a key in a gesture track."), theAppName));
				LIPSincCall(Function2)();
				return -1;
			}
			
			int nTime = (int)(time * 1000.0f);  // convert to msecs
			
			TLIPSincAnimationKey thisKey(nTime, value);
			
			thisTrack.AddKey(thisKey);
		}
		
		myAnim.AddTrack(thisTrack);
	}
	
	if( LIPSincCall(Function9)(FINAL_PARAM(&pAnalysis)) != LIPSincType(NOERR) )
	{
		appMsgf(0, *FString::Printf(TEXT("Error: %s could not free the analysis."), theAppName));
		LIPSincCall(Function2)();
		return -1;
	}
	
	
	if( LIPSincCall(Function2)() != LIPSincType(NOERR) )
	{
		appMsgf(0, *FString::Printf(TEXT("Error: %s could not shut down."), theAppName));
		return -1;
	}
	
	debugf(TEXT("[LIPSinc]: Impersonator shutdown."));
	
	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController )
	{
		WorkMesh->LIPSincController->AddAnimation( myAnim );
	}

	RefreshAll();
	RefreshViewport();
	
	return 1;
	
	unguard;
}

void WBrowserLIPSinc::OnLIPSincAnimListRightClick()
{
	guard(WBrowserLIPSinc::OnLIPSincAnimListRightClick);

	GUnrealEd->EdCallback(EDC_LIPSincAnimListRtClick, 0, 0);
	RefreshViewport();

	unguard;
}

void WBrowserLIPSinc::OnLIPSincAnimListSelectionChange()
{
	guard(WBrowserLIPSinc::OnLIPSincAnimListSelectionChange);

	RefreshLIPSincAnimProperties();

	if( WorkMesh )
	{
		INT index;
		index = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString(LIPSincAnimList->GetCurrent()) );
		
		// Reset the scrub bar pos.
		ScrubBar->SetPos( 0 );
	}

	unguard;
}

void WBrowserLIPSinc::OnBlendWithChange()
{
	guard(WBrowserLIPSinc::OnBlendWithChange);
	
	if ( !Viewport || !Viewport->Actor)
		return;		// Called too soon

	UBOOL isPlaying = BlendWithCheck->bChecked;

	bRefPose = !isPlaying;
	bPlayJustStarted = bPlaying = isPlaying;

	RefreshViewport();
		
	unguard;
}

void WBrowserLIPSinc::OnPlayButton()
{
	guard(WBrowserLIPSinc::OnPlayButton);

	if( bPlayingLIPSinc && !bPausedLIPSinc )
	{
		bPausedLIPSinc = 1;
		PlayButton->SetBitmap(PlayBitmap);
		CurrentMeshInstance()->PauseLIPSincAnim();
	}
	else if( bPlayingLIPSinc && bPausedLIPSinc )
	{
		bPausedLIPSinc = 0;
		PlayButton->SetBitmap(PauseBitmap);
		CurrentMeshInstance()->ResumeLIPSincAnim();
	}
	else if( !bPlayingLIPSinc )
	{
		bPlayingLIPSinc = 1;
		bPausedLIPSinc  = 0;
		PlayButton->SetBitmap(PauseBitmap);
		GUnrealEd->Exec( TEXT("AUDIO FINDVIEWPORT") );
		CurrentMeshInstance()->PlayLIPSincAnim( FName(*(LIPSincAnimList->GetString(LIPSincAnimList->GetCurrent()))), 1.0, GAudioDefaultRadius, 1.0 );
		RefreshViewport();
	}
	
	unguard;
}

void WBrowserLIPSinc::StopLIPSincAnimation( )
{
	guard(WBrowserLIPSinc::StopLIPSincAnimation);

	if( bPlayingLIPSinc )
	{
		PlayButton->SetBitmap( PlayBitmap );
		
		USkeletalMeshInstance* meshInstance = CurrentMeshInstance();
		if( meshInstance && meshInstance->IsPlayingLIPSincAnim() )
		{
			meshInstance->StopLIPSincAnim();

			ScrubBar->SetPos( 0 );
			
			// Clear out the 'white' / active values
			meshInstance->CachedLIPSincValues.Empty();
			meshInstance->CachedLIPSincValues.AddZeroed( 27 );  // FIXME

			if( WorkMesh )
			{
				for( INT e = 0; e < WorkMesh->LIPSincController->NumExpressions(); ++e )
				{
					WorkMesh->LIPSincController->GetExpression( e )->m_fWeight = 0.0;
				}
			}
		}
		
		if( !bPlaying )
			Viewport->Actor->ShowFlags &= ~SHOW_RealTime;
	}	

	bPlayingLIPSinc = 0;

	unguard;
}


void WBrowserLIPSinc::OnStopButton()
{
	guard(WBrowserLIPSinc::OnStopButton);
	
	StopLIPSincAnimation();
	RefreshViewport();
	
	unguard;
}

void WBrowserLIPSinc::OnLIPSincGenerateAnimation( )
{
	guard(WBrowserLIPSinc::OnLIPSincGenerateAnimation);

	// ensure the mesh has a valid lipsinc controller
	if( WorkMesh && !WorkMesh->LIPSincController )
	{
		NoControllerMessage();
		return;
	}

	OPENFILENAMEA ofn;
	char File[8192] = "\0";
	
	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	
	ofn.lpstrFilter = "WAV Files (*.wav)\0*.wav\0\0";
	ofn.lpstrDefExt = "wav";
	
	ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GLastDir[eLASTDIR_WAV]) );								
	
	ofn.lpstrTitle = "Select wave file(s) for analysis";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	
	if( GetOpenFileNameA(&ofn) )
	{
		INT NumNULLs;
		NumNULLs = FormatFilenames( File );

		TArray<FString> StringArray;
		FString S = ANSI_TO_TCHAR( File );  // avoid 1024 char limit imposed by appFromAnsi()
		S.ParseIntoArray( TEXT("|"), &StringArray );

		if( StringArray.Num() == 1 )
			GLastDir[eLASTDIR_WAV] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
		else
			GLastDir[eLASTDIR_WAV] = StringArray(0);
				
		if (StringArray.Num() == 1)
		{
			FString FullName = StringArray(0);
			
			FString Ext = FullName.Right( FullName.Len() - FullName.InStr( TEXT("."), 1 ) - 1 );

			if( Ext.Caps() != FString(TEXT("WAV")) )
			{
				FString ErrorMsg = FString::Printf(TEXT("The file %s is not a .WAV file!"), FullName);

				appMsgf( 0, *(ErrorMsg) );
				
				StringArray.Empty();

				return;
			}
			
			// Process single wave file
			WDlgCreateLIPSincAnim l_dlg( NULL, this );
			if( l_dlg.DoModal( FString(TEXT("")), FString(TEXT("")), FullName ) ) // Present user with import window..
			{
				GWarn->BeginSlowTask( TEXT("Generating LIPSinc Animation"), 1 );
				
				debugf(TEXT("[LIPSinc]: Running Analysis..."));
				INT result = RunAnalysis( FullName, l_dlg.Name, l_dlg.Package, l_dlg.DoText, l_dlg.Text );
				debugf(TEXT("[LIPSinc]: Done."));
				GWarn->StatusUpdatef( 1, 1, TEXT("Processing %s"), *(FullName) );
				
				if(result == -1)
				{
					debugf(TEXT("[LIPSinc]: WARNING: Analysis failed for file %s!"), FullName);

					appMsgf(0, TEXT("WARNING: Analysis failed for file %s!"), FullName );

					if(l_dlg.DoImportAudio)
					{
						debugf(TEXT("[LIPSinc]: WARNING: Sound will not be imported!"));

						appMsgf(0, TEXT("WARNING: The sound will not be imported!"));
					}
				}
				else
				{
					if(l_dlg.DoImportAudio)
					{
						debugf(TEXT("[LIPSinc]: Importing Audio..."));
						
						TCHAR l_chCmd[512];				
						appSprintf( l_chCmd, TEXT("AUDIO IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\""),
							*(FullName), *(l_dlg.Name), *(l_dlg.Package) );
						GUnrealEd->Exec( l_chCmd );
						
						debugf(TEXT("[LIPSinc]: Done."));
					}

					debugf(TEXT("[LIPSinc]: OK!"));
				}
				
				GWarn->EndSlowTask();
			}
		}
		else
		{
			// Process multiple wave files
			WDlgCreateLIPSincAnimBatch l_dlg( NULL, this );

			INT NumNonWavs = 0;
			INT NumFails   = 0;

			if (l_dlg.DoModal( StringArray ) )
			{
				GWarn->BeginSlowTask( TEXT("Generating LIPSinc Animations"), 1 );

				for( INT i=1; i<StringArray.Num(); ++i )
				{
					FString FullName = StringArray(0);
					FullName += TEXT("\\");
					FullName += StringArray(i);

					FString Ext = FullName.Right( FullName.Len() - FullName.InStr( TEXT("."), 1 ) - 1 );

					if( Ext.Caps() != FString(TEXT("WAV")) )
					{
						NumNonWavs++;

						debugf(TEXT("[LIPSinc]: WARNING: The file %s is not a .WAV file!"), FullName);
						
						continue;
					}
					
					debugf(TEXT("[LIPSinc]: Processing %s..."), *(FullName));					
					GWarn->StatusUpdatef( i, StringArray.Num(), TEXT("Processing %s"), *(FullName) );
					FString AudioName = GetFilenameOnly( FullName );
										
					debugf(TEXT("[LIPSinc]: Running Analysis..."));					
					INT result = RunAnalysis( FullName, AudioName, l_dlg.Package, l_dlg.DoText );
					debugf(TEXT("[LIPSinc]: Done."));
					
					if(result == -1)
					{
						NumFails++;

						debugf(TEXT("[LIPSinc]: WARNING: Analysis failed for file %s!"), FullName);

						if(l_dlg.DoImportAudio)
						{
							debugf(TEXT("[LIPSinc]: WARNING: Sound will not be imported!"));
						}
					}
					else
					{
						if(l_dlg.DoImportAudio)
						{
							debugf(TEXT("[LIPSinc]: Importing Audio..."));
							
							TCHAR l_chCmd[512];						
							appSprintf( l_chCmd, TEXT("AUDIO IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\""),
								*(FullName), *(AudioName), *(l_dlg.Package) );					
							GUnrealEd->Exec( l_chCmd );
							
							debugf(TEXT("[LIPSinc]: Done."));
						}

						RefreshAll();

						debugf(TEXT("[LIPSinc]: OK!"));
					}
				}
				GWarn->EndSlowTask();
			}

			if( NumNonWavs > 0 )
			{
				FString ErrorMsg = FString::Printf(TEXT("%d files were not .WAV files.  Please check the log file."), NumNonWavs);
				appMsgf( 0, *(ErrorMsg) );
			}

			if( NumFails > 0 )
			{
				FString ErrorMsg = FString::Printf(TEXT("%d files could not be analyzed.  Please check the log file."), NumFails);
				appMsgf( 0, *(ErrorMsg) );
			}
		}	
		StringArray.Empty();

		GBrowserMaster->RefreshAll();
	}

	unguard;
}

void WBrowserLIPSinc::OnLIPSincQuickLoadLTF( )
{
	guard(WBrowserLIPSinc::OnLIPSincQuickLoadLTF);

	if( WorkMesh && !WorkMesh->LIPSincController )
	{
		NoControllerMessage();
		return;
	}

	INT Index = LIPSincAnimList->GetCurrent();

	if( Index >= 0 )
	{
		// Find this entry in the QuickLoad list.
		INT indexInController = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString( Index ) );

		FString LTFPath;

		if( GQuickLoadList.FindLastLTF( *WorkMesh->LIPSincController, indexInController, LTFPath ) )
		{
			// If it's there, use the entry.
			OnLIPSincImportLTF( 1, LTFPath );
		}
		else
		{
			// If it's not there, call OnLIPSincImportLTF() such that the user browses for the LTF file.
			OnLIPSincImportLTF();
		}
	}

	unguard;
}

//!! FIXME
void WBrowserLIPSinc::OnLIPSincImportLTF( UBOOL bUpdateExisting, FString InLTFFile )
{
	guard(WBrowserLIPSinc::OnLIPSincImportLTF);

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController )
	{
		TArray<FString> StringArray;
		FString FullName;

		if( InLTFFile == TEXT("") )
		{
			OPENFILENAMEA ofn;
			char File[8192] = "\0";

			ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
			ofn.lStructSize = sizeof(OPENFILENAMEA);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = File;
			ofn.nMaxFile = sizeof(char) * 8192;

			ofn.lpstrFilter = "LTF Files (*.ltf)\0*.ltf\0\0";
			ofn.lpstrDefExt = "ltf";

			ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GLastDir[eLASTDIR_LTF]) );								

			ofn.lpstrTitle = "Select LTF file to import";
			ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER;

			if( GetOpenFileNameA(&ofn) )
			{
				INT NumNULLs = FormatFilenames( File );

				FString S = ANSI_TO_TCHAR( File );
				S.ParseIntoArray( TEXT("|"), &StringArray );

				INT iStart = 0;
				FString Prefix = TEXT("\0");

				if( NumNULLs )
				{
					iStart = 1;
					Prefix = *(StringArray(0));
					Prefix += TEXT("\\");
				}

				if( StringArray.Num() == 1 )
					GLastDir[eLASTDIR_LTF] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
				else
					GLastDir[eLASTDIR_LTF] = StringArray(0);

				if( StringArray.Num() > 1 )
				{
					FullName = StringArray(0);
					FullName += TEXT("\\");
					FullName += StringArray(1);
				}
				else
				{
					FullName = StringArray(0);
				}
			}
			else
			{
				return;
			}
		}
		else
		{
			FullName = InLTFFile;
		}

		debugf(TEXT("[LIPSinc]: Parsing %s..."), FullName);

		FString Ext = FullName.Right( FullName.Len() - FullName.InStr( TEXT("."), 1 ) - 1 );

		if( Ext.Caps() != FString(TEXT("LTF")) )
		{
			FString ErrorMsg = FString::Printf(TEXT("The file %s is not a valid .LTF file!"), FullName);

			appMsgf( 0, *(ErrorMsg) );

			debugf(TEXT("[LIPSinc]: done."));

			StringArray.Empty();

			return;
		}

		// Helper object to parse .LTF files.
		TLIPSincLTFFileParser ltfParser;

		// Used to parse into so we don't screw up the current animation at this point.
		TLIPSincAnimation TempAnim;

		// Parse the .LTF file.
		if( !ltfParser.Parse( FullName, &TempAnim, WorkMesh ) )
		{

		}

		TempAnim.Digest();

		if( bUpdateExisting )
		{
			INT toUpdate = LIPSincAnimList->GetCurrent();

			USkeletalMeshInstance* meshInstance;
			meshInstance = CurrentMeshInstance();

			if( toUpdate >= 0 )
			{
				StopLIPSincAnimation();

				INT indexInController = WorkMesh->LIPSincController->FindAnimIndex( LIPSincAnimList->GetString(toUpdate) );
				if ( -1 != indexInController )
				{
					TLIPSincAnimation *pAnimation = WorkMesh->LIPSincController->GetAnimation( indexInController );

					TempAnim.SetName         ( pAnimation->Name()            );
					TempAnim.SetFullPkgName  ( pAnimation->FullPkgName()     );
					TempAnim.SetBlendInTime  ( pAnimation->GetBlendInTime()  );
					TempAnim.SetBlendOutTime ( pAnimation->GetBlendOutTime() );
					TempAnim.SetInterruptible( pAnimation->IsInterruptible() );

					GQuickLoadList.UpdateList( *WorkMesh->LIPSincController, indexInController, FullName );

					WorkMesh->LIPSincController->DeleteAnimation( indexInController );
					WorkMesh->LIPSincController->AddAnimation( TempAnim );
					WorkMesh->LIPSincController->SetDirty();

					RefreshAll();
				}
			}
		}
		else
		{
			WDlgNameLIPSincAnimation NameDlg( NULL, this );

			TArray<FString> InitialNameParts;
			FullName.ParseIntoArray( TEXT("\\"), &InitialNameParts );

			FString InitialName = InitialNameParts(InitialNameParts.Num()-1).Left( InitialNameParts(InitialNameParts.Num()-1).InStr( TEXT("."), 1) );

			InitialNameParts.Empty();

			if( NameDlg.DoModal( InitialName ) )
			{				
				TempAnim.SetName( NameDlg.Name );
				WorkMesh->LIPSincController->AddAnimation( TempAnim );
				WorkMesh->LIPSincController->SetDirty();

				INT indexInController = WorkMesh->LIPSincController->FindAnimIndex( TempAnim.Name() );

				GQuickLoadList.UpdateList( *WorkMesh->LIPSincController, indexInController, FullName );

				RefreshAll();
			}
		}
	}
	else
	{
		NoControllerMessage();
	}

	unguard;
}

void WBrowserLIPSinc::OnLIPSincRenameAnimation( )
{
	guard(WBrowserLIPSinc::OnLIPSincRenameAnimation);

	if( WorkMesh && !WorkMesh->LIPSincController )
	{
		NoControllerMessage();
		return;
	}

	INT ToRename = LIPSincAnimList->GetCurrent();

	if( ToRename >= 0 )
	{
		FString CurrentName = LIPSincAnimList->GetString(ToRename);

		INT indexInController = WorkMesh->LIPSincController->FindAnimIndex( CurrentName );
		
		if ( -1 != indexInController )
		{
			WDlgNameLIPSincAnimation NameDialog( NULL, this );

			if( NameDialog.DoModal( CurrentName ) )
			{
				WorkMesh->LIPSincController->GetAnimation( indexInController )->SetName( NameDialog.Name );
				WorkMesh->LIPSincController->SetDirty();
				RefreshAll();
			}
		}
	}

	unguard;
}

void WBrowserLIPSinc::OnLIPSincExportExpressions( )
{
	guard(WBrowserLIPSinc::OnLIPSincExportExpressions);

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController )
	{
		OPENFILENAMEA ofn;
		char File[8192] = "\0";
		
		ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFile = File;
		ofn.nMaxFile = sizeof(char) * 8192;
		
		ofn.lpstrFilter = "IEX Files (*.iex)\0*.iex\0\0";
		ofn.lpstrDefExt = "iex";
		
		ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GLastDir[eLASTDIR_IEX]) );								
		
		ofn.lpstrTitle = "Save IEX as...";
		ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER;
		
		if( GetSaveFileNameA(&ofn) )
		{
			INT NumNULLs = FormatFilenames( File );
			
			TArray<FString> StringArray;
			FString S = ANSI_TO_TCHAR( File );
			S.ParseIntoArray( TEXT("|"), &StringArray );
			
			INT iStart = 0;
			FString Prefix = TEXT("\0");
			
			if( NumNULLs )
			{
				iStart = 1;
				Prefix = *(StringArray(0));
				Prefix += TEXT("\\");
			}
			
			if( StringArray.Num() == 1 )
				GLastDir[eLASTDIR_IEX] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
			else
				GLastDir[eLASTDIR_IEX] = StringArray(0);
			
			FString FullName;
			
			if( StringArray.Num() > 1 )
			{
				FullName = StringArray(0);
				FullName += TEXT("\\");
				FullName += StringArray(1);
			}
			else
			{
				FullName = StringArray(0);
			}

			debugf(TEXT("[LIPSinc]: Saving %s..."), *(FullName));

			FString ExpFile;

			ExpFile = FString::Printf(TEXT("%d\r\n"), WorkMesh->LIPSincController->NumExpressions());

			for( INT i = 0; i < WorkMesh->LIPSincController->NumExpressions(); ++i )
			{
				ExpFile += FString::Printf(TEXT("%s\r\n"), *(WorkMesh->LIPSincController->GetExpression(i)->GetName()));
			}

			appSaveStringToFile(ExpFile, *(FullName));
		}
	}
	else
	{
		NoControllerMessage();
	}

	unguard;
}

void WBrowserLIPSinc::OnLIPSincDeleteExpression( )
{
	guard(WBrowserLIPSinc::OnLIPSincDeleteExpression);

	if( WorkMesh && WorkMesh->LIPSincController )
	{
		WDlgDeleteLIPSincExpression deleteDialog(NULL, this);
		deleteDialog.DoModal( WorkMesh->LIPSincController, CurrentMeshInstance() );
	}
	else
	{
		NoControllerMessage();
	}
	
	unguard;
}

void WBrowserLIPSinc::OnLIPSincImportExpression( )
{
	guard(WBrowserLIPSinc::OnLIPSincImportExpression);

	// Check to see if the current mesh has a controller attached to it.
	if ( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && !WorkMesh->LIPSincController)
	{
		NoControllerMessage();
		return;
	}

	OPENFILENAMEA ofn;
	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;

	ofn.lpstrFilter = "LBP Files (*.lbp)\0*.lbp\0\0";
	ofn.lpstrDefExt = "lbp";

	ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GLastDir[eLASTDIR_LBP]) );								

	ofn.lpstrTitle = "Select LBP file to import";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	if( GetOpenFileNameA(&ofn) )
	{
		INT NumNULLs = FormatFilenames( File );

		TArray<FString> StringArray;
		FString S = ANSI_TO_TCHAR( File );
		S.ParseIntoArray( TEXT("|"), &StringArray );

		INT iStart = 0;
		FString Prefix = TEXT("\0");

		if( NumNULLs )
		{
			iStart = 1;
			Prefix = *(StringArray(0));
			Prefix += TEXT("\\");
		}

		if( StringArray.Num() == 1 )
			GLastDir[eLASTDIR_LBP] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
		else
			GLastDir[eLASTDIR_LBP] = StringArray(0);

		FString FullName;

		if( StringArray.Num() > 1 )
		{
			FullName = StringArray(0);
			FullName += TEXT("\\");
			FullName += StringArray(1);
		}
		else
		{
			FullName = StringArray(0);
		}

		if( StringArray.Num() == 1 )
		{
			FullName = StringArray(0);

			debugf(TEXT("[LIPSinc]: Parsing %s..."), FullName);

			FString Ext = FullName.Right( FullName.Len() - FullName.InStr( TEXT("."), 1 ) - 1 );

			if( Ext.Caps() != FString(TEXT("LBP")) )
			{
				FString ErrorMsg = FString::Printf(TEXT("The file %s is not a valid .LBP file!"), FullName);

				appMsgf( 0, *(ErrorMsg) );

				debugf(TEXT("[LIPSinc]: done."));

				StringArray.Empty();

				return;
			}

			// Helper object to parse .LBP files.
			TLIPSincLBPFileParser lbpParser;

			// Used to parse into, so we don't scrap the working poses when importing a set that doesn't link up.
			TLIPSincBonePoseInfo TempBPI;

			TLIPSincPhonemeMap PhonemeMap;

			// Bones that could not be linked up.
			TArray<FString> DanglingBones;

			// Parse the .LBP file.
			if( !lbpParser.Parse( FullName, &TempBPI, &PhonemeMap, WorkMesh->LIPSincController->GetLookAtInfo(), WorkMesh, &DanglingBones ) )
			{
				// This set of poses did not link up, so clear them.
				TempBPI.Clear();

				// Notify user of bones that could not be linked up.
				char boneList[8192] = "\0";

				debugf(TEXT("[LIPSinc]: There were %d unmatched bones."), DanglingBones.Num());

				for( INT dbi=0; dbi<DanglingBones.Num(); ++dbi )
				{
					strcat(boneList, "\t");
					strcat(boneList, TCHAR_TO_ANSI(*(DanglingBones(dbi))));
					strcat(boneList, "\n\r");
					debugf(TEXT("[LIPSinc]: Unmatched Bone: %s"), *(DanglingBones(dbi)));
				}

				FString FailedMessage;

				if( DanglingBones.Num() > 0 )
				{
					FailedMessage = FString::Printf(TEXT("The .LBP file does not match mesh %s's bone structure.  Please pick another .LBP file.\n\n\rThe following %d bone(s) could not be matched (this list also appears in the Editor's log file):\n\n\r %s"),
						WorkMesh->GetName(), DanglingBones.Num(), ANSI_TO_TCHAR(boneList));
				}
				else
				{
					FailedMessage = FString::Printf(TEXT("The .LBP file appears to be in an obsolete format.  Please pick another .LBP file."));
				}

				// Clear the dangling bones list.
				DanglingBones.Empty();

				appMsgf( 0, *(FailedMessage) );
			}
			else
			{
				//!! FIXME For now, get the expression name from the file name.
				FString ExpressionName = FullName;

				while( ExpressionName.InStr( TEXT("\\") ) != -1 )
					ExpressionName = ExpressionName.Mid( ExpressionName.InStr( TEXT("\\") ) + 1, ExpressionName.Len() );

				if( ExpressionName.InStr( TEXT(".") ) != -1 )
					ExpressionName = ExpressionName.Left( ExpressionName.InStr( TEXT(".") ) );

				// Always delete this expression if it exists.  If it doesn't, nothing happens.
				WorkMesh->LIPSincController->DeleteExpression( ExpressionName );

				debugf(TEXT("[LIPSinc]: Importing Expression %s"), *(ExpressionName));

				TLIPSincExpressionInfo Expression;

				Expression.SetName( ExpressionName );

				for( INT i=0; i<TempBPI.NumBonePoses(); ++i )
				{
					Expression.AddPose( *(TempBPI.GetPose(i)) );
				}

				for( INT j=0; j<TempBPI.NumBonesPerPose(); ++j )
				{
					Expression.AddBone( TempBPI.GetBoneName(j) );
				}

				Expression.BuildBoneMap( WorkMesh, NULL );

                WorkMesh->LIPSincController->AddExpression( Expression );

				// Force the bones to be initialized.
				WorkMesh->LIPSincController->InitializeBones( CurrentMeshInstance()->CachedLIPSincBones );

				// Clear out the temporary set of bone poses.
				TempBPI.Clear();

				// We've made changes so we have to be dirty.
				WorkMesh->LIPSincController->SetDirty();

				debugf(TEXT("[LIPSinc]: done."));
			}
		}
		else
		{
			for( INT i=1; i < StringArray.Num(); ++i )
			{
				FullName = StringArray(0);
				FullName += TEXT("\\");
				FullName += StringArray(i);

				debugf(TEXT("[LIPSinc]: Parsing %s..."), FullName);

				FString Ext = FullName.Right( FullName.Len() - FullName.InStr( TEXT("."), 1 ) - 1 );

				if( Ext.Caps() != FString(TEXT("LBP")) )
				{
					FString ErrorMsg = FString::Printf(TEXT("The file %s is not a valid .LBP file!"), FullName);

					appMsgf( 0, *(ErrorMsg) );

					debugf(TEXT("[LIPSinc]: done."));

					StringArray.Empty();

					return;
				}

				// Helper object to parse .LBP files.
				TLIPSincLBPFileParser lbpParser;

				// Used to parse into, so we don't scrap the working poses when importing a set that doesn't link up.
				TLIPSincBonePoseInfo TempBPI;

				TLIPSincPhonemeMap PhonemeMap;

				// Bones that could not be linked up.
				TArray<FString> DanglingBones;

				// Parse the .LBP file.
				if( !lbpParser.Parse( FullName, &TempBPI, &PhonemeMap, WorkMesh->LIPSincController->GetLookAtInfo(), WorkMesh, &DanglingBones ) )
				{
					// This set of poses did not link up, so clear them.
					TempBPI.Clear();

					// Notify user of bones that could not be linked up.
					char boneList[8192] = "\0";

					debugf(TEXT("[LIPSinc]: There were %d unmatched bones."), DanglingBones.Num());

					for( INT dbi=0; dbi<DanglingBones.Num(); ++dbi )
					{
						strcat(boneList, "\t");
						strcat(boneList, TCHAR_TO_ANSI(*(DanglingBones(dbi))));
						strcat(boneList, "\n\r");
						debugf(TEXT("[LIPSinc]: Unmatched Bone: %s"), *(DanglingBones(dbi)));
					}

					FString FailedMessage;

					if( DanglingBones.Num() > 0 )
					{
						FailedMessage = FString::Printf(TEXT("The .LBP file does not match mesh %s's bone structure.  Please pick another .LBP file.\n\n\rThe following %d bone(s) could not be matched (this list also appears in the Editor's log file):\n\n\r %s"),
							WorkMesh->GetName(), DanglingBones.Num(), ANSI_TO_TCHAR(boneList));
					}
					else
					{
						FailedMessage = FString::Printf(TEXT("The .LBP file appears to be in an obsolete format.  Please pick another .LBP file."));
					}

					// Clear the dangling bones list.
					DanglingBones.Empty();

					appMsgf( 0, *(FailedMessage) );
				}
				else
				{
					//!! FIXME For now, get the expression name from the file name.
					FString ExpressionName = FullName;

					while( ExpressionName.InStr( TEXT("\\") ) != -1 )
						ExpressionName = ExpressionName.Mid( ExpressionName.InStr( TEXT("\\") ) + 1, ExpressionName.Len() );

					if( ExpressionName.InStr( TEXT(".") ) != -1 )
						ExpressionName = ExpressionName.Left( ExpressionName.InStr( TEXT(".") ) );

					// Always delete this expression if it exists.  If it doesn't, nothing happens.
					WorkMesh->LIPSincController->DeleteExpression( ExpressionName );

					debugf(TEXT("[LIPSinc]: Importing Expression %s"), *(ExpressionName));

					TLIPSincExpressionInfo Expression;

					Expression.SetName( ExpressionName );

					for( INT i=0; i<TempBPI.NumBonePoses(); ++i )
					{
						Expression.AddPose( *(TempBPI.GetPose(i)) );
					}

					for( INT j=0; j<TempBPI.NumBonesPerPose(); ++j )
					{
						Expression.AddBone( TempBPI.GetBoneName(j) );
					}

					Expression.BuildBoneMap( WorkMesh, NULL );

					WorkMesh->LIPSincController->AddExpression( Expression );

					// Force the bones to be initialized.
					WorkMesh->LIPSincController->InitializeBones( CurrentMeshInstance()->CachedLIPSincBones );

					// Clear out the temporary set of bone poses.
					TempBPI.Clear();

					// We've made changes so we have to be dirty.
					WorkMesh->LIPSincController->SetDirty();

					debugf(TEXT("[LIPSinc]: done."));
				}
			}
		}
	}

	unguard;
}

void WBrowserLIPSinc::OnLIPSincImportLBP( )
{
	guard(WBrowserLIPSinc::OnLIPSincImportLBP);

	UBOOL bAddedLIPSincController = 0;

	// Check to see if the current mesh has a controller attached to it.
	if ( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && !WorkMesh->LIPSincController)
	{
		// Add a LIPSinc controller to the database and hook it up to the current skeletal mesh.
		TLIPSincController theController;
		theController.SetName( WorkMesh->GetName() );
		GLIPSincDB.AddController( theController );
		bAddedLIPSincController = 1;
		WorkMesh->LIPSincController = GLIPSincDB.FindControllerByName( WorkMesh->GetName() );
	}

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass()) && WorkMesh->LIPSincController )
	{
		OPENFILENAMEA ofn;
		char File[8192] = "\0";
		
		ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFile = File;
		ofn.nMaxFile = sizeof(char) * 8192;
		
		ofn.lpstrFilter = "LBP Files (*.lbp)\0*.lbp\0\0";
		ofn.lpstrDefExt = "lbp";
		
		ofn.lpstrInitialDir = TCHAR_TO_ANSI( *(GLastDir[eLASTDIR_LBP]) );								
		
		ofn.lpstrTitle = "Select LBP file to import";
		ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER;
		
		if( GetOpenFileNameA(&ofn) )
		{
			INT NumNULLs = FormatFilenames( File );
			
			TArray<FString> StringArray;
			FString S = ANSI_TO_TCHAR( File );
			S.ParseIntoArray( TEXT("|"), &StringArray );
			
			INT iStart = 0;
			FString Prefix = TEXT("\0");
			
			if( NumNULLs )
			{
				iStart = 1;
				Prefix = *(StringArray(0));
				Prefix += TEXT("\\");
			}
			
			if( StringArray.Num() == 1 )
				GLastDir[eLASTDIR_LBP] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
			else
				GLastDir[eLASTDIR_LBP] = StringArray(0);
			
			FString FullName;
			
			if( StringArray.Num() > 1 )
			{
				FullName = StringArray(0);
				FullName += TEXT("\\");
				FullName += StringArray(1);
			}
			else
			{
				FullName = StringArray(0);
			}

			debugf(TEXT("[LIPSinc]: Parsing %s..."), FullName);

			FString Ext = FullName.Right( FullName.Len() - FullName.InStr( TEXT("."), 1 ) - 1 );

			if( Ext.Caps() != FString(TEXT("LBP")) )
			{
				FString ErrorMsg = FString::Printf(TEXT("The file %s is not a valid .LBP file!"), FullName);

				appMsgf( 0, *(ErrorMsg) );
				
				debugf(TEXT("[LIPSinc]: done."));
			
				StringArray.Empty();

				return;
			}
			
			// Helper object to parse .LBP files.
			TLIPSincLBPFileParser lbpParser;
			
			// Used to parse into, so we don't scrap the working poses when importing a set that doesn't link up.
			TLIPSincBonePoseInfo TempBPI;

			TLIPSincPhonemeMap PhonemeMap;

			// Bones that could not be linked up.
			TArray<FString> DanglingBones;

			// Parse the .LBP file.
			if( !lbpParser.Parse( FullName, &TempBPI, &PhonemeMap, WorkMesh->LIPSincController->GetLookAtInfo(), WorkMesh, &DanglingBones ) )
			{
				// This set of poses did not link up, so clear them.
				TempBPI.Clear();
				
				// If we just added a LIPSinc controller to this mesh (ie this is the first attempt to import poses),
				// we must delete the controller from the LIPSinc database and unlink it with this mesh.
				if( bAddedLIPSincController )
				{
					INT lci = GLIPSincDB.GetControllerIndex( WorkMesh->GetName() );
					GLIPSincDB.DeleteController( lci );
					WorkMesh->LIPSincController = NULL;
				}

				// Notify user of bones that could not be linked up.
				char boneList[8192] = "\0";

				debugf(TEXT("[LIPSinc]: There were %d unmatched bones."), DanglingBones.Num());

				for( INT dbi=0; dbi<DanglingBones.Num(); ++dbi )
				{
					strcat(boneList, "\t");
					strcat(boneList, TCHAR_TO_ANSI(*(DanglingBones(dbi))));
					strcat(boneList, "\n\r");
					debugf(TEXT("[LIPSinc]: Unmatched Bone: %s"), *(DanglingBones(dbi)));
				}

				
				FString FailedMessage;
					
				if( DanglingBones.Num() > 0 )
				{
					FailedMessage = FString::Printf(TEXT("The .LBP file does not match mesh %s's bone structure.  Please pick another .LBP file.\n\n\rThe following %d bone(s) could not be matched (this list also appears in the Editor's log file):\n\n\r %s"),
						WorkMesh->GetName(), DanglingBones.Num(), ANSI_TO_TCHAR(boneList));
				}
				else
				{
					FailedMessage = FString::Printf(TEXT("The .LBP file appears to be in an obsolete format.  Please pick another .LBP file."));
				}

				// Clear the dangling bones list.
				DanglingBones.Empty();

				appMsgf( 0, *(FailedMessage) );
			}
			else
			{
				// This set of poses did link up, so clear the current set
				WorkMesh->LIPSincController->BonePoseInfo()->Clear();

				// Make the temp poses the current set
				// Copy the poses
				for( INT i=0; i<TempBPI.NumBonePoses(); ++i )
				{
					WorkMesh->LIPSincController->BonePoseInfo()->AddPose( *(TempBPI.GetPose(i)) );
				}

				// Copy the bone names
				for( INT j=0; j<TempBPI.NumBonesPerPose(); ++j )
				{
					WorkMesh->LIPSincController->BonePoseInfo()->AddBone( TempBPI.GetBoneName(j) );
				}

				// Force the bone map to be rebuilt (we don't care about the dangling bones this time, since we've already
				// verified that there are none).
				WorkMesh->LIPSincController->BonePoseInfo()->BuildBoneMap( WorkMesh, NULL );

				// Force the bones to be initialized.
				WorkMesh->LIPSincController->InitializeBones( CurrentMeshInstance()->CachedLIPSincBones );

				// Save the mapping file
				FString MappingFilename = FString(appBaseDir()) + FString(TEXT("LIPSincData/Mappings/")) + WorkMesh->GetName() +
										  FString(TEXT(".map"));

				PhonemeMap.SaveToDisk( MappingFilename, WorkMesh->GetName() );

				// Clear out the temporary set of bone poses.
				TempBPI.Clear();

				// We've made changes so we have to be dirty.
				WorkMesh->LIPSincController->SetDirty();
			}
			
			debugf(TEXT("[LIPSinc]: done."));
			
			StringArray.Empty();
		}
		else
		{
			// If we just added a LIPSinc controller to this mesh (ie this is the first attempt to import poses),
			// we must delete the controller from the LIPSinc database and unlink it with this mesh.
			if( bAddedLIPSincController )
			{
				INT lci = GLIPSincDB.GetControllerIndex( WorkMesh->GetName() );
				GLIPSincDB.DeleteController( lci );
				WorkMesh->LIPSincController = NULL;
			}
		}
	}
	unguard;
}

// Need to account for possible mesh deletion
void WBrowserLIPSinc::OnPaint( )
{
	guard(WBrowserLIPSinc::OnPaint);

	OnPackageSelectionChange();
	WBrowser::OnPaint();

	unguard;
}

#endif