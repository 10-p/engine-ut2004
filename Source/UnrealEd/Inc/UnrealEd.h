/*=============================================================================
	UnrealEd.h: UnrealEd public header file.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	* Created out of frustration by Jack Porter
=============================================================================*/

#ifndef _INC_UNREALED
#define _INC_UNREALED

#define UNREALED_API DLL_EXPORT

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#pragma warning( disable : 4201 )
#ifndef STRICT
#define STRICT
#endif
#pragma pack(push,8)
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <process.h>
#pragma pack(pop)

#include "Engine.h"
#include "UnRender.h"
#include "Window.h"
#include "..\..\Editor\Src\EditorPrivate.h"		//!!

#include "UnrealEdClasses.h"

// old editor.dll stuff
#include "EdModes.h"
#include "UnEdModeTools.h"
#include "UnMaterialTools.h"
#include "UnEditorOptions.h"

// unrealed stuff
#include "GenericDlgOptions.h"
#include "UnTexAlignTools.h"
#include "Res\resource.h"
#include "UnrealEdMisc.h"
#include "Browser.h"
#include "BrowserMaster.h"
#include "MRUList.h"
#include "DlgTexProp.h"
#include "BrowserAnimation.h"

#ifdef WITH_LIPSINC
#include "BrowserLIPSinc.h"
#endif

#include "SurfacePropSheet.h"
#include "BuildPropSheet.h"
#include "ViewportFrame.h"
// Added by Demiurge (PSE)
#include "EmitterWizard.h"
#include "ParticleEditorControls.h"
#include "ParticleEditor.h"
#include "ParticleEditorTab.h"
#include "ParticleEditorToolLabel.h"
#include "ParticleEditorComponent.h"
#include "ParticleEditorTools.h"
// End add by Demiurge (PSE)


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

extern UNREALED_API class FTBOptions* GTBOptions;
extern UNREALED_API class FEdModeTools* GEdModeTools;
extern UNREALED_API class FMaterialTools* GMaterialTools;

// Browsers
extern WBrowserMaster* GBrowserMaster;
extern WBrowserAnimation* GBrowserAnimation;

#ifdef WITH_LIPSINC
extern WBrowserLIPSinc* GBrowserLIPSinc;
#endif

extern UPrefab* GCurrentPrefab;
extern ULevel* GPrefabLevel;		// A temporary level we assign to the prefab viewport, where we hold the prefabs actors for viewing.
extern FTexAlignTools GTexAlignTools;



extern INT GViewportStyle, GViewportConfig;

extern ENGINE_API FRebuildTools GRebuildTools;
/*-----------------------------------------------------------------------------
	Structs.
-----------------------------------------------------------------------------*/

enum EViewportStyle
{
	VSTYLE_Floating		= 0,
	VSTYLE_Fixed		= 1,
};

typedef struct {
	INT RendMap;
	FLOAT PctLeft, PctTop, PctRight, PctBottom;	// Percentages of the parent window client size (VSTYLE_Fixed)
	FLOAT Left, Top, Right, Bottom;				// Literal window positions (VSTYLE_Floatin)
	WViewportFrame* ViewportFrame;
} VIEWPORTCONFIG;

// This is a list of all the viewport configs that are currently in effect.
extern TArray<VIEWPORTCONFIG> GViewports;


/*-----------------------------------------------------------------------------
	FEditorHitObserver.
-----------------------------------------------------------------------------*/

