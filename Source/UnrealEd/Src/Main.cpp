/*=============================================================================
	Main.cpp: UnrealEd Windows startup.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

    Revision history:
		* Created by Tim Sweeney.
		* Secret Level's Michael Arnold's changes integrated 07/03/02 - Erik de Neve
		   - WinMain split into CallWInMain, MainLoop, ShutDownWinMain
			 so that the different sections may be launched/called separately.
		   - Implementation of UnrealEdDLL.dll interface functions.
		* Secret Level's Joshua Lee's changes for COM based exporter interfaces - integrated Oct 2003 - Erik de Neve

    Work-in-progress todo's:

=============================================================================*/


/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnrealEd.h"
#include "UnEngineWin.h"

//#define ENABLE_COM_IF

#if defined( ENABLE_COM_IF )
#include "UnrealEdImport.h"
#include "UnrealEdServerInterface.h"
#endif



/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

UNREALED_API class FEdModeTools* GEdModeTools;
UNREALED_API class FTBOptions* GTBOptions;
UNREALED_API class FMaterialTools* GMaterialTools;

class UUnrealEdEngine* GUnrealEd;

/*-----------------------------------------------------------------------------
	Option proxies.
-----------------------------------------------------------------------------*/


enum
{
	OPTIONS_BRUSHSCALE,
	OPTIONS_2DSHAPERSHEET,
	OPTIONS_2DSHAPEREXTRUDE,
	OPTIONS_2DSHAPEREXTRUDETOPOINT,
	OPTIONS_2DSHAPEREXTRUDETOBEVEL,
	OPTIONS_2DSHAPERREVOLVE,
	OPTIONS_2DSHAPERBEZIERDETAIL,
	OPTIONS_SURFBEVEL,
	OPTIONS_TEXALIGNPLANAR,
	OPTIONS_TEXALIGNCYLINDER,
	OPTIONS_NEWTERRAIN,
	OPTIONS_NEWTERRAINLAYER,
	OPTIONS_MAPSCALE,
	OPTIONS_MATNEWCAMERAPATH,
	OPTIONS_MATNEWSTATICMESH,
	OPTIONS_DUPTEXTURE,
	OPTIONS_ROTATEACTORS,
	OPTIONS_NEWCLASSFROMSEL,
};
TMap<INT,UOptionsProxy*>* GOptionProxies;

#define CREATE_OPTION_PROXY( ID, Class )\
{\
	GOptionProxies->Set( ID,\
		CastChecked<UOptionsProxy>(GUnrealEd->StaticConstructObject(Class::StaticClass(),GUnrealEd->Level->GetOuter(),NAME_None,RF_Public|RF_Standalone) ) );\
	UOptionsProxy* Proxy = *GOptionProxies->Find( ID );\
	check(Proxy);\
	Proxy->InitFields();\
}

#include "DlgProgress.h"
#include "DlgMapCheck.h"
#include "DlgLoadErrors.h"
#include "DlgRename.h"
#include "DlgDepth.h"
#include "DlgSearchActors.h"
WBrowserMaster* GBrowserMaster = NULL;
#include "CodeFrame.h"
#include "DlgGeneric.h"
#include "DlgBrushBuilder.h"
#include "DlgAddSpecial.h"
#include "DlgScaleLights.h"
#include "DlgTipOfTheDay.h"
#include "DlgTexReplace.h"
#include "TerrainEditSheet.h"
#include "MatineePreview.h"
#include "MatineeSheet.h"
#include "DlgBrushImport.h"
#include "DlgViewportConfig.h"
#include "DlgMapImport.h"
#include "DlgNewMaterial.h"

// 3DBuzz - The array tool dialog
#include "DlgArray.h"

UStruct* GColorStruct = NULL;

class StringPair
{
public:
	StringPair() {}
	StringPair( FString InDesc, FString InClassName, INT InID ) : Desc(InDesc), ClassName(InClassName), ID(InID) {}

	FString Desc;
	FString ClassName;
	INT ID;
};
TArray<StringPair> GActorPopupItems;
#define IDMN_ACTORPOPUPITEMS_BASE	10000
#define IDMN_ACTORPOPUPITEMS_MAX	10100

const DWORD GShowFlags =
	SHOW_Frame | SHOW_Actors | SHOW_Brush | SHOW_StandardView
	| SHOW_ChildWindow | SHOW_MovingBrushes | SHOW_Volumes | SHOW_StaticMeshes
	| SHOW_SelectionHighlight | SHOW_MatPaths | SHOW_FluidSurfaces | SHOW_Projectors;


#include "TwoDeeShapeEditor.h"
#include "BrowserSound.h"
#include "BrowserMusic.h"
#include "BrowserGroup.h"
#include "BrowserTexture.h"
#include "BrowserStaticMesh.h"
#include "BrowserMesh.h"
#include "BrowserPrefab.h"
#include "..\..\core\inc\unmsg.h"

// The last viewport to get the focus.  The main editor
// app uses this to draw a white outline around the current viewport.
HWND GCurrentViewportFrame = NULL;

MRUList* GMRUList;
INT GScrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);
HWND GhwndEditorFrame = NULL;

// This is a list of all the viewport configs that are currently in effect.
TArray<VIEWPORTCONFIG> GViewports;

// Prefebbed viewport configs.  These should be in the same order as the buttons in DlgViewportConfig.
VIEWPORTCONFIG GTemplateViewportConfigs[4][4] =
{
	// 0
	REN_OrthXY,		0,		0,		.65f,		.50f,		0, 0, 0, 0,		NULL,
	REN_OrthXZ,		.65f,	0,		.35f,		.50f,		0, 0, 0, 0,		NULL,
	REN_DynLight,	0,		.50f,	.65f,		.50f,		0, 0, 0, 0,		NULL,
	REN_OrthYZ,		.65f,	.50f,	.35f,		.50f,		0, 0, 0, 0,		NULL,

	// 1
	REN_OrthXY,		0,		0,		.40f,		.40f,		0, 0, 0, 0,		NULL,
	REN_OrthXZ,		.40f,	0,		.30f,		.40f,		0, 0, 0, 0,		NULL,
	REN_OrthYZ,		.70f,	0,		.30f,		.40f,		0, 0, 0, 0,		NULL,
	REN_DynLight,	0,		.40f,	1.0f,		.60f,		0, 0, 0, 0,		NULL,

	// 2
	REN_DynLight,	0,		0,		.70f,		1.0f,		0, 0, 0, 0,		NULL,
	REN_OrthXY,		.70f,	0,		.30f,		.40f,		0, 0, 0, 0,		NULL,
	REN_OrthXZ,		.70f,	.40f,	.30f,		.30f,		0, 0, 0, 0,		NULL,
	REN_OrthYZ,		.70f,	.70f,	.30f,		.30f,		0, 0, 0, 0,		NULL,

	// 3
	REN_OrthXY,		0,		0,		1.0f,		.40f,		0, 0, 0, 0,		NULL,
	REN_DynLight,	0,		.40f,	1.0f,		.60f,		0, 0, 0, 0,		NULL,
	-1,	0, 0, 0, 0, 0, 0, 0, 0, NULL,
	-1,	0, 0, 0, 0, 0, 0, 0, 0, NULL,
};

INT GViewportStyle, GViewportConfig;


FString GLastDir[eLASTDIR_MAX];
FString GMapExt;
HMENU GMainMenu = NULL;

extern FString GLastText;
extern FString GMapExt;

class WMdiClient;
class WMdiFrame;
class WEditorFrame;
class WMdiDockingFrame;
class WLevelFrame;

// Memory allocator.
#ifdef _DEBUG
	#include "FMallocDebug.h"
	FMallocDebug Malloc;
#else
	#include "FMallocWindows.h"
	FMallocWindows Malloc;
#endif

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceWindowsError.h"
FOutputDeviceWindowsError Error;

// Feedback.
#include "FFeedbackContextWindows.h"
FFeedbackContextWindows Warn;

// File manager.
#include "FFileManagerWindows.h"
FFileManagerWindows FileManager;

// Config.
#include "FConfigCacheIni.h"

WCodeFrame* GCodeFrame = NULL;
#include "BrowserActor.h"

WEditorFrame* GEditorFrame = NULL;
WWindow* GEditorFrameW = NULL;

WMdiFrame* GMdiFrame=NULL;
WLevelFrame* GLevelFrame = NULL;
W2DShapeEditor* G2DShapeEditor = NULL;
WSurfacePropSheet* GSurfPropSheet = NULL;
WTerrainEditSheet* GTerrainEditSheet = NULL;
WMatineeSheet* GMatineeSheet = NULL;
HWND GMatineeSheetHwnd = NULL;
WBuildPropSheet* GBuildSheet = NULL;
WBrowserSound* GBrowserSound = NULL;
WBrowserMusic* GBrowserMusic = NULL;
WBrowserGroup* GBrowserGroup = NULL;
WBrowserActor* GBrowserActor = NULL;
WBrowserTexture* GBrowserTexture = NULL;

#ifdef WITH_LIPSINC
WBrowserLIPSinc* GBrowserLIPSinc = NULL;
#endif

HWND GBrowserTextureHwnd = NULL;
WBrowserStaticMesh* GBrowserStaticMesh = NULL;
WBrowserMesh* GBrowserMesh = NULL;
WBrowserAnimation* GBrowserAnimation = NULL;
WBrowserPrefab* GBrowserPrefab = NULL;
WDlgAddSpecial* GDlgAddSpecial = NULL;
WDlgScaleLights* GDlgScaleLights = NULL;
WDlgTipOfTheDay* GDlgTOTD = NULL;
WDlgProgress* GDlgProgress = NULL;
WDlgMapCheck* GDlgMapCheck = NULL;
WDlgLoadErrors* GDlgLoadErrors= NULL;
WDlgSearchActors* GDlgSearchActors = NULL;
WDlgTexReplace* GDlgTexReplace = NULL;
WUDNWindow* GUDNWindow = NULL;

// 3DBuzz - The array dialog
WDlgArray* GDlgArray = NULL;

// External caller ( i.e. SL Maya plugin )
CMainLoop * GExtCallLoopInstance = 0;
bool GExtCallLaunched = false;
bool GExtCallShutdown = false;
HMODULE GUnrealEdModule = 0;
WEditorFrame * GUnRealEdFrame = 0;


// Added by Demiurge (PSE)
WParticleEditor* GParticleEditor = NULL; 
// End Add by Demiurge (PSE)

