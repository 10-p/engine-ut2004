/*=============================================================================
	Filer.cpp: Unreal installer/filer.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#define SHOWLOG 0

// System includes.
#pragma warning( disable : 4201 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "Res\resource.h"

// Unreal includes.
#include "SetupPrivate.h"
#include "Window.h"
#include "USetupDefinitionWindows.h"

// for CD key validation
#define ENGINE_API
#include "../../Engine/Inc/UnCDKey.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Package implementation.
IMPLEMENT_PACKAGE(Setup)

// Functions.
class WWizardPage* NewAutoPlayPage( class WFilerWizard* InOwner, UBOOL ShowInstallOptions );
class FInstallPoll GNoPoll;

// Memory allocator.
#include "FMallocWindows.h"
FMallocWindows Malloc;

// Error handler.
#include "FOutputDeviceWindowsError.h"
FOutputDeviceWindowsError Error;

#if SHOWLOG
// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;
#endif

// Feedback.
#include "FFeedbackContextWindows.h"
FFeedbackContextWindows Warn;

// File manager.
#include "FFileManagerWindows.h"
FFileManagerWindows FileManager;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

// HRESULT checking.
#define verifyHRESULT(fn) {HRESULT hRes=fn; if( hRes!=S_OK ) appErrorf( TEXT(#fn) TEXT(" failed (%08X)"), hRes );}
#define verifyHRESULTSlow(fn) if(fn){}

void regSet( HKEY Base, FString Dir, FString Key, FString Value )
{
	guard(regSet);
	HKEY hKey = NULL;
	// gam ---
	verify(RegCreateKeyX( Base, *Dir, &hKey )==ERROR_SUCCESS);
	verify(RegSetValueExX( hKey, *Key, 0, REG_SZ, (BYTE*)*Value, (Value.Len()+1)*sizeof(TCHAR) )==ERROR_SUCCESS);
	verify(RegCloseKey( hKey )==ERROR_SUCCESS);
	// --- gam
	unguard;
}
UBOOL regGet( HKEY Base, FString Dir, FString Key, FString& Str )
{
	guard(regGetFString);
	HKEY hKey = NULL;
	if( RegOpenKeyX( Base, *Dir, &hKey )==ERROR_SUCCESS )
	{
		TCHAR Buffer[4096]=TEXT("");
		DWORD Type=REG_SZ, BufferSize=sizeof(Buffer);
		if
		(	RegQueryValueExX( hKey, *Key, 0, &Type, (BYTE*)Buffer, &BufferSize )==ERROR_SUCCESS
		&&	Type==REG_SZ )
		{
			Str = Buffer;
			return 1;
		}
	}
	Str = TEXT("");
	return 0;
	unguard;
}

// gam ---
typedef BOOL (WINAPI *pfnGetDiskFreeSpaceEx)( LPCSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes );

SQWORD FreeSpace( const TCHAR* Folder )
{
	guard(FreeSpace);

    SQWORD rc = 0;
	
	if( !appStrlen(Folder) || !appIsAlpha(Folder[0]) )
	    return( rc );
	    
	pfnGetDiskFreeSpaceEx pGetDiskFreeSpaceEx = NULL;
    HINSTANCE KernLib = NULL;
    
    KernLib = TCHAR_CALL_OS(LoadLibraryW(TEXT("kernel32.dll")),LoadLibraryA("kernel32.dll"));
    
    if( KernLib )
        pGetDiskFreeSpaceEx = (pfnGetDiskFreeSpaceEx)GetProcAddress( KernLib, "GetDiskFreeSpaceExA" );

    if( pGetDiskFreeSpaceEx )
    {
        char Root[]= "C:\\";
        Root[0] = char( Folder[0] );
        ULARGE_INTEGER lFreeBytesAvailableToCaller;
        ULARGE_INTEGER lTotalNumberOfBytes;
        ULARGE_INTEGER lTotalNumberOfFreeBytes;
        
        if( pGetDiskFreeSpaceEx( Root, &lFreeBytesAvailableToCaller, &lTotalNumberOfBytes, &lTotalNumberOfFreeBytes ) )
            rc = lFreeBytesAvailableToCaller.QuadPart;
    }		
    
    if( rc == 0 )
	{
		TCHAR Root[]=TEXT("C:") PATH_SEPARATOR;
		Root[0] = Folder[0];

		DWORD SectorsPerCluster=0, BytesPerSector=0, FreeClusters=0, TotalClusters=0;
		GetDiskFreeSpaceX( Root, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters );
		rc = (SQWORD)BytesPerSector * (SQWORD)SectorsPerCluster * (SQWORD)FreeClusters;
	}
	
	if( KernLib )
		FreeLibrary( KernLib );

    return( rc );

	unguard;
}
// --- gam

//
// Remove a directory if it's empty. Returns error.
//
static UBOOL IsDrive( const TCHAR* Path )
{
	if( appStricmp(Path,TEXT(""))==0 )
		return 1;
	else if( appToUpper(Path[0])!=appToLower(Path[0]) && Path[1]==':' && Path[2]==0 )
		return 1;
	else if( appStricmp(Path,TEXT("\\"))==0 )
		return 1;
	else if( appStricmp(Path,TEXT("\\\\"))==0 )
		return 1;
	else if( Path[0]=='\\' && Path[1]=='\\' && !appStrchr(Path+2,'\\') )
		return 1;
	else if( Path[0]=='\\' && Path[1]=='\\' && appStrchr(Path+2,'\\') && !appStrchr(appStrchr(Path+2,'\\')+1,'\\') )
		return 1;
	else
		return 0;
}
UBOOL RemoveEmptyDirectory( FString Dir )
{
	for( ; ; )
	{
		if( Dir.Right(1)==PATH_SEPARATOR )
			Dir = Dir.LeftChop(1);
		if( IsDrive(*Dir) )
			break;
		TArray<FString> List = GFileManager->FindFiles( *(Dir * TEXT("*")), 1, 1 );
		if( List.Num() )
			break;
		if( !GFileManager->DeleteDirectory( *Dir, 1, 0 ) )
			return 0;
		while( Dir.Len() && Dir.Right(1)!=PATH_SEPARATOR )
			Dir = Dir.LeftChop(1);
	}
	return 1;
}
void LocalizedFileError( const TCHAR* Key, const TCHAR* AdviceKey, const TCHAR* Filename )
{
	guard(LocalizedError);
	appErrorf( NAME_FriendlyError, *FString::Printf(TEXT("%s: %s (%s)\n\n%s"),LocalizeError(Key,TEXT("Setup")),Filename,appGetSystemErrorMessage(),LocalizeError(AdviceKey,TEXT("Setup"))) );
	unguard;
}

/*-----------------------------------------------------------------------------
	Install wizard.
-----------------------------------------------------------------------------*/

// Filer wizard.
class WFilerWizard : public WWizardDialog
{
	DECLARE_WINDOWCLASS(WFilerWizard,WWizardDialog,Setup)

	// Config info.
	WLabel LogoStatic;
	FWindowsBitmap LogoBitmap;
	USetupDefinitionWindows* Manager;

	// Constructor.
	WFilerWizard()
	:	LogoStatic		( this, IDC_Logo )
	,   Manager         ( new(UObject::CreatePackage(NULL,MANIFEST_FILE), TEXT("Setup"))USetupDefinitionWindows(this) )
	{
		guard(WFilerWizard::WFilerWizard);
		Manager->Init();
		if( Manager->Uninstalling )
			Manager->CreateRootGroup();
		unguard;
	}

	// WWindow interface.
	void OnInitDialog()
	{
		guard(WFilerWizard::OnInitDialog);

		// Dialog init.
		WWizardDialog::OnInitDialog();
		SendMessageX( *this, WM_SETICON, ICON_BIG, (WPARAM)LoadIconIdX(hInstance,IDICON_Setup1) );
		if( Manager->Logo==TEXT("") || !Manager->LocateSourceFile( Manager->Logo, 0, 0, 0 ) )
		{
			Manager->Logo = TEXT("..\\Help\\InstallerLogo.bmp");//!!for setup
			if( GFileManager->FileSize(*Manager->Logo)<=0 )
				Manager->Logo = TEXT("InstallerLogo.bmp");//!!for uninstaller / cdinstall
		}
		LogoBitmap.LoadFile( *Manager->Logo );
		SendMessageX( LogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)LogoBitmap.GetBitmapHandle() );

		// Windows init.
		CoInitialize(NULL);