//
// Hit observer for editor events.
//
class UNREALED_API FEditorHitObserver : public FHitObserver
{
public:
	// FHitObserver interface.
	void Click( const FHitCause& Cause, const HHitProxy& Hit )
	{
		if     ( Hit.IsA(TEXT("HBspSurf"			)) ) Click( Cause, *(HBspSurf*)&Hit );
		else if( Hit.IsA(TEXT("HActor"				)) ) Click( Cause, *(HActor*)&Hit );
		else if( Hit.IsA(TEXT("HBrushVertex"		)) ) Click( Cause, *(HBrushVertex*)&Hit );
		else if( Hit.IsA(TEXT("HActorVertex"		)) ) Click( Cause, *(HActorVertex*)&Hit );
		else if( Hit.IsA(TEXT("HBezierControlPoint"	)) ) Click( Cause, *(HBezierControlPoint*)&Hit );
		else if( Hit.IsA(TEXT("HGlobalPivot"		)) ) Click( Cause, *(HGlobalPivot*)&Hit );
		else if( Hit.IsA(TEXT("HBrowserMaterial"	)) ) Click( Cause, *(HBrowserMaterial*)&Hit );
		else if( Hit.IsA(TEXT("HTerrain"			)) ) Click( Cause, *(HTerrain*)&Hit );
		else if( Hit.IsA(TEXT("HTerrainToolLayer"	)) ) Click( Cause, *(HTerrainToolLayer*)&Hit );
		else if( Hit.IsA(TEXT("HMatineeTimePath"	)) ) Click( Cause, *(HMatineeTimePath*)&Hit );
		else if( Hit.IsA(TEXT("HMatineeScene"		)) ) Click( Cause, *(HMatineeScene*)&Hit );
		else if( Hit.IsA(TEXT("HMatineeAction"		)) ) Click( Cause, *(HMatineeAction*)&Hit );
		else if( Hit.IsA(TEXT("HMatineeSubAction"	)) ) Click( Cause, *(HMatineeSubAction*)&Hit );
		else if( Hit.IsA(TEXT("HGizmoAxis"			)) ) Click( Cause, *(HGizmoAxis*)&Hit );
		else if( Hit.IsA(TEXT("HMaterialTree"		)) ) Click( Cause, *(HMaterialTree*)&Hit );
		else FHitObserver::Click( Cause, Hit );
	}

	// FEditorHitObserver interface.
	virtual void Click( const FHitCause& Cause, const struct HBspSurf&        Hit );
	virtual void Click( const FHitCause& Cause, const struct HActor&          Hit );
	virtual void Click( const FHitCause& Cause, const struct HBrushVertex&    Hit );
	virtual void Click( const FHitCause& Cause, const struct HActorVertex&    Hit );
	virtual void Click( const FHitCause& Cause, const struct HBezierControlPoint& Hit );
	virtual void Click( const FHitCause& Cause, const struct HGlobalPivot&    Hit );
	virtual void Click( const FHitCause& Cause, const struct HBrowserMaterial& Hit );
	virtual void Click( const FHitCause& Cause, const struct HTerrain&	      Hit );
	virtual void Click( const FHitCause& Cause, const struct HTerrainToolLayer& Hit );
	virtual void Click( const FHitCause& Cause, const struct HMatineeAction& Hit );
	virtual void Click( const FHitCause& Cause, const struct HMatineeTimePath& Hit );
	virtual void Click( const FHitCause& Cause, const struct HMatineeScene& Hit );
	virtual void Click( const FHitCause& Cause, const struct HMatineeSubAction& Hit );
	virtual void Click( const FHitCause& Cause, const struct HGizmoAxis&    Hit );
	virtual void Click( const FHitCause& Cause, const struct HMaterialTree&   Hit );
};


/*-----------------------------------------------------------------------------
	UnrealEdEngine.
-----------------------------------------------------------------------------*/

class UNREALED_API UUnrealEdEngine : public UEditorEngine, public FNotifyHook
{
	DECLARE_CLASS(UUnrealEdEngine,UEditorEngine,CLASS_Transient|CLASS_Config,UnrealEd)

	HWND hWndMain;