void UpdateMenu()
{
	guard(UpdateMenu);

	CheckMenuItem( GMainMenu, IDMN_VIEWPORT_FLOATING, MF_BYCOMMAND | (GViewportStyle == VSTYLE_Floating ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem( GMainMenu, IDMN_VIEWPORT_FIXED, MF_BYCOMMAND | (GViewportStyle == VSTYLE_Fixed ? MF_CHECKED : MF_UNCHECKED) );

	EnableMenuItem( GMainMenu, ID_ViewNewFree, MF_BYCOMMAND | (GViewportStyle == VSTYLE_Floating ? MF_ENABLED : MF_GRAYED) );

	unguard;
}

/* scion ======================================================================
* ::StripLine
* Author: jg
* 
* Strips references to a specified property from the defaultproperties
*
* input:	szProp TCHAR* The property to strip out
*			SourceStr FString The string to remove myLevel from
*
* ============================================================================
*/
UBOOL StripLine(const TCHAR* szProp,FString& SourceStr)
{
	guard(StripLine);
	// Find the property to pull out
	INT nIndex = SourceStr.InStr(szProp,1);
	if (nIndex != -1)
	{
		// Grab everything up to the property
		FString Left(SourceStr.Left(nIndex));
		// Now split the string here so it can be parsed
		FString Right(SourceStr.Right(SourceStr.Len() - nIndex));
		// Find the CR at the end of this property
		INT nCR = Right.InStr(TEXT("\r\n"));
		if (nCR != -1)
		{
			// Snip out the property + CRLF
			Right = Right.Right(Right.Len() - nCR);
		}
		// Put the string back together
		SourceStr = Left + Right;
	}
	// Tell the caller if the line was found or not
	return nIndex != -1;
	unguard;
}

/* scion ======================================================================
* ::StripLines
* Author: jg
* 
* Strips all references to a specified property from the defaultproperties
*
* input:	szProp TCHAR* The property to strip out
*			SourceStr FString The string to remove myLevel from
*
* ============================================================================
*/
void StripLines(const TCHAR* szProp,FString& SourceStr)
{
	guard(StripLines);
	UBOOL bFound = 1;
	while (bFound)
	{
		bFound = StripLine(szProp,SourceStr);
	}
	unguard;
}

/* scion ======================================================================
* ::StripOuter
* Author: jg
* 
* Strips references to the outer from a defaultproperties export
*
* input:	szOuter The outer string to strip out ("myLevel.", "Emitter1.", etc)
*			SourceStr FString The string to remove myLevel/outer from
*
* ============================================================================
*/
void StripOuter(const TCHAR* szOuter,FString& SourceStr)
{
	guard(StripMyLevel);
	// Pull out all "myLevel."/outer text since that screws up in defaultprops
	INT nMyLevelIndex = -1;
	do
	{
		// See if there are anymore myLevel. refs
		nMyLevelIndex = SourceStr.InStr(szOuter);
		if (nMyLevelIndex != -1)
		{
			// Grab the right side of the myLevel. string
			FString Right(SourceStr.Right(SourceStr.Len() -
				nMyLevelIndex - appStrlen(szOuter)));
			// Grab the left side of the myLevel.
			SourceStr = SourceStr.Left(nMyLevelIndex);
			// Rebuild into one
			SourceStr += Right;
		}
	}
	while(nMyLevelIndex != -1);
	unguard;
}

/*-----------------------------------------------------------------------------
	WMdiClient.
-----------------------------------------------------------------------------*/

// An MDI client window.
class WMdiClient : public WControl
{
	DECLARE_WINDOWSUBCLASS(WMdiClient,WControl,UnrealEd)
	WMdiClient( WWindow* InOwner )
	: WControl( InOwner, 0, SuperProc )
	{}
	void OpenWindow( CLIENTCREATESTRUCT* ccs )
	{
		guard(WMdiFrame::OpenWindow);
		//must make nccreate work!! GetWindowClassName(),
		//!! WS_VSCROLL | WS_HSCROLL
        HWND hWndCreated = TCHAR_CALL_OS(CreateWindowEx(0,TEXT("MDICLIENT"),NULL,WS_CHILD|WS_CLIPCHILDREN | WS_CLIPSIBLINGS,0,0,0,0,OwnerWindow->hWnd,(HMENU)0xCAC,hInstance,ccs),CreateWindowExA(0,"MDICLIENT",NULL,WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,0,0,OwnerWindow->hWnd,(HMENU)0xCAC,hInstance,ccs));
		check(hWndCreated);
		check(!hWnd);
		_Windows.AddItem( this );
		hWnd = hWndCreated;
		Show( 1 );
		unguard;
	}
};
WNDPROC WMdiClient::SuperProc;

#include "DockingFrame.h"
#include "MDIFrame.h"

#include "ButtonBar.h"
#include "BottomBar.h"
#include "TopBar.h"
WButtonBar* GButtonBar;
WBottomBar* GBottomBar;
WTopBar* GTopBar;

void FileOpen( HWND hWnd );

// InFlags - bitflag using values from ERefreshEditor
void RefreshEditor( INT InFlags )
{
	guard(RefreshEditor);

	if( InFlags&ERefreshEditor_ActorBrowser )
		GBrowserMaster->RefreshBrowser( eBROWSER_ACTOR );
	if( InFlags&ERefreshEditor_GroupBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_GROUP );
	if( InFlags&ERefreshEditor_MeshBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_MESH );
	if( InFlags&ERefreshEditor_AnimationBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_ANIMATION );
	if( InFlags&ERefreshEditor_MusicBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_MUSIC );
	if( InFlags&ERefreshEditor_PrefabBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_PREFAB );
	if( InFlags&ERefreshEditor_SoundBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_SOUND );
	if( InFlags&ERefreshEditor_StaticMeshBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_STATICMESH );
	if( InFlags&ERefreshEditor_TextureBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_TEXTURE );

#ifdef WITH_LIPSINC
	if( InFlags&ERefreshEditor_LIPSincBrowser ) GBrowserMaster->RefreshBrowser( eBROWSER_LIPSINC );
#endif

	if( InFlags&ERefreshEditor_Matinee )
	{
		if( GMatineeSheet )
		{
			GMatineeSheet->Refresh();
			GMatineeSheet->PropSheet->RefreshPages();
		}
	}
	if( InFlags&ERefreshEditor_Terrain )
		if( GTerrainEditSheet ) GTerrainEditSheet->PropSheet->RefreshPages();

	if( InFlags&ERefreshEditor_Misc )
	{
		if( GBuildSheet ) GBuildSheet->PropSheet->RefreshPages();
		if( GBottomBar ) GBottomBar->BottomBarStandard->Refresh();
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	WBackgroundHolder.
-----------------------------------------------------------------------------*/

// Test.
class WBackgroundHolder : public WWindow
{
	DECLARE_WINDOWCLASS(WBackgroundHolder,WWindow,Window)

	// Structors.
	WBackgroundHolder( FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
	{}

	// WWindow interface.
	void OpenWindow()
	{
		guard(WBackgroundHolder::OpenWindow);
		MdiChild = 0;
		PerformCreateWindowEx
		(
			WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE,
			NULL,
			WS_CHILD | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			0,
			0,
			512,
			256,
			OwnerWindow ? OwnerWindow->hWnd : NULL,
			NULL,
			hInstance
		);
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WLevelFrame.
-----------------------------------------------------------------------------*/

enum eBIMODE {
	eBIMODE_CENTER	= 0,
	eBIMODE_TILE	= 1,
	eBIMODE_STRETCH	= 2
};

class WLevelFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WLevelFrame,WWindow,Window)

	// Variables.
	ULevel* Level;
	HBITMAP hImage;
	FString BIFilename;
	INT BIMode;	// eBIMODE_

	// Structors.
	WLevelFrame( ULevel* InLevel, FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
	,	Level( InLevel )
	{
		SetMapFilename( TEXT("") );
		hImage = NULL;
		BIMode = eBIMODE_CENTER;
		BIFilename = TEXT("");

		for( INT x = 0 ; x < GViewports.Num() ; x++)
			GViewports(x).ViewportFrame = NULL;
		GViewports.Empty();
	}
	void SetMapFilename( TCHAR* InMapFilename )
	{
		appStrcpy( MapFilename, InMapFilename );
		if( ::IsWindow( hWnd ) )
			SetText( MapFilename );
	}
	TCHAR* GetMapFilename()
	{
		return MapFilename;
	}

	void OnDestroy()
	{
		guard(WLevelFrame::OnDestroy);

		ChangeViewportStyle();

		for( INT group = 0 ; group < GButtonBar->Groups.Num() ; ++group )
			GConfig->SetInt( TEXT("Groups"), *GButtonBar->Groups(group).GroupName, GButtonBar->Groups(group).iState, TEXT("UnrealEd.ini") );

		// Save data out to config file, and clean up...
		GConfig->SetInt( TEXT("Viewports"), TEXT("Style"), GViewportStyle, TEXT("UnrealEd.ini") );
		GConfig->SetInt( TEXT("Viewports"), TEXT("Config"), GViewportConfig, TEXT("UnrealEd.ini") );

		for( INT x = 0 ; x < GViewports.Num() ; x++)
		{
			TCHAR l_chName[20];
			appSprintf( l_chName, TEXT("U2Viewport%d"), x);

			if( GViewports(x).ViewportFrame 
					&& ::IsWindow( GViewports(x).ViewportFrame->hWnd ) 
					&& !::IsIconic( GViewports(x).ViewportFrame->hWnd )
					&& !::IsZoomed( GViewports(x).ViewportFrame->hWnd ))
			{
				FRect R = GViewports(x).ViewportFrame->GetWindowRect();
			
				GConfig->SetInt( l_chName, TEXT("Active"), 1, TEXT("UnrealEd.ini") );
				GConfig->SetInt( l_chName, TEXT("RendMap"), GViewports(x).ViewportFrame->Viewport->Actor->RendMap, TEXT("UnrealEd.ini") );

				GConfig->SetFloat( l_chName, TEXT("PctLeft"), GViewports(x).PctLeft, TEXT("UnrealEd.ini") );
				GConfig->SetFloat( l_chName, TEXT("PctTop"), GViewports(x).PctTop, TEXT("UnrealEd.ini") );
				GConfig->SetFloat( l_chName, TEXT("PctRight"), GViewports(x).PctRight, TEXT("UnrealEd.ini") );
				GConfig->SetFloat( l_chName, TEXT("PctBottom"), GViewports(x).PctBottom, TEXT("UnrealEd.ini") );

				GConfig->SetFloat( l_chName, TEXT("Left"), GViewports(x).Left, TEXT("UnrealEd.ini") );
				GConfig->SetFloat( l_chName, TEXT("Top"), GViewports(x).Top, TEXT("UnrealEd.ini") );
				GConfig->SetFloat( l_chName, TEXT("Right"), GViewports(x).Right, TEXT("UnrealEd.ini") );
				GConfig->SetFloat( l_chName, TEXT("Bottom"), GViewports(x).Bottom, TEXT("UnrealEd.ini") );

				FString Device = GViewports(x).ViewportFrame->Viewport->RenDev->GetClass()->GetFullName();
				Device = Device.Right( Device.Len() - Device.InStr( TEXT(" "), 0 ) - 1 );
				GConfig->SetString( l_chName, TEXT("Device"), *Device, TEXT("UnrealEd.ini") );
			}
			else {

				GConfig->SetInt( l_chName, TEXT("Active"), 0, TEXT("UnrealEd.ini") );
			}

			delete GViewports(x).ViewportFrame;                        
			GViewports(x).ViewportFrame = 0; 
		}

		// "Last Directory"
		GConfig->SetString( TEXT("Directories"), TEXT("PCX"), *GLastDir[eLASTDIR_PCX], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("WAV"), *GLastDir[eLASTDIR_WAV], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("BRUSH"), *GLastDir[eLASTDIR_BRUSH], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("2DS"), *GLastDir[eLASTDIR_2DS], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("PSK"), *GLastDir[eLASTDIR_PSA], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("PSA"), *GLastDir[eLASTDIR_PSK], TEXT("UnrealEd.ini") );

#ifdef WITH_LIPSINC
		GConfig->SetString( TEXT("Directories"), TEXT("LBP"), *GLastDir[eLASTDIR_LBP], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("LTF"), *GLastDir[eLASTDIR_LTF], TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Directories"), TEXT("IEX"), *GLastDir[eLASTDIR_IEX], TEXT("UnrealEd.ini") );
#endif

		// Background image
		GConfig->SetInt( TEXT("Background Image"), TEXT("Active"), (hImage != NULL), TEXT("UnrealEd.ini") );
		GConfig->SetInt( TEXT("Background Image"), TEXT("Mode"), BIMode, TEXT("UnrealEd.ini") );
		GConfig->SetString( TEXT("Background Image"), TEXT("Filename"), *BIFilename, TEXT("UnrealEd.ini") );

		::DeleteObject( hImage );                
		hImage = 0;

		GViewports.Empty();

		unguard;
	}
	// Looks for an empty viewport slot, allocates a viewport and returns a pointer to it.
	WViewportFrame* NewViewportFrame( FName* pName, UBOOL bNoSize )
	{
		guard(WLevelFrame::NewViewportFrame);

		// Clean up dead windows first.
		for( INT x = 0 ; x < GViewports.Num() ; x++)
			if( GViewports(x).ViewportFrame && !::IsWindow( GViewports(x).ViewportFrame->hWnd ) )
				GViewports.Remove(x);

		if( GViewports.Num() > dED_MAX_VIEWPORTS )
		{
			appMsgf( 0, TEXT("You are at the limit for open viewports.") );
			return NULL;
		}

		// Make up a unique name for this viewport.
		TCHAR l_chName[20];
		for( INT x = 0 ; x < dED_MAX_VIEWPORTS ; x++)
		{
			appSprintf( l_chName, TEXT("U2Viewport%d"), x);

			// See if this name is already taken
			BOOL bIsUnused = 1;
			for( INT y = 0 ; y < GViewports.Num() ; y++)
				if( !appStricmp(GViewports(y).ViewportFrame->Viewport->GetName(),l_chName) )
				{
					bIsUnused = 0;
					break;
				}

			if( bIsUnused )
				break;
		}

		*pName = l_chName;

		// Create the viewport.
		new(GViewports)VIEWPORTCONFIG();
		INT Index = GViewports.Num() - 1;
		GViewports(Index).PctLeft = 0;
		GViewports(Index).PctTop = 0;
		GViewports(Index).PctRight = bNoSize ? 0 : 50;
		GViewports(Index).PctBottom = bNoSize ? 0 : 50;
		GViewports(Index).Left = 0;
		GViewports(Index).Top = 0;
		GViewports(Index).Right = bNoSize ? 0 : 320;
		GViewports(Index).Bottom = bNoSize ? 0 : 200;
		GViewports(Index).ViewportFrame = new WViewportFrame( *pName, this );
		GViewports(Index).ViewportFrame->m_iIdx = Index;

		return GViewports(Index).ViewportFrame;

		unguard;
	}
	// Changes the visual style of all open viewports to whatever the current style is.  This is also good
	// for forcing all viewports to recompute their positional data.
	void ChangeViewportStyle()
	{
		guard(WLevelFrame::ChangeViewportStyle);

		for( INT x = 0 ; x < GViewports.Num() ; x++)
		{
			if( GViewports(x).ViewportFrame && ::IsWindow( GViewports(x).ViewportFrame->hWnd ) )
			{
				switch( GViewportStyle )
				{
					case VSTYLE_Floating:
						SetWindowLongA( GViewports(x).ViewportFrame->hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
						break;
					case VSTYLE_Fixed:
						SetWindowLongA( GViewports(x).ViewportFrame->hWnd, GWL_STYLE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
						break;
				}

				GViewports(x).ViewportFrame->ComputePositionData();
				SetWindowPos( GViewports(x).ViewportFrame->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE );

				GViewports(x).ViewportFrame->AdjustToolbarButtons();
			}
		}

		unguard;
	}
	// Resizes all existing viewports to fit properly on the screen.
	void FitViewportsToWindow()
	{
		guard(WLevelFrame::FitViewportsToWindow);

		RECT R;
		::GetClientRect( GLevelFrame->hWnd, &R );
		FDeferWindowPos dwp;

		for( INT x = 0 ; x < GViewports.Num() ; x++)
		{
			VIEWPORTCONFIG* pVC = &(GViewports(GViewports(x).ViewportFrame->m_iIdx));
			if( GViewportStyle == VSTYLE_Floating )
				dwp.MoveWindow(GViewports(x).ViewportFrame->hWnd,
					pVC->Left, pVC->Top, pVC->Right, pVC->Bottom, 1);
			else
				dwp.MoveWindow(GViewports(x).ViewportFrame->hWnd,
					pVC->PctLeft * R.right, pVC->PctTop * R.bottom,
					pVC->PctRight * R.right, pVC->PctBottom * R.bottom, 1);
		}
		unguard;
	}
	void CreateNewViewports( INT InStyle, INT InConfig )
	{
		guard(WLevelFrame::CreateNewViewports);

		GViewportStyle = InStyle;
		GViewportConfig = InConfig;

		// Get rid of any existing viewports.
		for( INT x = 0 ; x < GViewports.Num() ; x++)
		{
			delete GViewports(x).ViewportFrame;
			GViewports(x).ViewportFrame = NULL;
		}
		GViewports.Empty();

		// Create new viewports
		switch( GViewportConfig )
		{
			case 0:		// classic
			{
				GLevelFrame->OpenFrameViewport( REN_OrthXY,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_OrthXZ,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_DynLight,0,0,10,10,GShowFlags | SHOW_Terrain | SHOW_DistanceFog | SHOW_Coronas | SHOW_Particles | SHOW_BSP);
				GLevelFrame->OpenFrameViewport( REN_OrthYZ,0,0,10,10,GShowFlags );
			}
			break;

			case 1:		// big one on buttom, small ones along top
			{
				GLevelFrame->OpenFrameViewport( REN_OrthXY,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_OrthXZ,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_OrthYZ,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_DynLight,0,0,10,10,GShowFlags | SHOW_Terrain | SHOW_DistanceFog | SHOW_Coronas | SHOW_Particles | SHOW_BSP );
			}
			break;

			case 2:		// big one on left side, small along right
			{
				GLevelFrame->OpenFrameViewport( REN_DynLight,0,0,10,10,GShowFlags | SHOW_Terrain | SHOW_DistanceFog | SHOW_Coronas | SHOW_Particles | SHOW_BSP);
				GLevelFrame->OpenFrameViewport( REN_OrthXY,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_OrthXZ,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_OrthYZ,0,0,10,10,GShowFlags );
			}
			break;

			case 3:		// 2 large windows, split horizontally
			{
				GLevelFrame->OpenFrameViewport( REN_OrthXY,0,0,10,10,GShowFlags );
				GLevelFrame->OpenFrameViewport( REN_DynLight,0,0,10,10,GShowFlags | SHOW_Terrain | SHOW_DistanceFog | SHOW_Coronas | SHOW_Particles | SHOW_BSP);
			}
			break;
		}

		// Load initial data from templates
		for( INT x = 0 ; x < GViewports.Num() ; ++x )
			if( GTemplateViewportConfigs[0][x].PctLeft != -1 )
			{
				GViewports(x).PctLeft = GTemplateViewportConfigs[GViewportConfig][x].PctLeft;
				GViewports(x).PctTop = GTemplateViewportConfigs[GViewportConfig][x].PctTop;
				GViewports(x).PctRight = GTemplateViewportConfigs[GViewportConfig][x].PctRight;
				GViewports(x).PctBottom = GTemplateViewportConfigs[GViewportConfig][x].PctBottom;
			}

		// Set the viewports to their proper sizes.
		INT SaveViewportStyle = VSTYLE_Fixed;
		Exchange( GViewportStyle, SaveViewportStyle );
		FitViewportsToWindow();
		Exchange( SaveViewportStyle, GViewportStyle );
		ChangeViewportStyle();

		unguard;
	}
	// WWindow interface.
	void OnKillFocus( HWND hWndNew )
	{
		guard(WLevelFrame::OnKillFocus);
		GUnrealEd->Client->MakeCurrent( NULL );
		unguard;
	}
	void Serialize( FArchive& Ar )
	{
		guard(WLevelFrame::Serialize);
		WWindow::Serialize( Ar );
		Ar << Level;
		unguard;
	}
	void OpenWindow( UBOOL bMdi, UBOOL bMax )
	{
		guard(WLevelFrame::OpenWindow);
		MdiChild = bMdi;
		PerformCreateWindowEx
		(
			MdiChild
			?	(WS_EX_MDICHILD)
			:	(0),
			TEXT("Level"),
			(bMax ? WS_MAXIMIZE : 0 ) |
			(MdiChild
			?	(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
			:	(WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS)),
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			512,
			384,
			MdiChild ? OwnerWindow->OwnerWindow->hWnd : OwnerWindow->hWnd,
			NULL,
			hInstance
		);
		if( !MdiChild )
		{
			SetWindowLongA( hWnd, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
			OwnerWindow->Show(1);
		}

		// Open the proper configuration of viewports.
		if(!GConfig->GetInt( TEXT("Viewports"), TEXT("Style"), GViewportStyle, TEXT("UnrealEd.ini") ))		GViewportStyle = VSTYLE_Fixed;
		if(!GConfig->GetInt( TEXT("Viewports"), TEXT("Config"), GViewportConfig, TEXT("UnrealEd.ini") ))	GViewportConfig = 0;

		for( INT x = 0 ; x < dED_MAX_VIEWPORTS ; x++)
		{
			TCHAR l_chName[20];
			appSprintf( l_chName, TEXT("U2Viewport%d"), x);
			INT Active, RendMap;

			if(!GConfig->GetInt( l_chName, TEXT("Active"), Active, TEXT("UnrealEd.ini") ))		Active = 0;

			if( Active )
			{
				if(!GConfig->GetInt( l_chName, TEXT("RendMap"), RendMap, TEXT("UnrealEd.ini") ))	RendMap = REN_OrthXY;

				OpenFrameViewport( RendMap, 0, 0, 50, 50, GShowFlags | ((RendMap == REN_DynLight || RendMap == REN_PlainTex || RendMap == REN_LightingOnly || RendMap == REN_Zones || RendMap == REN_Wire) ? SHOW_Terrain | SHOW_Coronas | SHOW_Particles : 0) | SHOW_DistanceFog | SHOW_BSP );
				VIEWPORTCONFIG* pVC = &(GViewports.Last());

				if(!GConfig->GetFloat( l_chName, TEXT("PctLeft"), pVC->PctLeft, TEXT("UnrealEd.ini") ))	pVC->PctLeft = 0;
				if(!GConfig->GetFloat( l_chName, TEXT("PctTop"), pVC->PctTop, TEXT("UnrealEd.ini") ))	pVC->PctTop = 0;
				if(!GConfig->GetFloat( l_chName, TEXT("PctRight"), pVC->PctRight, TEXT("UnrealEd.ini") ))	pVC->PctRight = .5f;
				if(!GConfig->GetFloat( l_chName, TEXT("PctBottom"), pVC->PctBottom, TEXT("UnrealEd.ini") ))	pVC->PctBottom = .5f;
				if(!GConfig->GetFloat( l_chName, TEXT("Left"), pVC->Left, TEXT("UnrealEd.ini") ))	pVC->Left = 0;
				if(!GConfig->GetFloat( l_chName, TEXT("Top"), pVC->Top, TEXT("UnrealEd.ini") ))	pVC->Top = 0;
				if(!GConfig->GetFloat( l_chName, TEXT("Right"), pVC->Right, TEXT("UnrealEd.ini") ))	pVC->Right = 320;
				if(!GConfig->GetFloat( l_chName, TEXT("Bottom"), pVC->Bottom, TEXT("UnrealEd.ini") ))	pVC->Bottom = 200;

				FString Device;
				INT SizeX, SizeY;
				SizeX = pVC->ViewportFrame->Viewport->SizeX;
				SizeY = pVC->ViewportFrame->Viewport->SizeY;

				GConfig->GetString( l_chName, TEXT("Device"), Device, TEXT("UnrealEd.ini") );
				if( !Device.Len() )		Device = TEXT("D3DDrv.D3DRenderDevice");

				pVC->ViewportFrame->Viewport->TryRenderDevice( *Device, SizeX, SizeY, 0 );
			}
		}

		FitViewportsToWindow();

		// Background image
		UBOOL bActive;
		if(!GConfig->GetInt( TEXT("Background Image"), TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = 0;

		if( bActive )
		{
			if(!GConfig->GetInt( TEXT("Background Image"), TEXT("Mode"), BIMode, TEXT("UnrealEd.ini") ))	BIMode = eBIMODE_CENTER;
			if(!GConfig->GetString( TEXT("Background Image"), TEXT("Filename"), BIFilename, TEXT("UnrealEd.ini") ))	BIFilename.Empty();
			LoadBackgroundImage(BIFilename);
		}

		unguard;
	}
	void LoadBackgroundImage( FString Filename )
	{
		guard(WLevelFrame::LoadBackgroundImage);

		if( hImage ) 
			DeleteObject( hImage );

		hImage = (HBITMAP)LoadImageA( hInstance, appToAnsi( *Filename ), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE );

		if( hImage )
			BIFilename = Filename;
		else
			appMsgf ( 0, TEXT("Error loading bitmap for background image.") );

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WLevelFrame::OnSize);
		WWindow::OnSize( Flags, NewX, NewY );

		FitViewportsToWindow();

		unguard;
	}
	INT OnSetCursor()
	{
		guard(WLevelFrame::OnSetCursor);
		WWindow::OnSetCursor();
		SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
		return 0;
		unguard;
	}
	void OnPaint()
	{
		guard(WLevelFrame::OnPaint);
		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS );
		FillRect( hDC, GetClientRect(), (HBRUSH)(COLOR_WINDOW+1) );
		DrawImage( hDC );
		EndPaint( *this, &PS );

		// Put the name of the map into the titlebar.
		SetText( GetMapFilename() );

		unguard;
	}
	void DrawImage( HDC InHDC )
	{
		guard(WLevelFrame::DrawImage);
		if( !hImage ) return;

		HDC hdcMem;
		HBITMAP hbmOld;
		BITMAP bitmap;

		// Prepare the bitmap.
		//
		GetObjectA( hImage, sizeof(BITMAP), (LPSTR)&bitmap );
		hdcMem = CreateCompatibleDC(InHDC);
		hbmOld = (HBITMAP)SelectObject(hdcMem, hImage);

		// Display it.
		//
		RECT rc;
		::GetClientRect( hWnd, &rc );
		switch( BIMode )
		{
			case eBIMODE_CENTER:
			{
				BitBlt(InHDC,
				   (rc.right - bitmap.bmWidth) / 2, (rc.bottom - bitmap.bmHeight) / 2,
				   bitmap.bmWidth, bitmap.bmHeight,
				   hdcMem,
				   0, 0,
				   SRCCOPY);
			}
			break;

			case eBIMODE_TILE:
			{
				INT XSteps = (INT)(rc.right / bitmap.bmWidth) + 1;
				INT YSteps = (INT)(rc.bottom / bitmap.bmHeight) + 1;

				for( INT x = 0 ; x < XSteps ; ++x )
					for( INT y = 0 ; y < YSteps ; ++y )
						BitBlt(InHDC,
						   (x * bitmap.bmWidth), (y * bitmap.bmHeight),
						   bitmap.bmWidth, bitmap.bmHeight,
						   hdcMem,
						   0, 0,
						   SRCCOPY);
			}
			break;

			case eBIMODE_STRETCH:
			{
				StretchBlt(
					InHDC,
					0, 0,
					rc.right, rc.bottom,
					hdcMem,
					0, 0,
					bitmap.bmWidth, bitmap.bmHeight,
					SRCCOPY );
			}
			break;
		}

		// Clean up.
		//
		SelectObject(hdcMem, hbmOld);
		DeleteDC(hdcMem);
		unguard;
	}

	// Opens a new viewport window.  It creates a viewportframe of the specified size, then creates
	// a viewport that fits inside of it.
	virtual void OpenFrameViewport( INT RendMap, INT X, INT Y, INT W, INT H, DWORD ShowFlags )
	{
		guard(WLevelFrame::OpenFrameViewport);

		FName Name = TEXT("");

		// Open a viewport frame.
		WViewportFrame* pViewportFrame = NewViewportFrame( &Name, 1 );

		if( pViewportFrame ) 
		{
			pViewportFrame->OpenWindow();

			// Create the viewport inside of the frame.
			UViewport* Viewport = GUnrealEd->Client->NewViewport( Name );
			Level->SpawnViewActor( Viewport );
			Viewport->Actor->ShowFlags = ShowFlags;
			Viewport->Actor->RendMap   = RendMap;
			Viewport->Input->Init( Viewport );
			pViewportFrame->SetViewport( Viewport );
			::MoveWindow( (HWND)pViewportFrame->hWnd, X, Y, W, H, 1 );
			::BringWindowToTop( pViewportFrame->hWnd );
			pViewportFrame->ComputePositionData();
		}

		unguard;
	}
private:

	TCHAR MapFilename[512];
};

// gam ---

bool CanWriteFile( HWND hWnd, const TCHAR *FileName)
{
	TCHAR l_chMsg [2048];

    if (GFileManager->FileSize (FileName) < 0)
        return (true);

    if (!GFileManager->IsReadOnly (FileName))
        return (true);

    appSprintf
    (
        l_chMsg,
        TEXT("Can't write to %s -- it's read-only!\n"),
        FileName
    );

	MessageBox( hWnd, l_chMsg, TEXT("UnrealEd"), MB_OK | MB_ICONERROR );

    return( false );
}

// true if saved, false if cancelled
bool FileSaveAs( HWND hWnd )
{
	// Make sure we have a level loaded...
	if( !GLevelFrame ) { return (true); }

	OPENFILENAMEA ofn;
	char File[8192], *pFilename;
	TCHAR l_chCmd[512];
    bool bSaved = false;

	pFilename = TCHAR_TO_ANSI( GLevelFrame->GetMapFilename() );
	strcpy( File, pFilename );

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	char Filter[255];
	::sprintf( Filter,
		"Map Files (*.%s)%c*.%s%cAll Files%c*.*%c%c",
		appToAnsi( *GMapExt ),
		'\0',
		appToAnsi( *GMapExt ),
		'\0',
		'\0',
		'\0',
		'\0' );
	ofn.lpstrFilter = Filter;
	ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UNR]) );
	ofn.lpstrDefExt = appToAnsi( *GMapExt );
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

	// Display the Open dialog box. 
	if( GetSaveFileNameA(&ofn) )
	{
        if (!CanWriteFile( hWnd, ANSI_TO_TCHAR(File)))
            return (false);

		// Convert the ANSI filename to UNICODE, and tell the editor to open it.
		GUnrealEd->Exec( TEXT("BRUSHCLIP DELETE") );
		GUnrealEd->Exec( TEXT("POLYGON DELETE") );
		appSprintf( l_chCmd, TEXT("MAP SAVE FILE=\"%s\""), ANSI_TO_TCHAR(File));
		if( GUnrealEd->Exec( l_chCmd ) )
		{
			// Save the filename.
			GLevelFrame->SetMapFilename( ANSI_TO_TCHAR(File) );
			GMRUList->AddItem( GLevelFrame->GetMapFilename() );
			GMRUList->AddToMenu( hWnd, GMainMenu, 1 );
			GMRUList->WriteINI();

			FString S = ANSI_TO_TCHAR(File);
			GLastDir[eLASTDIR_UNR] = S.Left( S.InStr( TEXT("\\"), 1 ) );

			GUnrealEd->Trans->Reset( TEXT("Map Saved") );
			// gam ---
			GUnrealEd->Trans->HasBeenSaved ();
			// --- gam
			bSaved = true;
		}
	}

	GFileManager->SetDefaultDirectory(appBaseDir());

    return( bSaved ); // gam
}

// true if saved, false if cancelled
bool FileSave( HWND hWnd )
{
	if( GLevelFrame ) 
	{

		if( ::appStrlen( GLevelFrame->GetMapFilename() ) )
		{
            if (!CanWriteFile( hWnd, GLevelFrame->GetMapFilename()))
                return (false);

			GUnrealEd->Exec( TEXT("BRUSHCLIP DELETE") );
			GUnrealEd->Exec( TEXT("POLYGON DELETE") );
			GUnrealEd->Exec( *FString::Printf(TEXT("MAP SAVE FILE=\"%s\""), GLevelFrame->GetMapFilename()));

			GMRUList->AddItem( GLevelFrame->GetMapFilename() );
			GMRUList->AddToMenu( hWnd, GMainMenu, 1 );
			GMRUList->WriteINI();

			GUnrealEd->Trans->Reset( TEXT("Map Saved") );
            // gam ---
            GUnrealEd->Trans->HasBeenSaved ();
            return( true );
            // --- gam
		}
		else
			return FileSaveAs( hWnd );
	}

    return (true);
}
// -1 => cancel, 0 => no, 1 => yes
int FileSaveChangesFirst( HWND hWnd )
{
	TCHAR l_chMsg [2048];
    int rc;

	if( !GLevelFrame )
        return (0);
    
    if ( !GUnrealEd->Trans->NeedsToBeSaved() )
        return (0);

    if (appStrlen (GLevelFrame->GetMapFilename()))
    	appSprintf( l_chMsg, TEXT("Save changes to %s?"), GLevelFrame->GetMapFilename() );
    else
    	appSprintf( l_chMsg, TEXT("Save changes to map?") );

	rc = ::MessageBox( hWnd, l_chMsg, TEXT("UnrealEd"), MB_YESNOCANCEL | MB_ICONQUESTION);

    if (rc == IDYES)
        return 1;
    else if (rc == IDNO)
        return 0;
    else
        return -1;
}

// -1 => cancel, 0 => no, 1 => yes
int FileExportIntRequired (HWND hWnd)
{
    FString IntName;
    int rc;

    if (!GLevelFrame)
        return (0);

    if (!::appStrlen( GLevelFrame->GetMapFilename()))
        return (0);

    IntGetNameFromPackageName (GLevelFrame->GetMapFilename(), IntName);

    if (GFileManager->FileSize (*IntName) < 0)
	    rc = ::MessageBox( hWnd, TEXT("The internationalization file for this level does not exist. Do you want to export it now?"), TEXT("UnrealEd"), MB_YESNOCANCEL | MB_ICONQUESTION);
    else
    {
		FStringOutputDevice GetPropResult;
		GUnrealEd->Get( TEXT("LEV"), *(FString::Printf(TEXT("MATCHES_INT PACKAGE=%s"), GLevelFrame->GetMapFilename())), GetPropResult );

		if( !appStrcmp( *GetPropResult, TEXT("TRUE") ) )
            return (0);

	    rc = ::MessageBox( hWnd, TEXT("The internationalization file for this level is out of date. Do you want to export it now?"), TEXT("UnrealEd"), MB_YESNOCANCEL | MB_ICONQUESTION);
    }

    if (rc == IDYES)
        return (1);
    else if (rc == IDNO)
        return (0);
    else
        return (-1);
}

void FileIntExportAs (HWND hWnd)
{
	OPENFILENAMEA ofn;
	char File[8192] = "\0";

    FString IntName;
    
    check (GLevelFrame);
    check (::appStrlen( GLevelFrame->GetMapFilename()));

    if (::appStrlen( GLevelFrame->GetMapFilename()))
    {
        IntGetNameFromPackageName (GLevelFrame->GetMapFilename(), IntName);
        strcpy (File, appToAnsi (*IntName));
    }

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Internationalization Files (*.int)\0*.int\0All Files\0*.*\0\0";
	ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UNR]) );
	ofn.lpstrDefExt = "t3d";
	ofn.lpstrTitle = "Export Map";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

	if( GetSaveFileNameA(&ofn) )
	{
        if (!CanWriteFile( hWnd, ANSI_TO_TCHAR(File)))
            return;

    	GEditor->Exec( *(FString::Printf(TEXT("DUMPINT FILE=\"%s\" FRESH=1"), appFromAnsi( File ))) );

		FString S = appFromAnsi( File );
		GLastDir[eLASTDIR_UNR] = S.Left( S.InStr( TEXT("\\"), 1 ) );
	}

	GFileManager->SetDefaultDirectory(appBaseDir());
}

bool FileIntExport (HWND hWnd)
{
    FString IntName;

    check (GLevelFrame);
    check (::appStrlen( GLevelFrame->GetMapFilename()));

    IntGetNameFromPackageName (GLevelFrame->GetMapFilename(), IntName);

    if (!CanWriteFile( hWnd, *IntName))
        return (false);

  	GEditor->Exec( *(FString::Printf(TEXT("DUMPINT FILE=\"%s\" FRESH=1"), *IntName)) );
	GFileManager->SetDefaultDirectory(appBaseDir());

    return (true);
}

bool FileIntImport (HWND hWnd)
{
    FString IntName;

    if (!GLevelFrame)
        return (true);

    if (!::appStrlen( GLevelFrame->GetMapFilename()))
    {
        debugf (NAME_Warning, TEXT("Can't import until map has a name.") );
        return (false);
	}

    IntGetNameFromPackageName (GLevelFrame->GetMapFilename(), IntName);
  	GEditor->Exec( *(FString::Printf(TEXT("LOADINT FILE=\"%s\""), *IntName )) );

    return (true);
}

// --- gam

enum {
	GI_NUM_SELECTED			= 1,
	GI_CLASSNAME_SELECTED	= 2,
	GI_NUM_SURF_SELECTED	= 4,
	GI_CLASS_SELECTED		= 8
};

typedef struct
{
	INT iValue;
	FString String;
	UClass*	pClass;
} FGetInfoRet;

FGetInfoRet GetInfo( ULevel* Level, INT Item )
{
	guard(GetInfo);

	FGetInfoRet Ret;

	Ret.iValue = 0;
	Ret.String = TEXT("");

	// ACTORS
	if( Item & GI_NUM_SELECTED
			|| Item & GI_CLASSNAME_SELECTED 
			|| Item & GI_CLASS_SELECTED )
	{
		INT NumActors = 0;
		BOOL bAnyClass = FALSE;
		UClass*	AllClass = NULL;

		for( INT i=0; i<Level->Actors.Num(); ++i )
		{
			if( Level->Actors(i) && Level->Actors(i)->bSelected )
			{
				if( bAnyClass && Level->Actors(i)->GetClass() != AllClass ) 
					AllClass = NULL;
				else 
					AllClass = Level->Actors(i)->GetClass();

				bAnyClass = TRUE;
				NumActors++;
			}
		}

		if( Item & GI_NUM_SELECTED )
		{
			Ret.iValue = NumActors;
		}
		if( Item & GI_CLASSNAME_SELECTED )
		{
			if( bAnyClass && AllClass )
				Ret.String = AllClass->GetName();
			else 
				Ret.String = TEXT("Actor");
		}
		if( Item & GI_CLASS_SELECTED )
		{
			if( bAnyClass && AllClass )
				Ret.pClass = AllClass;
			else 
				Ret.pClass = NULL;
		}
	}

	// SURFACES
	if( Item & GI_NUM_SURF_SELECTED)
	{
		INT NumSurfs = 0;

		for( INT i=0; i<Level->Model->Surfs.Num(); ++i )
		{
			FBspSurf *Poly = &Level->Model->Surfs(i);

			if( Poly->PolyFlags & PF_Selected )
			{
				NumSurfs++;
			}
		}

		if( Item & GI_NUM_SURF_SELECTED )
		{
			Ret.iValue = NumSurfs;
		}
	}

	return Ret;

	unguard;
}

void ShowCodeFrame( WWindow* Parent )
{
	if( GCodeFrame
			&& ::IsWindow( GCodeFrame->hWnd ) )
	{
		GCodeFrame->Show(1);
		::BringWindowToTop( GCodeFrame->hWnd );
	}
}

// Initializes the classes for the generic dialog system (UOption* classes).  This should be called ONCE per
// editor instance.
void OptionProxyInit()
{
	guard(OptionProxyInit);

	// To edit color fields, we need a pointer to the color struct in UObject.
	UClass* Class = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Core.Object") );	check(Class);
	for( TFieldIterator<UStruct> It(Class) ; It ; ++It )
		if(It->GetFName() == NAME_Color)	GColorStruct = *It;

	CREATE_OPTION_PROXY( OPTIONS_BRUSHSCALE, UOptionsBrushScale );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPERSHEET, UOptions2DShaperSheet );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPEREXTRUDE, UOptions2DShaperExtrude );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPEREXTRUDETOPOINT, UOptions2DShaperExtrudeToPoint );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPEREXTRUDETOBEVEL, UOptions2DShaperExtrudeToBevel );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPERREVOLVE, UOptions2DShaperRevolve );
	CREATE_OPTION_PROXY( OPTIONS_2DSHAPERBEZIERDETAIL, UOptions2DShaperBezierDetail );
	CREATE_OPTION_PROXY( OPTIONS_SURFBEVEL, UOptionsSurfBevel );
	CREATE_OPTION_PROXY( OPTIONS_TEXALIGNPLANAR, UOptionsTexAlignPlanar );
	CREATE_OPTION_PROXY( OPTIONS_TEXALIGNCYLINDER, UOptionsTexAlignCylinder );
	CREATE_OPTION_PROXY( OPTIONS_NEWTERRAIN, UOptionsNewTerrain );
	CREATE_OPTION_PROXY( OPTIONS_NEWTERRAINLAYER, UOptionsNewTerrainLayer );
	CREATE_OPTION_PROXY( OPTIONS_MAPSCALE, UOptionsMapScale );
	CREATE_OPTION_PROXY( OPTIONS_MATNEWCAMERAPATH, UOptionsMatNewCameraPath );
	CREATE_OPTION_PROXY( OPTIONS_MATNEWSTATICMESH, UOptionsMatNewStaticMesh );
	CREATE_OPTION_PROXY( OPTIONS_DUPTEXTURE, UOptionsDupTexture );
	CREATE_OPTION_PROXY( OPTIONS_ROTATEACTORS, UOptionsRotateActors);
	CREATE_OPTION_PROXY( OPTIONS_NEWCLASSFROMSEL, UOptionsNewClassFromSel );

	unguard;
}