		unguard;
	}

	// WFilerWizard interface.
	void OnFinish()
	{
		guard(WFilerWizard::OnFinish);
		WWizardDialog::OnFinish();
		Manager->PreExit();
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Product information.
-----------------------------------------------------------------------------*/

// Product information box.
class WProductInfo : public WDialog
{
	DECLARE_WINDOWCLASS(WProductInfo,WDialog,Setup)

	// Variables.
	USetupProduct* SetupProduct;
	WLabel ProductText;
	WLabel VersionText;
	WLabel DeveloperText;
	WButton ProductHolder;
	WUrlButton Product;
	WUrlButton Version;
	WUrlButton Developer;

	// Constructor.
	WProductInfo( WWindow* InOwner, USetupDefinition* Manager, USetupProduct* InSetupProduct )
	: WDialog		( TEXT("ProductInfo"), IDDIALOG_ProductInfo, InOwner )
	, SetupProduct  ( InSetupProduct )
	, ProductText   ( this, IDC_ProductText )
	, VersionText   ( this, IDC_VersionText )
	, DeveloperText ( this, IDC_DeveloperText )
	, ProductHolder ( this, IDC_ProductHolder )
	, Product       ( this, TEXT(""), IDC_Product   )
	, Version       ( this, TEXT(""), IDC_Version   )
	, Developer     ( this, TEXT(""), IDC_Developer )
	{}

	// WDialog interface.
	void LanguageChange()
	{
		guard(WProductInfo::LanguageChange);
		//!!super::languagechange

		// Product text.
		Product  .URL    =  SetupProduct->ProductURL;
		Version  .URL    =  SetupProduct->VersionURL;
		Developer.URL    =  SetupProduct->DeveloperURL;
		Product  .SetText( *SetupProduct->LocalProduct );
		Developer.SetText( *SetupProduct->Developer    );
		Version  .SetText( *SetupProduct->Version      );

		// General text.
		ProductText   .SetText(Localize(TEXT("IDDIALOG_ProductInfo"),TEXT("IDC_ProductText"),TEXT("Setup")));
		VersionText   .SetText(Localize(TEXT("IDDIALOG_ProductInfo"),TEXT("IDC_VersionText"),TEXT("Setup")));
		DeveloperText .SetText(Localize(TEXT("IDDIALOG_ProductInfo"),TEXT("IDC_DeveloperText"),TEXT("Setup")));
		ProductHolder .SetText(Localize(TEXT("IDDIALOG_ProductInfo"),TEXT("IDC_ProductHolder"),TEXT("Setup")));

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Failed requirement.
-----------------------------------------------------------------------------*/

// A password dialog box.
class WFailedRequirement : public WDialog
{
	DECLARE_WINDOWCLASS(WFailedRequirement,WDialog,Setup)

	// Controls.
	WCoolButton OkButton;
	WProductInfo ProductInfo;
	WLabel FailedText;
	FString Title;
	FString Message;

	// Constructor.
	WFailedRequirement( WWindow* InOwnerWindow, USetupDefinition* Manager, USetupProduct* InProduct, const TCHAR* InTitle, const TCHAR* InMessage )
	: WDialog	  ( TEXT("FailedRequirement"), IDDIALOG_FailedRequirement, InOwnerWindow )
	, FailedText  ( this, IDC_FailedMessage )
	, OkButton    ( this, IDOK, FDelegate(this,(TDelegate)EndDialogTrue) )
	, ProductInfo ( this, Manager, InProduct )
	, Title       ( InTitle )
	, Message     ( InMessage )
	{}
	void OnInitDialog()
	{
		guard(WFailedRequirement::OnInitDialog);
		WDialog::OnInitDialog();
		SetText( *Title );
		ProductInfo.OpenChildWindow( IDC_ProductInfoHolder, 1 );
		ProductInfo.LanguageChange();
		ProductInfo.ProductHolder.SetText( Localize(TEXT("IDDIALOG_FailedRequirement"),TEXT("IDC_ProductHolder"),TEXT("Setup")) );
		FailedText.SetText( LineFormat(*Message) );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Install components.
-----------------------------------------------------------------------------*/

// Base of components page.
class WFilerPageComponentsBase : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageComponentsBase,WWizardPage,Setup)
	WFilerWizard* Owner;
	WFilerPageComponentsBase( const TCHAR* InText, INT InID, WFilerWizard* InOwner )
	: WWizardPage( InText, InID, InOwner )
	, Owner( InOwner )
	{}
	virtual void OnGroupChange( class FComponentItem* Group )=0;
};

// An component list item.
class FComponentItem : public FHeaderItem
{
public:
	// Variables.
	WFilerPageComponentsBase* OwnerComponents;
	USetupGroup* SetupGroup;
	UBOOL Forced;

	// Constructors.
	FComponentItem( WFilerPageComponentsBase* InOwnerComponents, USetupGroup* InSetupGroup, WPropertiesBase* InOwnerProperties, FTreeItem* InParent )
	: FHeaderItem		( InOwnerProperties, InParent, 1 )
	, OwnerComponents	( InOwnerComponents )
	, SetupGroup        ( InSetupGroup )
	, Forced            ( 0 )
	{
		guard(FComponentItem::FComponentItem);

		// Get subgroups.
		Sorted = SetupGroup->Manager->Uninstalling;
		for( TArray<USetupGroup*>::TIterator It(SetupGroup->Subgroups); It; ++It )
			if( (*It)->Visible )
				Children.AddItem( new(TEXT("FComponentItem"))FComponentItem(OwnerComponents,*It,OwnerProperties,this) );
		Expandable = Children.Num()>0;

		unguard;
	}
	FRect GetCheckboxRect()
	{
		guard(FComponentItem::GetCheckboxRect);
		return FRect(GetRect()-GetRect().Min).Right(16).Inner(FPoint(0,1))+FPoint(0,1);
		unguard;
	}

	// FTreeItem interface.
	UBOOL Greyed()
	{
		guard(FComponentItem::Greyed);
		if( SetupGroup->Manager->Uninstalling )
			return Forced;
		else
			for( FComponentItem* Item=(FComponentItem*)Parent; Item; Item=(FComponentItem*)Item->Parent )
				if( !Item->SetupGroup->Selected )
					return 1;
		return 0;
		unguard;
	}
	void Draw( HDC hDC )
	{
		guard(FComponentItem::Draw);
		FHeaderItem::Draw( hDC );
		DWORD GreyedFlags = SetupGroup->Manager->Uninstalling ? (DFCS_INACTIVE|DFCS_CHECKED) : (DFCS_INACTIVE);
		DrawFrameControl( hDC, GetCheckboxRect()+GetRect().Min, DFC_BUTTON, DFCS_BUTTONCHECK|(Greyed()?GreyedFlags:0)|(SetupGroup->Selected?DFCS_CHECKED:0) );
		unguard;
	}
	void ToggleSelection()
	{
		guard(FTreeItem::ToggleSelection);
		if( SetupGroup->Optional && !Greyed() )
		{
			SetupGroup->Selected = !SetupGroup->Selected;
			OwnerComponents->OnGroupChange( this );
			InvalidateRect( OwnerProperties->List, NULL, 0 );
			UpdateWindow( OwnerProperties->List );
		}
		unguard;
	}
	void OnItemDoubleClick()
	{
		guard(FTreeItem::OnItemDoubleClick);
		ToggleSelection();
		unguard;
	}
	void OnItemLeftMouseDown( FPoint P )
	{
		guard(FComponentItem::OnItemLeftMouseDown);
		if( GetCheckboxRect().Inner(FPoint(-1,-1)).Contains(P) )
			ToggleSelection();
		else
			FHeaderItem::OnItemLeftMouseDown( P );
		unguard;
	}
	void OnItemSetFocus()
	{
		guard(FComponentItem::OnItemSetFocus);
		FHeaderItem::OnItemSetFocus();
		OwnerComponents->OnGroupChange( this );
		unguard;
	}
	QWORD GetId() const
	{
		guard(FComponentItem::GetId);
		return (INT)this;
		unguard;
	}
	virtual FString GetCaption() const
	{
		guard(FComponentItem::GetText);
		return SetupGroup->Caption;
		unguard;
	}
};

// Group properties.
class WComponentProperties : public WProperties
{
	DECLARE_WINDOWCLASS(WComponentProperties,WProperties,Setup)
	FComponentItem Root;
	WComponentProperties( WFilerPageComponentsBase* InComponents )
	: WProperties( NAME_None, InComponents )
	, Root( InComponents, InComponents->Owner->Manager->RootGroup, this, NULL )
	{
		ShowTreeLines = 0;
	}
	FTreeItem* GetRoot()
	{
		return &Root;
	}
};

/*-----------------------------------------------------------------------------
	Install wizard pages.
-----------------------------------------------------------------------------*/

// Progess.
class WFilerPageProgress : public WWizardPage, public FInstallPoll
{
	DECLARE_WINDOWCLASS(WFilerPageProgress,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel InstallText;
	WLabel InstallingText;
	WLabel ProgressText;
	WLabel TotalText;
	WLabel Installing;
	WProgressBar Progress;
	WProgressBar Total;
	UBOOL Finished;
	UBOOL CancelFlag;

	// Constructor.
	WFilerPageProgress( WFilerWizard* InOwner, const TCHAR* Template )
	: WWizardPage   ( Template, IDDIALOG_FilerPageProgress, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, InstallText   ( this, IDC_InstallText )
	, InstallingText( this, IDC_InstallingText )
	, ProgressText  ( this, IDC_ProgressText )
	, TotalText     ( this, IDC_TotalText )
	, Installing    ( this, IDC_Installing )
	, Progress      ( this, IDC_Progress )
	, Total         ( this, IDC_Total )
	, Finished      ( 0 )
	, CancelFlag	( 0 )
	{}

	// FInstallPoll interface.
	UBOOL Poll( const TCHAR* Label, SQWORD LocalBytes, SQWORD LocalTotal, SQWORD RunningBytes, SQWORD TotalBytes )
	{
		guard(WWizardPageProgress::Poll);
		static TCHAR Saved[256]=TEXT("");
		if( appStricmp(Label,Saved)!=0 )
		{
			Installing.SetText( Label );
			appStrcpy( Saved, Label );
		}
		Progress.SetProgress( LocalBytes, LocalTotal );
		Total.SetProgress( RunningBytes, TotalBytes );
		MSG Msg;
		while( PeekMessageX(&Msg,NULL,0,0,1) )
			DispatchMessageX(&Msg);
		UpdateWindow( *this );
		if( CancelFlag )
			if( MessageBox( *OwnerWindow, *FString::Printf(LocalizeGeneral(TEXT("CancelPrompt"),TEXT("Setup")),*Manager->LocalProduct), LocalizeGeneral(TEXT("InstallCancel"),TEXT("Setup")), MB_YESNO )==IDYES )
				return 0;
		CancelFlag = 0;
		return 1;
		unguard;
	}
	const TCHAR* GetBackText()
	{
		return NULL;
	}
	void OnCancel()
	{
		CancelFlag = 1;
	}
	const TCHAR* GetNextText()
	{
		return Finished ? WWizardPage::GetNextText() : NULL;
	}
};

// Install progess.
class WFilerPageInstallProgress : public WFilerPageProgress
{
	DECLARE_WINDOWCLASS(WFilerPageInstallProgress,WFilerPageProgress,Setup)
	WFilerPageInstallProgress( WFilerWizard* InOwner )
	: WFilerPageProgress( InOwner, TEXT("FilerPageInstallProgress") )
	{}
	WWizardPage* GetNext()
	{
		guard(WFilerPageInstallProgress::GetNext);
		return NewAutoPlayPage( Owner, 0 );
		unguard;
	}
	void OnCurrent()
	{
		guard(WFilerPageInstallProgress::OnCurrent);
		UpdateWindow( *this );
		Manager->DoInstallSteps( this );
		Finished = 1;
		Owner->OnNext();
		unguard;
	}
};

// Done.
class WFilerPagePreInstall : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPagePreInstall,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel Message;

	// Constructor.
	WFilerPagePreInstall( WFilerWizard* InOwner )
	: WWizardPage   ( TEXT("FilerPagePreInstall"), IDDIALOG_FilerPagePreInstall, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, Message       ( this, IDC_Message )
	{}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		return new WFilerPageInstallProgress( Owner );
	}
	const TCHAR* GetNextText()
	{
		return LocalizeGeneral("InstallButton",TEXT("Window"));
	}
	void OnInitDialog()
	{
		guard(WFilerPagePreInstall::OnInitDialog);
		WWizardPage::OnInitDialog();
		Message.SetText( *FString::Printf( LineFormat(LocalizeGeneral(TEXT("Ready"),TEXT("Setup"))), *Manager->LocalProduct, *Manager->DestPath, *Manager->LocalProduct ) );
		unguard;
	}
};

// Folder.
class WFilerPageCdFolder : public WWizardPage, public FControlSnoop
{
	DECLARE_WINDOWCLASS(WFilerPageCdFolder,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel FolderDescription;
	WLabel SpaceAvailable;
	WLabel SpaceAvailableMsg;
	WLabel SpaceRequired;
	WLabel SpaceRequiredMsg;
	WButton FolderHolder;
	WCoolButton DefaultButton;
	WEdit Folder;

	// Constructor.
	WFilerPageCdFolder( WFilerWizard* InOwner )
	: WWizardPage      ( TEXT("FilerPageFolder"), IDDIALOG_FilerPageFolder, InOwner )
	, Owner            ( InOwner )
	, Manager          ( InOwner->Manager )
	, FolderDescription( this, IDC_FolderDescription )
	, FolderHolder     ( this, IDC_FolderHolder )
	, DefaultButton    ( this, IDC_Default, FDelegate(this,(TDelegate)OnReset) )
	, Folder           ( this, IDC_Folder )
	, SpaceAvailable   ( this, IDC_SpaceAvailable )
	, SpaceAvailableMsg( this, IDC_SpaceAvailableMessage )
	, SpaceRequired    ( this, IDC_SpaceRequired )
	, SpaceRequiredMsg ( this, IDC_SpaceRequiredMessage )
	{
		guard(WFilerPageCdFolder::WFilerPageCdFolder);
		Manager->RefPath = TEXT("");
		unguard;
	}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		guard(WFilerPageCdFolder::GetNext);
		FString Saved      = Manager->RefPath;
		Manager->CdOk      = TEXT("");
		Manager->RefPath   = Folder.GetText();
		Manager->InstallTree( TEXT("ProcessVerifyCd"), &GNoPoll, USetupDefinition::ProcessVerifyCd );
		if( Manager->CdOk!=TEXT("") )
		{
			MessageBox( *Owner, *FString::Printf(LineFormat(LocalizeError(TEXT("WrongCd"),TEXT("Setup"))),*Manager->Product,*Manager->CdOk), LocalizeError(TEXT("WrongCdTitle"),TEXT("Setup")), MB_OK );
			Manager->RefPath = Saved;
			return NULL;
		}
		return new WFilerPagePreInstall( Owner );
		unguard;
	}
	UBOOL GetShow()
	{
		guard(WFilerPageCdFolder::GetShow);

#if DEMOVERSION
		// This is a hack!!
		Manager->RefPath = Manager->DestPath;
		Manager->AnyRef = 0;
		Manager->InstallTree( TEXT("ProcessCheckRef"), &GNoPoll, USetupDefinition::ProcessCheckRef );
		return 0;
#endif

		// Only show if there are installable files stored as deltas relative to version on CD.
		Manager->AnyRef = 0;
		Manager->InstallTree( TEXT("ProcessCheckRef"), &GNoPoll, USetupDefinition::ProcessCheckRef );
		return Manager->AnyRef;

		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageCdFolder::OnInitDialog);
		WWizardPage::OnInitDialog();
		SpaceAvailable.Show(0);
		SpaceAvailableMsg.Show(0);
		SpaceRequired.Show(0);
		SpaceRequiredMsg.Show(0);
		FolderHolder.SetText( LocalizeGeneral(TEXT("CdDrive"),TEXT("Setup")) );
		FolderDescription.SetText( *FString::Printf(LineFormat(LocalizeGeneral(TEXT("CdDescription"),TEXT("Setup"))),*Manager->LocalProduct,*Manager->Product,*Manager->LocalProduct) );
		OnReset();
		unguard;
	}
	void OnReset()
	{
		guard(WFilerPageCdFolder::OnReset);
#if DEMOVERSION
		Folder.SetText( *Manager->DestPath );
#else
		Folder.SetText( TEXT("D:") PATH_SEPARATOR );
		TCHAR Str[4] = TEXT("A:") PATH_SEPARATOR;
		for( TCHAR Ch='A'; Ch<='Z'; Ch++ )
		{
			Str[0] = Ch;
			if( GetDriveTypeX(Str)==DRIVE_CDROM )
			{
				Folder.SetText( Str );
				break;
			}
		}
#endif
		unguard;
	}
};

// Components.
class WFilerPageComponents : public WFilerPageComponentsBase
{
	DECLARE_WINDOWCLASS(WFilerPageComponents,WFilerPageComponentsBase,Setup)

	// Variables.
	USetupDefinition* Manager;
	WButton DescriptionFrame;
	WButton DiskSpaceFrame;
	WLabel ComponentsDescription;
	WLabel ComponentsPrompt;
	WLabel DescriptionText;
	WLabel SpaceRequiredMessage;
	WLabel SpaceAvailableMessage;
	WLabel SpaceRequired;
	WLabel SpaceAvailable;
	WComponentProperties Components;

	// Constructor.
	WFilerPageComponents( WFilerWizard* InOwner )
	: WFilerPageComponentsBase( TEXT("FilerPageComponents"), IDDIALOG_FilerPageComponents, InOwner )
	, Manager              ( InOwner->Manager )
	, ComponentsDescription( this, IDC_ComponentsDescription )
	, ComponentsPrompt     ( this, IDC_ComponentsPrompt )
	, DiskSpaceFrame       ( this, IDC_DiskSpaceFrame )
	, DescriptionFrame     ( this, IDC_DescriptionFrame )
	, DescriptionText      ( this, IDC_DescriptionText )
	, SpaceRequiredMessage ( this, IDC_SpaceRequiredMessage )
	, SpaceAvailableMessage( this, IDC_SpaceAvailableMessage )
	, SpaceRequired        ( this, IDC_SpaceRequired )
	, SpaceAvailable       ( this, IDC_SpaceAvailable )
	, Components		   ( this )
	{
		Components.ShowTreeLines = 0;
	}

	// Functions.
	void OnGroupChange( class FComponentItem* Group )
	{
		guard(WFilerPageComponents::OnGroupChange);

		// Update space required.
		Manager->RequiredSpace = PER_INSTALL_OVERHEAD + Manager->RootGroup->SpaceRequired();
		SpaceAvailable.SetText( *FString::Printf(LocalizeGeneral(TEXT("Space"),TEXT("Setup")), FreeSpace(*Manager->DestPath)/(1024*1024) ) );
		SpaceRequired .SetText( *FString::Printf(LocalizeGeneral(TEXT("Space"),TEXT("Setup")), Manager->RequiredSpace/(1024*1024) ) );

		// Update description text.
		DescriptionText.SetText( Group ? *Group->SetupGroup->Description : TEXT("") );

		unguard;
	}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		guard(WFilerPageComponents::GetNext);
		if( FreeSpace(*Manager->DestPath) < Manager->RequiredSpace )
		{
			TCHAR Root[]=TEXT("C:") PATH_SEPARATOR;
			Root[0] = (*Manager->DestPath)[0];
			MessageBox( *Owner, *FString::Printf(LineFormat(LocalizeGeneral(TEXT("NotEnoughSpace"),TEXT("Setup"))),Root,*Manager->LocalProduct), LocalizeGeneral(TEXT("NotEnoughSpaceTitle"),TEXT("Setup")), MB_OK );
			return NULL;
		}
		return new WFilerPageCdFolder(Owner);
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageComponents::OnInitDialog);
		WWizardPage::OnInitDialog();
		Components.OpenChildWindow( IDC_ComponentsHolder );

		OnGroupChange( NULL );
		Components.GetRoot()->Expand();
		Components.ResizeList();
		Components.List.SetCurrent( 0, 1 );
		Components.SetItemFocus( 1 );

		unguard;
	}
	UBOOL GetShow()
	{
		return Components.Root.SetupGroup->Visible;
	}
};

// Folder.
class WFilerPageFolder : public WWizardPage, public FControlSnoop
{
	DECLARE_WINDOWCLASS(WFilerPageFolder,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel FolderDescription;
	WLabel SpaceAvailable;
	WLabel SpaceRequired;
	WButton FolderHolder;
	WCoolButton DefaultButton;
	WEdit Folder;

	// Constructor.
	WFilerPageFolder( WFilerWizard* InOwner )
	: WWizardPage      ( TEXT("FilerPageFolder"), IDDIALOG_FilerPageFolder, InOwner )
	, Owner            ( InOwner )
	, Manager          ( InOwner->Manager )
	, FolderDescription( this, IDC_FolderDescription )
	, FolderHolder     ( this, IDC_FolderHolder )
	, DefaultButton    ( this, IDC_Default, FDelegate(this,(TDelegate)OnReset) )
	, Folder           ( this, IDC_Folder )
	, SpaceAvailable   ( this, IDC_SpaceAvailable )
	, SpaceRequired    ( this, IDC_SpaceRequired )
	{
		guard(WFilerPageFolder::WFilerPageFolder);
		Manager->DestPath = Manager->RegistryFolder;
		unguard;
	}

	// WWizardPage interface.
	void Update()
	{
		guard(WFilerPageFolder::Update);
		SpaceAvailable.SetText( *FString::Printf(LocalizeGeneral(TEXT("Space"),TEXT("Setup")), FreeSpace(*Folder.GetText())/(1024*1024) ) );
		SpaceRequired .SetText( *FString::Printf(LocalizeGeneral(TEXT("Space"),TEXT("Setup")), Manager->RequiredSpace/(1024*1024) ) );
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageFolder::GetNext);

		// Get folder name.
		FString NewFolder = Folder.GetText();
		if( NewFolder.Right(1)==PATH_SEPARATOR )
			NewFolder = NewFolder.LeftChop(1);

		// Make sure all requirements are met.
		USetupProduct* RequiredProduct=NULL;
		FString FailMessage;
		if( !Manager->CheckAllRequirements(Folder.GetText(),RequiredProduct,FailMessage) )
		{
			FString Title = FString::Printf( LocalizeError(TEXT("MissingProductTitle"),TEXT("Setup")), *Manager->LocalProduct, Manager->Patch ? LocalizeError(TEXT("MissingProductPatched"),TEXT("Setup")) : LocalizeError(TEXT("MissingProductInstalled"),TEXT("Setup")) );
			WFailedRequirement( OwnerWindow, Manager, RequiredProduct, *Title, *FailMessage ).DoModal();
			return NULL;
		}

		// Try to create folder.
		if
		(	NewFolder.Len()>=4
		&&	appIsAlpha((*NewFolder)[0])
		&&	(*NewFolder)[1]==':'
		&&	(*NewFolder)[2]==PATH_SEPARATOR[0] )
		{
			// Attempt to create the folder.
			if( NewFolder.Right(1)==PATH_SEPARATOR )
				NewFolder = NewFolder.LeftChop(1);
			if( GFileManager->MakeDirectory( *NewFolder, 1 ) )
			{
				Manager->DestPath = NewFolder;
				Manager->CreateRootGroup();
				return new WFilerPageComponents( Owner );
			}
		}
		FString Title = FString::Printf( LocalizeError(TEXT("FolderTitle"),TEXT("Setup")), *NewFolder );
		FString Msg   = FString::Printf( LocalizeError(TEXT("FolderFormat"),TEXT("Setup")), *NewFolder );
		MessageBox( *Owner, *Msg, *Title, MB_OK );
		OnReset();
		return NULL;
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageFolder::OnInitDialog);
		WWizardPage::OnInitDialog();
		FolderDescription.SetText( *FString::Printf(LineFormat(Localize(TEXT("IDDIALOG_FilerPageFolder"),Manager->Patch ? TEXT("IDC_FolderDescriptionPatch") : TEXT("IDC_FolderDescription"),TEXT("Setup"))), Manager->LocalProduct ) );
		OnReset();
		Folder.ChangeDelegate=FDelegate(this,(TDelegate)OnChange);
		unguard;
	}
	void OnChange()
	{
		guard(WFilerPageFolder::OnChange);
		Update();
		unguard;
	}
	void OnReset()
	{
		guard(WFilerPageFolder::OnReset);
		Folder.SetText( *Manager->RegistryFolder );
		Update();
		unguard;
	}
};