	// FNotify interface.
	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src );
	void NotifyPostChange( void* Src );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// UEngine Interface.
	void Draw( UViewport* Viewport, UBOOL Blit=1, BYTE* HitData=NULL, INT* HitSize=NULL );
	void MouseDelta( UViewport* Viewport, DWORD Buttons, FLOAT DX, FLOAT DY );
	void MousePosition( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y );
	void MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta );
	void Click( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y );
	void UnClick( UViewport* Viewport, DWORD Buttons, INT MouseX, INT MouseY );
	void Init();

	// Selection.
	virtual void SelectActor( ULevel* Level, AActor* Actor, UBOOL bSelect = 1, UBOOL bNotify = 1 );
	virtual void SelectBSPSurf( ULevel* Level, INT iSurf, UBOOL bSelect = 1, UBOOL bNotify = 1 );
	virtual void SelectNone( ULevel* Level, UBOOL Notify, UBOOL BSPSurfs = 1 );

	// Editor mode virtuals from UnEdCam.cpp.
	virtual void edcamSetMode( INT Mode );
	virtual int edcamMode( UViewport* Viewport );
	virtual int edcamTerrainBrush();
	virtual int edcamMouseControl( UViewport* InViewport );

	// General functions.
	virtual void UpdatePropertiesWindows();

	virtual void edSetClickLocation( FVector& InLocation );
	virtual void edDrawAxisIndicator( FSceneNode* SceneNode );
	virtual void Draw3DAxis(FSceneNode* SceneNode, FCoords& AxisCoords);

	virtual AActor* AddActor( ULevel* Level, UClass* Class, FVector V, UBOOL bSilent = 0 );
	virtual void NoteSelectionChange( ULevel* Level );
	virtual void NoteActorMovement( ULevel* Level );
	virtual void FinishAllSnaps( ULevel* Level );

	// UnrealEdSrv stuff.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	UBOOL Exec_Edit( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Actor( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Emitter( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Light( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Prefab( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Mode( const TCHAR* Str, FOutputDevice& Ar );
	UBOOL Exec_Script( const TCHAR* Str, FOutputDevice& Ar );
	int Key( UViewport* Viewport, EInputKey Key, TCHAR Unicode );

	virtual void SetPivot( FVector NewPivot, UBOOL SnapPivotToGrid, UBOOL MoveActors, UBOOL bIgnoreAxis );
	virtual FVector GetPivotLocation();
	virtual void ResetPivot();

	// Editor actor virtuals from UnEdAct.cpp.
	virtual void edactSelectAll( ULevel* Level );
	virtual void edactSelectInside( ULevel* Level );
	virtual void edactSelectInvert( ULevel* Level );
	virtual void edactSelectOfClass( ULevel* Level, UClass* Class );
	virtual void edactSelectSubclassOf( ULevel* Level, UClass* Class );
	virtual void edactSelectDeleted( ULevel* Level );
	virtual void edactSelectMatchingStaticMesh( ULevel* Level );
	virtual void edactSelectMatchingZone( ULevel* Level );
	virtual void edactDeleteSelected( ULevel* Level );
	virtual void edactCopySelected( ULevel* Level );
	virtual void edactPasteSelected( ULevel* Level, UBOOL Duplicate );
	virtual void edactReplaceSelectedBrush( ULevel* Level );
	virtual void edactReplaceSelectedWithClass( ULevel* Level, UClass* Class );
	virtual void edactReplaceClassWithClass( ULevel* Level, UClass* Class, UClass* WithClass );
	virtual void edactAlignVertices( ULevel* Level );
	virtual void edactHideSelected( ULevel* Level );
	virtual void edactHideUnselected( ULevel* Level );
	virtual void edactUnHideAll( ULevel* Level );
	virtual void edactApplyTransform( ULevel* Level );
	virtual void edactApplyTransformToBrush( ABrush* InBrush );
	virtual void edactBoxSelect( UViewport* Viewport, ULevel* Level, FVector Start, FVector End );

	// Far-plane Z clipping state control functions.
	virtual void SetZClipping();
	virtual void ResetZClipping();

	// Editor rendering functions.
	virtual void DrawGridSection( FSceneNode* SceneNode, INT ViewportLocX, INT ViewportSXR, INT ViewportGridY, FVector* A, FVector* B, FLOAT* AX, FLOAT* BX, INT AlphaCase );
	virtual void DrawWireBackground( FSceneNode* SceneNode );

	// Topics.
	virtual void EdCallback( DWORD Code, UBOOL Send, DWORD lParam );

	// Hook replacements.
	void ShowPreferences();
	void ShowActorProperties();
	void ShowLevelProperties();
	void PlayMap();
	void DisableRealtimeViewports();
	void ShowClassProperties( UClass* Class );
	void ShowSoundProperties( USound* Sound ); // gam
};

UNREALED_API extern class UUnrealEdEngine* GUnrealEd;

UNREALED_API extern HMODULE GUnrealEdModule;

#if __STATIC_LINK
#define NAMES_ONLY
#define NATIVE_DEFS_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "UnrealEdClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVE_DEFS_ONLY
#undef NAMES_ONLY
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