// jij ---
// Searches a list of filenames and replaces all single NULL's with | characters.  This allows
// the regular parse routine to work correctly.  The return value is the number of NULL's
// that were replaced -- if this is greater than zero, you have multiple filenames.
//
INT FormatFilenames( char* _pchFilenames )
{
	guard(WBrowser::FormatFilenames);
	char *pch = _pchFilenames;
	INT l_iNULLs = 0;

	while( true )
	{
		if( *pch == '\0' )
		{
			if( *(pch+1) == '\0') break;

			*pch = '|';
			l_iNULLs++;
		}
		pch++;
	}

	return l_iNULLs;
	unguard;
}
// --- jij

/*-----------------------------------------------------------------------------
	WEditorFrame.
-----------------------------------------------------------------------------*/

// Editor frame window.
class WEditorFrame : public WMdiFrame, public FNotifyHook
{
	DECLARE_WINDOWCLASS(WEditorFrame,WMdiFrame,UnrealEd)

	// Variables.
	WBackgroundHolder BackgroundHolder;
	WConfigProperties* Preferences;

	// Constructors.
	WEditorFrame()
	: WMdiFrame( TEXT("EditorFrame") )
	, BackgroundHolder( NAME_None, &MdiClient )
	, Preferences( NULL )
	{
	}

	// WWindow interface.
	void OnCreate()
	{
		guard(WEditorFrame::OnCreate);
		WMdiFrame::OnCreate();
		SetText( *(FString::Printf( LocalizeGeneral(TEXT("FrameWindow"),TEXT("UnrealEd")), LocalizeGeneral(TEXT("Product"),TEXT("Core"))) + FString(TEXT(" - ")) + FString(GBuildLabel) )); // gam

		// Create MDI client.
		CLIENTCREATESTRUCT ccs;
        ccs.hWindowMenu = NULL; 
        ccs.idFirstChild = 60000;
		MdiClient.OpenWindow( &ccs );

		// Background.
		BackgroundHolder.OpenWindow();

		// Set up progress dialog.
		GDlgProgress = new WDlgProgress( NULL, this );
		GDlgProgress->DoModeless(0);

		GDlgMapCheck = new WDlgMapCheck( NULL, this );
		GDlgMapCheck->DoModeless(0);

		GDlgLoadErrors = new WDlgLoadErrors( NULL, this );
		GDlgLoadErrors->DoModeless(0);

		Warn.hWndProgressBar = ::GetDlgItem( GDlgProgress->hWnd, IDPG_PROGRESS);
		Warn.hWndProgressText = ::GetDlgItem( GDlgProgress->hWnd, IDSC_MSG);
		Warn.hWndProgressDlg = GDlgProgress->hWnd;
		Warn.hWndMapCheckDlg = GDlgMapCheck->hWnd;

		// 3DBuzz - Create the array dialog to use later
		GDlgArray = new WDlgArray( NULL, this );
		GDlgArray->DoModeless(0);

		GDlgSearchActors = new WDlgSearchActors( NULL, this );
		GDlgSearchActors->DoModeless(0);

		GDlgScaleLights = new WDlgScaleLights( NULL, this );
		GDlgScaleLights->DoModeless(0);

		GDlgTOTD = new WDlgTipOfTheDay( NULL, this );
		GDlgTOTD->DoModeless(0);

		GDlgTexReplace = new WDlgTexReplace( NULL, this );
		GDlgTexReplace->DoModeless(0);

		GUDNWindow = new WUDNWindow( this );
		GUDNWindow->OpenWindow();

		GEditorFrame = this;
		GEditorFrameW = this;
		GMdiFrame = (WMdiFrame*)this;

		unguard;
	}
	virtual void OnTimer()
	{
		guard(WEditorFrame::OnTimer);
		if( GUnrealEd->AutoSave )
			GUnrealEd->Exec( TEXT("MAYBEAUTOSAVE") );
		unguard;
	}
	void RepositionClient()
	{
		guard(WEditorFrame::RepositionClient);
		LockWindowUpdate( hWnd );
		WMdiFrame::RepositionClient();
		BackgroundHolder.MoveWindow( MdiClient.GetClientRect(), 1 );
		LockWindowUpdate( NULL );
		unguard;
	}
	void doOnClose()
	{
		guard(WEditorFrame::doOnClose);

		::DestroyWindow( GLevelFrame->hWnd );
		delete GLevelFrame;

		KillTimer( hWnd, 900 );

		GMRUList->WriteINI();

		delete GSurfPropSheet;
		delete GTerrainEditSheet;
		delete GMatineeSheet;
		delete GBuildSheet;
		delete G2DShapeEditor;
		delete GBottomBar;
		delete GTopBar;
		delete GButtonBar;
		SAFEDELETENULL(GBrowserSound);
		SAFEDELETENULL(GBrowserMusic);
		SAFEDELETENULL(GBrowserGroup);

#ifdef WITH_LIPSINC
		SAFEDELETENULL(GBrowserLIPSinc);
#endif

		SAFEDELETENULL(GBrowserMaster);
		SAFEDELETENULL(GBrowserActor);
		SAFEDELETENULL(GBrowserTexture);
		SAFEDELETENULL(GBrowserStaticMesh);
		SAFEDELETENULL(GBrowserMesh);
		SAFEDELETENULL(GBrowserPrefab);
		SAFEDELETENULL(GParticleEditor); // Added by Demiurge (PSE)
		delete GDlgLoadErrors;
		delete GDlgMapCheck;
		delete GDlgAddSpecial;
		delete GDlgScaleLights;
		delete GDlgTOTD;
		delete GDlgProgress;
		delete GDlgSearchActors;
		delete GDlgTexReplace;
		delete GUDNWindow;
		
		// 3DBuzz - Remove the array dialog from memory
		delete GDlgArray;

		unguard;
	}

	bool OnClose()
	{
		guard(WEditorFrame::OnClose);

                // gam ---
		int rc = FileSaveChangesFirst( GLevelFrame->hWnd );
		if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
			return (false);

		if( GEditor->ShowIntWarnings )
		{
			rc = FileExportIntRequired( GLevelFrame->hWnd );
			if ((rc < 0) || ((rc > 0) && !FileIntExport( hWnd )))
				return (false);
		}
		// --- gam
		
		// External caller: Don't actually close down as the dll may still be used.
		if( GExtCallLaunched && !GExtCallShutdown ) 
		{
			GIsRequestingExit = true;
			return (false);
		}

		doOnClose();
                appRequestExit( 0 );
                return WMdiFrame::OnClose(); // gam
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WEditorFrame::OnCommand);
		TCHAR l_chCmd[512];

#ifdef WITH_LIPSINC
		static UBOOL bLIPSincCanPaste = 0;
#endif

		if( Command >= IDMN_ACTORPOPUPITEMS_BASE && Command < IDMN_ACTORPOPUPITEMS_MAX )
		{
			GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR ADD CLASS=%s"), *GActorPopupItems( Command - IDMN_ACTORPOPUPITEMS_BASE ).ClassName ) );
			return;
		}