class WFilerPageCDKey : public WWizardPage, public FControlSnoop
{
	DECLARE_WINDOWCLASS(WFilerPageCDKey,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WButton CDKeyHolder;
	WLabel CDKeyDescription;
	WEdit CDKey;

	// Constructor.
	WFilerPageCDKey( WFilerWizard* InOwner )
	: WWizardPage      ( TEXT("FilerPageCDKey"), IDDIALOG_FilerPageCDKey, InOwner )
	, Owner            ( InOwner )
	, Manager          ( InOwner->Manager )
	, CDKeyDescription ( this, IDC_CDKeyDescription )
	, CDKeyHolder      ( this, IDC_CDKeyHolder )
	, CDKey            ( this, IDC_CDKey )
	{
		guard(WFilerPageFolder::WFilerPageFolder);
		unguard;
	}

	void OnInitDialog()
	{
		guard(WFilerPageLicense::OnInitDialog);
		WWizardPage::OnInitDialog();
		SetFocus(CDKey.hWnd);
		unguard;
	}
	WWizardPage* GetNext()
	{
		if( GetShow() )
		{
			FString EnteredCDKey = CDKey.GetText().Caps().Left(23);
			if( !ValidateCDKey( *EnteredCDKey ) )
			{
				MessageBox( *Owner, LocalizeError(TEXT("CDKeyFormat"),TEXT("Setup")), LocalizeError(TEXT("CDKeyTitle"),TEXT("Setup")), MB_OK );
				return NULL;
			}
			Manager->CDKey = EnteredCDKey;
		}
		return new WFilerPageFolder( Owner );
	}
	UBOOL GetShow()
	{
		return Manager->CheckCDKey;
	}
};

// License.
class WFilerPageLicense : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageLicense,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel LicenseText;
	WLabel LicenseQuestion;
	WEdit  LicenseEdit;

	// Constructor.
	WFilerPageLicense( WFilerWizard* InOwner )
	: WWizardPage    ( TEXT("FilerPageLicense"), IDDIALOG_FilerPageLicense, InOwner )
	, Owner          ( InOwner )
	, Manager        ( InOwner->Manager )
	, LicenseText    ( this, IDC_LicenseText     )
	, LicenseQuestion( this, IDC_LicenseQuestion )
	, LicenseEdit    ( this, IDC_LicenseEdit     )
	{}

	// WWizardPage interface.
	const TCHAR* GetNextText()
	{
		return LocalizeGeneral("AgreeButton",TEXT("Window"));
	}
	WWizardPage* GetNext()
	{
		return new WFilerPageCDKey( Owner );
	}
	void OnInitDialog()
	{
		guard(WFilerPageLicense::OnInitDialog);
		WWizardPage::OnInitDialog();
		FString Str;
		if( **Manager->License && appLoadFileToString(Str,*Manager->License) )
			LicenseEdit.SetText( *Str );
		unguard;
	}
	UBOOL GetShow()
	{
		return LicenseEdit.GetText()!=TEXT("");
	}
};

// Progess.
class WFilerPageUninstallProgress : public WFilerPageProgress
{
	DECLARE_WINDOWCLASS(WFilerPageUninstallProgress,WFilerPageProgress,Setup)
	WFilerPageUninstallProgress ( WFilerWizard* InOwner )
	: WFilerPageProgress( InOwner, TEXT("FilerPageUninstallProgress") )
	{}
	WWizardPage* GetNext()
	{
		guard(WFilerPageUninstallProgress::GetNext);
		Owner->OnFinish();
		return NULL;
		unguard;
	}
	void OnCurrent()
	{
		guard(WFilerPageProgress::OnCurrent);
		UpdateWindow( *this );
		Progress.Show(0);
		ProgressText.Show(0);
		Manager->DoUninstallSteps( this );
		Finished = 1;
		Owner->OnNext();
		unguard;
	}
};