		switch( Command )
		{
			case WM_REDRAWALLVIEWPORTS:
				{
					GUnrealEd->RedrawLevel( GUnrealEd->Level );
					GTopBar->UpdateButtons();
					for( INT x = 0 ; x < GViewports.Num() ; ++x)
						InvalidateRect( GViewports(x).ViewportFrame->hWnd, NULL, 1 );
					GUnrealEd->RedrawAllViewports( 1 );
				}
				break;

			case WM_REDRAWCURRENTVIEWPORT:
				GUnrealEd->RedrawCurrentViewport();
				break;

			case ID_FileNew:
			{
                // gam ---
                int rc = FileSaveChangesFirst( GLevelFrame->hWnd );
                if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
                    break;

                if( GEditor->ShowIntWarnings )
                {
                    rc = FileExportIntRequired( GLevelFrame->hWnd );
                    if ((rc < 0) || ((rc > 0) && !FileIntExport( hWnd )))
                        break;
                }
                // --- gam

				GBottomBar->Reset();
				GUnrealEd->Exec(TEXT("MAP NEW"));
				GButtonBar->RefreshBuilders();
				GLevelFrame->SetMapFilename( TEXT("") );
				OpenLevelView();
				if( GBrowserGroup )
					GBrowserGroup->RefreshGroupList();
			}
			break;

			case ID_FILE_IMPORT:
			{
				OPENFILENAMEA ofn;
				ANSICHAR File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Unreal Text (*.t3d)\0*.t3d\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UNR]) );
				ofn.lpstrDefExt = "t3d";
				ofn.lpstrTitle = "Import Map";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

				// Display the Open dialog box. 
				if( GetOpenFileNameA(&ofn) )
				{
					WDlgMapImport l_dlg( this );
					if( l_dlg.DoModal( appFromAnsi( File ) ) )
					{
						GWarn->BeginSlowTask( TEXT("Importing Map"), 1 );
						if( l_dlg.bImportIntoExistingCheck )
							appSprintf( l_chCmd, TEXT("MAP IMPORTADD FILE=\"%s\""), appFromAnsi( File ) );
						else
						{
							GLevelFrame->SetMapFilename( TEXT("") );
							OpenLevelView();
							appSprintf( l_chCmd, TEXT("MAP IMPORT FILE=\"%s\""), appFromAnsi( File ) );
						}
						GUnrealEd->Exec( l_chCmd );
						GWarn->EndSlowTask();
						GUnrealEd->RedrawLevel( GUnrealEd->Level );

						FString S = appFromAnsi( File );
						GLastDir[eLASTDIR_UNR] = S.Left( S.InStr( TEXT("\\"), 1 ) );

						RefreshEditor( ERefreshEditor_Misc | ERefreshEditor_AllBrowsers | ERefreshEditor_ActorBrowser );
						if( !l_dlg.bImportIntoExistingCheck )
						{
							GButtonBar->RefreshBuilders();
						}
					}
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case ID_FILE_EXPORT:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Supported formats (*.t3d,*.stl)\0*.t3d;*.stl\0Unreal Text (*.t3d)\0*.t3d\0Stereo Litho (*.stl)\0*.stl\0Wavefront Object (*.obj)\0*.obj\0All Files\0*.*\0\0"; // gam
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UNR]) );
				ofn.lpstrDefExt = "t3d";
				ofn.lpstrTitle = "Export Map";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				if( GetSaveFileNameA(&ofn) )
				{
					GUnrealEd->Exec( TEXT("BRUSHCLIP DELETE") );
					GUnrealEd->Exec( TEXT("POLYGON DELETE") );
					GUnrealEd->Exec( *FString::Printf(TEXT("MAP EXPORT FILE=\"%s\""), appFromAnsi( File )));

					FString S = appFromAnsi( File );
					GLastDir[eLASTDIR_UNR] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

            // gam ---
			case ID_INT_IMPORT:
            {   
                FileIntImport( hWnd );
                break;
            }

			case ID_INT_EXPORT:
            {
                int rc;

                if (!GLevelFrame)
                    break;

                rc = FileSaveChangesFirst( GLevelFrame->hWnd );
                if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
                    break;

                FileIntExport( hWnd );
                break;
            }

			case ID_INT_EXPORT_AS:
            {   
                int rc;

                if (!GLevelFrame)
                    break;

                rc = FileSaveChangesFirst( GLevelFrame->hWnd );
                if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
                    break;

                FileIntExportAs( hWnd );
                break;
            }
            // --- gam

			case IDMN_ALIGN_WALL:
				GUnrealEd->Exec( TEXT("POLY TEXALIGN WALL") );
				break;

			case IDMN_TOOL_CHECK_ERRORS:
			{
				GUnrealEd->Exec(TEXT("MAP CHECK"));
			}
			break;

			case ID_MapScale:
			{
				WDlgGeneric dlg( NULL, this, OPTIONS_MAPSCALE, TEXT("Map Scaling") );
				if( dlg.DoModal( TEXT("") ) )
				{
					UOptionsMapScale* Proxy = Cast<UOptionsMapScale>(dlg.Proxy);

					if( Proxy->Factor == 0.f )
					{
						appMsgf( 0, TEXT("Cannot scale by a factor of zero." ) );
						break;
					}

					GUnrealEd->Exec( *FString::Printf(TEXT("MAP SCALE FACTOR=%f ADJUSTLIGHTS=%d SCALESPRITES=%d SCALELOCATIONS=%d SCALECOLLISION=%d"),
						Proxy->Factor, Proxy->bAdjustLights, Proxy->bScaleSprites, Proxy->bScaleLocations, Proxy->bScaleCollision ) );
					GUnrealEd->RedrawLevel( GUnrealEd->Level );
				}
				}
			break;

			case ID_ReviewPaths:
			{
				GUnrealEd->Exec( *FString::Printf(TEXT("PATHS REVIEW")));
			}
			break;

			// 3DBuzz - Respond to the array tool menu item
			case ID_ArrayTool:
			{
				GDlgArray->Show(1);
			}
			break;

			case ID_RotateActors:
			{
				WDlgGeneric dlg( NULL, this, OPTIONS_ROTATEACTORS, TEXT("Rotate Actors") );
				if( dlg.DoModal( TEXT("") ) )
				{
					UOptionsRotateActors* Proxy = Cast<UOptionsRotateActors>(dlg.Proxy);
					GUnrealEd->Exec( *FString::Printf(TEXT("ACTOR ROTATERANGE PITCHMIN=%d PITCHMAX=%d YAWMIN=%d YAWMAX=%d ROLLMIN=%d ROLLMAX=%d"),
						Proxy->PitchMin, Proxy->PitchMax, Proxy->YawMin, Proxy->YawMax, Proxy->RollMin, Proxy->RollMax ) );
					GUnrealEd->RedrawLevel( GUnrealEd->Level );
				}
			}
			break;

			case ID_ResetParticleEmitters:
			{
				GUnrealEd->Exec( TEXT("EMITTER RESETALL") );
			}
			break;

			case IDMN_MRU1:
			case IDMN_MRU2:
			case IDMN_MRU3:
			case IDMN_MRU4:
			case IDMN_MRU5:
			case IDMN_MRU6:
			case IDMN_MRU7:
			case IDMN_MRU8:
			{
				FString Filename =  GMRUList->Items[Command - IDMN_MRU1];
				if( GFileManager->FileSize( *Filename ) == -1 )
				{
					appMsgf( 0, TEXT("'%s' does not exist."), *Filename );
					GMRUList->RemoveItem( Filename );
					GMRUList->AddToMenu( GEditorFrame->hWnd, GMainMenu, 1 );
				}
				else
				{
                    // gam ---
                    int rc = FileSaveChangesFirst( GLevelFrame->hWnd );
                    if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
                        break;
            
                    if( GUnrealEd->ShowIntWarnings )
                    {
                        rc = FileExportIntRequired( GLevelFrame->hWnd );
                        if ((rc < 0) || ((rc > 0) && !FileIntExport( hWnd )))
                            break;
                    }
                    // --- gam

					GBottomBar->Reset();
					GLevelFrame->SetMapFilename( (TCHAR*)(*(GMRUList->Items[Command - IDMN_MRU1] ) ) );
					GUnrealEd->Exec( *FString::Printf(TEXT("MAP LOAD FILE=\"%s\""), *GMRUList->Items[Command - IDMN_MRU1] ));
					RefreshEditor( ERefreshEditor_Misc | ERefreshEditor_AllBrowsers | ERefreshEditor_ActorBrowser );
					GButtonBar->RefreshBuilders();

					GMRUList->MoveToTop( Command - IDMN_MRU1 );
					GMRUList->AddToMenu( GEditorFrame->hWnd, GMainMenu, 1 );
				}
			}
			break;

			case IDMN_LOAD_BACK_IMAGE:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Bitmaps (*.bmp)\0*.bmp\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = "..\\maps";
				ofn.lpstrTitle = "Open Image";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UTX]) );
				ofn.lpstrDefExt = "bmp";
				ofn.Flags = OFN_NOCHANGEDIR;

				// Display the Open dialog box. 
				//
				if( GetOpenFileNameA(&ofn) )
				{
					GLevelFrame->LoadBackgroundImage(appFromAnsi( File ));

					FString S = appFromAnsi( File );
					GLastDir[eLASTDIR_UTX] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}

				InvalidateRect( GLevelFrame->hWnd, NULL, FALSE );
			}
			break;

			case IDMN_CLEAR_BACK_IMAGE:
			{
				::DeleteObject( GLevelFrame->hImage );
				GLevelFrame->hImage = NULL;
				GLevelFrame->BIFilename = TEXT("");
				InvalidateRect( GLevelFrame->hWnd, NULL, FALSE );
			}
			break;

			case IDMN_BI_CENTER:
			{
				GLevelFrame->BIMode = eBIMODE_CENTER;
				InvalidateRect( GLevelFrame->hWnd, NULL, FALSE );
			}
			break;

			case IDMN_BI_TILE:
			{
				GLevelFrame->BIMode = eBIMODE_TILE;
				InvalidateRect( GLevelFrame->hWnd, NULL, FALSE );
			}
			break;

			case IDMN_BI_STRETCH:
			{
				GLevelFrame->BIMode = eBIMODE_STRETCH;
				InvalidateRect( GLevelFrame->hWnd, NULL, FALSE );
			}
			break;

			case ID_FileOpen:
			{
				FileOpen( hWnd );
			}
			break;

			case ID_FileClose:
			{
                // gam ---
                int rc = FileSaveChangesFirst( GLevelFrame->hWnd );
                if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
                    break;

                if( GEditor->ShowIntWarnings )
                {
                    rc = FileExportIntRequired( GLevelFrame->hWnd );
                    if ((rc < 0) || ((rc > 0) && !FileIntExport( hWnd )))
                        break;
                }
                // --- gam

				if( GLevelFrame )
				{
					GLevelFrame->_CloseWindow();
					delete GLevelFrame;
					GLevelFrame = NULL;
				}
			}
			break;

			case ID_FileSave:
			{
				FileSave( hWnd );
			}
			break;

			case ID_FileSaveAs:
			{
				FileSaveAs( hWnd );
			}
			break;

			case ID_BrowserMaster:
			{
				GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserTexture:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_TEXTURE);
				if( GBrowserTexture->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserStaticMesh:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_STATICMESH);
				if( GBrowserStaticMesh->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserMesh:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_MESH);
				if( GBrowserMesh->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserAnimation:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_ANIMATION);
				if( GBrowserAnimation->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserPrefab:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_PREFAB);
				if( GBrowserPrefab->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserActor:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_ACTOR);
				if( GBrowserActor->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserSound:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_SOUND);
				if( GBrowserSound->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserMusic:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_MUSIC);
				if( GBrowserMusic->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case ID_BrowserGroup:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_GROUP);
				if( GBrowserGroup->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

#ifdef WITH_LIPSINC
			case ID_BrowserLIPSinc:
				{
					GBrowserMaster->ShowBrowser(eBROWSER_LIPSINC);
					if( GBrowserLIPSinc->IsDocked() ) GBrowserMaster->Show(1);
				}
				break;
#endif

			case IDMN_CODE_FRAME:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_ACTOR);
				ShowCodeFrame( this );
			}
			break;

			case ID_FileExit:
			{
				OnClose();
			}
			break;

			case ID_EditUndo:
			{
				GUnrealEd->Exec( TEXT("TRANSACTION UNDO") );
			}
			break;

			case ID_EditRedo:
			{
				GUnrealEd->Exec( TEXT("TRANSACTION REDO") );
			}
			break;

			case ID_EditDuplicate:
			{
				GUnrealEd->Exec( TEXT("DUPLICATE") );
			}
			break;

			case IDMN_EDIT_SEARCH:
			{
				GDlgSearchActors->Show(1);
			}
			break;

			case IDMN_EDIT_SCALE_LIGHTS:
			{
				GDlgScaleLights->Show(1);
			}
			break;

			case ID_EditDelete:
			{
				GUnrealEd->Exec( TEXT("DELETE") );
			}
			break;

			case ID_EditCut:
			{
				GUnrealEd->Exec( TEXT("EDIT CUT") );
			}
			break;

			case ID_EditCopy:
			{
				GUnrealEd->Exec( TEXT("EDIT COPY") );
			}
			break;

			case ID_EditPaste:
			{
				GUnrealEd->Exec( TEXT("EDIT PASTE") );
			}
			break;

			case ID_EditSelectNone:
			{
				GUnrealEd->Exec( TEXT("SELECT NONE") );
			}
			break;

			case ID_EditSelectAllActors:
			{
				GUnrealEd->Exec( TEXT("ACTOR SELECT ALL") );
			}
			break;

			case ID_EditSelectAllSurfs:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT ALL") );
			}
			break;

			case ID_ViewActorProp:
			{
				GUnrealEd->ShowActorProperties();
			}
			break;

			case ID_ViewSurfaceProp:
			{
				GSurfPropSheet->Show( TRUE );
			}
			break;

			case ID_ViewLevelProp:
			{
				GUnrealEd->ShowLevelProperties();
			}
			break;

			case ID_BrushClip:
			{
				GUnrealEd->Exec( TEXT("BRUSHCLIP") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushClipSplit:
			{
				GUnrealEd->Exec( TEXT("BRUSHCLIP SPLIT") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushClipFlip:
			{
				GUnrealEd->Exec( TEXT("BRUSHCLIP FLIP") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushClipDelete:
			{
				GUnrealEd->Exec( TEXT("BRUSHCLIP DELETE") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushScale:
			{
				WDlgGeneric dlg( NULL, this, OPTIONS_BRUSHSCALE, TEXT("Brush Scaling") );
				if( dlg.DoModal( TEXT("") ) )
				{
					UOptionsBrushScale* Proxy = Cast<UOptionsBrushScale>(dlg.Proxy);
					GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH SCALE X=%f, Y=%f, Z=%f"), Proxy->X, Proxy->Y, Proxy->Z ) );
					GUnrealEd->RedrawLevel( GUnrealEd->Level );
				}
			}
			break;

			case ID_BrushAdd:
			{
				GUnrealEd->Exec( TEXT("BRUSH ADD") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushSubtract:
			{
				GUnrealEd->Exec( TEXT("BRUSH SUBTRACT") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushIntersect:
			{
				GUnrealEd->Exec( TEXT("BRUSH FROM INTERSECTION") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushDeintersect:
			{
				GUnrealEd->Exec( TEXT("BRUSH FROM DEINTERSECTION") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case ID_BrushAddMover:
			{
				GUnrealEd->Exec( TEXT("BRUSH ADDMOVER") );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;
			
			case ID_BrushAddAntiPortal:
			{
				GUnrealEd->Exec(TEXT("BRUSH ADDANTIPORTAL"));
			}
			break;

			case ID_BrushAddSpecial:
			{
				if( !GDlgAddSpecial )
				{
					GDlgAddSpecial = new WDlgAddSpecial( NULL, GEditorFrame );
					GDlgAddSpecial->DoModeless(1);
				}
				else
					GDlgAddSpecial->Show(1);
			}
			break;

			case ID_BrushOpen:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Brushes (*.u3d)\0*.u3d\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = "..\\maps";
				ofn.lpstrDefExt = "u3d";
				ofn.lpstrTitle = "Open Brush";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

				// Display the Open dialog box. 
				if( GetOpenFileNameA(&ofn) )
				{
					GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH LOAD FILE=\"%s\""), appFromAnsi( File )));
					GUnrealEd->RedrawLevel( GUnrealEd->Level );
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
				GButtonBar->RefreshBuilders();
			}
			break;

			case ID_BrushSaveAs:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Brushes (*.u3d)\0*.u3d\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = "..\\maps";
				ofn.lpstrDefExt = "u3d";
				ofn.lpstrTitle = "Save Brush";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				if( GetSaveFileNameA(&ofn) )
					GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH SAVE FILE=\"%s\""), appFromAnsi( File )));

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case ID_BRUSH_IMPORT:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Import Types (*.t3d, *.dxf, *.asc, *.ase)\0*.t3d;*.dxf;*.asc;*.ase;\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_BRUSH]) );
				ofn.lpstrDefExt = "t3d";
				ofn.lpstrTitle = "Import Brush";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

				// Display the Open dialog box. 
				if( GetOpenFileNameA(&ofn) )
				{
					WDlgBrushImport l_dlg( NULL, this );
					l_dlg.DoModal( appFromAnsi( File ) );
					GUnrealEd->RedrawLevel( GUnrealEd->Level );

					FString S = appFromAnsi( File );
					GLastDir[eLASTDIR_BRUSH] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
				GButtonBar->RefreshBuilders();
			}
			break;

			// jij --- importing static meshes from Lightwave
			case ID_BRUSH_BATCH_IMPORT:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";
                FString FileName;
                FString ActorClassName;

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Lightwave Object Files (*.lwo)\0*.lwo;\0;\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_BRUSH]) );
				ofn.lpstrDefExt = "lwo";
				ofn.lpstrTitle = "Import Static Mesh";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

				if( GetOpenFileNameA(&ofn) )
                {
                    INT iNULLs = FormatFilenames( File );

					TArray<FString> StringArray;
					FString S = appFromAnsi( File );
					S.ParseIntoArray( TEXT("|"), &StringArray );

					INT iStart = 0;
					FString Prefix = TEXT("\0");

					if( iNULLs )
					{
						iStart = 1;
						Prefix = *(StringArray(0));					                        
                        Prefix += TEXT("\\");                        
					}

					GLastDir[eLASTDIR_BRUSH] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
                    
                    ActorClassName = TEXT("StaticMeshActor");

                    for( INT x = iStart; x < StringArray.Num(); ++x )
                    {
		                GEditor->Exec( *FString::Printf( TEXT("BRUSH STATIC_IMPORT FILE=\"%s%s\" CLASS=\"%s\""), *Prefix, *(StringArray(x)), *ActorClassName ) );
		                GEditor->Level->Brush()->Brush->BuildBound();
                        GEditor->RedrawLevel( GEditor->Level );
				        GFileManager->SetDefaultDirectory(appBaseDir());
				        GButtonBar->RefreshBuilders();
                    }                    
                }
			}
			break;
			// --- jij

			case ID_BRUSH_EXPORT:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Unreal Text (*.t3d)\0*.t3d\0Wavefront Object (*.obj)\0*.obj\0All Files\0*.*\0\0"; // gam
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_BRUSH]) );
				ofn.lpstrDefExt = "t3d";
				ofn.lpstrTitle = "Export Brush";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				if( GetSaveFileNameA(&ofn) )
				{
					GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH EXPORT FILE=\"%s\""), appFromAnsi( File )));

					FString S = appFromAnsi( File );
					GLastDir[eLASTDIR_BRUSH] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
				GButtonBar->RefreshBuilders();
			}
			break;

			case ID_BuildPlay:
			{
				CloseWindow( hWnd );
				GUnrealEd->PlayMap();
			}
			break;

			case ID_BuildGeometry:
			{
				((WPageOptions*)GBuildSheet->PropSheet->Pages(0))->BuildGeometry();
				GBuildSheet->PropSheet->RefreshPages();
			}
			break;

			case ID_BuildLighting:
			{
				((WPageOptions*)GBuildSheet->PropSheet->Pages(0))->BuildLighting();
				GBuildSheet->PropSheet->RefreshPages();
			}
			break;

			case ID_BuildPaths:
			{
				((WPageOptions*)GBuildSheet->PropSheet->Pages(0))->BuildPaths();
				GBuildSheet->PropSheet->RefreshPages();
			}
			break;

			case ID_BuildChangedPaths:
			{
				((WPageOptions*)GBuildSheet->PropSheet->Pages(0))->OnPathsChangedClick();
				GBuildSheet->PropSheet->RefreshPages();
			}
			break;

			case ID_BuildAll:
			{
				((WPageOptions*)GBuildSheet->PropSheet->Pages(0))->OnBuildClick();
				GBuildSheet->PropSheet->RefreshPages();
			}
			break;

			case ID_BuildOptions:
			{
				GBuildSheet->Show( TRUE );
			}
			break;

			case ID_BuildChangedLighting:
			{
				GUnrealEd->Exec(TEXT("LIGHT APPLY CHANGED=1"));
			}
			break;

			case IDMN_EMU_NONE:
				GUnrealEd->Exec(TEXT("RENDEREMULATE none"));
				break;

			case IDMN_EMU_GF1:
				GUnrealEd->Exec(TEXT("RENDEREMULATE gf1"));
				break;

			case IDMN_EMU_GF2:
				GUnrealEd->Exec(TEXT("RENDEREMULATE gf2"));
				break;

			case IDMN_EMU_XBOX:
				GUnrealEd->Exec(TEXT("RENDEREMULATE xbox"));
				break;

			case ID_ToolsLog:
			{
				if( GLogWindow )
				{
					GLogWindow->Show(1);
					SetFocus( *GLogWindow );
					GLogWindow->Display.ScrollCaret();
				}
			}
			break;

			case ID_Tools2DEditor:
			{
				delete G2DShapeEditor;

				G2DShapeEditor = new W2DShapeEditor( TEXT("2D Shape Editor"), this );
				G2DShapeEditor->OpenWindow();
			}
			break;

			// Added by Demiurge (PSE)
			case ID_ParticleEditor:
			{
				if(!GParticleEditor)
				{
					GParticleEditor = new WParticleEditor(TEXT("Particle Editor"), this);
					GParticleEditor->OpenWindow();
				}
				GParticleEditor->Show(1);
			}
			break;
			// End Add by Demiurge (PSE)

			case ID_ViewNewFree:
			{
				if( GViewportStyle == VSTYLE_Floating )
					GLevelFrame->OpenFrameViewport( REN_OrthXY, 0, 0, 256, 256, GShowFlags | SHOW_Terrain | SHOW_DistanceFog | SHOW_Coronas | SHOW_Particles | SHOW_BSP);
			}
			break;

			case IDMN_VIEWPORT_CLOSEALL:
			{
				for( INT x = 0 ; x < GViewports.Num() ; x++)
				{
					delete GViewports(x).ViewportFrame;
					GViewports(x).ViewportFrame = NULL;
				}
				GViewports.Empty();
			}
			break;

			case IDMN_VIEWPORT_FLOATING:
			{
				GViewportStyle = VSTYLE_Floating;
				UpdateMenu();
				GLevelFrame->ChangeViewportStyle();
			}
			break;

			case IDMN_VIEWPORT_FIXED:
			{
				GViewportStyle = VSTYLE_Fixed;
				UpdateMenu();
				GLevelFrame->ChangeViewportStyle();
			}
			break;

			case IDMN_VIEWPORT_CONFIG:
			{
				WDlgViewportConfig l_dlg( NULL, this );
				if( l_dlg.DoModal( GViewportConfig ) )
					GLevelFrame->CreateNewViewports( GViewportStyle, l_dlg.ViewportConfig );
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case IDMN_HELP_UDN:
			{
				ShellExecuteA( NULL, "open", "http://udn.epicgames.com", NULL, NULL, SW_SHOWNORMAL );
			}
			break;

			case IDMN_HELP_UDN_CONTEXT:
			{
				GUDNWindow->Capture();
			}
			break;

			case IDMN_HELP_TOTD:
				{
				GDlgTOTD->Show(1);
				}
			break;

			case ID_ToolsPrefs:
			{
				GUnrealEd->ShowPreferences();
			}
			break;

			// 3DBuzz
			// Message from BottomBarStandard.h
			case IDCK_MAXIMIZE:
			{	
				// Loop through the available viewports
				for( INT x = 0 ; x < GViewports.Num() ; x++)
				{
					// Check if this is the current viewport
					if ( GViewports(x).ViewportFrame->Viewport == GUnrealEd->GetCurrentViewport() )
					{
						// Check to see if it's already maximized
						if ( IsZoomed( GViewports(x).ViewportFrame->hWnd ) )
							ShowWindow( GViewports(x).ViewportFrame->hWnd, SW_RESTORE );
						else
							ShowWindow( GViewports(x).ViewportFrame->hWnd, SW_MAXIMIZE );
					}
				}
			}
			break;

			case WM_EDC_ACTORPROPERTIESCHANGE:
			{
				// Update collision hash
				if( GUnrealEd && GUnrealEd->Level )
				{
					for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
					{
						AActor* Actor = GUnrealEd->Level->Actors(i);
						if( Actor && Actor->bSelected && Actor->bCollideActors )
						{
							GUnrealEd->Level->Hash->RemoveActor(Actor);
							GUnrealEd->Level->Hash->AddActor(Actor);
						}
					}

                    if( GBottomBar ) GBottomBar->BottomBarStandard->Refresh(); // gam
				}
			}
			break;

			case WM_EDC_REFRESHEDITOR:
			{
				RefreshEditor( LastlParam );
				//GUnrealEd->RedrawLevel( GUnrealEd->Level );
			}
			break;

			case WM_EDC_SAVEMAP:
			{
				FileSave( hWnd );
			}
			break;

			case WM_EDC_SAVEMAPAS:
			{
				FileSaveAs( hWnd );
			}
			break;

			case WM_BROWSER_DOCK:
			{
				guard(WM_BROWSER_DOCK);
				INT Browsr = LastlParam;
				switch( Browsr )
				{
					case eBROWSER_ACTOR:
						SAFEDELETENULL(GBrowserActor);
						GBrowserActor = new WBrowserActor( TEXT("Actor Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserActor);
						GBrowserActor->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_ACTOR);
						break;

					case eBROWSER_GROUP:
						SAFEDELETENULL(GBrowserGroup);
						GBrowserGroup = new WBrowserGroup( TEXT("Group Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserGroup);
						GBrowserGroup->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_GROUP);
						break;

#ifdef WITH_LIPSINC
					case eBROWSER_LIPSINC:
						SAFEDELETENULL(GBrowserLIPSinc);
						GBrowserLIPSinc = new WBrowserLIPSinc( TEXT("LIPSinc Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserLIPSinc);
						GBrowserLIPSinc->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_LIPSINC);
						break;
#endif

					case eBROWSER_MUSIC:
						SAFEDELETENULL(GBrowserMusic);
						GBrowserMusic = new WBrowserMusic( TEXT("Music Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserMusic);
						GBrowserMusic->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_MUSIC);
						break;

					case eBROWSER_SOUND:
						SAFEDELETENULL(GBrowserSound);
						GBrowserSound = new WBrowserSound( TEXT("Sound Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserSound);
						GBrowserSound->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_SOUND);
						break;

					case eBROWSER_TEXTURE:
						SAFEDELETENULL(GBrowserTexture);
						GBrowserTexture = new WBrowserTexture( TEXT("Texture Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserTexture);
						GBrowserTexture->OpenWindow( 1 );
						GBrowserTextureHwnd = GBrowserTexture->hWnd;
						GBrowserMaster->ShowBrowser(eBROWSER_TEXTURE);
						break;

					case eBROWSER_STATICMESH:
						SAFEDELETENULL(GBrowserStaticMesh);
						GBrowserStaticMesh = new WBrowserStaticMesh( TEXT("Static Mesh Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserStaticMesh);
						GBrowserStaticMesh->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_STATICMESH);
						break;

					case eBROWSER_MESH:
						SAFEDELETENULL(GBrowserMesh);
						GBrowserMesh = new WBrowserMesh( TEXT("Mesh Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserMesh);
						GBrowserMesh->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_MESH);
						break;

					case eBROWSER_ANIMATION:
						SAFEDELETENULL(GBrowserAnimation);
						GBrowserAnimation = new WBrowserAnimation( TEXT("Animation Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserAnimation);
						GBrowserAnimation->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_ANIMATION);
						break;

					case eBROWSER_PREFAB:
						SAFEDELETENULL(GBrowserPrefab);
						GBrowserPrefab = new WBrowserPrefab( TEXT("Prefab Browser"), GBrowserMaster, GEditorFrame->hWnd );
						check(GBrowserPrefab);
						GBrowserPrefab->OpenWindow( 1 );
						GBrowserMaster->ShowBrowser(eBROWSER_PREFAB);
						break;
				}
				unguard;
			}
			break;

			case WM_BROWSER_UNDOCK:
			{
				guard(WM_BROWSER_UNDOCK);
				INT Browsr = LastlParam;
				switch( Browsr )
				{
					case eBROWSER_ACTOR:
						SAFEDELETENULL(GBrowserActor);
						GBrowserActor = new WBrowserActor( TEXT("Actor Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserActor);
						GBrowserActor->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_ACTOR);
						break;

					case eBROWSER_GROUP:
						SAFEDELETENULL(GBrowserGroup);
						GBrowserGroup = new WBrowserGroup( TEXT("Group Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserGroup);
						GBrowserGroup->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_GROUP);
						break;

#ifdef WITH_LIPSINC
					case eBROWSER_LIPSINC:
						SAFEDELETENULL(GBrowserLIPSinc);
						GBrowserLIPSinc = new WBrowserLIPSinc( TEXT("LIPSinc Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserLIPSinc);
						GBrowserLIPSinc->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_LIPSINC);
						break;
#endif
					case eBROWSER_MUSIC:
						SAFEDELETENULL(GBrowserMusic);
						GBrowserMusic = new WBrowserMusic( TEXT("Music Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserMusic);
						GBrowserMusic->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_MUSIC);
						break;

					case eBROWSER_SOUND:
						SAFEDELETENULL(GBrowserSound);
						GBrowserSound = new WBrowserSound( TEXT("Sound Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserSound);
						GBrowserSound->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_SOUND);
						break;

					case eBROWSER_TEXTURE:
						SAFEDELETENULL(GBrowserTexture);
						GBrowserTexture = new WBrowserTexture( TEXT("Texture Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserTexture);
						GBrowserTexture->OpenWindow( 0 );
						GBrowserTextureHwnd = GBrowserTexture->hWnd;
						GBrowserMaster->ShowBrowser(eBROWSER_TEXTURE);
						break;

					case eBROWSER_STATICMESH:
						SAFEDELETENULL(GBrowserStaticMesh);
						GBrowserStaticMesh = new WBrowserStaticMesh( TEXT("Static Mesh Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserStaticMesh);
						GBrowserStaticMesh->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_STATICMESH);
						break;

					case eBROWSER_MESH:
						SAFEDELETENULL(GBrowserMesh);
						GBrowserMesh = new WBrowserMesh( TEXT("Mesh Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserMesh);
						GBrowserMesh->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_MESH);
						break;

					case eBROWSER_ANIMATION:
						SAFEDELETENULL(GBrowserAnimation);
						GBrowserAnimation = new WBrowserAnimation( TEXT("Animation Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserAnimation);
						GBrowserAnimation->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_ANIMATION);
						break;

					case eBROWSER_PREFAB:
						SAFEDELETENULL(GBrowserPrefab);
						GBrowserPrefab = new WBrowserPrefab( TEXT("Prefab Browser"), GEditorFrame, GEditorFrame->hWnd );
						check(GBrowserPrefab);
						GBrowserPrefab->OpenWindow( 0 );
						GBrowserMaster->ShowBrowser(eBROWSER_PREFAB);
						break;
				}

				GBrowserMaster->RefreshBrowserTabs( -1 );
				unguard;
			}
			break;

			case WM_EDC_CAMMODECHANGE:
			{
				if( GButtonBar )
				{
					GButtonBar->UpdateButtons();
					GTopBar->UpdateButtons();
				}

				if( GBottomBar )
					GBottomBar->Refresh();

				if( GTerrainEditSheet )
					GTerrainEditSheet->Show( GUnrealEd->Mode == EM_TerrainEdit );

				if( GMatineeSheet )
					GMatineeSheet->Show( GUnrealEd->Mode == EM_Matinee );

				// If we are leaving terrain editing mode, deselect all terraininfos.
				if( GUnrealEd->Mode != EM_TerrainEdit )
					if( GUnrealEd && GUnrealEd->Level )
					{
						for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
						{
							ATerrainInfo* TerrainInfo = Cast<ATerrainInfo>(GUnrealEd->Level->Actors(i));
							if( TerrainInfo )
								GUnrealEd->SelectActor( GUnrealEd->Level, TerrainInfo, 0, 0 );
						}
						GUnrealEd->PostEditChange();
					}
			}
			break;

			case WM_EDC_LOADMAP:
			{
				FileOpen( hWnd );
			}
			break;

			case WM_EDC_PLAYMAP:
			{
				CloseWindow( hWnd );
				GUnrealEd->PlayMap();
			}
			break;

			case WM_EDC_BROWSE:
			{
				FStringOutputDevice	GetPropResult;
				GUnrealEd->Get( TEXT("OBJ"), TEXT("BROWSECLASS"), GetPropResult );

				if( !appStrcmp( *GetPropResult, TEXT("Texture") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_TEXTURE);

				if( !appStrcmp( *GetPropResult, TEXT("Palette") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_TEXTURE);

				if( !appStrcmp( *GetPropResult, TEXT("Sound") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_SOUND);

				if( !appStrcmp( *GetPropResult, TEXT("Music") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_MUSIC);

				if( !appStrcmp( *GetPropResult, TEXT("Class") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_ACTOR);

				if( !appStrcmp( *GetPropResult, TEXT("Mesh") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_MESH);

				if( !appStrcmp( *GetPropResult, TEXT("Animation") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_ANIMATION);
					
				if( !appStrcmp( *GetPropResult, TEXT("StaticMesh") ) )
					GBrowserMaster->ShowBrowser(eBROWSER_STATICMESH);

			}
			break;

			case WM_EDC_USECURRENT:
			{
				FStringOutputDevice	GetPropResult;
				FString Cur;

				GUnrealEd->Get( TEXT("OBJ"), TEXT("BROWSECLASS"), GetPropResult );
				UClass* BrowseClass = FindObject<UClass>( ANY_PACKAGE, *GetPropResult );
				
				if( GUnrealEd->CurrentMaterial && BrowseClass && GUnrealEd->CurrentMaterial->GetClass()->IsChildOf(BrowseClass) )
				{
					Cur = GUnrealEd->CurrentMaterial->GetPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("Palette") ) )
				{
					if( Cast<UTexture>(GUnrealEd->CurrentMaterial) )
						Cur = Cast<UTexture>(GUnrealEd->CurrentMaterial)->Palette->GetPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("Sound") ) )
				{
					if( GBrowserSound )
						Cur = *GBrowserSound->GetCurrentPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("Music") ) )
				{
					if( GBrowserMusic )
						Cur = *GBrowserMusic->GetCurrentPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("Class") ) )
				{
					if( GUnrealEd->CurrentClass )
						Cur = GUnrealEd->CurrentClass->GetPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("Mesh") ) )
				{
					if( GBrowserMesh )
						Cur = GUnrealEd->CurrentMesh->GetPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("Animation") ) )
				{
					if( GBrowserAnimation )
						Cur = GUnrealEd->CurrentMesh->GetPathName();
				}
				else if( !appStrcmp( *GetPropResult, TEXT("StaticMesh") ) )
				{
					if( GBrowserStaticMesh )
						Cur = GUnrealEd->CurrentStaticMesh->GetPathName();
				}
				else
				{
					// Use the first selected actor we find
					for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
					{
						AActor* Actor = GUnrealEd->Level->Actors(i);
						if( Actor && Actor->bSelected )
						{
							Cur = Actor->GetPathName();
							break;
						}
					}
				}

				if( Cur.Len() )
					GUnrealEd->Set( TEXT("OBJ"), TEXT("NOTECURRENT"), *FString::Printf(TEXT("CLASS=%s OBJECT=%s"), *GetPropResult, *Cur));
			}
			break;

			case WM_EDC_CURTEXCHANGE:
			{
				if( GBrowserMaster->CurrentBrowser == eBROWSER_TEXTURE )
				{
					GBrowserTexture->SetCaption();
					//GBrowserTexture->Viewport->Repaint(1);
				}
			}
			break;

			case WM_EDC_CURSTATICMESHCHANGE:
			{
				if( GBrowserMaster->CurrentBrowser == eBROWSER_STATICMESH )
				{
					GBrowserStaticMesh->SetCaption();
					GBrowserStaticMesh->Viewport->Repaint(1);
				}
			}
			break;

			case WM_EDC_SELPOLYCHANGE:
			case WM_EDC_SELCHANGE:
			{
				GSurfPropSheet->PropSheet->RefreshPages();

                if( GBottomBar ) GBottomBar->BottomBarStandard->Refresh(); // gam
				// Added by Demiurge (PSE)
				if(GParticleEditor)
				{
					GParticleEditor->NotifySelectionChange();
				}
				// End add by Demiurge (PSE)
			}
			break;

			case WM_EDC_RTCLICKTEXTURE:
			{
				POINT pt;
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_BrowserTexture_Context),
					submenu = GetSubMenu( menu, 0 );
				::GetCursorPos( &pt );
				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0,
					GBrowserTexture->hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_RTCLICKANIMSEQ:
			{
				POINT pt;
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_BrowserAnimation_Context),
					submenu = GetSubMenu( menu, 0 );
				::GetCursorPos( &pt );
				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0,
					GBrowserAnimation->hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

#ifdef WITH_LIPSINC
			case WM_EDC_LIPSINC_CANPASTE:
				{
					bLIPSincCanPaste = 1;
				}
				break;

			case WM_EDC_LIPSINC_NOPASTE:
				{
					bLIPSincCanPaste = 0;
				}
				break;

			case WM_EDC_LIPSINCANIMLISTRTCLICK:
				{
					POINT pt;
					HMENU menu = LoadMenuIdX( hInstance, IDMENU_BrowserLIPSinc_Context );
					HMENU submenu = GetSubMenu( menu, 0 );

					if (bLIPSincCanPaste)
					{
						// Enable the paste menu item

					}
					else
					{
						// Disable the paste menu item

					}

					::GetCursorPos( &pt );
					TrackPopupMenu( submenu,
						TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
						pt.x, pt.y, 0,
						GBrowserLIPSinc->hWnd, NULL );
					DestroyMenu( menu );
				}
				break;
#endif

			case WM_EDC_RTCLICKMATSCENE:
			{
				POINT pt;
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_MatineeScene_Context),
					submenu = GetSubMenu( menu, 0 );
				::GetCursorPos( &pt );
				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0,
					GMatineeSheet->ScenesPage->hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_RTCLICKMATACTION:
			{
				POINT pt;
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_MatineeAction_Context),
					submenu = GetSubMenu( menu, 0 );
				::GetCursorPos( &pt );
				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0,
					GMatineeSheet->ActionsPage->hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_RTCLICKSTATICMESH:
			{
				POINT pt;
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_BrowserStaticMesh_Context),
					submenu = GetSubMenu( menu, 0 );
				::GetCursorPos( &pt );
				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0,
					GBrowserStaticMesh->hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_RTCLICKTERRAINLAYER:
			{
				POINT pt;
				HMENU menu, submenu;
				
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( GTerrainTools.CurrentLayer == 0 )
				{
					menu = LoadMenuIdX(hInstance, 
						IDMENU_Terrains_Context);
					submenu = GetSubMenu( menu, 0 );

					// Customize the menu
					if( TI && TI->TerrainMap->Format == TEXF_G16 )
					{
						DeleteMenu( submenu, IDMN_TL_CONVERT_TO_16BIT, MF_BYCOMMAND );
						break;
					}
				}
				else if( GTerrainTools.CurrentLayer >= 32 )
				{
					menu = LoadMenuIdX(hInstance, IDMENU_TerrainDecoLayer_Context);
					submenu = GetSubMenu( menu, 0 );
				}
				else
				{
					menu = LoadMenuIdX(hInstance, IDMENU_TerrainLayer_Context);
					submenu = GetSubMenu( menu, 0 );
				}

				::GetCursorPos( &pt );
				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0,
					GTerrainEditSheet->hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_RTCLICKPOLY:
			{
				POINT point;

				::GetCursorPos( &point );
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_SurfPopup),
					submenu = GetSubMenu( menu, 0 );

				// Customize the menu options we need to.
				MENUITEMINFOA mif;
				char Buffer[255];

				mif.cbSize = sizeof(MENUITEMINFO);
				mif.fMask = MIIM_TYPE;
				mif.fType = MFT_STRING;

				FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_NUM_SURF_SELECTED );

				sprintf( Buffer, "Surface &Properties (%i Selected)\tF5", gir.iValue );
				mif.dwTypeData = Buffer;
				SetMenuItemInfoA( submenu, ID_SurfProperties, FALSE, &mif );

				if( GUnrealEd->CurrentClass )
				{
					sprintf( Buffer, "&Add %s Here", TCHAR_TO_ANSI( GUnrealEd->CurrentClass->GetName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_SurfPopupAddClass, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_SurfPopupAddClass, MF_BYCOMMAND );

				if( GUnrealEd->CurrentStaticMesh )
				{
					sprintf( Buffer, "&Add Static Mesh: '%s'", TCHAR_TO_ANSI( GUnrealEd->CurrentStaticMesh->GetPathName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddStaticMeshHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddStaticMeshHere, MF_BYCOMMAND );
				
				// Add/remove 'Add Karma Actor' menu item.
				if( GUnrealEd->CurrentStaticMesh  && GUnrealEd->CurrentStaticMesh->KPhysicsProps )
				{
					mif.dwTypeData = "Add Karma Actor";
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddKActorHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddKActorHere, MF_BYCOMMAND );

				if( GCurrentPrefab )
				{
					sprintf( Buffer, "&Add Prefab: '%s'", TCHAR_TO_ANSI( GCurrentPrefab->GetPathName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddPrefabHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddPrefabHere, MF_BYCOMMAND );

				if( GUnrealEd->CurrentMaterial )
				{
					sprintf( Buffer, "&Apply Texture : %s", TCHAR_TO_ANSI( GUnrealEd->CurrentMaterial->GetPathName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_SurfPopupApplyTexture, FALSE, &mif );
				}

				// Insert user defined items into the menu
				mif.fType = MFT_STRING;
				mif.fMask = MIIM_TYPE | MIIM_ID;
				for( INT x = 0 ; x < GActorPopupItems.Num() ; ++x )
				{
					sprintf( Buffer, "Add %s Here", TCHAR_TO_ANSI( *GActorPopupItems(x).Desc ) );
					mif.dwTypeData = Buffer;
					mif.wID = GActorPopupItems(x).ID;
					InsertMenuItemA( submenu, ID_SurfPopupAddLight, FALSE, &mif );
				}

				// Texture alignment options
				/*
				HMENU AlignMenu = CreateMenu();
				mif.dwTypeData = "Align";
				InsertMenuItemA( AlignMenu, -1, 1, &mif );

				AppendMenuA( 
				*/

				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					point.x, point.y, 0,
					hWnd, NULL);
				DestroyMenu( menu );
				//DestroyMenu( AlignMenu );
			}
			break;

			case WM_EDC_RTCLICKACTOR:
			case WM_EDC_RTCLICKACTORSTATICMESH:
			{
				POINT point;

				::GetCursorPos( &point );
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_ActorPopup),
					submenu = GetSubMenu( menu, 0 );

				// Customize the menu options we need to.
				MENUITEMINFOA mif;
				char Buffer[255];

				mif.cbSize = sizeof(MENUITEMINFO);
				mif.fMask = MIIM_TYPE;
				mif.fType = MFT_STRING;

				FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_NUM_SELECTED | GI_CLASSNAME_SELECTED | GI_CLASS_SELECTED );

				sprintf( Buffer, "%s &Properties (%i Selected)", TCHAR_TO_ANSI( *gir.String ), gir.iValue );
				mif.dwTypeData = Buffer;
				SetMenuItemInfoA( submenu, IDMENU_ActorPopupProperties, FALSE, &mif );

				sprintf( Buffer, "&Select All %s", TCHAR_TO_ANSI( *gir.String ) );
				mif.dwTypeData = Buffer;
				SetMenuItemInfoA( submenu, IDMENU_ActorPopupSelectAllClass, FALSE, &mif );

				EnableMenuItem( submenu, IDMENU_ActorPopupEditScript, (gir.pClass == NULL) );
				EnableMenuItem( submenu, IDMENU_ActorPopupMakeCurrent, (gir.pClass == NULL) );

				if( Command == WM_EDC_RTCLICKACTORSTATICMESH)
				{
					if( GUnrealEd->CurrentClass )
					{
						sprintf( Buffer, "&Add %s Here", TCHAR_TO_ANSI( GUnrealEd->CurrentClass->GetName() ) );
						mif.dwTypeData = Buffer;
						SetMenuItemInfoA( submenu, ID_SurfPopupAddClass, FALSE, &mif );
					}
					else
						DeleteMenu( submenu, ID_SurfPopupAddClass, MF_BYCOMMAND );

					if( GUnrealEd->CurrentStaticMesh )
					{
						sprintf( Buffer, "&Add Static Mesh: '%s'", TCHAR_TO_ANSI( GUnrealEd->CurrentStaticMesh->GetPathName() ) );
						mif.dwTypeData = Buffer;
						SetMenuItemInfoA( submenu, ID_BackdropPopupAddStaticMeshHere, FALSE, &mif );
					}
					else
						DeleteMenu( submenu, ID_BackdropPopupAddStaticMeshHere, MF_BYCOMMAND );

					// Add/remove 'Add Karma Actor' menu item.
					if( GUnrealEd->CurrentStaticMesh  && GUnrealEd->CurrentStaticMesh->KPhysicsProps )
					{
						mif.dwTypeData = "Add Karma Actor";
						SetMenuItemInfoA( submenu, ID_BackdropPopupAddKActorHere, FALSE, &mif );
					}
					else
						DeleteMenu( submenu, ID_BackdropPopupAddKActorHere, MF_BYCOMMAND );					

					if( GCurrentPrefab )
					{
						sprintf( Buffer, "&Add Prefab: '%s'", TCHAR_TO_ANSI( GCurrentPrefab->GetPathName() ) );
						mif.dwTypeData = Buffer;
						SetMenuItemInfoA( submenu, ID_BackdropPopupAddPrefabHere, FALSE, &mif );
					}
					else
						DeleteMenu( submenu, ID_BackdropPopupAddPrefabHere, MF_BYCOMMAND );

					// Insert user defined items into the menu
					mif.fType = MFT_STRING;
					mif.fMask = MIIM_TYPE | MIIM_ID;
					for( INT x = 0 ; x < GActorPopupItems.Num() ; ++x )
					{
						sprintf( Buffer, "Add %s Here", TCHAR_TO_ANSI( *GActorPopupItems(x).Desc ) );
						mif.dwTypeData = Buffer;
						mif.wID = GActorPopupItems(x).ID;
						InsertMenuItemA( submenu, ID_SurfPopupAddLight, FALSE, &mif );
					}
				}
				else
				{
					DeleteMenu( submenu, ID_SurfPopupAddClass, MF_BYCOMMAND );
					DeleteMenu( submenu, ID_BackdropPopupAddStaticMeshHere, MF_BYCOMMAND );
					DeleteMenu( submenu, ID_BackdropPopupAddPrefabHere, MF_BYCOMMAND );
					DeleteMenu( submenu, ID_SurfPopupAddLight, MF_BYCOMMAND );
					DeleteMenu( submenu, ID_BackdropPopupAddKActorHere, MF_BYCOMMAND );					
				}

				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					point.x, point.y, 0,
					hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_RTCLICKWINDOW:
			case WM_EDC_RTCLICKWINDOWCANADD:
			{
				POINT point;

				point.x = ((FVector*)LastlParam)->X;
				point.y = ((FVector*)LastlParam)->Y;
				HMENU menu = LoadMenuIdX(hInstance, IDMENU_BackdropPopup),
					submenu = GetSubMenu( menu, 0 );

				::SetCursorPos( point.x, point.y );

				// Customize the menu options we need to.
				MENUITEMINFOA mif;
				char Buffer[255];

				mif.cbSize = sizeof(MENUITEMINFO);
				mif.fMask = MIIM_TYPE;
				mif.fType = MFT_STRING;

				if( GUnrealEd->CurrentClass )
				{
					sprintf( Buffer, "&Add %s Here", TCHAR_TO_ANSI( GUnrealEd->CurrentClass->GetName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddClassHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddClassHere, MF_BYCOMMAND );

				if( GUnrealEd->CurrentStaticMesh )
				{
					sprintf( Buffer, "&Add Static Mesh: '%s'", TCHAR_TO_ANSI( GUnrealEd->CurrentStaticMesh->GetPathName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddStaticMeshHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddStaticMeshHere, MF_BYCOMMAND );
				
				// Add/remove 'Add Karma Actor' menu item.
				if( GUnrealEd->CurrentStaticMesh  && GUnrealEd->CurrentStaticMesh->KPhysicsProps )
				{
					mif.dwTypeData = "Add Karma Actor";
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddKActorHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddKActorHere, MF_BYCOMMAND );		

				if( GCurrentPrefab )
				{
					sprintf( Buffer, "&Add Prefab: '%s'", TCHAR_TO_ANSI( GCurrentPrefab->GetPathName() ) );
					mif.dwTypeData = Buffer;
					SetMenuItemInfoA( submenu, ID_BackdropPopupAddPrefabHere, FALSE, &mif );
				}
				else
					DeleteMenu( submenu, ID_BackdropPopupAddPrefabHere, MF_BYCOMMAND );

				// Put special options at the top of the menu depending on the mode the editor is in.
				mif.fMask = MIIM_TYPE | MIIM_ID;
				switch( GUnrealEd->Mode )
				{
					case EM_Polygon:
						mif.fType = MFT_SEPARATOR;
						InsertMenuItemA( submenu, 0, TRUE, &mif );

						mif.fType = MFT_STRING;
						mif.dwTypeData = "CREATE BRUSH";
						mif.wID = IDMENU_ModeSpecific_CreateBrush;
						InsertMenuItemA( submenu, 0, TRUE, &mif );
						break;
				}

				// Insert user defined items into the menu
				mif.fType = MFT_STRING;
				for( INT x = 0 ; x < GActorPopupItems.Num() ; ++x )
				{
					sprintf( Buffer, "Add %s Here", TCHAR_TO_ANSI( *GActorPopupItems(x).Desc ) );
					mif.dwTypeData = Buffer;
					mif.wID = GActorPopupItems(x).ID;
					InsertMenuItemA( submenu, ID_BackdropPopupAddLightHere, FALSE, &mif );
				}

				TrackPopupMenu( submenu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					point.x, point.y, 0,
					hWnd, NULL);
				DestroyMenu( menu );
			}
			break;

			case WM_EDC_MAPCHANGE:
			{
				GPrefabLevel = NULL;
					GBrowserPrefab->RefreshLevel();
					GBrowserPrefab->RefreshViewport();

				GBrowserMaster->RefreshAll();
				GTerrainEditSheet->PropSheet->RefreshPages();
				GMatineeSheet->Refresh();
				GMatineeSheet->PropSheet->RefreshPages();
				GMatineeSheet->CloseAllPreviews();

				// Rebuild the collision hash if necessary
				// A "1" will be in the lParam if this map change is something major ("new", "open", etc).  Minor things like 
				// brush subtraction will set it to "0".
				if( LastlParam == 1 )
				{
					GUnrealEd->Level->SetActorCollision(0, 1);
					GUnrealEd->Level->SetActorCollision(1);

					GMatineeTools.SetCurrent( GUnrealEd, GUnrealEd->Level, NULL );
				}
			}
			break;

			case WM_EDC_DISPLAYLOADERRORS:
			{
				GDlgLoadErrors->Refresh();
				GDlgLoadErrors->Show(1);
			}
			break;

			case WM_EDC_VIEWPORTUPDATEWINDOWFRAME:
			{
				for( INT x = 0 ; x < GViewports.Num() ; ++x)
					InvalidateRect( GViewports(x).ViewportFrame->hWnd, NULL, 1 );
			}
			break;

			case WM_EDC_SURFPROPS:
			{
				GSurfPropSheet->Show( TRUE );
			}
			break;

			case WM_EDC_MATERIALTREECLICK:
			{
				HMaterialTree* Hit = (HMaterialTree*)LastlParam;
				for( TArray<WDlgTexProp*>::TIterator It(GBrowserTexture->PropWindows) ; It ; ++It )
					if( (DWORD)(*It)->hWnd == Hit->hWnd )
						(*It)->MaterialTreeClick( Hit->Material );
			}
			break;

			//
			// BACKDROP POPUP
			//

			// Root
			case ID_BackdropPopupAddClassHere:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("ACTOR ADD CLASS=%s"), GUnrealEd->CurrentClass->GetName() ) );
				//GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
			}
			break;

			case ID_BackdropPopupAddStaticMeshHere:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("STATICMESH ADD NAME=%s SNAP=1"), GUnrealEd->CurrentStaticMesh->GetName() ) );
				//GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
			}
			break;

			case ID_BackdropPopupAddKActorHere:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("STATICMESH KACTORADD NAME=%s SNAP=1"), GUnrealEd->CurrentStaticMesh->GetName() ) );
				//GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
			}
			break;

			case ID_BackdropPopupAddPrefabHere:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("PREFAB ADD NAME=%s SNAP=1"), GCurrentPrefab->GetName() ) );
				//GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
			}
			break;

			case ID_BackdropPopupAddLightHere:
			{
				GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=LIGHT") );
				//GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
			}
			break;

			case ID_BackdropPopupLevelProperties:
			{
				if( !GUnrealEd->LevelProperties )
				{
					GUnrealEd->LevelProperties = new WObjectProperties( TEXT("LevelProperties"), CPF_Edit, TEXT("Level Properties"), NULL, 1 );
					GUnrealEd->LevelProperties->OpenWindow( hWnd );
					GUnrealEd->LevelProperties->SetNotifyHook( GUnrealEd );
				}
				GUnrealEd->LevelProperties->Root.SetObjects( (UObject**)&GUnrealEd->Level->Actors(0), 1 );
				GUnrealEd->LevelProperties->Show(1);
			}
			break;

			// Grid
			case ID_BackdropPopupGrid1:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=1 Y=1 Z=1") );
			}
			break;

			case ID_BackdropPopupGrid2:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=2 Y=2 Z=2") );
			}
			break;

			case ID_BackdropPopupGrid4:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=4 Y=4 Z=4") );
			}
			break;

			case ID_BackdropPopupGrid8:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=8 Y=8 Z=8") );
			}
			break;

			case ID_BackdropPopupGrid16:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=16 Y=16 Z=16") );
			}
			break;

			case ID_BackdropPopupGrid32:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=32 Y=32 Z=32") );
			}
			break;

			case ID_BackdropPopupGrid64:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=64 Y=64 Z=64") );
			}
			break;

			case ID_BackdropPopupGrid128:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=128 Y=128 Z=128") );
			}
			break;

			case ID_BackdropPopupGrid256:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=256 Y=256 Z=256") );
			}
			break;

			case ID_BackdropPopupGrid512:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=512 Y=512 Z=512") );
			}
			break;

			case ID_BackdropPopupGrid1024:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=1024 Y=1024 Z=1024") );
			}
			break;

			case ID_BackdropPopupGrid2048:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=2048 Y=2048 Z=2048") );
			}
			break;

			case ID_BackdropPopupGrid4096:
			{
				GUnrealEd->Exec( TEXT("MAP GRID X=4096 Y=4096 Z=4096") );
			}
			break;

			// Pivot
			case ID_BackdropPopupPivotSnapped:
			{
				GUnrealEd->Exec( TEXT("PIVOT SNAPPED") );
			}
			break;

			case ID_BackdropPopupPivot:
			{
				GUnrealEd->Exec( TEXT("PIVOT HERE") );
			}
			break;

			//
			// SURFACE POPUP MENU
			//

			// Root
			case ID_SurfProperties:
			{
				GSurfPropSheet->Show( TRUE );
			}
			break;

			case ID_SurfPopupBevel:
			{
				WDlgGeneric dlg( NULL, this, OPTIONS_SURFBEVEL, TEXT("Bevel") );
				if( dlg.DoModal( TEXT("") ) )
				{
					UOptionsSurfBevel* Proxy = Cast<UOptionsSurfBevel>(dlg.Proxy);
					GUnrealEd->Exec( *FString::Printf(TEXT("POLY BEVEL DEPTH=%d BEVEL=%d"), Proxy->Depth, Proxy->Bevel ) );
				}
			}
			break;

			case ID_SurfPopupExtrude16:
				GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=16") );
				break;
			case ID_SurfPopupExtrude32:
				GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=32") );
				break;
			case ID_SurfPopupExtrude64:
				GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=64") );
				break;
			case ID_SurfPopupExtrude128:
				GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=128") );
				break;
			case ID_SurfPopupExtrude256:
				GUnrealEd->Exec( TEXT("POLY EXTRUDE DEPTH=256") );
				break;
			case ID_SurfPopupExtrudeCustom:
			{
				WDlgDepth dlg( NULL, this );
				if( dlg.DoModal() )
				{
					GUnrealEd->Exec( *FString::Printf(TEXT("POLY EXTRUDE DEPTH=%d"), dlg.Depth ) );
				}
			}
			break;

			case ID_SurfPopupAddClass:
			{
				if( GUnrealEd->CurrentClass )
				{
					GUnrealEd->Exec( *FString::Printf(TEXT("ACTOR ADD CLASS=%s"), GUnrealEd->CurrentClass->GetName()));
					GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
				}
			}
			break;

			case ID_SurfPopupAddLight:
			{
				GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=LIGHT") );
				GUnrealEd->Exec( TEXT("POLY SELECT NONE") );
			}
			break;

			case ID_SurfPopupApplyTexture:
			{
				GUnrealEd->Exec( TEXT("POLY SETTEXTURE") );
			}
			break;

			// Align Selected
			case ID_SurfPopupAlignPlanarAuto:
			{
				GSurfPropSheet->AlignmentPage->Align( TEXALIGN_PlanarAuto );
			}
			break;

			case ID_SurfPopupAlignPlanarWall:
			{
				GSurfPropSheet->AlignmentPage->Align( TEXALIGN_PlanarWall );
			}
			break;

			case ID_SurfPopupAlignPlanarFloor:
			{
				GSurfPropSheet->AlignmentPage->Align( TEXALIGN_PlanarFloor );
			}
			break;

			case ID_SurfPopupAlignBox:
			{
				GSurfPropSheet->AlignmentPage->Align( TEXALIGN_Box );
			}
			break;

			case ID_SurfPopupAlignFace:
			{
				GSurfPropSheet->AlignmentPage->Align( TEXALIGN_Face );
			}
			break;

			case ID_SurfPopupUnalign:
			{
				GSurfPropSheet->AlignmentPage->Align( TEXALIGN_Default );
			}
			break;

			// Select Surfaces
			case ID_SurfPopupSelectMatchingGroups:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MATCHING GROUPS") );
			}
			break;

			case ID_SurfPopupSelectMatchingItems:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MATCHING ITEMS") );
			}
			break;

			case ID_SurfPopupSelectMatchingBrush:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MATCHING BRUSH") );
			}
			break;

			case ID_SurfPopupSelectMatchingTexture:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MATCHING TEXTURE") );
			}
			break;

			case ID_SurfPopupSelectAllAdjacents:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT ALL") );
			}
			break;

			case ID_SurfPopupSelectAdjacentCoplanars:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT COPLANARS") );
			}
			break;

			case ID_SurfPopupSelectAdjacentWalls:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT WALLS") );
			}
			break;

			case ID_SurfPopupSelectAdjacentFloors:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT FLOORS") );
			}
			break;

			case ID_SurfPopupSelectAdjacentSlants:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT ADJACENT SLANTS") );
			}
			break;

			case ID_SurfPopupSelectReverse:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT REVERSE") );
			}
			break;

			case ID_SurfPopupMemorize:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MEMORY SET") );
			}
			break;

			case ID_SurfPopupRecall:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MEMORY RECALL") );
			}
			break;

			case ID_SurfPopupOr:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MEMORY INTERSECTION") );
			}
			break;

			case ID_SurfPopupAnd:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MEMORY UNION") );
			}
			break;

			case ID_SurfPopupXor:
			{
				GUnrealEd->Exec( TEXT("POLY SELECT MEMORY XOR") );
			}
			break;


			//
			// ACTOR POPUP MENU
			//

			// Root
			case IDMENU_ModeSpecific_CreateBrush:
			{
				TArray<FVector> PolyMarkers;

				for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
				{
					AActor* pActor = GUnrealEd->Level->Actors(i);
					if( pActor && pActor->IsA(APolyMarker::StaticClass()) )
						new(PolyMarkers)FVector(pActor->Location);
				}

				if( PolyMarkers.Num() < 3 )
					appMsgf(0, TEXT("You must place at least 3 markers to create a brush."));
				else
				{
					WDlgDepth dlg( NULL, this );
					if( dlg.DoModal() )
					{
						FPolyBreaker breaker;
						FPlane plane( PolyMarkers(0), PolyMarkers(1), PolyMarkers(2) );
						FVector PlaneNormal = plane, WkVertex1, WkVertex2;
						breaker.Process( &PolyMarkers, PlaneNormal );

						FVector Origin;
						for( INT vtx = 0 ; vtx < PolyMarkers.Num() ; ++vtx )
							Origin += PolyMarkers(vtx);
						Origin /= PolyMarkers.Num();

						FString Cmd;

						Cmd += TEXT("BRUSH SET\n\n");

						for( INT poly = 0 ; poly < breaker.FinalPolys.Num() ; ++poly )
						{
							Cmd += TEXT("Begin Polygon Flags=0\n");
							for( INT vtx = 0 ; vtx < breaker.FinalPolys(poly).NumVertices ; ++vtx )
							{
								WkVertex1 = (breaker.FinalPolys(poly).Vertex[vtx] + (PlaneNormal * (dlg.Depth / 2.0f))) - Origin;
								Cmd += *FString::Printf(TEXT("Vertex   %1.1f, %1.1f, %1.1f\n"),
									WkVertex1.X, WkVertex1.Y, WkVertex1.Z );
							}
							Cmd += TEXT("End Polygon\n");

							Cmd += TEXT("Begin Polygon Flags=0\n");
							for( INT vtx = breaker.FinalPolys(poly).NumVertices-1 ; vtx > -1 ; --vtx )
							{
								WkVertex1 = (breaker.FinalPolys(poly).Vertex[vtx] - (PlaneNormal * (dlg.Depth / 2.0f))) - Origin;
								Cmd += *FString::Printf(TEXT("Vertex   %1.1f, %1.1f, %1.1f\n"),
									WkVertex1.X, WkVertex1.Y, WkVertex1.Z );
							}
							Cmd += TEXT("End Polygon\n");
						}

						// Sides ...
						//
						for( INT vtx = 0 ; vtx < PolyMarkers.Num() ; ++vtx )
						{
							Cmd += TEXT("Begin Polygon Flags=0\n");

							FVector* pvtxPrev = &PolyMarkers( (vtx ? vtx - 1 : PolyMarkers.Num() - 1 ) );
							FVector* pvtx = &PolyMarkers(vtx);

							WkVertex1 = (*pvtx + (PlaneNormal * (dlg.Depth / 2.0f) )) - Origin;
							WkVertex2 = (*pvtxPrev + (PlaneNormal * (dlg.Depth / 2.0f) )) - Origin;
							Cmd += *FString::Printf(TEXT("Vertex   %1.1f, %1.1f, %1.1f\n"),
								WkVertex1.X, WkVertex1.Y, WkVertex1.Z );
							Cmd += *FString::Printf(TEXT("Vertex   %1.1f, %1.1f, %1.1f\n"),
								WkVertex2.X, WkVertex2.Y, WkVertex2.Z );

							WkVertex1 = (*pvtx - (PlaneNormal * (dlg.Depth / 2.0f) )) - Origin;
							WkVertex2 = (*pvtxPrev - (PlaneNormal * (dlg.Depth / 2.0f) )) - Origin;
							Cmd += *FString::Printf(TEXT("Vertex   %1.1f, %1.1f, %1.1f\n"),
								WkVertex2.X, WkVertex2.Y, WkVertex2.Z );
							Cmd += *FString::Printf(TEXT("Vertex   %1.1f, %1.1f, %1.1f\n"),
								WkVertex1.X, WkVertex1.Y, WkVertex1.Z );

							Cmd += TEXT("End Polygon\n");
						}

						GUnrealEd->edactApplyTransformToBrush( GUnrealEd->Level->Brush() );
						GUnrealEd->Exec( *Cmd );
						GUnrealEd->Level->Brush()->Location = Origin;
					}

					// Delete all poly markers.
					GUnrealEd->Exec(TEXT("POLYGON DELETE"));
				}

				PolyMarkers.Empty();
			}
			break;

			case IDMENU_ActorPopupCut:
				GUnrealEd->Exec(TEXT("EDIT CUT"));
				break;

			case IDMENU_ActorPopupCopy:
				GUnrealEd->Exec(TEXT("EDIT COPY"));
				break;

			case IDMENU_ActorPopupPasteOriginal:
				GUnrealEd->Exec(TEXT("EDIT PASTE TO=ORIGINAL"));
				break;

			case IDMENU_ActorPopupPasteHere:
				GUnrealEd->Exec(TEXT("EDIT PASTE TO=HERE"));
				break;

			case IDMENU_ActorPopupPasteOrigin:
				GUnrealEd->Exec(TEXT("EDIT PASTE TO=ORIGIN"));
				break;

			case IDMENU_ActorPopupProperties:
			{
				GUnrealEd->ShowActorProperties();
			}
			break;

			case IDMENU_ActorPopupSelectMatchingStaticMeshes:
			{
				GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGSTATICMESH") );
			}
			break;

			case IDMENU_ActorPopupSelectAllClass:
			{
				FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_NUM_SELECTED | GI_CLASSNAME_SELECTED );

				if( gir.iValue )
				{
					appSprintf( l_chCmd, TEXT("ACTOR SELECT OFCLASS CLASS=%s"), *gir.String );
					GUnrealEd->Exec( l_chCmd );
				}
			}
			break;

			case IDMENU_ActorPopupSelectAll:
			{
				GUnrealEd->Exec( TEXT("ACTOR SELECT ALL") );
			}
			break;

			case IDMENU_ActorPopupSelectAllInZone:
			{
				GUnrealEd->Exec( TEXT("ACTOR SELECT MATCHINGZONE") );
			}
			break;

			case IDMENU_ActorPopupSelectNone:
			{
				GUnrealEd->Exec( TEXT("SELECT NONE") );
			}
			break;

			case IDMENU_ActorPopupDuplicate:
			{
				GUnrealEd->Exec( TEXT("ACTOR DUPLICATE") );
			}
			break;

			case IDMENU_ActorPopupDelete:
			{
				GUnrealEd->Exec( TEXT("ACTOR DELETE") );
			}
			break;

			case IDMENU_ActorPopupEditScript:
			{
				GBrowserMaster->ShowBrowser(eBROWSER_ACTOR);
				FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_CLASS_SELECTED );
				GCodeFrame->AddClass( gir.pClass );
				if( GBrowserActor->IsDocked() ) GBrowserMaster->Show(1);
			}
			break;

			case IDMENU_ActorPopupMakeCurrent:
			{
				FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_CLASSNAME_SELECTED );
				GUnrealEd->Exec( *FString::Printf(TEXT("SETCURRENTCLASS CLASS=%s"), *gir.String));
			}
			break;

			case IDMENU_ActorPopupAlignCameras:
			{
				GUnrealEd->Exec( TEXT("CAMERA ALIGN") );
			}
			break;

			case IDMENU_ActorPopupMerge:
			{
				GUnrealEd->Exec(TEXT("BRUSH MERGEPOLYS"));
			}
			break;

			case IDMENU_ActorPopupSeparate:
			{
				GUnrealEd->Exec(TEXT("BRUSH SEPARATEPOLYS"));
			}
			break;

			// Select Brushes
			case IDMENU_ActorPopupSelectBrushesAdd:
			{
				GUnrealEd->Exec( TEXT("MAP SELECT ADDS") );
			}
			break;

			case IDMENU_ActorPopupSelectBrushesSubtract:
			{
				GUnrealEd->Exec( TEXT("MAP SELECT SUBTRACTS") );
			}
			break;

			case IDMENU_ActorPopupSubtractBrushesSemisolid:
			{
				GUnrealEd->Exec( TEXT("MAP SELECT SEMISOLIDS") );
			}
			break;

			case IDMENU_ActorPopupSelectBrushesNonsolid:
			{
				GUnrealEd->Exec( TEXT("MAP SELECT NONSOLIDS") );
			}
			break;

			// Movers
			case IDMENU_ActorPopupKey0:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=0") );
			}
			break;

			case IDMENU_ActorPopupKey1:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=1") );
			}
			break;

			case IDMENU_ActorPopupKey2:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=2") );
			}
			break;

			case IDMENU_ActorPopupKey3:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=3") );
			}
			break;

			case IDMENU_ActorPopupKey4:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=4") );
			}
			break;

			case IDMENU_ActorPopupKey5:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=5") );
			}
			break;

			case IDMENU_ActorPopupKey6:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=6") );
			}
			break;

			case IDMENU_ActorPopupKey7:
			{
				GUnrealEd->Exec( TEXT("ACTOR KEYFRAME NUM=7") );
			}
			break;

			// Set
			case IDMENU_ActorLocRotFromCam:
			{
				GUnrealEd->Exec( TEXT("ACTOR SET LOCROTFROMCAMERA") );
			}
			break;

			// Reset
			case IDMENU_ActorPopupResetOrigin:
			{
				GUnrealEd->Exec( TEXT("ACTOR RESET LOCATION") );
			}
			break;

			case IDMENU_ActorPopupResetPivot:
			{
				GUnrealEd->Exec( TEXT("ACTOR RESET PIVOT") );
			}
			break;

			case IDMENU_ActorPopupResetRotation:
			{
				GUnrealEd->Exec( TEXT("ACTOR RESET ROTATION") );
			}
			break;

			case IDMENU_ActorPopupResetScaling:
			{
				GUnrealEd->Exec( TEXT("ACTOR RESET SCALE") );
			}
			break;

			case IDMENU_ActorPopupResetAll:
			{
				GUnrealEd->Exec( TEXT("ACTOR RESET ALL") );
			}
			break;

			// Transform
			case IDMENU_ActorPopupMirrorX:
			{
				GUnrealEd->Exec( TEXT("ACTOR MIRROR X=-1") );
			}
			break;

			case IDMENU_ActorPopupMirrorY:
			{
				GUnrealEd->Exec( TEXT("ACTOR MIRROR Y=-1") );
			}
			break;

			case IDMENU_ActorPopupMirrorZ:
			{
				GUnrealEd->Exec( TEXT("ACTOR MIRROR Z=-1") );
			}
			break;

			case IDMENU_ActorPopupPerm:
			{
				GUnrealEd->Exec( TEXT("ACTOR APPLYTRANSFORM") );
			}
			break;

			// Order
			case IDMENU_ActorPopupToFirst:
			{
				GUnrealEd->Exec( TEXT("MAP SENDTO FIRST") );
			}
			break;

			case IDMENU_ActorPopupToLast:
			{
				GUnrealEd->Exec( TEXT("MAP SENDTO LAST") );
			}
			break;

			case IDMENU_ActorPopupSwapOrder:
			{
				GUnrealEd->Exec( TEXT("MAP SENDTO SWAP") );
			}
			break;

			// Copy Polygons
			case IDMENU_ActorPopupToBrush:
			{
				GUnrealEd->Exec( TEXT("MAP BRUSH GET") );
			}
			break;

			case IDMENU_ActorPopupFromBrush:
			{
				GUnrealEd->Exec( TEXT("MAP BRUSH PUT") );
			}
			break;

			// Solidity
			case IDMENU_ActorPopupMakeSolid:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, 0 ) );
			}
			break;

			case IDMENU_ActorPopupMakeSemisolid:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, PF_Semisolid ) );
			}
			break;

			case IDMENU_ActorPopupMakeNonSolid:
			{
				GUnrealEd->Exec( *FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, PF_NotSolid ) );
			}
			break;

			// CSG
			case IDMENU_ActorPopupMakeAdd:
			{
				GUnrealEd->Exec( *FString::Printf(TEXT("MAP SETBRUSH CSGOPER=%d"), CSG_Add) );
			}
			break;

			case IDMENU_ActorPopupMakeSubtract:
			{
				GUnrealEd->Exec( *FString::Printf(TEXT("MAP SETBRUSH CSGOPER=%d"), CSG_Subtract) );
			}
			break;

			// CONVERT
			case IDMENU_ConvertToStaticMesh:
			{
				GBrowserStaticMesh->CreateFromSelection();
			}
			break;

			case IDMENU_ConvertToBrush:
			{
				GUnrealEd->Exec( TEXT("STATICMESH TO BRUSH") );
			}
			break;
		
			// ALIGN
			case IDMENU_SnapToFloor:
			{
				GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=0") );
			}
			break;

			case IDMENU_AlignToFloor:
			{
				GUnrealEd->Exec( TEXT("ACTOR ALIGN SNAPTOFLOOR ALIGN=1") );
			}
			break;

			// OTHER

			case IDMENU_SaveBrushAsCollision:
			{
				GUnrealEd->Exec( TEXT("STATICMESH SAVEBRUSHASCOLLISION") );
			}
			break;

			default:
				WMdiFrame::OnCommand(Command);
				break;
		}
		unguard;
	}
	void NotifyDestroy( void* Other )
	{
		if( Other==Preferences )
			Preferences=NULL;
	}

	virtual void OpenLevelView()
	{
		guard(WEditorFrame::OpenLevelView);

		// This is making it so you can only open one level window - it will reuse it for each
		// map you load ... which is not really MDI.  But the editor has problems with 2+ level windows open.  
		// Fix if you can...
		if( !GLevelFrame )
		{
			GLevelFrame = new WLevelFrame( GUnrealEd->Level, TEXT("LevelFrame"), &BackgroundHolder );
			GLevelFrame->OpenWindow( 1, 1 );
		}

		unguard;
	}
};