// Uninstall screen.
class WFilerPageUninstall : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageUninstall,WWizardPage,Setup)
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	TArray<USetupGroup*>& Dependencies;
	WLabel Prompt;
	WButton YesButton, NoButton;
	WEdit List;
	WFilerPageUninstall( WFilerWizard* InOwner )
	: WWizardPage   ( TEXT("FilerPageUninstall"), IDDIALOG_FilerPageUninstall, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, Dependencies  ( InOwner->Manager->UninstallComponents )
	, YesButton     ( this, IDC_Yes, FDelegate() )
	, NoButton      ( this, IDC_No,  FDelegate() )
	, Prompt        ( this, IDC_UninstallPrompt )
	, List			( this, IDC_UninstallListEdit )
	{}
	void OnInitDialog()
	{
		guard(WFilerPageUninstall::OnInitDialog);
		WWizardPage::OnInitDialog();
		SendMessageX( YesButton, BM_SETCHECK, 1, 0 );
		YesButton.SetText( LocalizeGeneral(TEXT("Yes"),TEXT("Core")) );
		NoButton .SetText( LocalizeGeneral(TEXT("No" ),TEXT("Core")) );
		Prompt   .SetText( *FString::Printf(Localize(TEXT("IDDIALOG_FilerPageUninstall"), TEXT("IDC_UninstallPrompt"), GetPackageName()), Manager->Product) );//!!LocalProduct isn't accessible
		SendMessageX( Prompt, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(0,0) );
		FString Str;
		for( INT i=0; i<Dependencies.Num(); i++ )
			Str += Dependencies(i)->Caption + TEXT("\r\n");
		List.SetText( *Str );
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageAutoPlay::GetNext);
		if( SendMessageX( YesButton, BM_GETCHECK, 0, 0 )!=BST_CHECKED )
		{
			Owner->OnFinish();
			return NULL;
		}
		else return new WFilerPageUninstallProgress( Owner );
		unguard;
	}
};

// Welcome.
class WFilerPageWelcome : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageWelcome,WWizardPage,Setup)

	// Variables.
	WFilerWizard*		Owner;
	USetupDefinition*	Manager;
	WLabel				WelcomePrompt;
	WLabel				LanguagePrompt;
	WProductInfo		ProductInfo;
	WListBox			LanguageList;
	TArray<FString>		LanguageNames;
	TArray<FRegistryObjectInfo> Results;

	// Constructor.
	WFilerPageWelcome( WFilerWizard* InOwner )
	: WWizardPage   ( TEXT("FilerPageWelcome"), IDDIALOG_FilerPageWelcome, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, WelcomePrompt ( this, IDC_WelcomePrompt )
	, LanguagePrompt( this, IDC_LanguagePrompt )
	, ProductInfo	( this, InOwner->Manager, InOwner->Manager )
	, LanguageList  ( this, IDC_LanguageList )
	{
		guard(WFilerPageWelcome::WFilerPageWelcome);
		LanguageList.SelectionChangeDelegate = FDelegate(this,(TDelegate)OnUserChangeLanguage);
		unguard;
	}

	// WDialog interface.
	void OnCurrent()
	{
		guard(WFilerPageWelcome::OnSetFocus);
		Owner->SetText( *Manager->SetupWindowTitle );
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageWelcome::OnInitDialog);
		WWizardPage::OnInitDialog();

		// Open product info window.
		ProductInfo.OpenChildWindow( IDC_ProductInfoHolder, 1 );

		// Get keyboard layout info.
		INT UserLangId = GetUserDefaultLangID() & ((1<<10)-1);
		INT UserSubLangId = GetUserDefaultLangID() >> 10;
		debugf( NAME_Init, TEXT("Language %i, Sublanguage %i"), UserLangId, UserSubLangId );

		// Get language list.
		INT Ideal=-1, Best=-1, Current=0;
		UBOOL IdealLocked=0, BestLocked=0, CurrentLocked=0;
		UObject::GetRegistryObjects( Results, UClass::StaticClass(), ULanguage::StaticClass(), 0 );
		if( Results.Num()==0 )
			appErrorf( TEXT("No Languages Found") );

		// Pick language matching keyboard layout if one exists, otherwise .int.
		for( INT i=0; i<Results.Num(); i++ )
		{
			TCHAR Name[256];
			INT LangId, SubLangId;
			FString Path = US + TEXT("Core.") + Results(i).Object;
			GConfig->GetString( TEXT("Language"), TEXT("Language"), Name, ARRAY_COUNT(Name), *Path );
			GConfig->GetInt( TEXT("Language"), TEXT("LangId"), LangId, *Path );
			GConfig->GetInt( TEXT("Language"), TEXT("SubLangId"), SubLangId, *Path );
			UBOOL locked=0;
			GConfig->GetBool( TEXT("Language"), TEXT("Locked"), locked, *Path );
			new(LanguageNames)FString( Name );
			if( appStricmp(*Results(i).Object,TEXT("int"))==0 )
			{
				Current = i;
				CurrentLocked = locked;
			}
			if( LangId==UserLangId )
			{
				Best = i;
				BestLocked = locked;
			}
			if( LangId==UserLangId && SubLangId==UserSubLangId )
			{
				Ideal = i;
				IdealLocked = locked;
			}
		}
		if( Best>=0 )
		{
			Current = Best;
			CurrentLocked = BestLocked;
		}
		if( Ideal>=0 )
		{
			Current = Ideal;
			CurrentLocked = IdealLocked;
		}

		if( CurrentLocked )
			LanguageList.AddString( *LanguageNames(Current) );
		else
			for( INT i=0;i<LanguageNames.Num();i++ )
                LanguageList.AddString( *LanguageNames(i) );

		LanguageList.SetCurrent( LanguageList.FindString(*LanguageNames(Current)), 1 );
		OnUserChangeLanguage();

		unguard;
	}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		guard(WFilerPageWelcome::GetNext);
		return new WFilerPageLicense(Owner);
		unguard;
	}
	const TCHAR* GetBackText()
	{
		return NULL;
	}

	// WFilerPageWelcome interface.
	void OnUserChangeLanguage()
	{
		guard(WFilerPageWelcome::OnUserChangeLanguage);
		INT Index;
		if( LanguageNames.FindItem(*LanguageList.GetString(LanguageList.GetCurrent()),Index) )
		{
			FString Language = *Results(Index).Object;
			UObject::SetLanguage( *Language );
			GConfig->SetString( TEXT("Setup"), TEXT("Language"), *Language, *Manager->ConfigFile );
		}
		else
		{
			FString Language = TEXT("kot"); //!!HACK
			UObject::SetLanguage( *Language );
			GConfig->SetString( TEXT("Setup"), TEXT("Language"), *Language, *Manager->ConfigFile );
		}
		LanguageChange();//!!
		unguard;
	}
	virtual void LanguageChange()
	{
		guard(WFilerPageWelcome::LanguageChange);

		// Welcome text.
		WelcomePrompt.SetText( *FString::Printf( LineFormat(Localize(TEXT("IDDIALOG_FilerPageWelcome"),Manager->Patch ? TEXT("IDC_WelcomePromptUpdate") : TEXT("IDC_WelcomePrompt"),TEXT("Setup"))), *Manager->LocalProduct, *Manager->Version ) );

		// Other text.
		Owner->SetText(LineFormat(Localize(TEXT("IDDIALOG_WizardDialog"),TEXT("IDC_WizardDialog"),TEXT("Setup"))));
		LanguagePrompt.SetText(LineFormat(Localize(TEXT("IDDIALOG_FilerPageWelcome"),TEXT("IDC_LanguagePrompt"),TEXT("Setup"))));
		ProductInfo.LanguageChange();//!!
		Owner->RefreshPage();//!!

		unguard;
	}
};

// Components.
class WFilerPageUninstallComponents : public WFilerPageComponentsBase
{
	DECLARE_WINDOWCLASS(WFilerPageUninstallComponents,WFilerPageComponentsBase,Setup)

	// Variables.
	USetupDefinition* Manager;
	TArray<USetupGroup*>& Dependencies;
	WComponentProperties Components;

	// Constructor.
	WFilerPageUninstallComponents( WFilerWizard* InOwner )
	: WFilerPageComponentsBase( TEXT("FilerPageUninstallComponents"), IDDIALOG_FilerPageUninstallComponents, InOwner )
	, Manager              ( InOwner->Manager )
	, Components		   ( this )
	, Dependencies		   ( InOwner->Manager->UninstallComponents )
	{}

	// Functions.
	void OnGroupChange( class FComponentItem* Group )
	{
		guard(WFilerPageComponents::OnSelectionChange);
		INT i, Added;

		// Unforce all.
		for( i=0; i<Components.Root.Children.Num(); i++ )
		{
			FComponentItem* Item = (FComponentItem*)Components.Root.Children(i);
			Item->Forced = 0;
		}

		// Build list of dependent components that must be uninstalled due to selected products.
		Dependencies.Empty();
		for( i=0; i<Components.Root.Children.Num(); i++ )
		{
			FComponentItem* Item = (FComponentItem*)Components.Root.Children(i);
			if( Item->SetupGroup->Selected )
				Dependencies.AddItem( Item->SetupGroup );
		}

		// All items that are dependent but not selected must be forced.
		do
		{
			Added = 0;
			for( i=0; i<Components.Root.Children.Num(); i++ )
			{
				FComponentItem* Item = (FComponentItem*)Components.Root.Children(i);
				if( !Item->Forced )
				{
					for( INT j=0; j<Item->SetupGroup->Requires.Num(); j++ )
					{
						for( INT k=0; k<Components.Root.Children.Num(); k++ )
						{
							FComponentItem* Other = (FComponentItem*)Components.Root.Children(k);
							if( Item->SetupGroup->Requires(j)==Other->SetupGroup->GetName() && (Other->SetupGroup->Selected || Other->Forced) )
							{
								Dependencies.AddUniqueItem( Item->SetupGroup );
								Item->Forced = 1;
								Added = 1;
							}
						}
					}
				}
			}
		} while( Added );

		// Refresh.
		Owner->RefreshPage();

		unguard;
	}

	// WWizardPage interface.
	const TCHAR* GetNextText()
	{
		guard(WFilerPageComponents::GetNextText);
		return Dependencies.Num() ? WWizardPage::GetNextText() : NULL;
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageComponents::GetNext);
		return Dependencies.Num() ? new WFilerPageUninstall(Owner) : NULL;
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageComponents::OnInitDialog);
		WWizardPage::OnInitDialog();
		Components.OpenChildWindow( IDC_ComponentsHolder );

		OnGroupChange( NULL );
		Components.GetRoot()->Expand();
		Components.ResizeList();
		Components.List.SetCurrent( 0, 1 );
		Components.SetItemFocus( 1 );

		unguard;
	}
	UBOOL GetShow()
	{
		guard(WFilerPageComponents::GetShow);
		if( Components.Root.Children.Num()==1 )
		{
			FComponentItem* Item = (FComponentItem*)Components.Root.Children(0);
			if( Item->Children.Num()==0 )
			{
				Dependencies.AddItem( Item->SetupGroup );
				return 0;
			}
		}
		return 1;
		unguard;
	}
};

// WFilerPageAutoplay.
class WFilerPageAutoPlay : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageAutoPlay,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel Options;
	WLabel CompleteLabel;
	WButton CompleteFrame;
	WCoolButton PlayButton;
	WCoolButton ReleaseNotesButton;
	WCoolButton ReinstallButton;
	WCoolButton UninstallButton;
	WCoolButton WebButton;
	UBOOL ShowInstallOptions;

	// Constructor.
	WFilerPageAutoPlay( WFilerWizard* InOwner, UBOOL InShowInstallOptions )
	: WWizardPage( TEXT("FilerPageAutoPlay"), IDDIALOG_FilerPageAutoPlay, InOwner )
	, Owner             ( InOwner )
	, Manager           ( InOwner->Manager )
	, Options           ( this, IDC_Options )
	, PlayButton        ( this, IDC_Play,         FDelegate(this,(TDelegate)OnPlay),         CBFF_ShowOver|CBFF_UrlStyle )
	, ReleaseNotesButton( this, IDC_ReleaseNotes, FDelegate(this,(TDelegate)OnReleaseNotes), CBFF_ShowOver|CBFF_UrlStyle )
	, WebButton         ( this, IDC_Web,          FDelegate(this,(TDelegate)OnWeb),          CBFF_ShowOver|CBFF_UrlStyle )
	, ReinstallButton   ( this, IDC_Reinstall,    FDelegate(this,(TDelegate)OnInstall),      CBFF_ShowOver|CBFF_UrlStyle )
	, UninstallButton   ( this, IDC_Uninstall,    FDelegate(this,(TDelegate)OnUninstall),    CBFF_ShowOver|CBFF_UrlStyle )
	, CompleteLabel     ( this, IDC_Complete )
	, CompleteFrame     ( this, IDC_Divider )
	, ShowInstallOptions( InShowInstallOptions )
	{}

	// Buttons.
	void OnPlay()
	{
		guard(WFilerPageAutoPlay::OnPlay);
		FString Exe = Manager->RegistryFolder * Manager->Exe;
		FString Folder = Exe;
		while( Folder.Len() && Folder.Right(1)!=PATH_SEPARATOR )
			Folder = Folder.LeftChop( 1 );
		if( Folder.Right(1)==PATH_SEPARATOR )
			Folder = Folder.LeftChop( 1 );
		ShellExecuteX( *this, TEXT("open"), *Exe, TEXT(""), *Folder, SW_SHOWNORMAL );
		appSleep(30);
		Owner->OnFinish();
		unguard;
	}
	void OnInstall()
	{
		guard(WFilerPageAutoPlay::OnInstall);
		{
			// Restart installer from temp dir on HD if we're running from CD.
			if( Manager->MultiCD && !Manager->HardDiskCDInstall )
			{
				Manager->PerformInstallCopy();
				Owner->EndDialogTrue();
			}
			else
				Owner->Advance( new WFilerPageWelcome(Owner) );
		}
		unguard;
	}
	void OnUninstall()
	{
		guard(WFilerPageAutoPlay::OnUninstall);
		FString Path = Manager->RegistryFolder*TEXT("System");
		ShellExecuteX( NULL, TEXT("open"), *(Path*TEXT("Setup.exe")), *(US+TEXT("uninstall \"") + Manager->Product + TEXT("\"")), *Path, SW_SHOWNORMAL );
		Owner->OnFinish();
		unguard;
	}
	void OnReleaseNotes()
	{
		guard(WFilerPageAutoPlay::OnReleaseNotes);
		ShellExecuteX( *this, TEXT("open"), *(Manager->RegistryFolder * Manager->ReadMe), TEXT(""), NULL, SW_SHOWNORMAL );
		unguard;
	}
	void OnWeb()
	{
		guard(WFilerPageAutoPlay::OnWeb);
		ShellExecuteX( *this, TEXT("open"), *Manager->ProductURL, TEXT(""), appBaseDir(), SW_SHOWNORMAL );
		unguard;
	}

	// WWizardPage interface.
	void OnCurrent()
	{
		guard(WFilerPageAutoplay::OnCurrent);
		Owner->SetText( *Manager->AutoplayWindowTitle );
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageAutoPlay::OnInitDialog);
		WWizardPage::OnInitDialog();
		Options.SetFont( hFontHeadline );
		if( ShowInstallOptions )
		{
			CompleteLabel.Show(0);
			CompleteFrame.Show(0);
		}
		else
		{
			ReinstallButton.Show(0);
			UninstallButton.Show(0);
		}
		if( !Manager->Exists || Manager->Exe==TEXT("") || Manager->MustReboot )
			PlayButton.Show(0);
		if( Manager->MustReboot )
			CompleteLabel.SetText( LineFormat(Localize(TEXT("IDDIALOG_FilerPageAutoPlay"),TEXT("IDC_CompleteReboot"),TEXT("Setup"))) );
		Options.SetText( *Manager->AutoplayWindowTitle );
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageAutoPlay::GetNext);
		Manager->MustReboot = 0;
		Owner->OnFinish();
		return NULL;
		unguard;
	}
	const TCHAR* GetBackText()
	{
		return NULL;
	}
	const TCHAR* GetFinishText()
	{
		return ShowInstallOptions ? NULL : Manager->MustReboot ? LocalizeGeneral(TEXT("RebootButton"),TEXT("Setup")) : LocalizeGeneral(TEXT("FinishButton"),TEXT("Window"));
	}
	const TCHAR* GetNextText()
	{
		return (Manager->MustReboot && !ShowInstallOptions) ? LocalizeGeneral(TEXT("ExitButton"),TEXT("Setup")) : NULL;
	}
	virtual const TCHAR* GetCancelText()
	{
		return ShowInstallOptions ? WWizardPage::GetCancelText() : NULL;
	}
};
WWizardPage* NewAutoPlayPage( WFilerWizard* InOwner, UBOOL ShowInstallOptions )
{
	return new WFilerPageAutoPlay( InOwner, ShowInstallOptions );
}