void FileOpen( HWND hWnd )
{
	guard(FileOpen);

    // gam ---
    int rc = FileSaveChangesFirst( GLevelFrame->hWnd );
    if ((rc < 0) || ((rc > 0) && !FileSave( hWnd )))
        return;

    if( GEditor->ShowIntWarnings )
    {
        rc = FileExportIntRequired( GLevelFrame->hWnd );
        if ((rc < 0) || ((rc > 0) && !FileIntExport( hWnd )))
            return;
    }
    // --- gam

	OPENFILENAMEA ofn;
	char File[512] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(File);
	char Filter[255];
	::sprintf( Filter,
		"Map Files (*.%s)%c*.%s%cAll Files%c*.*%c%c",
		appToAnsi( *GMapExt ),
		'\0',
		appToAnsi( *GMapExt ),
		'\0',
		'\0',
		'\0',
		'\0' );
	ofn.lpstrFilter = Filter;
	ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UNR]) );
	ofn.lpstrDefExt = appToAnsi( *GMapExt );
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	// Display the Open dialog box. 
	if( GetOpenFileNameA(&ofn) )
	{
		// Make sure there's a level frame open.
		GEditorFrame->OpenLevelView();
			
		GBottomBar->Reset();

		// Convert the ANSI filename to UNICODE, and tell the editor to open it.
		GLevelFrame->SetMapFilename( (TCHAR*)appFromAnsi(File) );
		GUnrealEd->Exec( *FString::Printf(TEXT("MAP LOAD FILE=\"%s\""), GLevelFrame->GetMapFilename() ) );

		FString S = GLevelFrame->GetMapFilename();
		GMRUList->AddItem( GLevelFrame->GetMapFilename() );
		GMRUList->AddToMenu( hWnd, GMainMenu, 1 );

		GLastDir[eLASTDIR_UNR] = S.Left( S.InStr( TEXT("\\"), 1 ) );

		GMRUList->AddItem( GLevelFrame->GetMapFilename() );
		GMRUList->AddToMenu( hWnd, GMainMenu, 1 );
		GMRUList->WriteINI();
	}

	// Make sure that the browsers reflect any new data the map brought with it.
	//RefreshEditor( ERefreshEditor_Misc | ERefreshEditor_AllBrowsers | ERefreshEditor_ActorBrowser );
	GButtonBar->RefreshBuilders();

	GFileManager->SetDefaultDirectory(appBaseDir());
	unguard;
}