/*-----------------------------------------------------------------------------
	Autopatch wizard.
-----------------------------------------------------------------------------*/

// Autopatch wizard.
BOOL Patched=0;
class WAutopatchWizard : public WWizardDialog
{
	DECLARE_WINDOWCLASS(WAutopatchWizard,WWizardDialog,Setup)

	// Config info.
	WLabel LogoStatic;
	FWindowsBitmap LogoBitmap;

	// Constructor.
	WAutopatchWizard()
	:	LogoStatic		( this, IDC_Logo )
	{
		guard(WAutopatchWizard::WFilerWizard);
		CoInitialize(NULL);
		unguard;
	}

	// WWindow interface.
	void OnInitDialog()
	{
		guard(WAutopatchWizard::OnInitDialog);
		WWizardDialog::OnInitDialog();
		LogoBitmap.LoadFile( TEXT("..\\Help\\InstallerLogo.bmp") );
		SendMessageX( LogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)LogoBitmap.GetBitmapHandle() );
		unguard;
	}

	// WWizard interface.
	void OnFinish()
	{
		guard(WAutopatchWizard::OnFinish);
		if(Patched)
			ShellExecuteA(0,"open","Autopatch.exe","","",SW_SHOWDEFAULT);
		WWizardDialog::OnFinish();
		unguard;
	}
};

// Autopatch page.
class WAutopatchPageGo : public WWizardPage
{
	DECLARE_WINDOWCLASS(WAutopatchPageGo,WWizardPage,Setup)

	// Variables.
	WAutopatchWizard* Owner;
	WLabel AutoUpdateText;
	WLabel UpdatingText;
	WLabel ProgressText;
	WProgressBar Progress;
	WUrlButton Manual;
	UBOOL CancelFlag;
	HANDLE hAsync;
	char HostEnt[MAXGETHOSTSTRUCT];
	TArray<FString> Servers,Files;
	INT Index;
	SOCKET Socket;
	TArray<BYTE> Data;
	enum EPatchState {PATCHSTATE_None,PATCHSTATE_Resolving,PATCHSTATE_Connecting,PATCHSTATE_Receiving,PATCHSTATE_Finished} State;
	INT Msec,TotalSize,AsyncCount;

	// Constructor.
	WAutopatchPageGo( WAutopatchWizard* InOwner )
	: WWizardPage   ( TEXT("AutopatchPageGo"), IDDIALOG_AutopatchPageGo, InOwner )
	, Owner         ( InOwner )
	, AutoUpdateText( this, IDC_AutoUpdateText )
	, UpdatingText	( this, IDC_UpdatingText )
	, ProgressText  ( this, IDC_ProgressText )
	, Progress      ( this, IDC_Progress )
	, Manual        ( this, Localize(TEXT("IDDIALOG_AutopatchPageGo"),TEXT("ManualDownloadPage"),TEXT("Setup")), IDC_Manual )
	, Index         ( -1 )
	, Socket        ( -1 )
	, State         ( PATCHSTATE_None )
	, Msec          ( 0 )
	, TotalSize     ( 0 )
	, AsyncCount    ( 0 )
	{
		WSAData data;
		WSAStartup(0x0101,&data);
	}
	void OnInitDialog()
	{
		guard(WAutopatchPageGo::OnInitDialog);
		WWizardPage::OnInitDialog();
		FString ServerList;
		appLoadFileToString(ServerList,TEXT("AutoPatch.txt"));
		FString X;
		const TCHAR* Stream=&ServerList[0];
		while(ParseLine(&Stream,X,1))
		{
			if((X[0]=='h'||X[0]=='H')&&(X[1]=='t'||X[1]=='T')&&(X[2]=='t'||X[2]=='T')&&(X[3]=='p'||X[3]=='P')&&X[4]==':'&&X[5]=='/'&&X[6]=='/')
			{
				X=X.Mid(7); INT i;
				for(i=0; i<X.Len() && X[i]!='/'; i++);
				if(i<X.Len())
				{
					new(Servers)FString(X.Left(i));
					new(Files)FString(X.Mid(i));
				}
			}
		}
		check(Servers.Num());
		SetTimer(hWnd,123,25,0);
		TryNext();
		unguard;
	}
	void TryNext()
	{
		guard(WAutopatchPageGo::TryNext);
		if(Socket!=-1)
		{
			closesocket(Socket);
			Socket=-1;
		}
		GotoState(PATCHSTATE_Resolving);
		if(++Index>=Servers.Num())
		{
			Index=0; //!!Or should we give the user an error and abort if failed?
		}
		AsyncCount++;
		hAsync=WSAAsyncGetHostByName(hWnd,WM_USER+AsyncCount,appToAnsi(*Servers(Index)),HostEnt,sizeof(HostEnt));
		check(hAsync!=0);
		FString ProgressString=FString(Localize(TEXT("IDDIALOG_AutopatchPageGo"),TEXT("ContactingText"),TEXT("Setup")))+Servers(Index);
		UpdatingText.SetText(*ProgressString);
		unguard;
	}
	void GotoState(EPatchState NewState)
	{
		guard(WAutopatchPage::GotoState);
		State=NewState;
		Msec=0;
		if(State!=PATCHSTATE_Finished)
		{
			TotalSize=0;
			Data.Empty();
		}
		unguard;
	}
	LONG WndProc( UINT Message, UINT wParam, LONG lParam )
	{
		guard(WAutopatchPageGo::WndProc);
		if(Message==WM_USER+AsyncCount && State==PATCHSTATE_Resolving)
		{
			int err=WSAGETASYNCERROR(lParam);
			if(err==0)
			{
				FString ProgressString=FString(Localize(TEXT("IDDIALOG_AutopatchPageGo"),TEXT("DownloadingText"),TEXT("Setup")))+Files(Index);
				UpdatingText.SetText(*ProgressString);
				SOCKADDR_IN SockAddr;
				SockAddr.sin_family=AF_INET;
				SockAddr.sin_port=80*256+0/256; //!!Limitation: Doesn't support port specifiers in URL's.
				SockAddr.sin_addr.S_un.S_addr=*(DWORD*)((HOSTENT*)HostEnt)->h_addr;
				Socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
				check(Socket!=INVALID_SOCKET);
				DWORD NoBlock=1;
				ioctlsocket(Socket,FIONBIO,&NoBlock);
				int r=connect(Socket,(sockaddr*)&SockAddr,sizeof(SockAddr));
				check(r==-1);
				GotoState(PATCHSTATE_Connecting);
			}
			else TryNext();
		}
		if(Message==WM_TIMER)
		{
			Msec+=10;
			if(State==PATCHSTATE_Connecting)
			{
				guard(PATCHSTATE_Connecting);
				fd_set Sox; Sox.fd_count=1; Sox.fd_array[0]=Socket;
				timeval Now; Now.tv_sec=0; Now.tv_usec=0;
				int r=select(0,0,&Sox,0,&Now); // If it's writable, we've connected.
				if(r==1)
				{
					FString Request=FString(TEXT("GET "))+Files(Index)+TEXT(" HTTP/1.1\nHost: ")+Servers(Index)+TEXT("\n\n");
					//appErrorf(TEXT("REQ [%s]"),*Request);
					send(Socket,appToAnsi(*Request),appStrlen(*Request),0);
					GotoState(PATCHSTATE_Receiving);
				}
				else
				{
					fd_set Sox; Sox.fd_count=1; Sox.fd_array[0]=Socket;
					timeval Now; Now.tv_sec=0; Now.tv_usec=0;
					int r=select(0,0,0,&Sox,&Now); // If it's erroneous, connect failed.
					if(r==1)
						TryNext();
				}
				unguard;
			}
			else if(State==PATCHSTATE_Receiving)
			{
				guard(PATCHSTATE_Receiving);
				for(;;)
				{
					BYTE Tmp[65536];
					INT n=recv(Socket,(char*)Tmp,sizeof(Tmp),0);
					if(n>0)
					{
						Data.Add(n);
						for(INT i=0; i<n; i++)
							Data(Data.Num()-n+i)=Tmp[i];
						if(TotalSize==0)
						{
							int i,hs=0;
							for(i=0; i<Data.Num(); i++)
								if(i<Data.Num()-1&&Data(i)=='\n'&&Data(i+1)=='\n')                                        {hs=i+2; break;}
								else if(i<Data.Num()-3&&Data(i)=='\r'&&Data(i+1)=='\n'&&Data(i+2)=='\r'&&Data(i+3)=='\n') {hs=i+4; break;}
							//{TCHAR Y[65537];
							//int k; for(k=0; k<Data.Num(); k++) Y[k]=Data(k); Y[k]=0;
							//static int c; c++; if(c==1) appErrorf(TEXT("DATA [%s] %i %i"),Y,i,Data.Num());}
							if(hs)
							{
								TCHAR* STmp=new TCHAR[i+1];
								for(INT j=0; j<i+1; j++) STmp[j]=Data(j);
								STmp[i]=0;
								FString X; const TCHAR* SData=STmp;
								INT Code=0;
								while(ParseLine(&SData,X,1))
								{
									TCHAR* End=0;
									if(X.Left( 9)==TEXT("HTTP/1.1 ")       ) Code     =appStrtoi(*X+ 9,&End,10);
									if(X.Left(16)==TEXT("Content-Length: ")) TotalSize=appStrtoi(*X+16,&End,10);
								}
								Data.Remove(0,hs);
								if(Code!=200||TotalSize==0)
								{
									TryNext();
									break;
								}
							}
						}
						if(TotalSize!=0 && Data.Num()==TotalSize) goto GotIt;
					}
					else if(n==0)
					{
						if(TotalSize && TotalSize==Data.Num())
						{
						GotIt:
 							appSaveArrayToFile(Data,TEXT("Autopatch.exe"));
							GotoState(PATCHSTATE_Finished);
							Owner->FinishButton.SetVisibleText( GetFinishText() );
							Owner->CancelButton.SetVisibleText( GetCancelText() );
							UpdatingText.SetText(Localize(TEXT("IDDIALOG_AutopatchPageGo"),TEXT("SuccessText"),TEXT("Setup")));
							Patched=1;
						}
						else TryNext();
						break;
					}
					else if(n==-1 && WSAGetLastError()==WSAEWOULDBLOCK)
					{
						break; // Waiting for data.
					}
					else
					{
						TryNext(); // Closed ungracefully.
						break;
					}
				}
				unguard;
			}
		}
		Progress.SetProgress(TotalSize? Data.Num(): 0,TotalSize? TotalSize: 1);
		return WWizardPage::WndProc(Message,wParam,lParam);
		unguard;
	}
	//!!start at a random offset into the site list
	//!!exit with error if cycled thru all sites and it still failed?
	//!!test timeout/failure everywhere during download (pull net cable) - coverage test, etc.
	//!!test with jack integration
	//!!test on win98/winme/win2k/winxp
	const TCHAR* GetBackText()
	{
		return NULL;
	}
	const TCHAR* GetNextText()
	{
		return NULL;
	}
	const TCHAR* GetFinishText()
	{
		return State==PATCHSTATE_Finished? LocalizeGeneral(TEXT("FinishButton"),TEXT("Window")): NULL;
	}
	virtual const TCHAR* GetCancelText()
	{
		return State==PATCHSTATE_Finished? NULL: WWizardPage::GetCancelText();
	}
	void OnCancel()
	{
		if(MessageBox(hWnd,Localize(TEXT("IDDIALOG_AutopatchPageGo"),TEXT("CancelText"),TEXT("Setup")),WWizardPage::GetCancelText(),MB_YESNO)==IDYES)
		{
			Patched=0;
			Owner->OnFinish();
		}
	}
};

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