/*-----------------------------------------------------------------------------
	Startup.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::Init()
{
	guard(UUnrealEdEngine::Init);
	UEditorEngine::Init();
	// Set editor mode.
	edcamSetMode( EM_ViewportMove );	
	unguard;
}

UUnrealEdEngine* InitUnrealEdEngine()
{
	guard(InitEngine);
	DOUBLE LoadTime = appSeconds();

	// Set exec hook.
	static FExecHook GLocalHook;
	GExec = &GLocalHook;

	// Editor.
	UClass* EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("UnrealEd.UnrealEdEngine"), NULL, LOAD_NoFail, NULL );
	UUnrealEdEngine* Engine = ConstructObject<UUnrealEdEngine>( EngineClass );	

	Engine->Init();
	debugf( TEXT("Startup time: %f seconds"), appSeconds()-LoadTime );

	return Engine;
	unguard;
}

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

//
// Main window entry point.
//
INT CallWinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, const TCHAR* RawCmdLine, char* InCmdLine, INT nCmdShow )
{
	// Remember instance.
	GIsStarted = 1;
	hInstance = hInInstance;
	GUnrealEdModule = hInstance; // Replaces lots of individual cases of GetModuleHandleA("unrealed.exe").

	// Set package name.
	// appStrcpy( GPackage, appPackage() ); //  Not essential ? This breaks the call-UnrealEd-as-a-DLL scheme - Erik.

	// Begin.
#ifndef _DEBUG
	try
	{
#endif
		// Set mode.
		GIsClient = GIsServer = GIsEditor = GLazyLoad = 1;
		GIsScriptable = 0;
		GIsClocking = 1;

		// Start main loop.
		GIsGuarded=1;

		// Create a fully qualified pathname for the log file.  If we don't do this, pieces of the log file
		// tends to get written into various directories as the editor starts up.
		TCHAR chLogFilename[512] = TEXT("\0");
		appSprintf( chLogFilename, TEXT("%s%s"), appBaseDir(), TEXT("Editor.log"));
		appStrcpy( Log.Filename, chLogFilename );
                
		appInit( TEXT("UT2004"), RawCmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 ); // gam               
                GTransientNaming = 1; // sjs - this cannot be overridden by cmdline, otherwise badness

		// Launch the bug report monitor.
#ifndef _DEBUG
#if 0
		const TCHAR* Parameters[] = { TEXT("BugReport"), appItoa(GetCurrentProcessId()), NULL };
		_wspawnv(
			_P_NOWAIT,
			TEXT("BugReport"),
			Parameters
			);
#endif
#endif

		// Init windowing.
		InitWindowing();
		IMPLEMENT_WINDOWCLASS(WMdiFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WEditorFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBackgroundHolder,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WLevelFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WDockingFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WCodeFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(W2DShapeEditor,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WViewportFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowser,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserSound,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserMusic,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserGroup,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);

#ifdef WITH_LIPSINC
		IMPLEMENT_WINDOWCLASS(WBrowserLIPSinc,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
#endif

		IMPLEMENT_WINDOWCLASS(WBrowserMaster,CS_DBLCLKS  | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserTexture,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserStaticMesh,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserMesh,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserAnimation,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserPrefab,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBrowserActor,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWSUBCLASS(WMdiClient,TEXT("MDICLIENT"));
		IMPLEMENT_WINDOWCLASS(WButtonBar,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WButtonGroup,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBottomBar,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBottomBarStandard,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WVFToolBar,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WTopBar,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WBuildPropSheet,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageOptions,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageLevelStats,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WSurfacePropSheet,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WTerrainEditSheet,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WMatineeSheet,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WMatineePreview,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageScenes,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageActions,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageSubActions,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageMatTools,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WSurfacePropPage,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageFlags,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPagePanRotScale,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageAlignment,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageStats,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageTools,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageMisc,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WTerrainToolsSheet,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageToolsTerrains,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageToolsLayers,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageToolsDecoLayers,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageMaterials,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageUsed,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WPageMRU,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		
		// Added by Demiurge (PSE)
		IMPLEMENT_WINDOWCLASS(WParticleEditor,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		//
		IMPLEMENT_WINDOWCLASS(WParticleEditorTab,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WParticleEditorBeamTab,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WParticleEditorSpriteTab,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WParticleEditorMeshTab,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WParticleEditorSparkTab,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		//
		IMPLEMENT_WINDOWCLASS(WParticleGeneralComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleTextureComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleMeshTextureComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleMeshComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleSparkComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WParticleMovementComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleSizeComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleFadingColorComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleRenderingComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleTimeComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleLocationComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleBasicRotationComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleSpriteRotationComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleMeshRotationComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleCollisionComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleBeamComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleBeamNoiseComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleBeamBranchingComponent,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		//
		IMPLEMENT_WINDOWCLASS(WColorScaleEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WRangeEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WRangeVectorEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WRangeRotatorEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WRangeColorEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WXYZVectorEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WColorEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WColorMultEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WPlaneEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WFadingEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleStaticLabel,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleExpandingLabel,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WFloatEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WPercentEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WIntEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WNoNegNumberEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WBoolEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WEnableEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WRotatorEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WStringEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WNameEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WTexturePickEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WSoundPickEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
		IMPLEMENT_WINDOWCLASS(WMeshPickEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WScaleEmitterEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WSpeedScaleEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticleTextureArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WNameTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WEnumEdit,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		//
		IMPLEMENT_WINDOWCLASS(WParticleStructTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticleFloatArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticlePlaneArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticleSizeScaleArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		//IMPLEMENT_WINDOWCLASS(WParticleColorScaleArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticleBeamEndPointArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticleBeamScaleArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		IMPLEMENT_WINDOWCLASS(WParticleSoundArrayTool,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);	
		//
		IMPLEMENT_WINDOWCLASS(WParticleEditorNewEmitterDlg,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WParticleEditorExportDlg,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WEmitterWizard,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WEmitterWizardPage,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WStartWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WEndWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WNewEmitterWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WShapeWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WMovementWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WTextureWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WMeshWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WDenseWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WRotationWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WTimeWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WAdditionalMovementWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 
		IMPLEMENT_WINDOWCLASS(WBeamNoiseWP,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW); 

		// End add by Demiurge (PSE)

		// Windows.
		if (GUnRealEdFrame == 0) 
		{
			GUnRealEdFrame = new WEditorFrame();
		}
		WEditorFrame& Frame = *GUnRealEdFrame;
		Frame.OpenWindow();

		UBOOL ShowLog = ParseParam(appCmdLine(),TEXT("log"));
		if( !ShowLog && !ParseParam(appCmdLine(),TEXT("server")) )
			InitSplash( TEXT("EdSplash.bmp") );

		// Init.
		GLogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("EditorLog"), &Frame );
		GLogWindow->OpenWindow( ShowLog, 0 );
		GLogWindow->MoveWindow( 100, 100, 450, 450, 0 );

#ifdef WITH_LIPSINC
		GLIPSincInitEditor();  
#endif

		// Init engine.
		GEditor = GUnrealEd = InitUnrealEdEngine();

		// Main window
		GUnrealEd->hWndMain = GEditorFrame->hWnd;
		GhwndEditorFrame = GEditorFrame->hWnd;

		// Set up autosave timer.  We ping the engine once a minute and it determines when and 
		// how to do the autosave.
		SetTimer( GEditorFrame->hWnd, 900, 60000, NULL);

		// UnrealEd INI.
		if( GFileManager->FileSize(TEXT("unrealed.ini"))<0 )
		{
			// Create UnrealEd.ini from DefUnrealEd.ini.
			FString S;
			if( !appLoadFileToString( S, TEXT("DefUnrealEd.ini"), GFileManager ) )
				appErrorf( LocalizeError(TEXT("Core"),TEXT("UnrealEd")), "DefUnrealEd.ini" );
			appSaveStringToFile( S, TEXT("unrealed.ini") );
		}

		// Initialize "last dir" array
		GLastDir[eLASTDIR_UNR] = TEXT("..\\maps");
		GLastDir[eLASTDIR_UTX] = TEXT("..\\textures");
		GLastDir[eLASTDIR_UAX] = TEXT("..\\sounds");
		GLastDir[eLASTDIR_UPX] = TEXT("..\\prefabs");
		GLastDir[eLASTDIR_USX] = TEXT("..\\staticmeshes");
		GLastDir[eLASTDIR_UKX] = TEXT("..\\animations");

		if( !GConfig->GetString( TEXT("Directories"), TEXT("PCX"), GLastDir[eLASTDIR_PCX], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_PCX] = TEXT("..\\textures");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("WAV"), GLastDir[eLASTDIR_WAV], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_WAV] = TEXT("..\\sounds");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("BRUSH"), GLastDir[eLASTDIR_BRUSH], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_BRUSH] = TEXT("..\\maps");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("2DS"), GLastDir[eLASTDIR_2DS], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_2DS] = TEXT("..\\maps");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("PSA"), GLastDir[eLASTDIR_PSA], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_PSA] = TEXT("..\\animations");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("PSK"), GLastDir[eLASTDIR_PSK], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_PSK] = TEXT("..\\animations");

#ifdef WITH_LIPSINC
		if( !GConfig->GetString( TEXT("Directories"), TEXT("LBP"), GLastDir[eLASTDIR_LBP], TEXT("UnrealEd.ini") ) )		GLastDir[eLASTDIR_LBP] = TEXT(".\\LIPSincData");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("LTF"), GLastDir[eLASTDIR_LTF], TEXT("UnrealEd.ini") ) )     GLastDir[eLASTDIR_LTF] = TEXT(".\\LIPSincData");
		if( !GConfig->GetString( TEXT("Directories"), TEXT("IEX"), GLastDir[eLASTDIR_IEX], TEXT("UnrealEd.ini") ) )     GLastDir[eLASTDIR_IEX] = TEXT(".\\LIPSincData");
#endif

		if( !GConfig->GetString( TEXT("URL"), TEXT("MapExt"), GMapExt, TEXT("UT2004.ini") ) )		GMapExt = TEXT("unr");
		GUnrealEd->Exec( *FString::Printf(TEXT("MODE MAPEXT=%s"), *GMapExt ) );

		UInput::StaticInitInput();

		// Init the editor tools.
		GTerrainTools.Init();
		GRebuildTools.Init();

		GEdModeTools = new FEdModeTools;
		GEdModeTools->Init();
		GTBOptions = new FTBOptions;
		GMaterialTools = new FMaterialTools;

		GTexAlignTools.Init();
		GMatineeTools.Init();

		GButtonBar = new WButtonBar( TEXT("EditorToolbar"), &Frame.LeftFrame );
		GButtonBar->OpenWindow();
		Frame.LeftFrame.Dock( GButtonBar );
		Frame.LeftFrame.OnSize( SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE, 0, 0 );

		GBottomBar = new WBottomBar( TEXT("BottomBar"), &Frame.BottomFrame, &Frame.BottomFrame );
		GBottomBar->OpenWindow();
		Frame.BottomFrame.Dock( GBottomBar );
		Frame.BottomFrame.OnSize( SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE, 0, 0 );

		GTopBar = new WTopBar( TEXT("TopBar"), &Frame.TopFrame );
		GTopBar->OpenWindow();
		Frame.TopFrame.Dock( GTopBar );
		Frame.TopFrame.OnSize( SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE, 0, 0 );

		GBrowserMaster = new WBrowserMaster( TEXT("Master Browser"), GEditorFrame );
		GBrowserMaster->OpenWindow( 0 );
		GBrowserMaster->Browsers[eBROWSER_MESH] = (WBrowser**)(&GBrowserMesh);
		GBrowserMaster->Browsers[eBROWSER_ANIMATION] = (WBrowser**)(&GBrowserAnimation);
		GBrowserMaster->Browsers[eBROWSER_MUSIC] = (WBrowser**)(&GBrowserMusic);
		GBrowserMaster->Browsers[eBROWSER_SOUND] = (WBrowser**)(&GBrowserSound);
		GBrowserMaster->Browsers[eBROWSER_ACTOR] = (WBrowser**)(&GBrowserActor);
		GBrowserMaster->Browsers[eBROWSER_GROUP] = (WBrowser**)(&GBrowserGroup);
		GBrowserMaster->Browsers[eBROWSER_TEXTURE] = (WBrowser**)(&GBrowserTexture);
		GBrowserMaster->Browsers[eBROWSER_STATICMESH] = (WBrowser**)(&GBrowserStaticMesh);
		GBrowserMaster->Browsers[eBROWSER_PREFAB] = (WBrowser**)(&GBrowserPrefab);

#ifdef WITH_LIPSINC
		GBrowserMaster->Browsers[eBROWSER_LIPSINC] = (WBrowser**)(&GBrowserLIPSinc);
#endif 
		
		GBuildSheet = new WBuildPropSheet( TEXT("Build Options"), GEditorFrame );
		GBuildSheet->OpenWindow();
		GBuildSheet->Show( FALSE );

		GSurfPropSheet = new WSurfacePropSheet( TEXT("Surface Properties"), GEditorFrame );
		GSurfPropSheet->OpenWindow();
		GSurfPropSheet->Show( FALSE );

		GTerrainEditSheet = new WTerrainEditSheet( TEXT("Terrain Editing"), GEditorFrame );
		GTerrainEditSheet->OpenWindow();
		GTerrainEditSheet->Show( FALSE );

		GMatineeSheet = new WMatineeSheet( TEXT("Matinee"), GEditorFrame );
		GMatineeSheet->OpenWindow();
		GMatineeSheetHwnd = GMatineeSheet->hWnd;
		GMatineeSheet->Show( FALSE );

		// Open a blank level on startup.
		Frame.OpenLevelView();

		// Reopen whichever windows we need to.
		UBOOL bDocked = 0, bActive = 0;

		if(!GConfig->GetInt( TEXT("Mesh Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_MESH );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserMesh->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserMesh->Show( bActive );
		}

		if(!GConfig->GetInt( TEXT("Animation Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_ANIMATION );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserAnimation->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserAnimation->Show( bActive );
		}
		
		if(!GConfig->GetInt( TEXT("Prefab Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_PREFAB );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserPrefab->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserPrefab->Show( bActive );
		}
		
		if(!GConfig->GetInt( TEXT("Music Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_MUSIC );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserMusic->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserMusic->Show( bActive );
		}

		if(!GConfig->GetInt( TEXT("Sound Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_SOUND );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserSound->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserSound->Show( bActive );
		}

		if(!GConfig->GetInt( TEXT("Actor Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_ACTOR );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserActor->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserActor->Show( bActive );
		}

		if(!GConfig->GetInt( TEXT("Group Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_GROUP );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserGroup->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserGroup->Show( bActive );
		}

#ifdef WITH_LIPSINC
		if(!GConfig->GetInt( TEXT("LIPSinc Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_LIPSINC );
		if( !bDocked )
		{
			if(!GConfig->GetInt( *GBrowserLIPSinc->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserLIPSinc->Show( bActive );
		}
#endif

		if(!GConfig->GetInt( TEXT("Static Mesh Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_STATICMESH );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserStaticMesh->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserStaticMesh->Show( bActive );
		}
		
		if(!GConfig->GetInt( TEXT("Texture Browser"), TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") ))	bDocked = FALSE;
		SendMessageX( GEditorFrame->hWnd, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, eBROWSER_TEXTURE );
		if( !bDocked ) 
		{
			if(!GConfig->GetInt( *GBrowserTexture->PersistentName, TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
			GBrowserTexture->Show( bActive );
		}
		
		if(!GConfig->GetInt( TEXT("CodeFrame"), TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = FALSE;
		if( bActive )	ShowCodeFrame( GEditorFrame );

		GCodeFrame = new WCodeFrame( TEXT("CodeFrame"), GEditorFrame );
		GCodeFrame->OpenWindow( 0, 0 );

		GMainMenu = LoadMenuIdX( hInstance, IDMENU_MainMenu );
		SetMenu( GEditorFrame->hWnd, GMainMenu );

		GMRUList = new MRUList( TEXT("MRU") );
		GMRUList->ReadINI();
		GMRUList->AddToMenu( GEditorFrame->hWnd, GMainMenu, 1 );

		FString Wk;
		for( INT x = 0 ; GConfig->GetString( TEXT("PopupActorList"), *FString::Printf(TEXT("Item%d"),x), Wk, TEXT("UnrealEd.ini") ) ; ++x )
			new( GActorPopupItems )StringPair( 
				Wk.Left( Wk.InStr( TEXT(","), 0 ) ),
				Wk.Right( Wk.Len() - Wk.InStr( TEXT(","), 1 ) - 1 ), 
				IDMN_ACTORPOPUPITEMS_BASE+x );

		ExitSplash();

		GOptionProxies = new TMap<INT,UOptionsProxy*>;
		OptionProxyInit();

		if(!GConfig->GetInt( TEXT("MasterBrowser"), TEXT("Active"), bActive, TEXT("UnrealEd.ini") ))	bActive = TRUE;
		if( bActive ) GBrowserMaster->Show(1);
		ShowWindow( Frame.hWnd, SW_SHOWMAXIMIZED );

		GUnrealEd->Level->SetActorCollision(1);

		// Tip of the Day
		UBOOL bShowTips;
		if(!GConfig->GetInt( TEXT("Tip Of The Day"), TEXT("ShowTips"), bShowTips, TEXT("UnrealEd.ini") ))	bShowTips = FALSE;
		if( bShowTips )
		{
			GDlgTOTD->Show(1);
		}

#ifndef _DEBUG
	}
	catch( ... )
	{
		// Crashed.
		Error.HandleError();
	}
#endif

	return 0;
}

INT ShutDownWinMain(void)
{
#ifndef _DEBUG
	try
	{
#endif
		if (GUnRealEdFrame) 
                {
			if (GExtCallLaunched) 
                        {
			//	GUnRealEdFrame->OnDestroy(); // NEW
				GUnRealEdFrame->doOnClose();
			}
			delete GUnRealEdFrame;
			GUnRealEdFrame = 0;
		}

#ifdef WITH_LIPSINC
		GLIPSincShutdownEditor();
#endif
		GRebuildTools.Shutdown();
		GFileManager->Delete(TEXT("Running.ini"),0,0);
		if( GLogWindow )
			delete GLogWindow;
		appPreExit();
		GIsGuarded = 0;
		delete GMRUList;
		delete GCodeFrame;
		delete GEdModeTools;
		delete GTBOptions;
		delete GMaterialTools;
		delete GOptionProxies;	
                GMapExt.Empty();
		GActorPopupItems.Empty();
		for( INT x = 0 ; x < eLASTDIR_MAX ; ++x )
			GLastDir[x].Empty();

#ifndef _DEBUG
	}
	catch( ... )
	{
		// Crashed.
		Error.HandleError();
	}
#endif

	// Shut down.
	appExit();
	GIsStarted = 0;
	return 0;
}


INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char* InCmdLine, INT nCmdShow )
{
	appStrcpy( GPackage, TEXT("UnrealEd") );

	const TCHAR * rawCmdLine;
#ifdef  _UNICODE
	//UNICODE
	rawCmdLine = GetCommandLine();
#else	
	//SBCS and MBCS
	rawCmdLine = lpCmdLine;
#endif

#if defined( ENABLE_COM_IF )
	if( ! StartCOMServices( rawCmdLine ) )
	{
		return( FALSE );
	}
#endif

	INT res = CallWinMain( hInInstance, 0, rawCmdLine, InCmdLine, nCmdShow );


#ifndef _DEBUG
	try
#endif
	{
		MainLoop(GUnrealEd);

		ShutDownWinMain();
	}
#ifndef _DEBUG
	catch(...)
	{
		// Uninitialize the COM Library.
		CoUninitialize();

		Error.HandleError();
	}
#endif

	// Uninitialize the COM Library.
	CoUninitialize();

	return res;
}


//////////////////////////////////////////////////////////////////////////

UNREALED_API INT LaunchEditor( HINSTANCE hInInstance, char* InCmdLine, INT nCmdShow )
{
	if (GExtCallLaunched) {
		return 0;
	}
	GExtCallLaunched = true;
	
	// Set package name.
	appStrcpy( GPackage, TEXT("UnrealEd") );

	const TCHAR * RawCmdLine;
#ifdef  _UNICODE
	//UNICODE
	RawCmdLine = GetCommandLine();
#else	
	//SBCS and MBCS
	RawCmdLine = lpCmdLine;
#endif

	INT res = CallWinMain( hInInstance, 0, RawCmdLine, InCmdLine, nCmdShow );
	
	if (GExtCallLoopInstance == 0) {
		GExtCallLoopInstance = new CMainLoop( GUnrealEd, false );
		GExtCallLoopInstance->RunLoop();
	}

	return res;
}

UNREALED_API INT ShutDownEditor(void)
{
	if (!GExtCallLaunched) {
		return 0;
	}
	if (GExtCallShutdown) {
		return 0;
	}
	GExtCallShutdown = true;

	if (GExtCallLoopInstance) {
		delete GExtCallLoopInstance;
		GExtCallLoopInstance = 0;
		return ShutDownWinMain();
	} else {
		return 0;
	}
}

UNREALED_API void PollEditor(void)
{
	if (GExtCallLoopInstance != 0) {
		GExtCallLoopInstance->RunLoop();
		if (GExtCallLoopInstance->Finished()) {
			ShutDownEditor();
		}
	}
}

UNREALED_API	bool IsLaunched(void)
{
	return GExtCallLaunched;
}

UNREALED_API	bool IsShutdown(void)
{
	return GExtCallShutdown;
}

UNREALED_API	void FinishAnimImport(void)
{
	GBrowserAnimation->RefreshAll();
}

UNREALED_API	void FinishMeshImport(void)
{
	GBrowserStaticMesh->RefreshAll();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