//
// Main window entry point.
//
INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char* InCmdLine, INT nCmdShow )
{
	// Remember instance info.
	GIsStarted = 1;
	hInstance = hInInstance;
	appStrcpy( GPackage, appPackage() );

	// Begin.
#ifndef _DEBUG
	try
	{
#endif
	{
		// Init.
		HANDLE hMutex = NULL;
		GIsEditor = 0;
		GIsClocking = 0;
		GIsScriptable = GIsClient = GIsServer = GIsGuarded = 1;
#if SHOWLOG
		appInit( GPackage, GetCommandLine(), &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 0 );
#else
		appInit( GPackage, GetCommandLine(), &Malloc, GNull, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 0 );
#endif
		GConfig->Detach( *(FString(GPackage)+TEXT(".ini")) );

		// Init windowing.
		InitWindowing();
		IMPLEMENT_WINDOWCLASS(WFilerPageWelcome,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageLicense,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageComponentsBase,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageComponents,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageCDKey,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageFolder,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageCdFolder,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageProgress,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageInstallProgress,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageUninstallProgress,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageUninstall,0);
		IMPLEMENT_WINDOWCLASS(WFilerWizard,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageAutoPlay,0);
		IMPLEMENT_WINDOWCLASS(WFailedRequirement,0);
		IMPLEMENT_WINDOWCLASS(WCDPromptDialog,0);
		IMPLEMENT_WINDOWCLASS(WProductInfo,0);
		IMPLEMENT_WINDOWCLASS(WComponentProperties,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageUninstallComponents,0);
		IMPLEMENT_WINDOWCLASS(WAutopatchPageGo,0);

#if SHOWLOG
		// Log window.
		GLogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("EditorLog"), NULL );
		GLogWindow->OpenWindow( 1, 0 );
#endif
		GIsRunning = 1;

		// See if Unreal or Filer is running.
		INT Count=0;
	RetryMutex:
		hMutex = CreateMutexX( NULL, 0, TEXT("UnrealIsRunning") );
		if( GetLastError()==ERROR_ALREADY_EXISTS )
		{
			CloseHandle( hMutex );
			Sleep(100);
			if( ++Count<20 )
				if( appStrfind(appCmdLine(),TEXT("autopatch")) || appStrfind(appCmdLine(),TEXT("reallyuninstall")) || appStrfind(appCmdLine(),TEXT("uninstall")) || appStrfind(appCmdLine(),TEXT("cdinstall")) )
					goto RetryMutex;
			if( MessageBox(NULL,LocalizeError(TEXT("AlreadyRunning"),TEXT("Setup")),LocalizeError(TEXT("AlreadyRunningTitle"),TEXT("Setup")),MB_OKCANCEL)==IDOK )
				goto RetryMutex;
			goto Finished;
		}

		// Filer interface.
		guard(Setup);
		if( appStrfind(appCmdLine(),TEXT("autopatch")) )
		{
			WAutopatchWizard D;
			D.Advance(new WAutopatchPageGo(&D));
			D.DoModal();
		}
		else
		{
			WFilerWizard D;
			if( !D.Manager->NoRun )
			{
				WWizardPage* Page=NULL;
				if( D.Manager->Uninstalling )
					Page = new WFilerPageUninstallComponents(&D);
				else if( D.Manager->Exists && D.Manager->CdAutoPlay && !D.Manager->InstallCDTempDir )
					Page = new WFilerPageAutoPlay(&D,1);
				else
				{
					// Restart installer from temp dir on HD if we're running from CD.
					if( !D.Manager->InstallCDTempDir && D.Manager->MultiCD && !D.Manager->HardDiskCDInstall )
						D.Manager->PerformInstallCopy();
					else
						Page = new WFilerPageWelcome(&D);
				}
				if( Page )
				{
					D.Advance( Page );
					D.DoModal();
				}
			}
		}
		unguard;

		// Exit.
	Finished:
		appPreExit();
		GIsGuarded = 0;
	}
#ifndef _DEBUG
	}
	catch( ... )
	{
		// Crashed.
		try
		{
			Error.HandleError();
		}
		catch( ... )
		{}
	}
#endif

	// Shut down.
	appExit();
	GIsStarted = 0;

	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

