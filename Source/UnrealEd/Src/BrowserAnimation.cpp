/*=============================================================================
	BrowserAnimation : Browser window for animations
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
		* Skeletal animation and mesh real-time import & property-editing - Erik
		* Massive rewrite - from .h to .cpp, GUI, camera, object-notifies and other improvements by Jack Porter.
		* Added refresh-on-import fixes and Maya coordinate rotation support by James Gwertzman.		
		* Added fix for temporary animation object import overwriting DefaultAnim by D.Isaac Gartner.
		* Added numerous refresh-related fixes by Bob Berry and D. Isaac Gartner - 6/13/02
		* Welding of all duplicate vertices on mesh import when WeldDuplicateVertices option set - 12/11/03

   Todo:  - misc windows/list refresh issues 
          - Split content-changed-refresh code out from window-size-changed refresh code.
        
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnrealEd.h"
//#include "UnRender.h"
#include "DlgRename.h"


/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#define SHOWFLAGS SHOW_StandardView | SHOW_ChildWindow | SHOW_Frame | SHOW_Actors | SHOW_StaticMeshes | SHOW_Particles
#define SCRUBBARRANGE (10000)

/*-----------------------------------------------------------------------------
	Buttons.
-----------------------------------------------------------------------------*/

//Scrub button sizes
#define SBTSIZE 16
TBBUTTON tbANIMATIONButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	, { 0, 0, TBSTATE_ENABLED,   TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_FileOpen, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_FileSave, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, {21, IDMN_AB_LOAD_ENTIRE_PACKAGE,	  TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	//, {10, IDAN_ANIMPLAY,      TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED,   TBSTYLE_SEP, 0L, 0}
	, { 6, IDMN_FILE_IMPORTMESH, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 5, IDMN_FILE_IMPORTANIM, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
    , {44, IDMN_FILE_IMPORTANIMMORE,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0} 
	, {16, IDMN_EDIT_LINKANIM,  TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED,   TBSTYLE_SEP, 0L, 0}
  //, {18, IDMN_REFRESH,       TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
  //, {23, IDMN_EDIT_UNDO,     TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}     
	, {27, IDMN_VIEW_INFO,     TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
  //, {20, IDMN_EDIT_APPLY,    TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
  //, { 4, IDMN_EDIT_MESHPROP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}   
  //, { 3, IDMN_EDIT_ANIMPROP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	, { 0, 0, TBSTATE_ENABLED,   TBSTYLE_SEP, 0L, 0}
	, { 9, IDMN_VIEW_BOUNDS,       TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 7, IDMN_VIEW_BONES,        TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0} 
	, {24, IDMN_VIEW_BONENAMES,    TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, {13, IDMN_VIEW_REFPOSE,      TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 8, IDMN_VIEW_INFLUENCES,   TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
    , {17, IDMN_VIEW_RAWOFFSET,    TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
  //, {11, IDMN_VIEW_BACKFACE,     TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, {12, IDMN_VIEW_WIRE,         TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED,   TBSTYLE_SEP, 0L, 0}
	, {30, IDMN_EDIT_COPYMESHPROPS, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, {31, IDMN_EDIT_PASTEMESHPROPS,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}		
	, { 45, IDMN_MESH_CYCLELOD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 46, IDMN_MESH_IMPORTLOD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
	, { 47, IDMN_MESH_REDIGESTLOD, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	  
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 48, IDMN_VIEW_LEVELANIM, TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}	

  //, {14, IDMN_EDIT_SEQUPROP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
  //, {19, IDMN_EDIT_NOTIFICATIONS, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
  //, {22, IDMN_EDIT_ADDNOTIFY,    TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
  //, {25, IDMN_EDIT_COPYNOTIFIES, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
  //, {26, IDMN_EDIT_PASTENOTIFIES,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
  //, {28, IDMN_EDIT_CLEARNOTIFIES,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	
	//, {32, IDMN_EDIT_GROUPS,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {34, IDMN_EDIT_COPYGROUPS, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {35, IDMN_EDIT_PASTEGROUPS,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {33, IDMN_EDIT_CLEARGROUPS,TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	
	//, {37, IDAN_ANIMPLAY,  TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {38, IDAN_ANIMRUN,   TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {39, IDAN_ANIMEND,   TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {40, IDAN_ANIMBEGIN, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {41, IDAN_ANIMBACK,  TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	//, {42, IDAN_ANIMLOOP,  TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}	
		
	//, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	
};

ToolTipStrings ToolTips_ANIMATION[] = 
{
	TEXT("Dock/undock from browser window"), IDMN_MB_DOCK,
	TEXT("Open animation package"), IDMN_FileOpen,
	TEXT("Save animation package"), IDMN_FileSave,
     TEXT("Load entire package"),        IDMN_AB_LOAD_ENTIRE_PACKAGE,
	
	TEXT("Info"),            IDMN_VIEW_INFO,
	TEXT("View bones"),      IDMN_VIEW_BONES,
	TEXT("View bounds"),     IDMN_VIEW_BOUNDS,
	TEXT("View influences"), IDMN_VIEW_INFLUENCES,
	TEXT("View wireframe"),  IDMN_VIEW_WIRE,

	TEXT("View reference pose"),  IDMN_VIEW_REFPOSE,
	TEXT("Forced synchronous in-level animation"),  IDMN_VIEW_LEVELANIM,
	TEXT("Toggle raw offset display"),   IDMN_VIEW_RAWOFFSET,
	TEXT("Link animation to mesh"),  IDMN_EDIT_LINKANIM,
	
    TEXT("Import skeletal mesh"),       IDMN_FILE_IMPORTMESH,
	TEXT("Import skeletal animation"),  IDMN_FILE_IMPORTANIM,
	TEXT("Import additional animation data"),  IDMN_FILE_IMPORTANIMMORE,	
	
	TEXT("View bone names"),            IDMN_VIEW_BONENAMES,	
	TEXT("Copy mesh properties"),   IDMN_EDIT_COPYMESHPROPS,
	TEXT("Paste mesh properties"),  IDMN_EDIT_PASTEMESHPROPS,
	TEXT("Cycle through the LOD mesh levels"), IDMN_MESH_CYCLELOD,
	TEXT("Import a single LOD mesh"), IDMN_MESH_IMPORTLOD,
	TEXT("Redigest LOD levels"), IDMN_MESH_REDIGESTLOD,
	NULL, 0
};


ToolTipStrings ToolTips_SCRUBBER[] =
{
	TEXT("Play/pause"), IDBM_AN_PLAY,
	TEXT("Goto start frame"), IDBM_AN_BEGIN, 
	TEXT("Goto last frame"), IDBM_AN_END,
	TEXT("advance one frame"), IDBM_AN_FORWARD,
	TEXT("back one frame"), IDBM_AN_BACKWARD,
	TEXT("toggle looping"), IDBM_AN_LOOP,
	NULL, 0
};


//
// Interface status helper objects.
//
// persistence needed for:
// - scrub bar time in each sequence;
// - scrub bar time for each mesh;
// - current mesh and mesh helper objects for each package.
//

class IStatMeshUIHelper
{
	DWORD  LastMeshSeq;
	TMap<FString,FLOAT> MapLastTimeForSeq;
	// FString: sequence name
	// FLOAT: Last sequence time.

	friend FArchive &operator<<( FArchive& Ar, IStatMeshUIHelper& I )
		{return Ar << I.LastMeshSeq << I.MapLastTimeForSeq;}
};

class IStatPackageUIHelper
{
	DWORD LastMeshSelected;
	TMap<FString,IStatMeshUIHelper> MapMeshUIHelpers;  
	// FString: package name.

	friend FArchive &operator<<( FArchive& Ar, IStatPackageUIHelper& I )
		{return Ar << I.LastMeshSelected << I.MapMeshUIHelpers;}			
};

class IStatAnimBrowserUIHelper
{
	TMap<FString,IStatPackageUIHelper> MapPackageUIHelpers;

	friend FArchive &operator<<( FArchive& Ar, IStatAnimBrowserUIHelper& I )
		{return Ar << I.MapPackageUIHelpers;}			
};


// --------------------------------------------------------------
//
// New mesh dialog..
//
// --------------------------------------------------------------

class WDlgNewMesh : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNewMesh,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WEdit PackageEdit;
	WEdit NameEdit;
	WComboBox ClassCombo;
	WCheckBox MayaCheck;
	WCheckBox MergeCheck;
	WCheckBox OverwriteCheck;

	

	FString defPackage, defGroup, defName;
	TArray<FString>* Filenames;

	FString Package, Group, Name;
	UMeshAnimation*  NewMeshAnim;

	UBOOL DoMergeAnims;
	UBOOL DoMayaCoords;
	UBOOL DoImportAnims;
	UBOOL DoOverwriteSeqs;
	UBOOL DoAppendExisting;

	// Constructor.
	WDlgNewMesh( UObject* InContext, WBrowser* InOwnerWindow, UBOOL ImportAnimFlag, UBOOL AnimAppendFlag )
		:	WDialog			( TEXT("New Mesh/Animation"), IDDIALOG_IMPORT_MESH, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	NameEdit		( this, IDEC_NAME )
	,   MayaCheck       ( this, IDCK_MAYACOORDS )
	,   MergeCheck      ( this, IDCK_SEQUENCEMERGE )
	,   OverwriteCheck  ( this, IDCK_SEQUENCEOVERWRITE )
	,   DoImportAnims   ( ImportAnimFlag )
	,   DoMergeAnims    ( AnimAppendFlag )
	,   DoAppendExisting( AnimAppendFlag )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgNewMesh::OnInitDialog);
		WDialog::OnInitDialog();

		NameEdit.SetText( *defName );
		PackageEdit.SetText( *defPackage );
		::SetFocus( NameEdit.hWnd );

		NewMeshAnim = NULL;

		if( !DoImportAnims )
		{
			EnableWindow( MergeCheck.hWnd, 0 );
			EnableWindow( OverwriteCheck.hWnd, 0 );
			MayaCheck.SetCheck(0);
		}
		else
		{
			EnableWindow( MayaCheck.hWnd, 0 );
			if( DoMergeAnims )
				MergeCheck.SetCheck(1);
		}		

		FString Classes;

		TArray<FString> Array;
		Classes.ParseIntoArray( TEXT(","), &Array );

		for( INT x = 0 ; x < Array.Num() ; ++x )
			ClassCombo.AddString( *(Array(x)) );		
		ClassCombo.SetCurrent(0);

		PackageEdit.SetText( *defPackage );

		Array.Empty();

		unguard;
	}
	virtual INT DoModal( FString InDefPackage, FString InDefGroup, FString InDefName )
	{
		guard(WDlgNewMesh::DoModal);

		defPackage = InDefPackage;
		defGroup = InDefGroup;
		defName = InDefName;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{		
		guard(WDlgNewMesh::OnOk);
		if( GetDataFromUser() )
		{			
			EndDialog(TRUE);
		}
		unguard;
	}

	BOOL GetDataFromUser( void )
	{
		guard(WDlgNewMesh::GetDataFromUser);

		Package = PackageEdit.GetText();
		//Group = GroupEdit.GetText();
		Name = NameEdit.GetText();

		DoMergeAnims = MergeCheck.IsChecked();
		DoMayaCoords = MayaCheck.IsChecked(); 
		DoOverwriteSeqs = OverwriteCheck.IsChecked();

		if( !Package.Len() || !Name.Len() )
		{
			appMsgf( 0, TEXT("Invalid input.") );
			return FALSE;
		}

		return TRUE;
		unguard;
	}
};


//
// Info dialog window.
//
class WDlgShowInfo : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgShowInfo,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;

	// Labels to print information into.
	WLabel MeshName;
	WLabel MeshSize;
	WLabel LODRenderSize;
	WLabel LODRawSize;
	WLabel AnimName;
	WLabel AnimSize;
	WLabel SequenceSize;
	WLabel TotalMeshSize;
	WLabel RenderDataSize;
	WLabel SequenceName;	

	WLabel InfluenceCount;
	WLabel VertCount;
	WLabel WedgeCount;
	WLabel FaceCount;

	WLabel MeshJointCount;
	WLabel AnimJointCount;
	WLabel RigidCount;
	WLabel SmoothCount;

	FName CurrentSeqName;


	USkeletalMeshInstance* CurrentInstance;
	UMeshAnimation* CurrentAnim;

	// Constructor.
	WDlgShowInfo( UObject* InContext, WBrowser* InOwnerWindow, USkeletalMeshInstance* MInst, UMeshAnimation* Anim, FName SeqName )
		:	WDialog			( TEXT("Mesh/Animation size info"), IDDIALOG_ANIM_INFO, InOwnerWindow )		
	,	OkButton	   ( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	MeshName	   ( this, IDC_MESHNAME)
	,	AnimName       ( this, IDC_CURANIMNAME)
	,   LODRenderSize  ( this, IDC_CURRENTLODRENDSIZE)
	,   LODRawSize     ( this, IDC_CURRENTLODRAWSIZE)
	,   AnimSize       ( this, IDC_CURANIMSIZE )
	,   TotalMeshSize  ( this, IDC_TOTALDATASIZE )
	,   RenderDataSize ( this, IDC_RENDERDATASIZE )
	,   SequenceName   ( this, IDC_CURSEQNAME )
	,   SequenceSize   ( this, IDC_CURSEQSIZE )
	,   InfluenceCount ( this, IDC_INFLUENCECOUNT )
	,   VertCount      ( this, IDC_VERTCOUNT )
	,   WedgeCount     ( this, IDC_WEDGECOUNT )
	,   FaceCount      ( this, IDC_FACECOUNT )
	,   MeshJointCount ( this, IDC_JOINTSM )
	,   AnimJointCount ( this, IDC_JOINTSA )
	,   SmoothCount    ( this, IDC_SECTIONSS )
	,   RigidCount     ( this, IDC_SECTIONSR )
	,   CurrentSeqName ( SeqName )
	{
		CurrentInstance = MInst;
		CurrentAnim = Anim;
	}

	//
	// WDialog interface.
	//
	void OnInitDialog()
	{
		guard(WDlgShowInfo::OnInitDialog);
		WDialog::OnInitDialog();

		USkeletalMesh* Mesh = (USkeletalMesh*)CurrentInstance->GetMesh();

		FString SizeReport;			
		FString NumReport;

		if( CurrentInstance && Mesh && ( CurrentInstance->CurrentLODLevel < Mesh->LODModels.Num() ))
		{
			MeshName.SetText( CurrentInstance->GetMesh()->GetName() );

			INT MeshMemSize = ((ULodMesh*)CurrentInstance->GetMesh())->MemFootprint(1);
			
			SizeReport = FString::Printf( TEXT("%i"),MeshMemSize );
			RenderDataSize.SetText( *SizeReport );

			INT MeshTotalSize = ((ULodMesh*)CurrentInstance->GetMesh())->MemFootprint(0);
			SizeReport = FString::Printf( TEXT("%i"),MeshTotalSize );
			TotalMeshSize.SetText( *SizeReport );

			// Report current LOD's size/raw size
			INT LODRenderBytes = Mesh->LODFootprint( CurrentInstance->CurrentLODLevel, 1 );
			SizeReport = FString::Printf( TEXT("%i"),LODRenderBytes);
			LODRenderSize.SetText( *SizeReport );

			INT LODRawBytes = Mesh->LODFootprint( CurrentInstance->CurrentLODLevel, 0 );
			SizeReport = FString::Printf( TEXT("%i"),LODRawBytes);
			LODRawSize.SetText( *SizeReport );			

			Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Influences.Load();
			INT AllInfluences = Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Influences.Num();
			NumReport = FString::Printf( TEXT("%i"),AllInfluences);
			InfluenceCount.SetText( *NumReport );

			Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Points.Load();
			INT AllVerts = Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Points.Num();
			NumReport = FString::Printf( TEXT("%i"),AllVerts);
			VertCount.SetText( *NumReport );

			Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Wedges.Load();
			INT AllWedges = Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Wedges.Num();
			NumReport = FString::Printf( TEXT("%i"),AllWedges);
			WedgeCount.SetText( *NumReport );

			Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Faces.Load();
			INT AllFaces = Mesh->LODModels( CurrentInstance->CurrentLODLevel ).Faces.Num();
			NumReport = FString::Printf( TEXT("%i"),AllFaces);
			FaceCount.SetText( *NumReport );

			//Joint counts and static vs smooth parts..
			INT AllSections = Mesh->LODModels( CurrentInstance->CurrentLODLevel ).SmoothSections.Num()
			                + Mesh->LODModels( CurrentInstance->CurrentLODLevel ).RigidSections.Num();
			NumReport = FString::Printf( TEXT("%i"),AllSections);
			SmoothCount.SetText( *NumReport );

			INT RigidSections = Mesh->LODModels( CurrentInstance->CurrentLODLevel ).RigidSections.Num();
			NumReport = FString::Printf( TEXT("%i"),RigidSections);
			RigidCount.SetText( *NumReport );

			INT MeshJoints = Mesh->RefSkeleton.Num();
			NumReport = FString::Printf( TEXT("%i"),MeshJoints);
			MeshJointCount.SetText( *NumReport );

			
		}
		else
		{
			MeshName.SetText( TEXT("NONE"));
		}

		if ( CurrentAnim )
		{			
			AnimName.SetText( CurrentAnim->GetName() );
			INT AnimMemSize = CurrentAnim->MemFootprint();
			
			SizeReport = FString::Printf( TEXT("%i"),AnimMemSize );
			AnimSize.SetText( *SizeReport );

			INT AnimJoints = CurrentAnim->RefBones.Num();
			NumReport = FString::Printf( TEXT("%i"),AnimJoints);
			AnimJointCount.SetText( *NumReport );		

			// See if Sequence....
			if( CurrentSeqName != NAME_None )
			{ 
				INT SeqMemSize = CurrentAnim->SequenceMemFootprint( CurrentSeqName );
				SizeReport = FString::Printf( TEXT("%i"),SeqMemSize);
				SequenceSize.SetText( *SizeReport);
				SequenceName.SetText( *CurrentSeqName );
			}
			else
			{
				SequenceName.SetText( TEXT("NONE"));
			}
		}
		else
		{
			AnimName.SetText( TEXT("NONE"));
		}

		unguard;
	}
	virtual INT DoModal() 
	{
		guard(WDlgShowInfo::DoModal);
		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{		
		guard(WDlgShowInfo::OnOk);			
		WDialog::EndDialog(TRUE);
		unguard;
	}

};


/*-----------------------------------------------------------------------------
	WBrowserAnimation.
-----------------------------------------------------------------------------*/

//
// Compare - for notify sorting.
//
INT Compare( FNotifyInfo& A, FNotifyInfo& B )
{
	return A.NotifyFrame - B.NotifyFrame;
}


// Reset the in-level meshes to spring back to their origin/reference pose.
void WBrowserAnimation::ResetLevelMeshes()
{
	for( INT i=0; i<TempForcedActorList.Num(); i++)
	{
		AActor* SyncedActor = TempForcedActorList(i);
		if( (! SyncedActor->bDeleteMe ) && (SyncedActor->Mesh == CurrentSkelMesh()) )
		{
			USkeletalMeshInstance* MInst = (USkeletalMeshInstance*)SyncedActor->Mesh->MeshGetInstance(SyncedActor);
			if( MInst )
			{						
				MInst->SetAnimSequence( 0, NAME_None );
			}							
		}
	}						
}


// Serialize function: needed to associate the custom objects with THIS window so they don't get GC'd at the wrong time; when a level is loaded etc.
void WBrowserAnimation::Serialize( FArchive& Ar )
{
	guard(WBrowserAnimation::Serialize);
	WBrowser::Serialize( Ar );		
	Ar << EditMeshProps << EditAnimProps << EditSequProps << EditNotifyProperties << EditPrefsProps << AnimBrowserLevel;
	unguard;
}

// Structors.
WBrowserAnimation::WBrowserAnimation( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
{		
	guard( WBrowserAnimation::Constructor);

	MeshPropertyWindow = NULL;
	AnimPropertyWindow = NULL;
	SeqPropertyWindow = NULL;
	NotifyPropertyWindow = NULL;
	PrefsPropertyWindow = NULL;
	
	PackageComboLabel = NULL;
	AnimComboLabel = NULL;
	MeshComboLabel = NULL;

	CurrentMeshAnim = NULL;
	CurrentSequence = NAME_None;
	
	ApplySequenceSort = NULL;
	MoveSequenceUp = NULL;
	MoveSequenceDown = NULL;
	
	ComboPackage = NULL;
	MeshCombo = NULL;
	AnimCombo = NULL;
	AnimSeqList = NULL;

	Viewport = NULL;		
	ViewportLabel = NULL;
	ScrubBar = NULL;
	LeftListLabel=NULL;
	MenuID = IDMENU_BrowserAnimation;
	BrowserID = eBROWSER_ANIMATION;
	Description = TEXT("Animations");
	mrulist = NULL;

	DoWeldDuplicateVertices = 0;

	bLevelAnim = bPlaying = bWireframe = bRawOffset = bPrintBones = bBackface = bRefpose = bDrawCollision = bDrawKarma = FALSE;

	ScrubButtonPlay  = NULL;
	ScrubButtonBegin = NULL;
	ScrubButtonEnd   = NULL;
	ScrubButtonForward = NULL;
	ScrubButtonBackward = NULL;
	ScrubButtonLoop = NULL;		

	EditMeshProps = NULL;
	EditAnimProps = NULL;
	EditSequProps = NULL;
	EditNotifyProperties = NULL;
	EditPrefsProps = NULL;

	bFilledCopiedMesh = FALSE;

	SequenceSortMethod = 0;
	SequenceSortDirection = 0;



	ScrubPlayBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_PLAY), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubPlayBitmap);
	ScrubPauseBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_PAUSE), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubPauseBitmap);
	ScrubBeginBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_BEGIN), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubBeginBitmap);
	ScrubEndBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_END), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubEndBitmap);
	ScrubForwardBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_FORWARD), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubForwardBitmap);
	ScrubBackwardBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_BACKWARD), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubBackwardBitmap);
	ScrubLoopBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_LOOP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubLoopBitmap);
	ScrubNoLoopBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_AN_NOLOOP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	
	check(ScrubNoLoopBitmap);

	unguard;
}

// WBrowser interface.
void WBrowserAnimation::OpenWindow( UBOOL bChild )
{
	guard(WBrowserAnimation::OpenWindow);
	WBrowser::OpenWindow( bChild );
	SetCaption();
	Show(1);

	unguard;
}

void WBrowserAnimation::AddToolTips(WToolTip* ToolTipWindow, HWND& hWndBar, ToolTipStrings* TipArray )
{
	for( INT tooltip = 0 ; TipArray[tooltip].ID > 0 ; ++tooltip )
	{
		// Figure out the rectangle for the toolbar button.
		INT index = SendMessageX( hWndBar, TB_COMMANDTOINDEX, TipArray[tooltip].ID, 0 );
		RECT rect;
		SendMessageX( hWndBar, TB_GETITEMRECT, index, (LPARAM)&rect);
		if( rect.left != rect.right ) // any real area?
			ToolTipCtrl->AddTool( hWndBar, TipArray[tooltip].ToolTip, tooltip, &rect );
		else
			appMsgf(0,TEXT("Invalid tooltip area or ID"));
	}
}	

void WBrowserAnimation::OnCreate()
{
	guard(WBrowserAnimation::OnCreate);
	WBrowser::OnCreate();			
	SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserAnimation) );

	// Create the toolbar.
	hWndToolBar = CreateToolbarEx( 
		hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
		IDB_BrowserAnimation_TOOLBAR,
		3,
		hInstance,
		IDB_BrowserAnimation_TOOLBAR,
		(LPCTBBUTTON)&tbANIMATIONButtons,
		ARRAY_COUNT(tbANIMATIONButtons), // Total number of buttons and dividers in toolbar. 
		16,16,
		16,16,
		sizeof(TBBUTTON));
	check(hWndToolBar);

	ToolTipCtrl = new WToolTip(this);
	ToolTipCtrl->OpenWindow();

	AddToolTips( ToolTipCtrl, hWndToolBar, ToolTips_ANIMATION );
	// AddToolTips( ToolTipCtrl, hWnd, ToolTips_SCRUBBER ); 

	// Labels for the combo boxes.
	//
	PackageComboLabel = new WLabel( this, IDAL_LABEL3 );	
	PackageComboLabel->OpenWindow( 1, 0 ); 	
	PackageComboLabel->SetText(TEXT("Package") );

	AnimComboLabel = new WLabel( this, IDAL_LABEL4 );	
	AnimComboLabel->OpenWindow( 1, 0 ); 
	AnimComboLabel->SetText(TEXT("Animation Set"));

	MeshComboLabel = new WLabel( this, IDAL_LABEL5 );	
	MeshComboLabel->OpenWindow( 1, 0 ); 
	MeshComboLabel->SetText( TEXT("Mesh"));


	// Package selection
	//
	ComboPackage = new WComboBox( this, IDCB_PACKAGE );
	ComboPackage->OpenWindow( 1, 1 );
	ComboPackage->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnPackageSelectionChange);
	
	// Mesh selection
	//
	MeshCombo = new WComboBox( this, IDCB_MESH );
	MeshCombo->OpenWindow( 1, 0 );  // UNsorted listbox.
	MeshCombo->SelectionChangeDelegate    = FDelegate(this,(TDelegate)OnMeshSelectionChange );
	
	// Animation combo
	//
	AnimCombo = new WComboBox( this, IDCB_ANIM );
	AnimCombo->OpenWindow( 1, 0); // 2nd parameter = false: unsorted.
	AnimCombo->SelectionChangeDelegate    = FDelegate(this,(TDelegate)OnAnimObjectSelectionChange);

	// Splitter
	//
	SplitterContainer = new WSplitterContainer( this );
	SplitterContainer->OpenWindow( 1 );
	SplitterContainer->ParentContainer = &Container;
	SplitterContainer->SetPct(70);

	// Viewport container
	//
	ViewportLabel = new WLabel( SplitterContainer->Pane1, IDSC_VIEWPORT );
	ViewportLabel->OpenWindow( 1, 0 );

	// Play buttons.
	ScrubButtonPlay = new WButton( SplitterContainer->Pane1, IDBT_SCRUBPLAY, FDelegate(this,(TDelegate)OnScrubPlay) );
	ScrubButtonPlay->OpenWindow( 1, 0, 0, SBTSIZE, SBTSIZE, NULL, 0, BS_OWNERDRAW );
	SetClassLongX( *ScrubButtonPlay, GCL_STYLE, GetClassLongX(*ScrubButtonPlay,GCL_STYLE) & ~CS_DBLCLKS);
	ScrubButtonBegin = new WButton( SplitterContainer->Pane1, IDBT_SCRUBBEGIN, FDelegate(this,(TDelegate)OnScrubBegin) );
	ScrubButtonBegin->OpenWindow( 1, 0, 0, SBTSIZE, SBTSIZE, NULL, 0, BS_OWNERDRAW );
	ScrubButtonEnd = new WButton( SplitterContainer->Pane1, IDBT_SCRUBEND, FDelegate(this,(TDelegate)OnScrubEnd) );
	ScrubButtonEnd->OpenWindow( 1, 0, 0, SBTSIZE, SBTSIZE, NULL, 0, BS_OWNERDRAW );

	ScrubButtonForward = new WButton( SplitterContainer->Pane1, IDBT_SCRUBEND, FDelegate(this,(TDelegate)OnScrubForward) );
	ScrubButtonForward->OpenWindow( 1, 0, 0, SBTSIZE, SBTSIZE, NULL, 0, BS_OWNERDRAW );
	SetClassLongX( *ScrubButtonForward, GCL_STYLE, GetClassLongX(*ScrubButtonForward,GCL_STYLE) & ~CS_DBLCLKS);
	ScrubButtonBackward = new WButton( SplitterContainer->Pane1, IDBT_SCRUBEND, FDelegate(this,(TDelegate)OnScrubBackward) );
	ScrubButtonBackward->OpenWindow( 1, 0, 0, SBTSIZE, SBTSIZE, NULL, 0, BS_OWNERDRAW );
	SetClassLongX( *ScrubButtonBackward, GCL_STYLE, GetClassLongX(*ScrubButtonBackward,GCL_STYLE) & ~CS_DBLCLKS);
	ScrubButtonLoop = new WButton( SplitterContainer->Pane1, IDBT_SCRUBEND, FDelegate(this,(TDelegate)OnScrubLoop) );
	ScrubButtonLoop->OpenWindow( 1, 0, 0, SBTSIZE, SBTSIZE, NULL, 0, BS_OWNERDRAW );
	SetClassLongX( *ScrubButtonLoop, GCL_STYLE, GetClassLongX(*ScrubButtonLoop,GCL_STYLE) & ~CS_DBLCLKS);
			
	ScrubButtonPlay->SetBitmap( ScrubPlayBitmap );		
	ScrubButtonBegin->SetBitmap( ScrubBeginBitmap );
	ScrubButtonEnd->SetBitmap( ScrubEndBitmap );

	ScrubButtonForward->SetBitmap( ScrubForwardBitmap );		
	ScrubButtonBackward->SetBitmap( ScrubBackwardBitmap );
	ScrubButtonLoop->SetBitmap( ScrubNoLoopBitmap );
	bDoScrubLoop = 0;

	// Scrub bar
	ScrubBar = new WTrackBar( SplitterContainer->Pane1, IDTB_SCRUBFRAMES );
	ScrubBar->ManualTicks = 1;
	ScrubBar->OpenWindow( 1, 0 );
	ScrubBar->SetRange( 0, SCRUBBARRANGE );
	ScrubBar->ThumbPositionDelegate = FDelegate(this, (TDelegate)OnSliderMove ); 
	ScrubBar->ThumbTrackDelegate = FDelegate(this, (TDelegate)OnSliderMove );    

	// Create the mesh viewport
	AnimBrowserLevel = new( UObject::GetTransientPackage(), TEXT("AnimBrowserLevel") )ULevel( GUnrealEd, 0 );

	Viewport = GUnrealEd->Client->NewViewport( TEXT("AnimationViewer") );
	check(Viewport);
	InitViewActor();
	Viewport->Input->Init( Viewport );
	Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 256, 256, 0, 0 );	


	INT SeqWinWidth = 182; 

	// Create the Animation Sequence List
	AnimSeqList = new WListBox( this, IDLB_ANIMATIONS );
	AnimSeqList->OpenWindow( 1, 0, 0, 0, 0, WS_VSCROLL );
	AnimSeqList->SelectionChangeDelegate  = FDelegate(this,(TDelegate)OnAnimSequenceSelectionChange);
	AnimSeqList->DoubleClickDelegate  = FDelegate(this,(TDelegate)OnAnimSequenceDoubleClick);		
	AnimSeqList->RightClickDelegate  = FDelegate(this,(TDelegate)OnAnimSequenceRightClick);		
	//AnimSeqList->Root.Sorted = 0;
	SendMessageX( AnimSeqList->hWnd, LB_SETCOLUMNWIDTH, 96, 0 );

	// Animation sequence list label
	LeftListLabel = new WLabel( this, IDAL_LABEL1 );
	LeftListLabel->OpenWindow( 1, 0 );
	LeftListLabel->SetText( TEXT(" Sequences ") );


	// Animation sequence sort buttons.	
	ApplySequenceSort = new WCheckBox( this, IDBT_DOSEQSORT1, FDelegate(this,(TDelegate)OnApplySequenceSort) );
	ApplySequenceSort->OpenWindow( 1, 0, 0, 1, 1, TEXT("sort"),1, 0, BS_PUSHLIKE ); 
	MoveSequenceUp = new WCheckBox( this, IDBT_DOSEQSORT2, FDelegate(this,(TDelegate)OnMoveSequenceUp) );
	MoveSequenceUp->OpenWindow( 1, 0, 0, 1, 1, TEXT("up"), 1, 0, BS_PUSHLIKE );	
	MoveSequenceDown = new WCheckBox( this, IDBT_DOSEQSORT3, FDelegate(this,(TDelegate)OnMoveSequenceDown) );
	MoveSequenceDown->OpenWindow( 1, 0, 0, 1, 1, TEXT("down"), 1, 0, BS_PUSHLIKE );	
	
	// Property editors
	if( !EditMeshProps) EditMeshProps = ConstructObject<UMeshEditProps>( UMeshEditProps::StaticClass() );
	if( !EditAnimProps) EditAnimProps = ConstructObject<UAnimEditProps>( UAnimEditProps::StaticClass() );
	if( !EditSequProps) EditSequProps = ConstructObject<USequEditProps>( USequEditProps::StaticClass() ) ;
	if( !EditPrefsProps) EditPrefsProps = ConstructObject<USkelPrefsEditProps>( USkelPrefsEditProps::StaticClass() );
	// Note: Notify list constructed/refreshed in separate function.

	// Tab control
	PropSheet = new WPropertySheet( SplitterContainer->Pane2, 0 );
	PropSheet->bMultiLine = 1;
	PropSheet->bResizable = 1;
	PropSheet->OpenWindow( 1, 1, 0 );
	PropSheet->ParentContainer = &Container;

	MeshPage = new WPropertyPage( PropSheet->Tabs );
	MeshPage->OpenWindow( 0, 0 );
	MeshPage->Caption = TEXT("Mesh");
	PropSheet->AddPage( MeshPage );

	AnimPage = new WPropertyPage( PropSheet->Tabs );
	AnimPage->OpenWindow( 0, 0 );
	AnimPage->Caption = TEXT("Animation Set");
	PropSheet->AddPage( AnimPage );

	SeqPage = new WPropertyPage( PropSheet->Tabs );
	SeqPage->OpenWindow( 0, 0 );
	SeqPage->Caption = TEXT("Sequence");
	PropSheet->AddPage( SeqPage );

	NotifyPage = new WPropertyPage( PropSheet->Tabs );
	NotifyPage->OpenWindow( 0, 0 );
	NotifyPage->Caption = TEXT("Notify");
	PropSheet->AddPage( NotifyPage );

	PrefsPage = new WPropertyPage( PropSheet->Tabs );
	PrefsPage->OpenWindow( 0, 0 );
	PrefsPage->Caption = TEXT("Prefs");
	PropSheet->AddPage( PrefsPage );

	MeshPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), MeshPage, 1 );
	MeshPropertyWindow->ShowTreeLines = 0;
	MeshPropertyWindow->SetNotifyHook( GUnrealEd );
	MeshPropertyWindow->OpenChildWindow(0);
	
	AnimPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), AnimPage, 1 );
	AnimPropertyWindow->ShowTreeLines = 0;
	AnimPropertyWindow->SetNotifyHook( GUnrealEd );
	AnimPropertyWindow->OpenChildWindow(0);

	SeqPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), SeqPage, 1 );
	SeqPropertyWindow->ShowTreeLines = 0;
	SeqPropertyWindow->SetNotifyHook( GUnrealEd );
	SeqPropertyWindow->OpenChildWindow(0);

	NotifyPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), NotifyPage, 1 );
	NotifyPropertyWindow->ShowTreeLines = 0;
	NotifyPropertyWindow->SetNotifyHook( GUnrealEd );
	NotifyPropertyWindow->OpenChildWindow(0);

	PrefsPropertyWindow = new WObjectProperties( TEXT(""), CPF_Edit, TEXT(""), PrefsPage, 1 );
	PrefsPropertyWindow->ShowTreeLines = 0;
	PrefsPropertyWindow->SetNotifyHook( GUnrealEd );
	PrefsPropertyWindow->OpenChildWindow(0);

	// Collision test-cylinder.
	TempCollisionRadius = 0.f;
	TempCollisionHeight = 0.f;

	INT Top = 30;
	Anchors.Set( (DWORD)PackageComboLabel->hWnd,FWindowAnchor( hWnd, PackageComboLabel->hWnd,	ANCHOR_TL, 0+5, Top+5,					ANCHOR_LEFT|ANCHOR_HEIGHT, 0+55, STANDARD_CTRL_HEIGHT-8 ) );
	Anchors.Set( (DWORD)ComboPackage->hWnd,		FWindowAnchor( hWnd, ComboPackage->hWnd,		ANCHOR_TL, 0+55,   Top,					ANCHOR_LEFT|ANCHOR_HEIGHT, 55+225, STANDARD_CTRL_HEIGHT ) );
	Top += STANDARD_CTRL_HEIGHT+1;

	Anchors.Set( (DWORD)MeshComboLabel->hWnd,	FWindowAnchor( hWnd, MeshComboLabel->hWnd,		ANCHOR_TL, 0+5, Top+5,					ANCHOR_LEFT|ANCHOR_HEIGHT, 0+55, STANDARD_CTRL_HEIGHT-8 ) );
	Anchors.Set( (DWORD)MeshCombo->hWnd,		FWindowAnchor( hWnd, MeshCombo->hWnd,			ANCHOR_TL, 0+55, Top,					ANCHOR_LEFT|ANCHOR_HEIGHT, 0+55 + 200, STANDARD_CTRL_HEIGHT ) );	

	Anchors.Set( (DWORD)AnimComboLabel->hWnd,	FWindowAnchor( hWnd, AnimComboLabel->hWnd,		ANCHOR_TL, 255+15,    Top+5,			ANCHOR_LEFT|ANCHOR_HEIGHT, 255+15+69, STANDARD_CTRL_HEIGHT-8 ) ); 
	Anchors.Set( (DWORD)AnimCombo->hWnd,		FWindowAnchor( hWnd, AnimCombo->hWnd,			ANCHOR_TL, 255+15+69+8, Top,			ANCHOR_LEFT|ANCHOR_HEIGHT, 255+15+69+8 + 200, STANDARD_CTRL_HEIGHT ) );	//
	Top += STANDARD_CTRL_HEIGHT+2;

	Anchors.Set( (DWORD)LeftListLabel->hWnd,	FWindowAnchor( hWnd, LeftListLabel->hWnd,		ANCHOR_TL, 0, Top+5,					ANCHOR_LEFT|ANCHOR_HEIGHT, SeqWinWidth, 15 ) );
	Anchors.Set( (DWORD)AnimSeqList->hWnd,		FWindowAnchor( hWnd, AnimSeqList->hWnd,			ANCHOR_TL, 0, Top+STANDARD_CTRL_HEIGHT,	ANCHOR_LEFT|ANCHOR_BOTTOM, SeqWinWidth, 0 ) );

	Anchors.Set( (DWORD)ApplySequenceSort->hWnd,FWindowAnchor( hWnd, ApplySequenceSort->hWnd,	ANCHOR_TL, SeqWinWidth-38-36*2, Top+3,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 34, 16 ) );
	Anchors.Set( (DWORD)MoveSequenceUp->hWnd,   FWindowAnchor( hWnd,MoveSequenceUp->hWnd,	ANCHOR_TL, SeqWinWidth-36-33, Top+4,	    ANCHOR_WIDTH|ANCHOR_HEIGHT, 31, 15  ) ); 
	Anchors.Set( (DWORD)MoveSequenceDown->hWnd, FWindowAnchor( hWnd,MoveSequenceDown->hWnd,	ANCHOR_TL, SeqWinWidth-36,    Top+4,	    ANCHOR_WIDTH|ANCHOR_HEIGHT, 31, 15  ) ); 
	
	Anchors.Set( (DWORD)ViewportLabel->hWnd,	FWindowAnchor( SplitterContainer->Pane1->hWnd, ViewportLabel->hWnd, ANCHOR_TL, 0, 0,				ANCHOR_BR, 0, -32 ) );
	Anchors.Set( (DWORD)Viewport->GetWindow(),	FWindowAnchor( ViewportLabel->hWnd, (HWND)Viewport->GetWindow(), ANCHOR_TL, 0, 0,					ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)ScrubBar->hWnd,			FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubBar->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 48, -32,	ANCHOR_BR, 0, 0 ) );

	Anchors.Set( (DWORD)ScrubButtonBackward->hWnd,FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubButtonBackward->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 0, -32,	ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16 ) );
	Anchors.Set( (DWORD)ScrubButtonPlay->hWnd,	FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubButtonPlay->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 16, -32,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16 ) );
	Anchors.Set( (DWORD)ScrubButtonForward->hWnd,FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubButtonForward->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 32, -32,	ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16 ) );
	Anchors.Set( (DWORD)ScrubButtonBegin->hWnd,	FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubButtonBegin->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 0, -16,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16 ) );
	Anchors.Set( (DWORD)ScrubButtonEnd->hWnd,	FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubButtonEnd->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 16, -16,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16 ) );
	Anchors.Set( (DWORD)ScrubButtonLoop->hWnd,	FWindowAnchor( SplitterContainer->Pane1->hWnd, ScrubButtonLoop->hWnd, ANCHOR_LEFT|ANCHOR_BOTTOM, 32, -16,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 16, 16 ) );

	Anchors.Set( (DWORD)PropSheet->hWnd,		FWindowAnchor( SplitterContainer->Pane2->hWnd, PropSheet->hWnd, ANCHOR_TL, 0, 0,					ANCHOR_BR, 0, 0 ) );

	Anchors.Set( (DWORD)MeshPropertyWindow->hWnd,FWindowAnchor( MeshPage->hWnd, MeshPropertyWindow->hWnd, ANCHOR_TL, 0, 0,							ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)AnimPropertyWindow->hWnd,FWindowAnchor( AnimPage->hWnd, AnimPropertyWindow->hWnd, ANCHOR_TL, 0, 0,							ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)SeqPropertyWindow->hWnd,FWindowAnchor( SeqPage->hWnd, SeqPropertyWindow->hWnd, ANCHOR_TL, 0, 0,								ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)NotifyPropertyWindow->hWnd,FWindowAnchor( NotifyPage->hWnd, NotifyPropertyWindow->hWnd, ANCHOR_TL, 0, 0,					ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)PrefsPropertyWindow->hWnd,FWindowAnchor( PrefsPage->hWnd, PrefsPropertyWindow->hWnd, ANCHOR_TL, 0, 0,					ANCHOR_BR, 0, 0 ) );
	Anchors.Set( (DWORD)SplitterContainer->hWnd,FWindowAnchor( hWnd, SplitterContainer->hWnd,	ANCHOR_TL, SeqWinWidth+4, Top,								ANCHOR_BR, 0, 0 ) ); 
	
	Container.SetAnchors( &Anchors );
	PositionChildControls();

	mrulist = new MRUList( *PersistentName );
	mrulist->ReadINI();	
	if( GBrowserMaster->GetCurrent()==BrowserID )
		mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
	
	SetCaption();

    WorkMesh = Cast<USkeletalMesh>(UObject::StaticFindObject(USkeletalMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));

	RefreshAll();
	OnPackageSelectionChange(); 

	unguard;
}

void WBrowserAnimation::InitViewActor()
{
	guard(WBrowserAnimation::InitViewActor);
	AnimBrowserLevel->SpawnViewActor( Viewport );
	check(Viewport->Actor);
	Viewport->Actor->XLevel = AnimBrowserLevel;
	Viewport->Actor->ShowFlags = SHOWFLAGS;
	Viewport->Actor->RendMap   = REN_Animation;
	Viewport->Group = NAME_None;
	Viewport->Actor->Misc1 = 0;
	Viewport->MiscRes = NULL;
	WorkMesh = NULL;
	MeshActor = AnimBrowserLevel->SpawnActor( AAnimBrowserMesh::StaticClass() );
	unguard;
}

void WBrowserAnimation::SetCaption( void )
{
	guard(WBrowserAnimation::SetCaption);

	FString Caption = TEXT("Animation Tool");

	if( GetCurrentMeshName().Len() )
		Caption += FString::Printf( TEXT(" - %s "),
			GetCurrentMeshName() );

	if( GetCurrentAnimName().Len() )
		Caption += FString::Printf( TEXT(" - %s "),
			GetCurrentAnimName() );

	SetText( *Caption );
	unguard;
}

void WBrowserAnimation::RefreshPackages( void )
{
	guard(WBrowserMesh::RefreshPackages);

	INT Current = ComboPackage->GetCurrent();
	Current = (Current != CB_ERR) ? Current : 0;

	// PACKAGES
	//
	ComboPackage->Empty();

	FStringOutputDevice GetPropResult = FStringOutputDevice();
	GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Mesh"), GetPropResult );

	TArray<FString> StringArray;
	GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

	for( INT x = 0 ; x < StringArray.Num() ; ++x )
		ComboPackage->AddString( *(StringArray(x)) );

	ComboPackage->SetCurrent( Current );

	StringArray.Empty();

	unguard;
}


void WBrowserAnimation::RefreshAll()
{
	guard(WBrowserAnimation::RefreshAll);

	RefreshPrefsEditProperties();
	RefreshPackages();			
	RefreshMeshList();		

	RefreshAnimObjList();				
	RefreshAnimSeqList();
	InitMeshDrawSettings();
	RefreshViewport();

	if( mrulist && (GBrowserMaster->GetCurrent()==BrowserID) )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

	unguard;
}

FString WBrowserAnimation::GetCurrentMeshName()
{
	guard(WBrowserAnimation::GetCurrentMeshName);
	return MeshCombo->GetString( MeshCombo->GetCurrent() );
	unguard;
}

FString WBrowserAnimation::GetCurrentAnimName()
{
	guard(WBrowserAnimation::GetCurrentAnimName);
	return AnimCombo->GetString( AnimCombo->GetCurrent() );
	unguard;
}

void WBrowserAnimation::RefreshMeshList()
{
	guard(WBrowserAnimation::RefreshMeshList);
	FStringOutputDevice GetPropResult = FStringOutputDevice();

	USkeletalMesh* SelectedMesh = CurrentSkelMesh();	

	// Discard same-mesh actor pointers because this may be a level change or mesh deletion.
	TempForcedActorList.Empty();
	// Snap out of forced-anim mode...
	bLevelAnim = false;
	INT Toggle = 0;
	SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_VIEW_LEVELANIM, (LPARAM) MAKELONG(Toggle, 0));


	// Only get skeletal meshes from the selected package..
	FString Package = ComboPackage->GetString( ComboPackage->GetCurrent() );
	if( Package.Len() < 1) return; 
	TCHAR l_ch[512];		
	appSprintf( l_ch, TEXT("QUERY TYPE=SkeletalMesh PACKAGE=\"%s\""), *Package );
	GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

	MeshCombo->Empty();
	TArray<FString> StringArray;
	GetPropResult.ParseIntoArray( TEXT(" "), &StringArray );
	for( INT x = 0 ; x < StringArray.Num() ; ++x )
		MeshCombo->AddString( *(StringArray(x)) );
	
	// Attempt to make old current mesh the current one in the refreshed list.
	INT  MeshIndex = 0;
	if( SelectedMesh ) 
	{		
		FString DefMeshName = SelectedMesh->GetName();
				
		// Find mesh in the list.
		for(INT x = 0 ; x < StringArray.Num(); x++ )
		{
			if( StringArray(x) == DefMeshName )
				MeshIndex = x;
		}		
	}		 		
	StringArray.Empty();		
	MeshCombo->SetCurrent(MeshIndex);		
			
	unguard;
}

void WBrowserAnimation::RefreshAnimObjList()
{
	guard(WBrowserAnimation::RefreshAnimObjList);
	FStringOutputDevice GetPropResult = FStringOutputDevice();

	// Only get animations from the selected package..
	FString Package = ComboPackage->GetString( ComboPackage->GetCurrent() );
	if( Package.Len() < 1) return; 
	TCHAR l_ch[512];		
	appSprintf( l_ch, TEXT("QUERY TYPE=MeshAnimation PACKAGE=\"%s\""), *Package );
	GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

	AnimCombo->Empty();
	TArray<FString> StringArray;
	GetPropResult.ParseIntoArray( TEXT(" "), &StringArray );

	for( INT x = 0 ; x < StringArray.Num(); x++ )
	{
		AnimCombo->AddString( *(StringArray(x)) );
	}
	//AnimCombo->SetCurrent(0);
	UBOOL FoundAnim = false;
	FString DefAnimName;

	// Attempt to make current mesh's DefaultAnim the current one in the list.
	CurrentMeshAnim = NULL;
	USkeletalMesh* SkelMesh = CurrentSkelMesh();
	if( SkelMesh && SkelMesh->DefaultAnim )
		CurrentMeshAnim = SkelMesh->DefaultAnim;

	if( CurrentMeshAnim ) 
	{
		DefAnimName = CurrentMeshAnim->GetName();
		// Find CurrentMeshAnim in the list.
		for(INT x = 0 ; x < StringArray.Num(); x++ )
		{
			if( StringArray(x) == DefAnimName )
			{
				FoundAnim = true;
				AnimCombo->SetCurrent(x); 					
				break;
			}
		}
	}		 
	
	// Not found: set current anim object to be the 0th element...
	if( !FoundAnim )
	{
		AnimCombo->SetCurrent(0);
		CurrentMeshAnim = Cast<UMeshAnimation>(UObject::StaticFindObject(UMeshAnimation::StaticClass(), ANY_PACKAGE,*( AnimCombo->GetString(AnimCombo->GetCurrent()))));
	}

	StringArray.Empty();		

	// update the animation set properties to display CurrentMeshAnim.
	RefreshAnimProperties();

	// Ensure current Mesh (-Instance) has valid (current/new..) animations linked to it
	if( CurrentMeshAnim && CurrentSkelMesh() )
	{
		CurrentMeshInstance()->ClearSkelAnims();
		if( !CurrentMeshInstance()->SetSkelAnim( CurrentMeshAnim, NULL ) )
			debugf(TEXT("SetSkelAnimFailed - Mesh: %s Animation: %s"),CurrentSkelMesh()->GetName(),CurrentMeshAnim->GetName());
	}
	

	unguard;
}

// Call ONLY if the real link between mesh & anim changes...
void WBrowserAnimation::RefreshAnimSeqList( FName NewCurrent )
{
	guard(WBrowserAnimation::RefreshAnimSeqList);

	AnimSeqList->Empty();
	if( !Viewport || !Viewport->Actor )
		return; // Called too soon..

	INT NewCurrentIndex = 0;
	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( MInst )
	{
		// Get anims directly from current mesh.			
		INT NumAnims = MInst->GetAnimCount();

		for( INT anim = 0 ; anim < NumAnims ; ++anim )
		{				
			HMeshAnim hAnim = MInst->GetAnimIndexed(anim);
			INT NumFrames = MInst->AnimGetFrameCount(hAnim);
			FName AnimFName = MInst->AnimGetName(hAnim);

			if( NewCurrent != NAME_None && NewCurrent == AnimFName )
				NewCurrentIndex = anim;
				
			FString Name = *AnimFName;
			
			if( Name != NAME_None && Name != TEXT("None") ) 
			{
				AnimSeqList->AddString( *FString::Printf(TEXT("%s [ %d ]"), *Name, NumFrames ));	
			}			
		}	
	}		
	AnimSeqList->SetCurrent(NewCurrentIndex);

	INT CurIdx = AnimSeqList->GetCurrent();
	CurrentSequence = FindAnimSeqNameFromIndex( CurIdx );
	// If we've just renamed a sequence, don't do the full refresh.
	if( NewCurrent==NAME_None )
		OnAnimSequenceSelectionChange();
	unguard;
}

void WBrowserAnimation::RefreshViewport()
{
	guard(WBrowserAnimation::RefreshViewport);

	if( Viewport )
	{
	    // Visible mesh and animation set in motion here.
		FString MeshName = MeshCombo->GetString(MeshCombo->GetCurrent());

		FString AnimName = AnimCombo->GetString(AnimCombo->GetCurrent());
		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("MESH"), *FString::Printf(TEXT("ANIMSEQ NAME=\"%s\" NUM=%d ACTOR=%s"), *MeshName, AnimSeqList->GetCurrent(), MeshActor? MeshActor->GetName() : TEXT("NULL") ), GetPropResult );
		
		// Set viewport flags
		INT UseFlags = 0;
		if(bForceFrame)
			UseFlags = SHOWFLAGS;
		else if(bPlaying|bNotifyEditing)
			UseFlags = SHOWFLAGS | SHOW_RealTime;
		else
			UseFlags = SHOWFLAGS;

		if(bDrawCollision)
			UseFlags |= SHOW_Collision;

		if(bDrawKarma)
			UseFlags |= (SHOW_KarmaPrimitives | SHOW_KarmaMassProps);

		//  As far as engine/editor is concerned, this is much like the meshviewer.
		GUnrealEd->Exec( *FString::Printf(TEXT("CAMERA UPDATE NAME=AnimationViewer MESH=\"%s\" FLAGS=%d REN=%d MISC1=%d MISC2=0"), 
			*MeshName,
			UseFlags,
			REN_Animation,
			bRefpose ? -1 : appAtoi(*(GetPropResult.Right(7).Left(3)))
			));	
	}

	unguard;
}

void WBrowserAnimation::OnDestroy()
{
	guard(WBrowserAnimation::OnDestroy);

	delete ComboPackage;
	delete Viewport;
	delete ScrubBar;
	delete LeftListLabel;
	delete MeshCombo;
	delete AnimCombo;
	delete AnimSeqList;

	::DestroyWindow( hWndToolBar );
	delete ToolTipCtrl;

	::DestroyWindow( MeshPropertyWindow->hWnd );
	MeshPropertyWindow->Root.SetObjects( NULL, 0 );
	delete MeshPropertyWindow;

	::DestroyWindow( AnimPropertyWindow->hWnd );
	AnimPropertyWindow->Root.SetObjects( NULL, 0 );
	delete AnimPropertyWindow;

	::DestroyWindow( SeqPropertyWindow->hWnd );
	SeqPropertyWindow->Root.SetObjects( NULL, 0 );
	delete SeqPropertyWindow;

	::DestroyWindow( NotifyPropertyWindow->hWnd );
	NotifyPropertyWindow->Root.SetObjects( NULL, 0 );
	delete NotifyPropertyWindow;

	::DestroyWindow( PrefsPropertyWindow->hWnd );
	PrefsPropertyWindow->Root.SetObjects( NULL, 0 );
	delete PrefsPropertyWindow;
	
	if( mrulist ) 
	{
		mrulist->WriteINI();
		delete mrulist;
	}
	
	delete ScrubButtonPlay;
	delete ScrubButtonBegin;
	delete ScrubButtonEnd;
	delete ScrubButtonLoop;
	delete ScrubButtonForward;
	delete ScrubButtonBackward;

	if( ApplySequenceSort )
		delete ApplySequenceSort;
	if( MoveSequenceUp )
		delete MoveSequenceUp;
	if( MoveSequenceDown )
		delete MoveSequenceDown;
	

	DeleteObject( ScrubPlayBitmap );
	DeleteObject( ScrubPauseBitmap );
	DeleteObject( ScrubBeginBitmap );
	DeleteObject( ScrubEndBitmap );
	DeleteObject( ScrubForwardBitmap );
	DeleteObject( ScrubBackwardBitmap );
	DeleteObject( ScrubLoopBitmap );
	DeleteObject( ScrubNoLoopBitmap );

	WBrowser::OnDestroy();
	unguard;
}

// Size change redrawing..
void WBrowserAnimation::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WBrowserAnimation::OnSize);
	WBrowser::OnSize(Flags, NewX, NewY);
	PositionChildControls();
	InvalidateRect( hWnd, NULL, FALSE );
	unguard;
}

void WBrowserAnimation::PositionChildControls()
{
	guard(WBrowserAnimation::PositionChildControls);
	Container.RefreshControls();
	unguard;
}

void WBrowserAnimation::OnCommand( INT Command )
{
	guard(WBrowserAnimation::OnCommand);
	UBOOL bImportAnim = false;
	UBOOL bImportAnimAppend = false;
	FString NewObject;

	switch( Command ) 
	{		

		// Sequence list context menu calls:			
		// Seq. props from the context popup menu.
		case IDMN_AB_SEQPROPS:
		{
			PropSheet->SetCurrent( SeqPage );
		}
		break;
		//
		case IDMN_AB_RENAME:
		{
			PropSheet->SetCurrent( SeqPage );
		}
		break;
		// Delete the current sequence - may just have changed (rightclick)
		case IDMN_AB_DELETE:
		{
			DeleteNamedSequence( CurrentSequence );
		}
		break;

		// Notifies
		case IDMN_AB_NOTIFIES:
		{				
			PropSheet->SetCurrent( NotifyPage );
		}
		break;


		case IDMN_AB_PREFS:
		{				
			PropSheet->SetCurrent( PrefsPage );
		}
		break;
		
		// Groups
		case IDMN_AB_GROUPS:
		{
			PropSheet->SetCurrent( SeqPage );
		}
		break;

		// Play 
		case IDAN_ANIMPLAY:
		{
			if( bPlaying )
				StopPlay();
			else
				StartPlay(); 		
		}
		break;	
		
		// Mesh
		case IDMN_EDIT_RENAMEMESH:
		{
			OnMeshRename();
		}
		break;
		case IDMN_EDIT_DELETEMESH:
		{
			OnMeshDelete();				
		}
		break;
		
		// Animation set
		case IDMN_EDIT_RENAMEANIM:
		{
			OnAnimRename();
		}
		break;
		case IDMN_EDIT_DELETEANIM:
		{
			OnAnimDelete();				
		}
		break;

		case IDMN_EDIT_APPLY:
		{
			//!!OnGeneralApply();
		}
		break;

		case IDMN_REFRESH:
		{
			RefreshAll();		
		}
		break;

		case IDMN_AB_LOAD_ENTIRE_PACKAGE:
		{
			FString Package = ComboPackage->GetString( ComboPackage->GetCurrent() );
			GUnrealEd->LoadPackage( NULL, *Package, LOAD_NoWarn );
			RefreshAll();			
			OnMeshSelectionChange();
		}
		break;

		case IDMN_EDIT_CLEARNOTIFIES:
		{
			// Clear notifies for currently selected sequence(s)
			OnClearNotifies();
		}
		break;

		case IDMN_EDIT_CLEARGROUPS:
		{
		}
		break;

		// Load a package containing animation / meshes / associated anim-notifies etc.
		case IDMN_FileOpen:
		{
			// Loading a .u (ukx) universal package:

			OPENFILENAMEA ofn;
			char File[8192] = "\0";			

			ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
			ofn.lStructSize = sizeof(OPENFILENAMEA);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = File;
			ofn.nMaxFile = sizeof(char) * 8192;
			ofn.lpstrFilter = "Animated Mesh Packages (*.ukx)\0*.ukx\0All Files\0*.*\0\0";
			ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UKX]) );
			ofn.lpstrDefExt = "ukx";
			ofn.lpstrTitle = "Open Animated Mesh Package";
			ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

			if( GetOpenFileNameA(&ofn) )
			{
				INT NumNULLs = FormatFilenames( File );

				TArray<FString> StringArray;
				FString S = appFromAnsi( File );
				S.ParseIntoArray( TEXT("|"), &StringArray );

				INT iStart = 0;
				FString Prefix = TEXT("\0");

				if( NumNULLs )
				{
					iStart = 1;
					Prefix = *(StringArray(0));
					Prefix += TEXT("\\");
				}

				if( StringArray.Num() > 0 )
				{
					if( StringArray.Num() == 1 )
					{
						SavePkgName = *(StringArray(0));
						SavePkgName = SavePkgName.Right( SavePkgName.Len() - (SavePkgName.Left( SavePkgName.InStr(TEXT("\\"), 1)).Len() + 1 ));
					}
					else
						SavePkgName = *(StringArray(1));
					SavePkgName = SavePkgName.Left( SavePkgName.InStr(TEXT(".")) );
				}

				if( StringArray.Num() == 1 )
					GLastDir[eLASTDIR_UKX] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
				else
					GLastDir[eLASTDIR_UKX] = StringArray(0);

				GWarn->BeginSlowTask( TEXT(""), 1 );

				for( INT x = iStart ; x < StringArray.Num() ; ++x )
				{
					GWarn->StatusUpdatef( x, StringArray.Num(), TEXT("Loading %s"), *(StringArray(x)) );
					GUnrealEd->Exec( *FString::Printf(TEXT("OBJ LOAD FILE=\"%s%s\""), *Prefix, *(StringArray(x))) );

					// Most-Recently-Used list ( UKX packages only ).
					mrulist->AddItem( *(StringArray(x)) );
						if( GBrowserMaster->GetCurrent()==BrowserID )
							mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
				}				

				GWarn->EndSlowTask();
				GBrowserMaster->RefreshAll();
				ComboPackage->SetCurrent( ComboPackage->FindStringExact( *SavePkgName ) );
				
				// #SKEL
				// RefreshGroups();
				// RefreshTextureList();

				StringArray.Empty();
			}

			GFileManager->SetDefaultDirectory(appBaseDir());
						
			RefreshAll();
			OnMeshSelectionChange(); 
			RefreshViewport();			
		}	
		break;


		// Save a package containing animation / meshes / associated anim-notifies etc.
		case IDMN_FileSave:
		{
			OPENFILENAMEA ofn;
			char File[8192] = "\0";

			FString Package = ComboPackage->GetString( ComboPackage->GetCurrent() ); //#SKEL
			//FString Package = TEXT("TempAnimPackage");

			::sprintf( File, "%s.ukx", TCHAR_TO_ANSI( *Package ) );

			ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
			ofn.lStructSize = sizeof(OPENFILENAMEA);    
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = File;
			ofn.nMaxFile = sizeof(char) * 8192;
			ofn.lpstrFilter = "Animation Packages (*.ukx)\0*.ukx\0All Files\0*.*\0\0";
			ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UKX]) );
			ofn.lpstrDefExt = "ukx";
			ofn.lpstrTitle = "Save Animation Package";
			ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

			if( GetSaveFileNameA(&ofn) )
			{
				TCHAR l_chCmd[512];

				appSprintf( l_chCmd, TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""),
					*Package, appFromAnsi( File ) );
				if( GUnrealEd->Exec( l_chCmd ) )
				{
					FString S = appFromAnsi( File );
					mrulist->AddItem( S );
					if( GBrowserMaster->GetCurrent()==BrowserID )
						mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
					GLastDir[eLASTDIR_UKX] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}
			}
			GFileManager->SetDefaultDirectory(appBaseDir());
		}
		break;


		case IDMN_EDIT_MESHPROP:
			PropSheet->SetCurrent( MeshPage );
			break;

		case IDMN_EDIT_ANIMPROP:
			PropSheet->SetCurrent( AnimPage );
			break;

		case IDMN_EDIT_SEQUPROP:
			PropSheet->SetCurrent( SeqPage );
			break;

		case IDMN_EDIT_NOTIFICATIONS:
			PropSheet->SetCurrent( NotifyPage );
			break;

		case IDMN_EDIT_PREFS:
			PropSheet->SetCurrent( PrefsPage );
			break;

		case IDMN_EDIT_GROUPS:
			PropSheet->SetCurrent( SeqPage );
			break;
	
		// Link current animation to current mesh as active animation AND as mesh's defaultanim.
		case IDMN_EDIT_LINKANIM:
		{				
			USkeletalMeshInstance* MInst = CurrentMeshInstance();				
			if( MInst )				
			{
				if( CurrentMeshAnim )
					((USkeletalMesh*)MInst->GetMesh())->DefaultAnim = CurrentMeshAnim;

				MInst->ClearSkelAnims();
				if(! MInst->SetSkelAnim( CurrentMeshAnim, NULL ) )
						debugf(TEXT("SetSkelAnim failed - Mesh: %s  Animation: %s "),MInst->GetMesh()->GetName(),CurrentMeshAnim->GetName());

				RefreshAnimSeqList();
			}
		}
		break;

        // Detach and erase default-animation from current mesh..
		case IDMN_EDIT_UNLINKANIM:
		{								
			USkeletalMeshInstance* MInst = CurrentMeshInstance();				
			if( MInst )				
			{							
				MInst->ClearSkelAnims();										
				((USkeletalMesh*)MInst->GetMesh())->DefaultAnim = NULL;
				RefreshAnimSeqList();
			}
		}
		break;

		// Import PSA, PSK data.
		case IDMN_FILE_IMPORTANIMMORE:				
			bImportAnimAppend=true; // Simply prompt using current animation, and 'append', on by default.
		case IDMN_FILE_IMPORTANIM:
			bImportAnim=true;
		case IDMN_FILE_IMPORTMESH:
		{
			OPENFILENAMEA ofn;
			char File[8192] = "\0";

			FString CurrentMesh;

			ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
			ofn.lStructSize = sizeof(OPENFILENAMEA);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFile = File;
			ofn.nMaxFile = sizeof(char) * 8192;

			DWORD LastDirTypeID;
			
			if( bImportAnim ) 
			{
				ofn.lpstrFilter = "Skeletal animation raw data (*.psa)\0*.psa\0All Files\0*.*\0\0";
				ofn.lpstrDefExt = "psa";
				LastDirTypeID = eLASTDIR_PSA;
				CurrentMesh = MeshCombo->GetString (MeshCombo->GetCurrent() );
			}
			else
			{
				ofn.lpstrFilter = "Skeletal mesh raw data (*.psk)\0*.psk\0All Files\0*.*\0\0";
				ofn.lpstrDefExt = "psk";
				LastDirTypeID = eLASTDIR_PSK;
			}
			ofn.lpstrInitialDir = appToAnsi( *(GLastDir[LastDirTypeID]) );								
				
			ofn.lpstrTitle = "Open Mesh/Animation raw data";
			ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

			if( GetOpenFileNameA(&ofn) )
			{
				INT NumNULLs = FormatFilenames( File );

				TArray<FString> StringArray;
				FString S = appFromAnsi( File );
				S.ParseIntoArray( TEXT("|"), &StringArray );

				INT iStart = 0;
				FString Prefix = TEXT("\0");

				if( NumNULLs )
				{
					iStart = 1;
					Prefix = *(StringArray(0));
					Prefix += TEXT("\\");
				}

				if( StringArray.Num() > 0 )
				{
					if( StringArray.Num() == 1 )
					{
						SavePkgName = *(StringArray(0));
						SavePkgName = SavePkgName.Right( SavePkgName.Len() - (SavePkgName.Left( SavePkgName.InStr(TEXT("\\"), 1)).Len() + 1 ));
					}
					else
						SavePkgName = *(StringArray(1));
					SavePkgName = SavePkgName.Left( SavePkgName.InStr(TEXT(".")) );
				}

				if( StringArray.Num() == 1 )
					GLastDir[LastDirTypeID] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
				else
					GLastDir[LastDirTypeID] = StringArray(0);
				
				// Assign group etc with the import dialog ? #SKEL

				FString Package = ComboPackage->GetString( ComboPackage->GetCurrent() );
				//FString Package = TEXT("TempAnimPackage");
				FString Group   = TEXT("Default");        
				
				WDlgNewMesh l_dlg( NULL, this, bImportAnim, bImportAnimAppend );

				FString ObjectName = TEXT("");				
				if( bImportAnimAppend )
				{
					ObjectName = AnimCombo->GetString( AnimCombo->GetCurrent() ); // Current anim name.
				}

				// Suggest PSK/PSA name as new mesh or anim obj name unless appending into current animation set.
				if( !bImportAnimAppend )
				{
					FString NewName = S.Left( S.InStr(TEXT(".")) );
					INT SlashLocation = NewName.InStr( TEXT("\\"),1 );
					if( SlashLocation > -1 )
					{
						NewName = NewName.Right( NewName.Len() - SlashLocation - 1 );
					}
					ObjectName = NewName;
				}

				if( l_dlg.DoModal( Package, Group, ObjectName ) ) // Present user with import window..
				{	
					FString NewPackage = *l_dlg.Package;
					NewObject  = *l_dlg.Name;
				
					GWarn->BeginSlowTask( TEXT(""), 1 );

					// Merging of animations: Object name forced to special temporary object, then merge into existing 'NewObject' or give error message.
					if( l_dlg.DoMergeAnims && bImportAnim )
					{
						NewObject = FString(TEXT("Temp1969ad"));
					}

					for( INT x = iStart ; x < StringArray.Num() ; ++x ) //#SKEL - only import one...
					{							
						GWarn->StatusUpdatef( x, StringArray.Num(), TEXT("Loading and digesting %s"), *(StringArray(x)) );
						
						FString AddCmdLine;
						
						if( bImportAnim && EditAnimProps ) 
							AddCmdLine = FString::Printf(TEXT("COMP=%4f "),EditAnimProps->GlobalCompression);

						if( !bImportAnim && DoWeldDuplicateVertices )
							AddCmdLine += FString(TEXT("WELDING=1"));
					
						if (l_dlg.DoMayaCoords)
							AddCmdLine += FString(TEXT("YAW=-64 PITCH=0 ROLL=64 "));
						else
							AddCmdLine += FString(TEXT("YAW=0 PITCH=0 ROLL=0 "));
						
						// Call EDITOR.DLL with the filename.  It will figure out what to do when loading a PSK or a PSA.												
						GEditor->Exec( *FString::Printf(TEXT("NEWANIM IMPORT FILE=\"%s%s\"  PACKAGE=\"%s\"  NAME=\"%s\" %s  BROWSER"), *Prefix, *(StringArray(x)), *NewPackage, *NewObject, *AddCmdLine ) );

					}

					GWarn->EndSlowTask();

					// if successfully input, start digesting stuff..
					if( !bImportAnimAppend )
					{
						RefreshPackages();
					}

					// If succesfully imported a new mesh, make sure it starts in the reference pose:
					if( !bImportAnim )
					{
						bRefpose = true;
					}
					
					ComboPackage->SetCurrent( ComboPackage->FindStringExact( *l_dlg.Package) );						

					// If new animation imported, link to current mesh as defaultanim ? -> Count on user pressing chain.

					// Merging in animations...
					if( l_dlg.DoMergeAnims && bImportAnim )
					{
						// Find animation object just imported.
						UPackage* Pkg = UObject::CreatePackage(NULL,*NewPackage);
						UMeshAnimation* NewAnimObject = Cast<UMeshAnimation>( UObject::StaticFindObject( UMeshAnimation::StaticClass(), Pkg , *NewObject ) );

						FString DestObjectName  = *l_dlg.Name;
						
						UMeshAnimation* DestAnimObject = (UMeshAnimation*) UObject::StaticFindObject( UMeshAnimation::StaticClass(), Pkg, *DestObjectName );
						
						// The typed-in animation object to ADD to needs to exist. 
						if( ! DestAnimObject)
						{								
							appMsgf( 0, TEXT("No animation set named [%s] exists yet. Aborting merge."),*DestObjectName);
							//UPackage* Pkg = UObject::CreatePackage(NULL,*NewPackage);
							//DestAnimObject = new( Pkg, *DestObjectName, RF_Public|RF_Standalone )UMeshAnimation();
							if( NewAnimObject ) delete NewAnimObject; //#SKEL
						}
						else
						{
							if( NewAnimObject && ( NewAnimObject->AnimSeqs.Num() == NewAnimObject->Moves.Num() ) )
							{
								
								appMsgf(0,TEXT(" Merging animations with %i bones, %i sequence(s) into existing set with %i bones, %i sequence(s)."),
									NewAnimObject->RefBones.Num(),NewAnimObject->AnimSeqs.Num(),DestAnimObject->RefBones.Num(),DestAnimObject->AnimSeqs.Num() );
																						
								// Conforming for merge: Detect bone matches, re-order bones accordingly, patch missing ones in from the reference skeleton if available.								
								USkeletalMesh* ReferenceMesh = NULL;
								USkeletalMesh* SkelMesh = CurrentSkelMesh();
								if( SkelMesh && SkelMesh->DefaultAnim && ( SkelMesh->DefaultAnim == DestAnimObject ) )
								{
									ReferenceMesh = SkelMesh;
								}
								NewAnimObject->ConformBones( DestAnimObject, ReferenceMesh );

								UBOOL OverwriteSeqs = l_dlg.DoOverwriteSeqs; 

								// Merge sequences (= AnimSeqs and Moves) into DestAnimObject. -> Any existing names: delete and overwrite ( Or pop up a menu. )
								for(INT i=0; i<NewAnimObject->AnimSeqs.Num(); i++)
								{		
									// Check name uniqueness, then decide to add or overwrite...
									FName NewSeqName = NewAnimObject->AnimSeqs(i).Name;
									INT SeqIndex = -1;
									//= DestAnimObject->GetAnimSeq( NewSeqName );
									for( INT j=0; j<DestAnimObject->AnimSeqs.Num(); j++ )
									{
										if( NewSeqName == DestAnimObject->AnimSeqs(j).Name )
										{
											SeqIndex = j;
											break;
										}
									}

									// If found, overwrite or ignore, otherwise add.
									if( SeqIndex > -1 )
									{
										if( !OverwriteSeqs )
										{
											// Don't overwrite, so we'll ignore the new one..
										}
										else
										{
											// Find alternative index,erase, put the sequence there..											
											DestAnimObject->AnimSeqs(SeqIndex) = NewAnimObject->AnimSeqs(i); //

											// Same with Moves - parallel the AnimSeqs.
											DestAnimObject->Moves(SeqIndex).Erase(); 
											DestAnimObject->Moves(SeqIndex) = NewAnimObject->Moves(i);
										}
									}
									else // Animation sequence doesn't exist yet
									{
										//FMeshAnimSeq FMNewSeq = NewAnimObject->AnimSeqs(i); //duplicates..
										INT NewIdx = DestAnimObject->AnimSeqs.AddZeroed(1);
										DestAnimObject->AnimSeqs(NewIdx) = NewAnimObject->AnimSeqs(i);

										// Same with Moves - parallel the AnimSeqs.
										NewIdx = DestAnimObject->Moves.AddZeroed(1);
										DestAnimObject->Moves(NewIdx) = NewAnimObject->Moves(i);
									}
								}

								/*
								for(INT i=0; i<NewAnimObject->Moves.Num(); i++)
								{							
									INT NewIdx = DestAnimObject->Moves.AddZeroed(1);
									DestAnimObject->Moves(NewIdx) = NewAnimObject->Moves(i);
								}
								*/

								if( NewAnimObject )
								{								
									delete NewAnimObject;
								}

							}
						}
					}			
				}
				GBrowserMaster->RefreshAll();
				
				StringArray.Empty();
			}

			GFileManager->SetDefaultDirectory(appBaseDir());			

			RefreshAll();
			SetCaption();				

			if( !bImportAnim ) 
			{
				// New mesh imported.
				if( NewObject.Len() >= 1 )
				{
					MeshCombo->SetCurrent( *NewObject );
					appMsgf(0, TEXT("new object: [%s]"),*NewObject );
				}

				// Ensure anim not linked to this (overwritten) mesh:
				USkeletalMeshInstance* MInst = CurrentMeshInstance();				
				if( MInst )				
				{	
					MInst->ClearSkelAnims();					
				}

				OnMeshSelectionChange();
				OnAnimObjectSelectionChange();				
			}
			else
			{
				if( NewObject.Len() >= 1 )
				{
					MeshCombo->SetCurrent( *CurrentMesh );
					AnimCombo->SetCurrent( *NewObject );
				}
				OnMeshSelectionChange();
				OnAnimObjectSelectionChange();
			}
			
			RefreshViewport();

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
			FString Filename = mrulist->Items[Command - IDMN_MRU1];
			if( GFileManager->FileSize( *Filename ) == -1 )
			{
				appMsgf( 0, TEXT("'%s' does not exist."), *Filename );
				mrulist->RemoveItem( Filename );
			}
			else
			{
				GWarn->StatusUpdatef( 0, 0, TEXT("Loading (from MRU list) %s"), *Filename);
				GUnrealEd->Exec( *FString::Printf(TEXT("OBJ LOAD FILE=\"%s\""), *Filename ));

				mrulist->MoveToTop( Command - IDMN_MRU1 );

				FString Package = Filename.Right( Filename.Len() - (Filename.InStr( TEXT("\\"), 1) + 1) );
				Package = Package.Left( Package.InStr( TEXT(".")) );

				ComboPackage->SetCurrent( ComboPackage->FindStringExact( *Package ) );			

				OnMeshSelectionChange();
				RefreshAll();
			}

			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
		}
		break;

		case IDMN_VIEW_INFO:
		{		
			if( CurrentSkelMesh() )
			{
				// Present user with info window.
				WDlgShowInfo l_dlg( NULL, this, CurrentMeshInstance(), CurrentMeshAnim, CurrentSequence );			
				l_dlg.DoModal(); 
			}
		}
		break;

		case IDMN_VIEW_BONES:	
		{
			static UBOOL bToggleHideSkin = false;
			// appMsgf(0,TEXT("Bones button pressed."));
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

		case IDMN_VIEW_RAWOFFSET:
		{
			bRawOffset = !bRawOffset;
			USkeletalMeshInstance* MInst = CurrentMeshInstance();
			if( MInst )				
			{										
				MInst->bForceRawOffset = bRawOffset;
			}
			RefreshViewport();
		}
		break;

		case IDMN_VIEW_BACKFACE:				
		{
			bBackface = !bBackface;	
			USkeletalMeshInstance* MInst = CurrentMeshInstance();
			if( MInst )								
			{					
				MInst->bForceBackfaceCulling = bBackface;
			}
			RefreshViewport();
		}
		break;

		case IDMN_VIEW_REFPOSE:				
		{
			bRefpose = !bRefpose;			
			if(bRefpose) 
				StopPlay();
			RefreshViewport();
		}
		break;

		case IDMN_VIEW_COLLISION:
		{
			bDrawCollision = !bDrawCollision;
			UpdateMenu();
			RefreshViewport();
		}
		break;

		case IDMN_VIEW_KARMA:
		{
			bDrawKarma = !bDrawKarma;
			UpdateMenu();
			RefreshViewport();
		}
		break;

		case IDMN_COPY_SKEL_PHYSPROPS:
		{
#ifdef WITH_KARMA
			USkeletalMesh* smesh = CurrentSkelMesh();
			if( smesh && smesh->KPhysicsProps )
			{
				KCopyMassPropsToClipboard(smesh->KPhysicsProps);
			}
#endif
		}
		break;

		// 'Joystick+a' - quick way to gauge placement of root-animating characters.
		case IDMN_VIEW_LEVELANIM:				
		{
			bLevelAnim = !bLevelAnim;						
			if( bLevelAnim ) 
			{
				TempForcedActorList.Empty();
				for( TObjectIterator<AActor> It ; It ; ++It )
				{
					AActor* TestActor = *It;
					// Refresh relevant in-level actor list
					// Iterate and find any actor with mesh matching current - that is NOT the animbrowser's actor..
					if( (!TestActor->bDeleteMe ) && 
						((USkeletalMesh*)TestActor->Mesh == CurrentSkelMesh())  && 
						(TestActor->Mesh->MeshGetInstance(TestActor) != CurrentMeshInstance() )
						)
					{
						TempForcedActorList.AddItem( TestActor );
					}
				}
			}			
			else
			{
				ResetLevelMeshes();
				TempForcedActorList.Empty();
			}
			RefreshViewport();
		}
		break;

		// Mesh props copy/paste
		
		case IDMN_EDIT_COPYMESHPROPS:
		{
			OnCopyMeshProps();
		}
		break;

		case IDMN_EDIT_PASTEMESHPROPS:
		{
			OnPasteMeshProps();
			RefreshViewport();
		}
		break;

		case IDMN_MESH_IMPORTLOD:
		case IDMN_FILE_IMPORTLOD:
		{
			OnImportMeshLOD();
			RefreshViewport();
		}
		break;
		
		case IDMN_MESH_REDIGESTLOD:
		{
			OnMeshRedigestLOD();
			RefreshViewport();
		}
		break;

		case IDMN_MESH_CYCLELOD:
		{
			CurrentMeshInstance()->ForcedLodModel++;
			if( CurrentMeshInstance()->ForcedLodModel > CurrentSkelMesh()->LODModels.Num() )
			{
				CurrentMeshInstance()->ForcedLodModel = 0;
			}			
			RefreshViewport();
		}
		break;

		// Notification functionality:
		case IDMN_EDIT_ADDNOTIFY:
		{
		}
		break;

		case IDMN_EDIT_COPYNOTIFIES:
		{
			OnCopyNotifies();
		}
		break;

		case IDMN_EDIT_PASTENOTIFIES:
		{
			OnPasteNotifies();
		}
		break;

		case IDMN_EDIT_COPYGROUPS:
		{
			OnCopyGroups();
		}
		break;

		case IDMN_EDIT_PASTEGROUPS:
		{
			OnPasteGroups();
		}
		break;

		default:
			WBrowser::OnCommand(Command);
			break;
	}
	unguard;
}


USkeletalMesh* WBrowserAnimation::CurrentSkelMesh()
{
	guard(WBrowserAnimation::CurrentMesh);
	
	// Ensure mesh current with WorkMesh.  Only returns a valid mesh if it's skeletal.
	if( MeshActor && WorkMesh && MeshActor->Mesh != WorkMesh )
		MeshActor->Mesh = WorkMesh;

	if( WorkMesh && WorkMesh->IsA(USkeletalMesh::StaticClass() ) )
		return (USkeletalMesh*) WorkMesh;
	else
		return NULL;
	unguard;
}


USkeletalMeshInstance* WBrowserAnimation::CurrentMeshInstance()
{
	guard(WBrowserAnimation::CurrentMeshInstance);
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


void WBrowserAnimation::UpdateMenu()
{
	guard(WBrowserAnimation::UpdateMenu);

	HWND hwnd = IsDocked() ? OwnerWindow->hWnd : hWnd;
	HMENU menu = GetMenu( hwnd );

	if( mrulist && ( GBrowserMaster->GetCurrent()==BrowserID ) )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

	if(menu)
	{
		CheckMenuItem( menu, IDMN_VIEW_COLLISION, bDrawCollision ? MF_CHECKED : MF_UNCHECKED  );
		CheckMenuItem( menu, IDMN_VIEW_KARMA, bDrawKarma ? MF_CHECKED : MF_UNCHECKED  );
	}

	unguard;
}

// Animation viewer/player controls
//
void WBrowserAnimation::StartPlay()
{
	guard(WBrowserAnimation::StartPlay);
	ScrubButtonPlay->SetBitmap( ScrubPauseBitmap );
	bForceFrame = false;
	bPlaying = 1;
	bNotifyEditing = 0;
	bPlayJustStarted = 1;
	// play again if at end
	if( ScrubBar->GetPos() >= SCRUBBARRANGE-1 )
		FrameTime = 0.f;
	RefreshViewport();

	// Snap out of refpose & update the button.
	bRefpose = 0;					
	INT Toggle = 0;
	SendMessageX( hWndToolBar, TB_PRESSBUTTON,  IDMN_VIEW_REFPOSE, (LPARAM) MAKELONG(Toggle, 0));		
	SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_VIEW_REFPOSE, (LPARAM) MAKELONG(Toggle, 0));		
	unguard;
}

void WBrowserAnimation::StopPlay()
{
	guard(WBrowserAnimation::StopPlay)
	
	if( bPlaying || bNotifyEditing )
	{
		ScrubButtonPlay->SetBitmap( ScrubPlayBitmap );
		bNotifyEditing = 0;
		bPlaying = 0;
		Viewport->Actor->ShowFlags &= ~SHOW_RealTime;
	}
	CleanupLevel();
	unguard;
}

//
// Clean up any notify-spawned effects
//
void WBrowserAnimation::CleanupLevel()
{
	guard(WBrowserAnimation::CleanupLevel);
	UBOOL PassedMeshActor = 0;
	for( INT i=AnimBrowserLevel->Actors.Num()-1;i>=0;i-- )
	{		
		if( AnimBrowserLevel->Actors(i) == MeshActor )
			PassedMeshActor = 1;
		if( AnimBrowserLevel->Actors(i) && !PassedMeshActor && AnimBrowserLevel->Actors(i) != Viewport->Actor )
			AnimBrowserLevel->DestroyActor( AnimBrowserLevel->Actors(i) );
	}
	unguard;
}

// Notification delegates for child controls.
//
void WBrowserAnimation::OnPackageSelectionChange()
{
	guard(WBrowserAnimation::OnComboPackageSelChange);

	RefreshMeshList(); // Sets current mesh to 0th entry...

	OnMeshSelectionChange(); //

	/*
	WorkMesh = Cast<UMesh>(UObject::StaticFindObject(USkeletalMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));
	InitMeshDrawSettings();
	RefreshAnimObjList();		
	RefreshAnimSeqList();
	RefreshViewport();
	SetCaption();
	*/
	unguard;
}

// Initialize CurrentMeshAnim and mesh-debug-rendering variables.
void WBrowserAnimation::InitMeshDrawSettings()
{
	if(  CurrentSkelMesh() )
	{								
		// Refresh per-MeshInstance rendermode flags.
		USkeletalMeshInstance* MInst = CurrentMeshInstance();
		if( MInst )				
		{										
			MInst->bForceRawOffset = bRawOffset;
			MInst->bPrintBoneNames = bPrintBones;
			MInst->bForceWireframe = bWireframe;				
			MInst->bForceBackfaceCulling = bBackface;
		}
	}		
}
	
void WBrowserAnimation::OnMeshSelectionChange()
{
	guard(WBrowserAnimation::OnMeshSelectionChange);

	WorkMesh = Cast<USkeletalMesh>(UObject::StaticFindObject(USkeletalMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));				
	// So that 'get current' will work when assigning meshes to actors in the editor..
	
	RefreshAnimObjList();		

	// Reset the forced LOD model to auto-LOD.
	if( CurrentMeshInstance() )
		CurrentMeshInstance()->ForcedLodModel = 0;

	// Defaultanimation assigned ? 
	if(  CurrentSkelMesh() )
	{					
		USkeletalMesh* SkelMesh = CurrentSkelMesh();
		// Since an animation was explicitly selected, make it current AND active now for the mesh...	
		CurrentMeshAnim = Cast<UMeshAnimation>(UObject::StaticFindObject(UMeshAnimation::StaticClass(), ANY_PACKAGE,*( AnimCombo->GetString(AnimCombo->GetCurrent()))));

		CurrentMeshInstance()->ClearSkelAnims();
		if(! CurrentMeshInstance()->SetSkelAnim( CurrentMeshAnim, NULL) )
					debugf(TEXT("SetSkelAnim failed - Mesh: %s  Animation: %s "),SkelMesh->GetName(),CurrentMeshAnim->GetName());			
	}

	RefreshAnimProperties();
	RefreshMeshProperties();
	RefreshAnimSeqList();		
	InitMeshDrawSettings();
	RefreshPrefsEditProperties();
	RefreshViewport();
	SetCaption();	

	unguard;
}


void WBrowserAnimation::OnAnimObjectSelectionChange()
{
	guard(WBrowserAnimation::OnAnimObjectSelectionChange);
			
	// Defaultanimation assigned ?
	if(  CurrentSkelMesh() )
	{					
		USkeletalMesh* SkelMesh = CurrentSkelMesh();
		// Since an animation was explicitly selected, make it current AND active now for the mesh...	
		CurrentMeshAnim = Cast<UMeshAnimation>(UObject::StaticFindObject(UMeshAnimation::StaticClass(), ANY_PACKAGE,*( AnimCombo->GetString(AnimCombo->GetCurrent()))));

		CurrentMeshInstance()->ClearSkelAnims();
		if(! CurrentMeshInstance()->SetSkelAnim( CurrentMeshAnim, NULL ) )
					debugf(TEXT("SetSkelAnim failed - Mesh: %s  Animation: %s "),SkelMesh->GetName(),CurrentMeshAnim->GetName());			

	}
	
	RefreshAnimProperties();
	RefreshAnimSeqList();
	InitMeshDrawSettings();	 //#skel	
	RefreshViewport();
	SetCaption();
	unguard;
}

void WBrowserAnimation::OnAnimSequenceSelectionChange()
{
	guard(WBrowserAnimation::OnAnimSequenceSelectionChange);
	//RefreshViewport();

	INT CurIdx = AnimSeqList->GetCurrent(); //GetSelected ???
	if( CurIdx >= 0 )
	{
		CurrentSequence = FindAnimSeqNameFromIndex( CurIdx );
		CurrentSeqFrames = GetSeqFramesFromIndex( CurIdx );
		
		// If refpose was set, automatically deactivate since obviously someone wants to see this sequence play ?
		// bRefpose = false;
	}

	// Refresh prop window on selection change if it is active
	RefreshNotifiesList();
	RefreshSequenceProperties();
	SetNotifyTicks();

	if( !bPlaying )
	{
		FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
		if( ThisAnim )
			FrameTime = ThisAnim->Bookmark;
		else
			FrameTime = 0.f;
	}

	if( bNotifyEditing )
		StopPlay();

	UpdateScrub();

	RefreshViewport(); 

	unguard;
}

// Double clicking an animation sequence brings up properties on the right.
void WBrowserAnimation::OnAnimSequenceDoubleClick()
{
	guard(WBrowserAnimation::OnAnimSequenceDoubleClick);
	PropSheet->SetCurrent( SeqPage );
	unguard;
}

void WBrowserAnimation::OnAnimSequenceRightClick()
{		
	guard(WBrowserAnimation::OnAnimSequenceRightClick);		
	// Rightclick may also change 'current' ! 
	INT CurIdx = AnimSeqList->GetCurrent(); //GetSelected ???
	if( CurIdx >=0 )
	{
		CurrentSequence = FindAnimSeqNameFromIndex( CurIdx );
		GUnrealEd->EdCallback( EDC_RtClickAnimSeq, 0, 0 );
		RefreshViewport();
	}
	unguard;
}


void WBrowserAnimation::OnApplySequenceSort()
{
	SendMessageX( ApplySequenceSort->hWnd, BM_SETCHECK, BST_UNCHECKED, 0 );
	if( CurrentMeshAnim && CurrentSkelMesh() )
	{
		INT SavedCurrentSeq = AnimSeqList->GetCurrent();
		CurrentMeshAnim->SortSequencesByName();

		RefreshAnimSeqList();
		AnimSeqList->SetCurrent( SavedCurrentSeq );
		OnAnimSequenceSelectionChange();		
	}	
}

void WBrowserAnimation::OnMoveSequenceDown()
{	
	SendMessageX( MoveSequenceDown->hWnd, BM_SETCHECK, BST_UNCHECKED, 0 );
	if( CurrentMeshAnim && CurrentSkelMesh() )
	{	
		INT SavedCurrentSeq = AnimSeqList->GetCurrent();
		INT Result = CurrentMeshAnim->MoveSequenceByIndex( SavedCurrentSeq,SavedCurrentSeq+1);
		if( Result >= 0 )
		{
			RefreshAnimSeqList();
			AnimSeqList->SetCurrent( SavedCurrentSeq+1 );
			OnAnimSequenceSelectionChange();						
		}
	}	
}

void WBrowserAnimation::OnMoveSequenceUp()
{	
	SendMessageX( MoveSequenceUp->hWnd, BM_SETCHECK, BST_UNCHECKED, 0 );
	if( CurrentMeshAnim && CurrentSkelMesh() )
	{	
		INT SavedCurrentSeq = AnimSeqList->GetCurrent();
		INT Result = CurrentMeshAnim->MoveSequenceByIndex( SavedCurrentSeq,SavedCurrentSeq-1);
		if( Result >= 0 )
		{
			RefreshAnimSeqList();
			AnimSeqList->SetCurrent( SavedCurrentSeq-1 );
			OnAnimSequenceSelectionChange();						
		}
	}
}


void WBrowserAnimation::OnSliderMove()
{		
	guard(WBrowserAnimation::OnSliderMove);
	StopPlay();
	bRefpose = 0;

	bForceFrame = 1;
	RefreshViewport();
	bForceFrame = 0;
	SaveBookmark();
	unguard;
}

void WBrowserAnimation::UpdateScrub()
{
	guard(WBrowserAnimation::UpdateScrub);
	ScrubBar->SetPos( Clamp<INT>( ((FLOAT)SCRUBBARRANGE * FrameTime*CurrentSeqFrames/(CurrentSeqFrames-1.f)), 0, SCRUBBARRANGE) );
	unguard;
}

void WBrowserAnimation::OnScrubPlay()
{
	guard(WBrowserAnimation::OnScrubPlay);
	if( bPlaying )
	{
		StopPlay();
		SaveBookmark();
	}
	else
		StartPlay(); 		
	unguard;
}
void WBrowserAnimation::OnScrubBegin()
{
	guard(WBrowserAnimation::OnScrubBegin);
	
	FrameTime = 0.f;
	UpdateScrub();

	StopPlay();
	bRefpose = 0;				
	bForceFrame = 1;				
	RefreshViewport();
	bForceFrame = 0;
	SaveBookmark();
	unguard; 
}

void WBrowserAnimation::OnScrubEnd()
{
	guard(WBrowserAnimation::OnScrubEnd);

	FrameTime = CurrentSeqFrames > 0.f ? (CurrentSeqFrames -1.0f)/CurrentSeqFrames : 0.f;
	UpdateScrub();

	StopPlay();
	bRefpose = 0;
	bForceFrame = 1;
	
	RefreshViewport();
	bForceFrame = 0;
	SaveBookmark();

	unguard;
}

void WBrowserAnimation::OnScrubBackward()
{
	guard(WBrowserAnimation::OnScrubBackward);

	StopPlay();
	bRefpose = 0;
	bForceFrame = 1;
	
	FrameTime = CurrentSeqFrames > 0.f ? Max<FLOAT>( (FLOAT)(appRound(FrameTime * CurrentSeqFrames) - 1) / CurrentSeqFrames, 0.f ) : 0.f;
	UpdateScrub();

	RefreshViewport();
	bForceFrame = 0;
	SaveBookmark();

	unguard;
}
void WBrowserAnimation::OnScrubForward()
{
	guard(WBrowserAnimation::OnScrubForward);

	StopPlay();
	bRefpose = 0;
	bForceFrame = 1;
	
	FrameTime = CurrentSeqFrames > 0.f ? Min<FLOAT>( (FLOAT)(appRound(FrameTime * CurrentSeqFrames) + 1) / CurrentSeqFrames, (CurrentSeqFrames-1.f)/CurrentSeqFrames ) : 0.f;
	UpdateScrub();

	RefreshViewport();
	bForceFrame = 0;
	SaveBookmark();

	unguard;
}
void WBrowserAnimation::OnScrubLoop()
{
	guard(WBrowserAnimation::OnScrubLoop);
	bDoScrubLoop = !bDoScrubLoop;
	ScrubButtonLoop->SetBitmap( bDoScrubLoop ? ScrubLoopBitmap : ScrubNoLoopBitmap );
	unguard;
}

void WBrowserAnimation::SaveBookmark()
{
	guard(void WBrowserAnimation::SaveBookmark);
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim )
		ThisAnim->Bookmark = FrameTime;
	unguard;
}

FName WBrowserAnimation::FindAnimSeqNameFromIndex( INT Index )
{
	guard(WBrowserAnimation::FindAnimSeqNameFromIndex);

	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( CurrentMeshAnim && ( Index >=0 ) &&  MInst )
	{								
		// Get current sequence FName from index
		HMeshAnim hAnim = MInst->GetAnimIndexed( Index );
		if( hAnim )
			return MInst->AnimGetName(hAnim);
	}
	return NAME_None;
	unguard;
}

FLOAT WBrowserAnimation::GetSeqFramesFromIndex( INT Index )
{
	guard(WBrowserAnimation::GetAnimSeqFramesFromIndex);
	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( CurrentMeshAnim && ( Index >=0 ) &&  MInst )
	{								
		// Get current sequence FName from index
		HMeshAnim hAnim = MInst->GetAnimIndexed( Index );
		return( (FLOAT) MInst->AnimGetFrameCount(hAnim) );
	}
	return 0.0f;
	unguard;
}

UBOOL WBrowserAnimation::DeleteNamedSequence( FName SeqName )
{
	UBOOL bErased = false;
	if( CurrentMeshAnim )
	{
		for( INT i=0; i<CurrentMeshAnim->AnimSeqs.Num(); i++ )
		{
			if( SeqName == CurrentMeshAnim->AnimSeqs(i).Name )
			{
				//#SKEL - check for correct removal..
				CurrentMeshAnim->Moves(i).Erase();
				CurrentMeshAnim->Moves.Remove(i);
				CurrentMeshAnim->AnimSeqs.Remove(i); 
				bErased = true;
				break;
			}
		}		
	}
	if( bErased) 
	{
		RefreshAnimSeqList();
		OnAnimSequenceSelectionChange();
	}

	return bErased;
}

// Find anim sequence in currently active animation (if any)
FMeshAnimSeq* WBrowserAnimation::FindAnimSeqByName( FName FindSeq )
{
	guard(WBrowserAnimation::FindAnimSeqByName);

	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( CurrentMeshAnim && ( FindSeq != NAME_None ) &&  MInst )
	{					
		for( INT i=0; i<CurrentMeshAnim->AnimSeqs.Num(); i++)
		{				
			//debugf(TEXT("Animseq num %i name %s current %s "),i,*(CurrentMeshAnim->AnimSeqs(i).Name),*FindSeq  ); 
			if( CurrentMeshAnim->AnimSeqs(i).Name == FindSeq )
			{
				return &(CurrentMeshAnim->AnimSeqs(i));
			}
		}			
	}

	return NULL;
	unguard;
}

void WBrowserAnimation::OnMeshRename()
{
	guard(WBrowserAnimation::OnMeshRename);
	USkeletalMesh* SkelMesh = CurrentSkelMesh();
	if( !SkelMesh )
	{
		appMsgf( 0, TEXT("Please select a skeletal mesh first.") );
		return;
	}

	WDlgRename dlg( NULL, this );
	FString Group, Package;
	if( !Cast<UPackage>(WorkMesh->GetOuter()->GetOuter()) )
	{
		Group = TEXT("");
		Package = WorkMesh->GetOuter()->GetName();
	}
	else
	{			
		Group = WorkMesh->GetOuter()->GetName();
		Package = WorkMesh->GetOuter()->GetOuter()->GetName();
	}					

	if( dlg.DoModal( WorkMesh->GetName(), Group, Package ) )
	{
		GUnrealEd->Exec(*FString::Printf(TEXT("OBJ RENAME OLDNAME=\"%s\" OLDGROUP=\"%s\" OLDPACKAGE=\"%s\" NEWNAME=\"%s\" NEWGROUP=\"%s\" NEWPACKAGE=\"%s\""), *dlg.OldName, *dlg.OldGroup, *dlg.OldPackage, *dlg.NewName, *dlg.NewGroup, *dlg.NewPackage) );
	}

	RefreshAll();		
	unguard;
}


//
// Delete a mesh. Tricky, because several objects can have dependencies on a mesh.
//
void WBrowserAnimation::OnMeshDelete()
{
	if( ! CurrentSkelMesh() )
	{
		appMsgf( 0, TEXT("Please select a mesh first.") );
		return;
	}

	USkeletalMesh* MeshToDelete = (USkeletalMesh*)WorkMesh;

	//
	// Try to unplug all possible animation /mesh / instance linkups.
	//		
	if( CurrentMeshInstance() )
	{
		CurrentMeshInstance()->ClearSkelAnims();
		CurrentMeshInstance()->SkinStream.AnimMesh = NULL;
		CurrentMeshInstance()->SkinStream.MeshInstance = NULL;
	}	
	
	UMeshAnimation* SavedAnim = MeshToDelete->DefaultAnim;
	MeshToDelete->DefaultAnim = NULL;  // Unlink animation object. 
	// Unlink mesh	
	WorkMesh = NULL; 
	MeshActor->Mesh = NULL;
	GUnrealEd->CurrentMesh = NULL;

	// Untangle any and all dependencies on this mesh - there may be instances lying around with both 'OurMesh' and 'SkelAnims' still filled..
	for( TObjectIterator<USkeletalMeshInstance> It ; It ; ++It )
	{
		USkeletalMeshInstance* TestInstance = *It;
		if( TestInstance->OurMesh ) // Not a remnant instance..
		{
			if( TestInstance->SkinStream.AnimMesh == MeshToDelete )
			{
				TestInstance->SkinStream.AnimMesh = NULL;
				// debugf(TEXT("# Found dependent SkinStream AnimMesh linkup to this mesh in instance: [%s] "),TestInstance->GetName());
			}
						
			// Any linkup dependent on this mesh ?
			INT DependentLinkups = 0;
			if( TestInstance->AnimLinkups.Num() )
			{
				for( INT i=0; i<TestInstance->AnimLinkups.Num(); i++)
				{
					if( (TestInstance->AnimLinkups(i).RefMesh == MeshToDelete)
						||
						(TestInstance->AnimLinkups(i).Mesh == MeshToDelete)
						)
					{
						DependentLinkups++;
						//debugf(TEXT("# Found dependent linkup to this mesh in instance: [%s] "),TestInstance->GetName());
					}
				}
			}
			// Kill the instance if the mesh needs deletion.
			if( (TestInstance->GetMesh() == MeshToDelete) )
			{
				//debugf(TEXT("# Found reference to this mesh in instance: [%s] "),TestInstance->GetName());			
				TestInstance->SetStatus( MINST_DeleteMe );
				TestInstance->OurMesh = NULL;				
			}		
			if( DependentLinkups )
			{
				//debugf(TEXT("# Found dependent animation linkup to this mesh in instance: [%s] "),TestInstance->GetName());
				TestInstance->ClearSkelAnims();
			}
		}
	}

	// Actors ?	
	for( TObjectIterator<AActor> It ; It ; ++It )
	{
		AActor* TestActor =  *It;		
		if( TestActor->Mesh == MeshToDelete )
		{
			//debugf(TEXT("## Found reference to this mesh in Actor: [%s] "),TestActor->GetName());
			TestActor->Mesh = NULL;
		}
	}

	// Viewport cleanup (dangling dependency that used to make mesh deletion impossible)
	if( Viewport )
		Viewport->MiscRes = NULL;
	
	FString Name = MeshToDelete->GetPathName(); 
	FStringOutputDevice GetPropResult = FStringOutputDevice();
	TCHAR l_chCmd[512];

	// debugf(TEXT("Trying to delete object: %s"),*Name);
	appSprintf( l_chCmd, TEXT("DELETE CLASS=SKELETALMESH OBJECT=\"%s\""), *Name);
	GUnrealEd->Get( TEXT("Obj"), l_chCmd, GetPropResult);

	if( !GetPropResult.Len() )
	{
		// Ensure an existing mesh is our current mesh... #SKEL
		WorkMesh = Cast<USkeletalMesh>(UObject::StaticFindObject(USkeletalMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));
		OnPackageSelectionChange(); 
		RefreshAll();

#ifdef WITH_LIPSINC
		GBrowserMaster->RefreshAll();
#endif

	}
	else
	{
		// Undo the main 'cleanups'..
		WorkMesh = MeshToDelete;		
		CurrentSkelMesh()->DefaultAnim = SavedAnim;		
		if( MeshActor )
		{			
			MeshActor->Mesh		   = MeshToDelete;			
			CurrentSkelMesh()->MeshGetInstance(MeshActor);
		}
		appMsgf( 0, TEXT("Can't delete mesh: \n\n%s"), *GetPropResult );
	}	
	
}


void WBrowserAnimation::OnAnimRename()
{
	guard(WBrowserAnimation::OnAnimRename);
	if( ! CurrentMeshAnim )
	{
		appMsgf( 0, TEXT("Select an animation first.") );
		return;
	}

	WDlgRename dlg( NULL, this );
	FString Group, Package;
	if( !Cast<UPackage>( CurrentMeshAnim->GetOuter()->GetOuter()) )
	{
		Group = TEXT("");
		Package = CurrentMeshAnim->GetOuter()->GetName();
	}
	else
	{			
		Group = CurrentMeshAnim->GetOuter()->GetName();
		Package = CurrentMeshAnim->GetOuter()->GetOuter()->GetName();
	}					

	if( dlg.DoModal( CurrentMeshAnim->GetName(), Group, Package ) )
	{
		GUnrealEd->Exec(*FString::Printf(TEXT("OBJ RENAME OLDNAME=\"%s\" OLDGROUP=\"%s\" OLDPACKAGE=\"%s\" NEWNAME=\"%s\" NEWGROUP=\"%s\" NEWPACKAGE=\"%s\""), *dlg.OldName, *dlg.OldGroup, *dlg.OldPackage, *dlg.NewName, *dlg.NewGroup, *dlg.NewPackage) );
	}

	RefreshAll();		
	unguard;
}


void WBrowserAnimation::OnAnimDelete()
{
	guard(WBrowserAnimation::OnAnimDelete());
	if( ! CurrentMeshAnim )
	{
		appMsgf( 0, TEXT("Select an animation first.") );
		return;
	}

	// Clean up all references to it.

	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( MInst )
	{
		MInst->ClearSkelAnims();
	}

	UMeshAnimation* SavedAnim = NULL;
	if( CurrentSkelMesh() )		
	{
		if( CurrentSkelMesh()->DefaultAnim == CurrentMeshAnim )
		{
			SavedAnim = CurrentSkelMesh()->DefaultAnim;
			CurrentSkelMesh()->DefaultAnim = NULL;
		}
	}

	// Iterate all other skeletal meshes to see if they have defaultanims linked up to it ? 
	// Yes - but only to warn...

	for( TObjectIterator<USkeletalMesh> It ; It ; ++It )
	{
		USkeletalMesh* TestInstance = *It;
		if( ( TestInstance != CurrentSkelMesh())  &&
		    ( TestInstance->DefaultAnim == CurrentMeshAnim ) )
		{
			appMsgf( 0, TEXT("Cant delete animation set, at least one DefaultAnim reference found, in mesh [%s]"),TestInstance->GetName() );		 
			break;
		}
	}
			
	if ( EditMeshProps )
	{
		if( EditMeshProps->DefaultAnimation == CurrentMeshAnim )
			EditMeshProps->DefaultAnimation = NULL;
	}

	FString Name = CurrentMeshAnim->GetPathName();
	FStringOutputDevice GetPropResult = FStringOutputDevice();
	TCHAR l_chCmd[512];
						
	appSprintf( l_chCmd, TEXT("DELETE CLASS=MESHANIMATION OBJECT=\"%s\""), *Name);
	GUnrealEd->Get( TEXT("Obj"), l_chCmd, GetPropResult);

	if( !GetPropResult.Len() )
	{
		CurrentMeshAnim = NULL;
		RefreshAll();
	}
	else
	{
		// Undo the deletion preparation.
		if( SavedAnim )
			CurrentSkelMesh()->DefaultAnim = SavedAnim;

		appMsgf( 0, TEXT("Can't delete animation set.\n\n%s"), *GetPropResult );
	}
	
	RefreshAll();
	unguard;
}


//
// Copy/paste mesh properties support. Mostly limited to scaling and orientation.
//

void WBrowserAnimation::OnCopyMeshProps()
{
	guard(WBrowserAnimation::OnCopyMeshProps);
	if(  CurrentSkelMesh() )		
	{
		USkeletalMesh* SkelMesh = CurrentSkelMesh();
		bFilledCopiedMesh = true;
		CopiedScale = SkelMesh->Scale;
		CopiedOrigin = SkelMesh->Origin;
		CopiedRotOrigin = SkelMesh->RotOrigin;
		CopiedMinVisBound = SkelMesh->BoundingBox.Min;
		CopiedMaxVisBound = SkelMesh->BoundingBox.Max;			
		CopiedLODStrength     = SkelMesh->LODStrength;		
		CopiedVisSphereRadius = SkelMesh->BoundingSphere.W;
		CopiedSkinTesselationFactor = SkelMesh->SkinTesselationFactor;

		// Copy sockets from mesh.
		CopiedSockets.Empty();
		if( SkelMesh->TagAliases.Num() )
		{			
			for( INT i=0; i<SkelMesh->TagAliases.Num(); i++)
			{
				if( (SkelMesh->TagNames.Num()>i) && (SkelMesh->TagCoords.Num()>i) )
				{
					FAttachSocket NewSocket;
					NewSocket.A_Rotation = SkelMesh->TagCoords(i).OrthoRotation();					
					NewSocket.A_Translation = SkelMesh->TagCoords(i).Origin.TransformVectorBy( SkelMesh->TagCoords(i) );
					NewSocket.AttachAlias = SkelMesh->TagAliases(i);
					NewSocket.BoneName = SkelMesh->TagNames(i);
					NewSocket.TestMesh = NULL;
					NewSocket.TestStaticMesh = NULL;					
					NewSocket.Test_Scale = 0.f;					
					CopiedSockets.AddItem( NewSocket );
				}				
			}
		}

		CopiedCollisionSpheres = SkelMesh->BoneCollisionSpheres;
		CopiedCollisionBoxes = SkelMesh->BoneCollisionBoxes;
	}	

	RefreshViewport(); // Redraw with new data.			

	unguard;
}

void WBrowserAnimation::OnPasteMeshProps()
{
	guard(WBrowserAnimation::OnPasteMeshProps);
	if(  CurrentSkelMesh() && bFilledCopiedMesh )		
	{
		USkeletalMesh* SkelMesh = CurrentSkelMesh();
		SkelMesh->Scale = CopiedScale;
		//SkelMesh->SetScale ??
		SkelMesh->Origin = CopiedOrigin;
		SkelMesh->RotOrigin = CopiedRotOrigin;
		SkelMesh->BoundingBox.Min = CopiedMinVisBound;
		SkelMesh->BoundingBox.Max = CopiedMaxVisBound;			
		SkelMesh->LODStrength = CopiedLODStrength;		
		//SkelMesh->DefaultAnim = CopiedDefaultAnim;
		SkelMesh->BoundingSphere.W = CopiedVisSphereRadius;
		SkelMesh->SkinTesselationFactor = CopiedSkinTesselationFactor;

		// Attachments: only copied onto EMPTY mesh - 		
		if( CopiedSockets.Num() && (SkelMesh->TagAliases.Num()==0) )
		{					
			// Ensure all are empty.
			//SkelMesh->TagAliases.Empty();
			  SkelMesh->TagNames.Empty();
			  SkelMesh->TagCoords.Empty();
			for( INT i=0; i< CopiedSockets.Num(); i++)
			{
				// Parse the adjustment.
				FCoords TagCoords = GMath.UnitCoords / CopiedSockets(i).A_Translation / CopiedSockets(i).A_Rotation;
				//TagCoords = TagCoords.Transpose();
				SkelMesh->SetAttachAlias( CopiedSockets(i).AttachAlias, CopiedSockets(i).BoneName, TagCoords );
			}
		}

		SkelMesh->BoneCollisionSpheres = CopiedCollisionSpheres;
		SkelMesh->BoneCollisionBoxes = CopiedCollisionBoxes;

		//
		RefreshMeshProperties(); // Update them in the property page too.
		SaveMeshProperties();
	}
	else
		appMsgf(0,TEXT("No copied mesh scaling available."));

	RefreshViewport(); // Redraw with new data.			

	unguard;
}


// 
// Import one specific MESH lod (internally, a temporary new mesh.)
//
void WBrowserAnimation::OnImportMeshLOD()
{
	guard(WBrowserAnimation::OnImportMeshLOD);

	if( !CurrentSkelMesh() || CurrentMeshInstance()->ForcedLodModel == 0 )
	{
		appMsgf(0,TEXT("Please select a LOD level to insert mesh into."));
		return;
	}

	// Ensure lazy-loaders are reset and discarded.	
	CurrentSkelMesh()->ResetLoaders( CurrentSkelMesh()->GetOuter(), 0, 1);
	
	// Load new PSK into currently active LOD.
	OPENFILENAMEA ofn;
	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	
	ofn.lpstrFilter = "Skeletal mesh static LOD level data (*.psk)\0*.psk\0All Files\0*.*\0\0";
	ofn.lpstrDefExt = "psk";
	
	ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_PSK]) );								
		
	ofn.lpstrTitle = "Import Mesh as static LOD level";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	if( GetOpenFileNameA(&ofn) )
	{
		INT NumNULLs = FormatFilenames( File );
		TArray<FString> StringArray;
		FString S = appFromAnsi( File );
		S.ParseIntoArray( TEXT("|"), &StringArray );
		INT iStart = 0;
		FString Prefix = TEXT("\0");
		if( NumNULLs )
		{
			iStart = 1;
			Prefix = *(StringArray(0));
			Prefix += TEXT("\\");
		}
		if( StringArray.Num() > 0 )
		{
			if( StringArray.Num() == 1 )
			{
				SavePkgName = *(StringArray(0));
				SavePkgName = SavePkgName.Right( SavePkgName.Len() - (SavePkgName.Left( SavePkgName.InStr(TEXT("\\"), 1)).Len() + 1 ));
			}
			else
				SavePkgName = *(StringArray(1));

			SavePkgName = SavePkgName.Left( SavePkgName.InStr(TEXT(".")) );
		}

		if( StringArray.Num() == 1 )
			GLastDir[eLASTDIR_PSK] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
		else
			GLastDir[eLASTDIR_PSK] = StringArray(0);			

		FString NewPackage = ComboPackage->GetString( ComboPackage->GetCurrent() );			
		FString Group   = TEXT("Default");      
		FString NewObject  = FString(TEXT("Temp1970()ad")); 
	
		// Import chosen file into temporary mesh...
		USkeletalMesh* RawMesh = NULL;

		FLOAT DisplayFactor = 1.0f; // Assumed edited by artist after import.
		
		//GWarn->StatusUpdatef( 0, 0, TEXT("Loading %s"), *(StringArray(0)) );
		// Call EDITOR.DLL with the filename. 
		GUnrealEd->Exec( *FString::Printf(TEXT("NEWANIM IMPORT FILE=\"%s%s\"  PACKAGE=\"%s\"  NAME=\"%s\""), *Prefix, *(StringArray(0)), *NewPackage, *NewObject) );
		//GWarn->EndSlowTask();

		if( NewObject )
		{
			// Find mesh object just imported.
			UPackage* Pkg = UObject::CreatePackage(NULL,*NewPackage);
			RawMesh = Cast<USkeletalMesh>( UObject::StaticFindObject( USkeletalMesh::StaticClass(), Pkg , *NewObject ) );

			// If valid (do check material index count) stuff it into current LOD...
			if( RawMesh )
			{
				GWarn->BeginSlowTask( TEXT("Inserting LOD model.."), 0);
				CurrentSkelMesh()->InsertLodModel( CurrentMeshInstance()->ForcedLodModel-1, RawMesh, DisplayFactor, 1 );
				GWarn->EndSlowTask();
			}
		}

		if( RawMesh )
		{
			delete RawMesh;
		}
		
		RefreshMeshProperties(); // Reflect back new LOD level..
	}
	

	unguard;
}

//
// Helper to check whether a tab is current.
//
UBOOL IsTabCurrent( WPropertySheet* PropSheet, WPropertyPage* TabPage)
{
	guard(IsTabCurrent);
	INT TabCurrentIndex = PropSheet->Tabs->GetCurrent();
	INT TabPageIndex = PropSheet->Pages.FindItemIndex(TabPage);	
	return ( TabCurrentIndex == TabPageIndex);
	unguard;
}

// 
// Redigest the mesh with the parameters as gotten from the LOD factor array in the mesh property editing window....
//
void WBrowserAnimation::OnMeshRedigestLOD()
{
	guard(WBrowserAnimation::OnImportMeshLOD);

	USkeletalMesh* SkelMesh = CurrentSkelMesh();
	if( !SkelMesh )
	{
		appMsgf( 0, TEXT("Please select a skeletal mesh first.") );
		return;
	}

	if( EditMeshProps->LODLevels.Num() == 0 )
	{
		appMsgf( 0, TEXT("Please set parameters for the LOD level(s) first.") );
		return;
	}

		
	// Ensure lazy-loaders are reset and discarded. Otherwise, confusion between some TLazyarrays that still
	// have active loaders and some that have been created without in a TArray of structs containing TLazyarrays
	// can cause crashes.
	SkelMesh->ResetLoaders( SkelMesh->GetOuter(), 0, 1);
	
	GWarn->BeginSlowTask(TEXT(""),1);

	// Generate new ones according to the EditMeshProps->LODLevels(i) data.
	for( INT i=0; i< EditMeshProps->LODLevels.Num(); i++)
	{
		GWarn->StatusUpdatef(i,EditMeshProps->LODLevels.Num(),TEXT("Redigesting LODs"));
		// Delete & replace only if redigestion was requested.
		if( SkelMesh->LODModels.Num() > i && !SkelMesh->LODModels(i).bUniqueSubset )
		{  
			// Existing data at this lod slot will be automatically erased.	
			// Existing data: use the existing factors (as from EditMeshprops, copied at posteditchange already..
			// generate new LOD model 
			SkelMesh->GenerateLodModel( i, 
										EditMeshProps->LODLevels(i).ReductionFactor,
										EditMeshProps->LODLevels(i).DistanceFactor,
										EditMeshProps->LODLevels(i).MaxInfluences,
										1
										);
			// Default or specified Hysteresis
			SkelMesh->LODModels(i).LODHysteresis = EditMeshProps->LODLevels(i).Hysteresis > 0.f ? EditMeshProps->LODLevels(i).Hysteresis : 0.01f;
			
		}	
		else if( SkelMesh->LODModels.Num() <= i && EditMeshProps->LODLevels(i).RedigestSwitch )
		{
			// Not yet present; generate all-new one.
			// LOD level slots are automatically created.
			SkelMesh->GenerateLodModel( i, 
										EditMeshProps->LODLevels(i).ReductionFactor,
										EditMeshProps->LODLevels(i).DistanceFactor,
										EditMeshProps->LODLevels(i).MaxInfluences,
										1
										);
			// Default or specified Hysteresis
			SkelMesh->LODModels(i).LODHysteresis = EditMeshProps->LODLevels(i).Hysteresis > 0.f ? EditMeshProps->LODLevels(i).Hysteresis : 0.01f;

		}

		// Optional rigid part extraction.
		if( EditMeshProps->LODLevels(i).Rigidize.MeshSectionMethod != MSM_SmoothOnly )
		{			 						
			SkelMesh->ExtractRigidParts( 
				i, 
				EditMeshProps->LODLevels(i).Rigidize.MinPartFaces, 
				EditMeshProps->LODLevels(i).Rigidize.MaxRigidParts, 
				EditMeshProps->LODLevels(i).Rigidize.MeshSectionMethod
				);
			//  SectionMethods:
			//	MSM_SmoothOnly,    // Smooth (software transformed) sections only.
			//	MSM_RigidOnly,     // Only draw rigid parts, throw away anything that's not rigid.
			//	MSM_Mixed,         // Convert suitable mesh parts to rigid and draw remaining sections smoothly (software transformation).
			//      MSM_SinglePiece,   // Freeze all as a single static piece just as in the refpose.
			//	MSM_ForcedRigid,   // Convert all faces to rigid parts using relaxed criteria ( entire smooth sections forced rigid).
		}

		if( i<SkelMesh->LODModels.Num()) // if successful set the bUniqueSubset switch, which functions as 'write protect'.
		{
			SkelMesh->LODModels(i).bUniqueSubset = !EditMeshProps->LODLevels(i).RedigestSwitch;
		}

	}

	// Explicitly delete if lod level got deleted from EditMeshProps..
	for( INT i=SkelMesh->LODModels.Num()-1; i>0 ; i--)
	{
		// Reduction factor 0.0f: delete LOD level - if possible.
		if( i >= EditMeshProps->LODLevels.Num() )
			SkelMesh->GenerateLodModel(i, 0.0f, 0.0f, 0, 0); 
	}

	// Ensure we don't try to force display of non-existing LODs.
	CurrentMeshInstance()->ForcedLodModel = Min( CurrentMeshInstance()->ForcedLodModel, SkelMesh->LODModels.Num() );
	CurrentMeshInstance()->CurrentLODLevel = 0;
	
	// Update to represent the new state of LOD levels and reduction parameters.
	RefreshMeshProperties();

	GWarn->EndSlowTask();
	unguard;
}


/*----------------------------------------------------------------
    General animbrowser import/export/interface preferences.
-----------------------------------------------------------------*/


void WBrowserAnimation::RefreshPrefsEditProperties()
{
	guard(WBrowserAnimation::RefreshPrefsEditProperties);

	PrefsPropertyWindow->Root.SetObjects( NULL, 0 );
	
	// Copy current sequence properties.	
	if( EditPrefsProps )
	{
		EditPrefsProps->WeldDuplicateVertices = DoWeldDuplicateVertices;			
		EditPrefsProps->WBrowserAnimationPtr = (INT)(this);
		PrefsPropertyWindow->Root.SetObjects( (UObject**)&EditPrefsProps, 1 );
	}

	unguard;
}

void USkelPrefsEditProps::PostEditChange()
{
	guard(USkelPrefsEditProps::PostEditChange);
	WBrowserAnimation* Browser;
	Browser = (WBrowserAnimation*)WBrowserAnimationPtr;

	// Save any changes made..
	if( Browser->EditPrefsProps )
	{
		Browser->DoWeldDuplicateVertices = Browser->EditPrefsProps->WeldDuplicateVertices;
	}

	unguard;
}
IMPLEMENT_CLASS(USkelPrefsEditProps);

/*-----------------------------------------------------------------------------
	Notify properties.
-----------------------------------------------------------------------------*/

//
//	UNotifyProperties.
//
void UNotifyProperties::PostEditChange()
{
	guard(UNotifyProperties::PostEditChange);

	WBrowserAnimation* Browser = (WBrowserAnimation*)WBrowserAnimationPtr;

	if( OldArrayCount < Notifys.Num() )
		Browser->AddNotify();
	Browser->SaveNotifys();

	unguard;
}
IMPLEMENT_CLASS(UNotifyProperties);


void WBrowserAnimation::RefreshNotifiesList()
{
	guard(WBrowserAnimation::RefreshNotifiesList);
	NotifyPropertyWindow->Root.SetObjects( NULL, 0 );
	if( !CurrentMeshAnim )
		return;

	// recreate the EditNotifyProperties object.
	if( EditNotifyProperties)
		delete EditNotifyProperties;
	// set the outer so new editinline objects have the correct outers.
	EditNotifyProperties = ConstructObject<UNotifyProperties>( UNotifyProperties::StaticClass(), CurrentMeshAnim->GetOuter(), NAME_None, RF_Transient );
	NotifyPropertyWindow->Root.SetObjects( (UObject**)&EditNotifyProperties, 1 );

	if( NotifyPropertyWindow->Root.Children.Num() )
		NotifyPropertyWindow->Root.Children(0)->Collapse();

	EditNotifyProperties->Notifys.Empty();
	EditNotifyProperties->WBrowserAnimationPtr = (INT)(this);
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim )
	{
		for( INT i=0; i<ThisAnim->Notifys.Num();i++)
		{
			FNotifyInfo& Info = EditNotifyProperties->Notifys(EditNotifyProperties->Notifys.AddZeroed());
			Info.Notify = ThisAnim->Notifys(i).NotifyObject;
			Info.NotifyFrame = ThisAnim->Notifys(i).Time * CurrentSeqFrames;
			Info.OldRevisionNum = Info.Notify ? Info.Notify->Revision : 0;
		}
	}

	EditNotifyProperties->OldArrayCount = EditNotifyProperties->Notifys.Num();
	if( EditNotifyProperties->Notifys.Num() )
		Sort( &EditNotifyProperties->Notifys(0), EditNotifyProperties->Notifys.Num() );

	if( NotifyPropertyWindow->Root.Children.Num() )
		NotifyPropertyWindow->Root.Children(0)->Expand();

	unguard;
}

void WBrowserAnimation::AddNotify()
{
	guard(WBrowserAnimation::OnAddNotify);
	for( INT i=0;i<EditNotifyProperties->Notifys.Num();i++ )
	{
		if( EditNotifyProperties->Notifys(i).NotifyFrame == 0.f && 
			EditNotifyProperties->Notifys(i).Notify == NULL )
		{
			EditNotifyProperties->Notifys(i).NotifyFrame = (FLOAT)(appRound(100.f * FrameTime * CurrentSeqFrames))/100.f;
			break;
		}
	}	
	unguard;
}

UBOOL WBrowserAnimation::SaveNotifys()
{
	guard(WBrowserAnimation::SaveNotifys);
	UBOOL Changed=0;
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	INT EditedNotify = -1;
	if( ThisAnim )
	{
		while( ThisAnim->Notifys.Num() < EditNotifyProperties->Notifys.Num() )
			ThisAnim->Notifys.AddZeroed();
		while( ThisAnim->Notifys.Num() > EditNotifyProperties->Notifys.Num() )
			ThisAnim->Notifys.Remove(0);

		// Copy the notifys back.
		for( INT i=0; i<EditNotifyProperties->Notifys.Num();i++)
		{
			if( !Changed &&
				(ThisAnim->Notifys(i).NotifyObject != EditNotifyProperties->Notifys(i).Notify ||
				 ThisAnim->Notifys(i).Time != EditNotifyProperties->Notifys(i).NotifyFrame / CurrentSeqFrames))
				 Changed = 1;
			ThisAnim->Notifys(i).NotifyObject = EditNotifyProperties->Notifys(i).Notify;
			ThisAnim->Notifys(i).Time = EditNotifyProperties->Notifys(i).NotifyFrame / CurrentSeqFrames;
			
			// If the properties of a notify object changed, record it.
			if( ThisAnim->Notifys(i).NotifyObject && 
				EditNotifyProperties->Notifys(i).OldRevisionNum != ThisAnim->Notifys(i).NotifyObject->Revision )
			{
				EditNotifyProperties->Notifys(i).OldRevisionNum = ThisAnim->Notifys(i).NotifyObject->Revision;
				if( EditedNotify == -1 )
					EditedNotify = i;
			}
		}
	}

	if( Changed )
		SetNotifyTicks();

	if( EditedNotify != -1 )
		GotoNotify( EditedNotify );

	return Changed;
	unguard;
}

void WBrowserAnimation::SetNotifyTicks()
{
	guard(WBrowserAnimation::SetNotifyTicks);
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim )
	{
		TArray<INT> NotifyPositions;
		for( INT i=0; i<ThisAnim->Notifys.Num();i++)
			NotifyPositions.AddItem( (FLOAT)SCRUBBARRANGE * ThisAnim->Notifys(i).Time * (FLOAT)CurrentSeqFrames / ((FLOAT)CurrentSeqFrames-1.f) );
		ScrubBar->SetTicks( &NotifyPositions(0), NotifyPositions.Num() );
	}
	else
		ScrubBar->SetTicks( NULL, 0 );	
	unguard;
}


//
// Move the scrub bar to the specified notify and trigger it to show the effect.
//
void WBrowserAnimation::GotoNotify( INT Notify )
{
	guard(WBrowserAnimation::GotoNotify);
	if( !bPlaying )
	{
		FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
		if( ThisAnim && Notify < ThisAnim->Notifys.Num() )
		{
			bNotifyEditing = 1;
			EditingNotifyNum = Notify;
			CleanupLevel();
			FrameTime = ThisAnim->Notifys(Notify).Time;
			RefreshViewport();
		}
	}
	unguard;
}

//
// See if we need to respawn the effect actor for the notify we're editing.
//
void WBrowserAnimation::CheckEditingNotifyActor()
{
	guard(WBrowserAnimation::CheckEditingNotifyActor);

	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim && bNotifyEditing && EditingNotifyNum < ThisAnim->Notifys.Num() )
	{
		UAnimNotify_Effect* Effect = Cast<UAnimNotify_Effect>(ThisAnim->Notifys(EditingNotifyNum).NotifyObject);
		if( Effect )
		{
			// Check the actor list for the last spawned notify object.
			if( Effect->LastSpawnedEffect )
				for( INT i=0;i<AnimBrowserLevel->Actors.Num();i++ )
					if( AnimBrowserLevel->Actors(i) == Effect->LastSpawnedEffect )
						return;

			// Trigger the notify again if the actor has been deleted.
			Effect->Notify( CurrentMeshInstance(), MeshActor );
		}
	}
	unguard;
}

// Clear notifies for current seq.
void WBrowserAnimation::OnClearNotifies()
{
	guard(WBrowserAnimation::OnClearNotifies);
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim )
	{
		EditNotifyProperties->Notifys.Empty();
		EditNotifyProperties->OldArrayCount = 0;
		ThisAnim->Notifys.Empty();			
	}
	bNotifyEditing = 0;
	unguard;
}

// Copy notifies to a temporary list, to match to a new animation repertoire for each sequence by name...
void WBrowserAnimation::OnCopyNotifies()
{		
	//#SKEL - emptying each sub tarray explicitly ?
	for(INT t=0;t<TempNotifies.Num(); t++)
	{
		TempNotifies(t).Notifications.Empty();
	}
	TempNotifies.Empty(); // TArray of TArrays....

	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( MInst )
	{
		// Get anims directly from current mesh.			
		INT NumAnims = MInst->GetAnimCount();

		for( INT anim = 0 ; anim < NumAnims ; ++anim )
		{								
			HMeshAnim hAnim = MInst->GetAnimIndexed(anim);
			//INT NumFrames = MInst->AnimGetFrameCount(hAnim);
			FName AnimFName = MInst->AnimGetName(hAnim);				

			TempNotifies.AddZeroed(1);
			NotifyStorage* ThisNotify = &(TempNotifies(TempNotifies.Num()-1));
			ThisNotify->SequenceName = AnimFName;
			FMeshAnimSeq* ThisAnim = FindAnimSeqByName( AnimFName );	

			if( ThisAnim )
			{					
				for( INT i=0; i<ThisAnim->Notifys.Num();i++)
				{
					FMeshAnimNotify NewNotify;
					NewNotify.Function = ThisAnim->Notifys(i).Function;
					NewNotify.Time     = ThisAnim->Notifys(i).Time;
					NewNotify.NotifyObject = ThisAnim->Notifys(i).NotifyObject;
					ThisNotify->Notifications.AddItem(NewNotify);
				}
			}		
		}	
	}
}

void WBrowserAnimation::OnPasteNotifies()
{
	USkeletalMeshInstance* MInst = CurrentMeshInstance();		
	INT TotalNotifiesAdded = 0;
	INT TotalSeqsChanged = 0;
	if( MInst )
	{
		// Get anims directly from current mesh.			
		INT NumAnims = MInst->GetAnimCount();
		for( INT anim = 0 ; anim < NumAnims ; ++anim )
		{								
			HMeshAnim hAnim = MInst->GetAnimIndexed(anim);
			FName AnimFName = MInst->AnimGetName(hAnim);				
			
			NotifyStorage* ThisNotify = NULL;
			// See if we can find the appropriate notify..
			for(INT i=0; i<TempNotifies.Num();i++)
			{
				if( TempNotifies(i).SequenceName == AnimFName )
					ThisNotify =&(TempNotifies(i));
			}

			FMeshAnimSeq* ThisAnim = FindAnimSeqByName( AnimFName );	
			if( ThisAnim && ThisNotify )
			{			
				ThisAnim->Notifys.Empty(); 
				INT OldNotifies = TotalNotifiesAdded;
				for( INT i=0; i<ThisNotify->Notifications.Num();i++)
				{
					FMeshAnimNotify NewNotify;
					NewNotify.Function = ThisNotify->Notifications(i).Function;
					NewNotify.NotifyObject = ThisNotify->Notifications(i).NotifyObject;
					NewNotify.Time     = ThisNotify->Notifications(i).Time;
					ThisAnim->Notifys.AddItem(NewNotify);
					TotalNotifiesAdded++;
				}
				if(TotalNotifiesAdded != OldNotifies)
					TotalSeqsChanged++;
			}		
		}	
	}
	RefreshNotifiesList();
	if( TotalNotifiesAdded )
		appMsgf(0,TEXT(" [%i] total new notifies assigned to %i animations."),TotalNotifiesAdded, TotalSeqsChanged );
}


/*-----------------------------------------------------------------------------
	Sequence properties.
-----------------------------------------------------------------------------*/
//
// USequEditProps
//
void USequEditProps::PostEditChange()
{
	guard(USequEditProps::PostEditChange);
	WBrowserAnimation* Browser = (WBrowserAnimation*)WBrowserAnimationPtr;
	Browser->SaveSequenceProperties();
	unguard;		
}
IMPLEMENT_CLASS(USequEditProps);

void WBrowserAnimation::RefreshSequenceProperties()
{
	guard(WBrowserAnimation::RefreshSequenceProperties);

	SeqPropertyWindow->Root.SetObjects( NULL, 0 );
	
	// Copy current sequence properties.
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim )
	{
		EditSequProps->Rate = ThisAnim->Rate;
		EditSequProps->SequenceName = ThisAnim->Name;

		EditSequProps->Groups.Empty();
		for( INT i=0;i<ThisAnim->Groups.Num();i++ )
			EditSequProps->Groups.AddItem( ThisAnim->Groups(i) );
	}

	EditSequProps->Rotation = FRotator(0,0,0);
	EditSequProps->Translation = FVector(0,0,0);

	EditSequProps->WBrowserAnimationPtr = (INT)(this);
	SeqPropertyWindow->Root.SetObjects( (UObject**)&EditSequProps, 1 );

	unguard;
}

void WBrowserAnimation::SaveSequenceProperties()
{
	guard(WBrowserAnimation::SaveSequenceProperties);
	FMeshAnimSeq* ThisAnim = FindAnimSeqByName( CurrentSequence );	
	if( ThisAnim )
	{
		ThisAnim->Rate = EditSequProps->Rate;

		// update groups
		while( ThisAnim->Groups.Num() > EditSequProps->Groups.Num() )
			ThisAnim->Groups.Remove(0);

		while( ThisAnim->Groups.Num() < EditSequProps->Groups.Num() )
			ThisAnim->Groups.AddZeroed();
		
		for( INT i=0;i<ThisAnim->Groups.Num();i++ )
			ThisAnim->Groups(i) = EditSequProps->Groups(i);

		// rename sequence
		if( EditSequProps->SequenceName != ThisAnim->Name )
		{
			if( EditSequProps->SequenceName==NAME_None )
			{
				EditSequProps->SequenceName = ThisAnim->Name;
			}
			else
			{
				ThisAnim->Name = EditSequProps->SequenceName;
				RefreshAnimSeqList( ThisAnim->Name );
			}
		}

		// Apply rotation/translation, if specified.
		// #SKEL - add an undo-function- but that can't be reliable without saving every key in a sequence ??
		if( ! EditSequProps->Rotation.IsZero() || (EditSequProps->Translation != FVector(0,0,0)) )
		{
			FCoords SeqAdjustCoords;
			SeqAdjustCoords = GMath.UnitCoords / EditSequProps->Rotation;
			SeqAdjustCoords.Origin = EditSequProps->Translation; 

			UMeshAnimation* AnimObject = CurrentMeshInstance()->FindAnimObjectForSequence( CurrentSequence );
			if( AnimObject )
			{			
				AnimObject->AdjustMovement( CurrentSequence, SeqAdjustCoords);
				// Zero immediately to prevent accidental re-application.
				EditSequProps->Rotation = FRotator(0,0,0);
				EditSequProps->Translation = FVector(0,0,0);
			}
		}

	}
	unguard;
}

// Copy Groups to a temporary list, to match to a new animation repertoire for each sequence by name...
void WBrowserAnimation::OnCopyGroups()
{		
	//#SKEL - emptying each sub tarray explicitly ?
	for(INT t=0;t<TempGroups.Num(); t++)
	{
		TempGroups(t).Groups.Empty();
	}
	TempGroups.Empty(); // TArray of TArrays....

	USkeletalMeshInstance* MInst = CurrentMeshInstance();
	if( MInst )
	{
		// Get anims directly from current mesh.			
		INT NumAnims = MInst->GetAnimCount();

		for( INT anim = 0 ; anim < NumAnims ; ++anim )
		{								
			HMeshAnim hAnim = MInst->GetAnimIndexed(anim);
			//INT NumFrames = MInst->AnimGetFrameCount(hAnim);
			FName AnimFName = MInst->AnimGetName(hAnim);				

			TempGroups.AddZeroed(1);
			GroupStorage* ThisSeq = &(TempGroups(TempGroups.Num()-1));
			ThisSeq->SequenceName = AnimFName;

			FMeshAnimSeq* ThisAnim = FindAnimSeqByName( AnimFName );	

			if( ThisAnim )
			{					
				for( INT i=0; i<ThisAnim->Groups.Num();i++)
				{
					FName NewGroup =ThisAnim->Groups(i);
					ThisSeq->Groups.AddItem(NewGroup);
				}
			}		
		}	
	}
}

void WBrowserAnimation::OnPasteGroups()
{
	USkeletalMeshInstance* MInst = CurrentMeshInstance();		
	INT TotalGroupsAdded = 0;
	INT TotalSeqsChanged = 0;
	if( MInst )
	{
		// Get anims directly from current mesh.			
		INT NumAnims = MInst->GetAnimCount();
		for( INT anim = 0 ; anim < NumAnims ; ++anim )
		{								
			HMeshAnim hAnim = MInst->GetAnimIndexed(anim);
			FName AnimFName = MInst->AnimGetName(hAnim);				
			
			GroupStorage* ThisSeq = NULL;
			// See if we can find the appropriate notify..
			for(INT i=0; i<TempGroups.Num();i++)
			{
				if( TempGroups(i).SequenceName == AnimFName )
					ThisSeq =&(TempGroups(i));
			}

			FMeshAnimSeq* ThisAnim = FindAnimSeqByName( AnimFName );	

			if( ThisAnim && ThisSeq )
			{					
				INT OldGroups = TotalGroupsAdded;
				for( INT i=0; i<ThisSeq->Groups.Num();i++)
				{
					FName NewGroup = ThisSeq->Groups(i);
					ThisAnim->Groups.AddUniqueItem(NewGroup);
					TotalGroupsAdded++;
				}
				if(TotalGroupsAdded != OldGroups)
					TotalSeqsChanged++;
			}		
		}	
	}

	RefreshSequenceProperties();

	if( TotalGroupsAdded )
		appMsgf(0,TEXT(" [%i] total groups assigned to %i animations "),TotalGroupsAdded, TotalSeqsChanged );
}

/*-----------------------------------------------------------------------------
	Mesh property editing
-----------------------------------------------------------------------------*/

// PostEdit callback
void UMeshEditProps::PostEditChange()
{
	guard(UMeshEditProps::PostEditChange);
	WBrowserAnimation* Browser = (WBrowserAnimation*)WBrowserAnimationPtr;
	Browser->SaveMeshProperties();
	unguard;		
}
IMPLEMENT_CLASS(UMeshEditProps);


// Initialize mesh edit properties window from mesh.
void WBrowserAnimation::RefreshMeshProperties()
{
	guard(WBrowserAnimation::RefreshMeshProperties);
	USkeletalMesh* SkelMesh = CurrentSkelMesh();
	if( SkelMesh )
	{	
		MeshPropertyWindow->Root.SetObjects( NULL, 0 );

		// Fill in the current params.

		// Scale.
		EditMeshProps->Scale = SkelMesh->Scale;

		// LOD 
		EditMeshProps->LOD_Strength = SkelMesh->LODStrength;
		EditMeshProps->SkinTesselationFactor = SkelMesh->SkinTesselationFactor;

		// Impostor
		EditMeshProps->bImpostorPresent = SkelMesh->bImpostorPresent;
		EditMeshProps->SpriteMaterial   = SkelMesh->ImpostorProps.Material;
		EditMeshProps->Scale3D		    = SkelMesh->ImpostorProps.Scale3D;
		EditMeshProps->RelativeRotation = SkelMesh->ImpostorProps.RelativeRotation;
		EditMeshProps->RelativeLocation = SkelMesh->ImpostorProps.RelativeLocation;
		EditMeshProps->ImpColor     = SkelMesh->ImpostorProps.ImpColor;
		EditMeshProps->ImpSpaceMode = SkelMesh->ImpostorProps.ImpSpaceMode;
		EditMeshProps->ImpDrawMode  = SkelMesh->ImpostorProps.ImpDrawMode;
		EditMeshProps->ImpLightMode = SkelMesh->ImpostorProps.ImpLightMode;		
		
		// Bounds.
		EditMeshProps->MinVisBound =SkelMesh->BoundingBox.Min;
		EditMeshProps->MaxVisBound =SkelMesh->BoundingBox.Max;

		EditMeshProps->VisSphereCenter = (FVector) SkelMesh->BoundingSphere;
		EditMeshProps->VisSphereRadius = SkelMesh->BoundingSphere.W;				

		// Rotation - #SKEL: would like intuitive numbers, but yaw/pitch/roll is always integer so any conversion has rounding trouble..
		EditMeshProps->Rotation.Yaw =    SkelMesh->RotOrigin.Yaw;    // 360.f*(1/65535.f) * ((SkelMesh->RotOrigin.Yaw  ) & 65535);
		EditMeshProps->Rotation.Pitch =  SkelMesh->RotOrigin.Pitch;  // 360.f*(1/65535.f) * ((SkelMesh->RotOrigin.Pitch) & 65535);
		EditMeshProps->Rotation.Roll =   SkelMesh->RotOrigin.Roll;   // 360.f*(1/65535.f) * ((SkelMesh->RotOrigin.Roll ) & 65535);

		// Translation
		EditMeshProps->Translation = SkelMesh->Origin;		

		// Defaultanim 
		EditMeshProps->DefaultAnimation = SkelMesh->DefaultAnim;
		
		// Materials.
		EditMeshProps->Material.Empty();
		EditMeshProps->Material.Add(SkelMesh->Materials.Num());
		for( INT m=0; m<SkelMesh->Materials.Num(); m++)
			 EditMeshProps->Material(m) = SkelMesh->Materials(m);

		// LOD level characteristics.
		EditMeshProps->LODLevels.Empty();
		EditMeshProps->LODLevels.Add(SkelMesh->LODModels.Num());

		SkelMesh->RawVerts.Load();
		for( INT i=0; i<SkelMesh->LODModels.Num(); i++)
		{
			
			EditMeshProps->LODLevels(i).DistanceFactor = SkelMesh->LODModels(i).DisplayFactor;
			EditMeshProps->LODLevels(i).MaxInfluences  = SkelMesh->LODModels(i).MaxInfluences;
			EditMeshProps->LODLevels(i).Hysteresis     = SkelMesh->LODModels(i).LODHysteresis;
			// Only redigest the non-imported auto-generated ones by default. 
			EditMeshProps->LODLevels(i).RedigestSwitch = !SkelMesh->LODModels(i).bUniqueSubset;
			
			// Set appropriate default values for rigid parts... we don't know with wich parameters a LOD model
			// was created earlier, but if all rigid, reset it to produce all rigid, etc.
			INT MeshPartsMethod = MSM_SmoothOnly;
			if( SkelMesh->LODModels(i).RigidSections.Num() ) 
			{
				MeshPartsMethod = MSM_RigidOnly;
				if( SkelMesh->LODModels(i).SmoothSections.Num() ) 
					MeshPartsMethod = MSM_Mixed;
				else
				if( SkelMesh->LODModels(i).RigidSections.Num() == 1 )
					MeshPartsMethod = MSM_SinglePiece;
			}

			EditMeshProps->LODLevels(i).Rigidize.MeshSectionMethod = MeshPartsMethod;
			EditMeshProps->LODLevels(i).Rigidize.MinPartFaces = 32; // Arbitrary number below which software skinnin probably is faster than a matrix setup and a d3d call.
			EditMeshProps->LODLevels(i).Rigidize.MeldSize = 1.0f;   // Reserved.
			EditMeshProps->LODLevels(i).Rigidize.MaxRigidParts = Max( SkelMesh->LODModels(i).RigidSections.Num(), 24 );

			// Approximate LOD auto-generation vertex-reduction factor from its actual size. 
			SkelMesh->LODModels(i).Points.Load();
			INT ThisModelTotalVerts = SkelMesh->LODModels(i).Points.Num();

			EditMeshProps->LODLevels(i).ReductionFactor = Min( 1.0f, 
				( SkelMesh->RawVerts.Num() && !SkelMesh->LODModels(i).bUniqueSubset ) ? 
				(FLOAT)ThisModelTotalVerts / (FLOAT)SkelMesh->RawVerts.Num() : 1.0f );
		}

		// Attachments
		EditMeshProps->ContinuousUpdate = false;
		EditMeshProps->ApplyNewSockets = false;
		EditMeshProps->Sockets.Empty();
		if( SkelMesh->TagAliases.Num() )
		{			
			for( INT i=0; i<SkelMesh->TagAliases.Num(); i++)
			{
				if( (SkelMesh->TagNames.Num()>i) && (SkelMesh->TagCoords.Num()>i) )
				{
					FAttachSocket NewSocket;
					NewSocket.A_Rotation = SkelMesh->TagCoords(i).OrthoRotation();					
					NewSocket.A_Translation = SkelMesh->TagCoords(i).Origin.TransformVectorBy( SkelMesh->TagCoords(i) );
					NewSocket.AttachAlias = SkelMesh->TagAliases(i);
					NewSocket.BoneName = SkelMesh->TagNames(i);
					NewSocket.TestMesh = NULL;
					NewSocket.TestStaticMesh = NULL;					
					NewSocket.Test_Scale = 0.f;					
					EditMeshProps->Sockets.AddItem( NewSocket );
				}				
			}
		}
				
		// Collision.
		EditMeshProps->CollisionSpheres.Empty();
		for( INT i=0; i<SkelMesh->BoneCollisionSpheres.Num() ;i++ )
		{
			int ex = EditMeshProps->CollisionSpheres.AddZeroed();
			FMEPBonePrimSphere* s = &EditMeshProps->CollisionSpheres(ex);

			s->BoneName = SkelMesh->BoneCollisionSpheres(i).BoneName;
			s->offset = SkelMesh->BoneCollisionSpheres(i).Offset;
			s->Radius = SkelMesh->BoneCollisionSpheres(i).Radius;			
			s->bBlockKarma = SkelMesh->BoneCollisionSpheres(i).bBlockKarma;
			s->bBlockNonZeroExtent = SkelMesh->BoneCollisionSpheres(i).bBlockNonZeroExtent;
			s->bBlockZeroExtent = SkelMesh->BoneCollisionSpheres(i).bBlockZeroExtent;
		}

		EditMeshProps->CollisionBoxes.Empty();
		for( INT i=0; i<SkelMesh->BoneCollisionBoxes.Num() ;i++ )
		{
			int ex = EditMeshProps->CollisionBoxes.AddZeroed();
			FMEPBonePrimBox* b = &EditMeshProps->CollisionBoxes(ex);

			b->BoneName = SkelMesh->BoneCollisionBoxes(i).BoneName;
			b->offset = SkelMesh->BoneCollisionBoxes(i).Offset;
			b->Radii = SkelMesh->BoneCollisionBoxes(i).Radii;			
			b->bBlockKarma = SkelMesh->BoneCollisionBoxes(i).bBlockKarma;
			b->bBlockNonZeroExtent = SkelMesh->BoneCollisionBoxes(i).bBlockNonZeroExtent;
			b->bBlockZeroExtent = SkelMesh->BoneCollisionBoxes(i).bBlockZeroExtent;
		}

		EditMeshProps->CollisionStaticMesh = SkelMesh->CollisionStaticMesh;

		// Add 'raw' 'max/maya' names into slots for refrence.. - not available any more ?? -> need special trick to store on import... #SKEL
		/*
		for( m=0; m< Min(10,SkelMesh->Textures.Num()); m++)
		{
			 *(((UTexture**)(&EditMeshProps->OrigMat0))+m) = SkelMesh->Textures(m);  
		}
		*/		
		
		EditMeshProps->WBrowserAnimationPtr = (INT)(this);
		MeshPropertyWindow->Root.SetObjects( (UObject**)&EditMeshProps, 1 );
	}
	unguard;
}

#ifdef WITH_KARMA
extern void KUpdateSkeletalPhysicsProps(USkeletalMesh* smesh);
#endif

#include "GeomFitUtils.h"
static void UpdateSkelBoxModels(USkeletalMesh* smesh)
{
	// Destroy any existing models.
	if(smesh->BoneCollisionBoxModels.Num() > 0)
	{
		for(INT i=0; i<smesh->BoneCollisionBoxModels.Num(); i++)
			delete smesh->BoneCollisionBoxModels(i);

		smesh->BoneCollisionBoxModels.Empty();
	}

	// Do nothing if scale is non-uniform.
	if( !smesh->Scale.IsUniform() )
		return;

	// Create a UModel from each FSkelBoneBox. 
	// Each is relative to the bone origin, and takes into account ULodMesh::Scale.
	for(INT i=0; i<smesh->BoneCollisionBoxes.Num(); i++)
	{
		FSkelBoneBox* b = &smesh->BoneCollisionBoxes(i);

		FCoords relBoneC(-1.0 * b->Offset * smesh->Scale); // Transformation from bone to primitive

		FPlane planes[6];
		planes[0] = FPlane( KDopDir6[0], b->Radii.X * smesh->Scale.X ).TransformBy( relBoneC );
		planes[1] = FPlane( KDopDir6[1], b->Radii.X * smesh->Scale.X ).TransformBy( relBoneC );
		planes[2] = FPlane( KDopDir6[2], b->Radii.Y * smesh->Scale.X ).TransformBy( relBoneC );
		planes[3] = FPlane( KDopDir6[3], b->Radii.Y * smesh->Scale.X ).TransformBy( relBoneC );
		planes[4] = FPlane( KDopDir6[4], b->Radii.Z * smesh->Scale.X ).TransformBy( relBoneC );
		planes[5] = FPlane( KDopDir6[5], b->Radii.Z * smesh->Scale.X ).TransformBy( relBoneC );

		UModel* newModel = new(smesh) UModel(NULL,1);
		
		for(INT i=0; i<6; i++)
		{
			FPoly*	Polygon = new(newModel->Polys->Element) FPoly();
			FVector Base, AxisX, AxisY;

			Polygon->Init();
			Polygon->Normal = planes[i];
			Polygon->NumVertices = 4;
			Polygon->Normal.FindBestAxisVectors(AxisX, AxisY);

			Base = planes[i] * planes[i].W;

			Polygon->Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
			Polygon->Vertex[1] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
			Polygon->Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
			Polygon->Vertex[3] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;

			for(INT j=0; j<6; j++)
			{
				if(i != j)
				{
					if(!Polygon->Split(-FVector(planes[j]), planes[j] * planes[j].W))
					{
						Polygon->NumVertices = 0;
						break;
					}
				}
			}

			if(Polygon->NumVertices < 3)
			{
				// If poly resulted in no verts, remove from array
				newModel->Polys->Element.Remove( newModel->Polys->Element.Num()-1 );
			}
			else
			{
				// Other stuff...
				Polygon->iLink = i;
				Polygon->CalcNormal(1);
			}
		}

		// Build bounding box.
		newModel->BuildBound();

		// Build BSP for the brush.
		GEditor->bspBuild(newModel, BSP_Good, 15, 70, 1, 0);
		GEditor->bspRefresh(newModel,1);
		GEditor->bspBuildBounds(newModel);

		// Add to models array
		smesh->BoneCollisionBoxModels.AddItem(newModel);
	}
}

void WBrowserAnimation::SaveMeshProperties()
{
	guard(WBrowserAnimation::SaveMeshProperties);

	USkeletalMesh* SkelMesh = CurrentSkelMesh();

	if( SkelMesh )
	{		
		// Apply materials.
		while( SkelMesh->Materials.Num() > EditMeshProps->Material.Num() )
			SkelMesh->Materials.Remove(0);

		while( SkelMesh->Materials.Num() < EditMeshProps->Material.Num() )
			SkelMesh->Materials.AddZeroed();
		
		for( INT m=0;m<EditMeshProps->Material.Num();m++ )
			SkelMesh->Materials(m) = EditMeshProps->Material(m);
		
		// Apply bounding box.
		SkelMesh->BoundingBox.Min = EditMeshProps->MinVisBound;
		SkelMesh->BoundingBox.Max = EditMeshProps->MaxVisBound;

		// Show bounding sphere ? 
		SkelMesh->BoundingSphere.X = EditMeshProps->VisSphereCenter.X;
		SkelMesh->BoundingSphere.Y = EditMeshProps->VisSphereCenter.Y;
		SkelMesh->BoundingSphere.Z = EditMeshProps->VisSphereCenter.Z;
		SkelMesh->BoundingSphere.W = EditMeshProps->VisSphereRadius;

		// Apply defaultanim if valid AND the current one is unlinked.
		if( ( EditMeshProps->DefaultAnimation != NULL ) && ( SkelMesh->DefaultAnim == NULL ) )
			SkelMesh->DefaultAnim = EditMeshProps->DefaultAnimation;
		
		// Apply impostor data.
		SkelMesh->bImpostorPresent=EditMeshProps->bImpostorPresent;
		SkelMesh->ImpostorProps.Material=EditMeshProps->SpriteMaterial;
		SkelMesh->ImpostorProps.Scale3D = EditMeshProps->Scale3D;
		SkelMesh->ImpostorProps.RelativeRotation=EditMeshProps->RelativeRotation;
		SkelMesh->ImpostorProps.RelativeLocation=EditMeshProps->RelativeLocation;
		SkelMesh->ImpostorProps.ImpColor = EditMeshProps->ImpColor;
		SkelMesh->ImpostorProps.ImpSpaceMode=EditMeshProps->ImpSpaceMode;
		SkelMesh->ImpostorProps.ImpDrawMode =EditMeshProps->ImpDrawMode;
		SkelMesh->ImpostorProps.ImpLightMode=EditMeshProps->ImpLightMode;
		
		
		// Rotation - note: artists like intuitive numbers, BUT yaw/pitch/roll are always integer, so 
		// conversion brings subtle rounding trouble causing serious inconsistensies after converting back and forth.
		SkelMesh->RotOrigin.Yaw   = EditMeshProps->Rotation.Yaw;   //65535 & ((INT)( EditMeshProps->Rotation.Yaw    * ( 65535.f / 360.f)));
		SkelMesh->RotOrigin.Pitch = EditMeshProps->Rotation.Pitch; //65535 & ((INT)( EditMeshProps->Rotation.Pitch  * ( 65535.f / 360.f)));
		SkelMesh->RotOrigin.Roll  = EditMeshProps->Rotation.Roll;  //65535 & ((INT)( EditMeshProps->Rotation.Roll   * ( 65535.f / 360.f)));

		// Translation
		SkelMesh->Origin = EditMeshProps->Translation;			

		// LOD
		SkelMesh->LODStrength = EditMeshProps->LOD_Strength;
		SkelMesh->SkinTesselationFactor = EditMeshProps->SkinTesselationFactor;

		// Apply scale.
		CurrentMeshInstance()->SetScale( EditMeshProps->Scale ); //#SKEL  Directly set instead ?

		// Temporary Test collision radius display..
		TempCollisionRadius = EditMeshProps->TestCollisionRadius;
		TempCollisionHeight = EditMeshProps->TestCollisionHeight;

		// LOD models data: only retrieved when redigest button is pressed; BUT: display-parameters 
		// that rdon't influence the LOD generation are always refreshed here:
		for( INT i=0;i< EditMeshProps->LODLevels.Num(); i++)
		{
			if( i< SkelMesh->LODModels.Num() )
			{
				SkelMesh->LODModels(i).DisplayFactor = EditMeshProps->LODLevels(i).DistanceFactor;
				SkelMesh->LODModels(i).LODHysteresis = EditMeshProps->LODLevels(i).Hysteresis;
			}
		}
		
		//
		// Possibly apply any required material order swizzle - if any of the order nums is > 0...? #SKEL
		//

		// Apply back any changes made to the attachment aliases.
		if( EditMeshProps->ApplyNewSockets || EditMeshProps->ContinuousUpdate )
		{		
			EditMeshProps->ApplyNewSockets = false;
			// Clean first, then reapply the EditMeshProps' tags from scratch again ?
			SkelMesh->TagAliases.Empty();
			SkelMesh->TagNames.Empty();
			SkelMesh->TagCoords.Empty();

			for( INT i=0; i< EditMeshProps->Sockets.Num(); i++)
			{
				// Parse the adjustment
				FCoords TagCoords = GMath.UnitCoords / EditMeshProps->Sockets(i).A_Translation / EditMeshProps->Sockets(i).A_Rotation;
                //TagCoords = TagCoords.Transpose();
				SkelMesh->SetAttachAlias( EditMeshProps->Sockets(i).AttachAlias, EditMeshProps->Sockets(i).BoneName, TagCoords );
			}
		}

		// Collision primitives

		// First empty current spheres array in skeletal mesh
		SkelMesh->BoneCollisionSpheres.Empty();
		SkelMesh->BoneCollisionBoxes.Empty();

		// Only allow collision prims if we have a uniform draw scale.
		if( EditMeshProps->Scale.IsUniform() )
		{
			// // SPHERE // //

			// Then iterate over spheres array in edit mesh props
			for( INT i=0; i< EditMeshProps->CollisionSpheres.Num(); i++)
			{
				// Check the bone name exists. OTherwise, dont bother adding it.
				INT BoneIx = SkelMesh->MatchRefBone(*(EditMeshProps->CollisionSpheres(i).BoneName));
				if(BoneIx == INDEX_NONE)
				{
					debugf(TEXT("Unknown Bone: %s"), *(EditMeshProps->CollisionSpheres(i).BoneName));
					continue;
				}

				// Dont use if radius is too small
				if( EditMeshProps->CollisionSpheres(i).Radius < 0.001f )
					continue;

				int ex = SkelMesh->BoneCollisionSpheres.AddZeroed();
				FSkelBoneSphere* s = &SkelMesh->BoneCollisionSpheres(ex);

				s->BoneName = EditMeshProps->CollisionSpheres(i).BoneName;
				s->Offset = EditMeshProps->CollisionSpheres(i).offset;
				s->Radius = EditMeshProps->CollisionSpheres(i).Radius;
				s->bBlockKarma = EditMeshProps->CollisionSpheres(i).bBlockKarma;
				s->bBlockNonZeroExtent = EditMeshProps->CollisionSpheres(i).bBlockNonZeroExtent;
				s->bBlockZeroExtent = EditMeshProps->CollisionSpheres(i).bBlockZeroExtent;
			}

			// // BOX // //

			// Do the same for boxes
			for( INT i=0; i< EditMeshProps->CollisionBoxes.Num(); i++)
			{
				INT BoneIx = SkelMesh->MatchRefBone(*(EditMeshProps->CollisionBoxes(i).BoneName));
				if(BoneIx == INDEX_NONE)
				{
					debugf(TEXT("Unknown Bone: %s"), *(EditMeshProps->CollisionBoxes(i).BoneName));
					continue;
				}

				if( EditMeshProps->CollisionBoxes(i).Radii.GetMin() < 0.001f  )
					continue;

				int ex = SkelMesh->BoneCollisionBoxes.AddZeroed();
				FSkelBoneBox* b = &SkelMesh->BoneCollisionBoxes(ex);

				b->BoneName = EditMeshProps->CollisionBoxes(i).BoneName;
				b->Offset = EditMeshProps->CollisionBoxes(i).offset;
				b->Radii = EditMeshProps->CollisionBoxes(i).Radii;
				b->bBlockKarma = EditMeshProps->CollisionBoxes(i).bBlockKarma;
				b->bBlockNonZeroExtent = EditMeshProps->CollisionBoxes(i).bBlockNonZeroExtent;
				b->bBlockZeroExtent = EditMeshProps->CollisionBoxes(i).bBlockZeroExtent;
			}

			// Update model array to keep in sync with boxes array
			UpdateSkelBoxModels(SkelMesh);
		}

		SkelMesh->CollisionStaticMesh = EditMeshProps->CollisionStaticMesh;

#ifdef WITH_KARMA
		// Update the KPhysicsProps from the skeletal collision data
		KUpdateSkeletalPhysicsProps(SkelMesh);
#endif

		// Refresh the viewport in case any mesh display properties were changed.
		RefreshViewport();
	}
	unguard;
}


/*------------------------------------------------------------------------------
    Animation sequence properties
------------------------------------------------------------------------------*/

void UAnimEditProps::PostEditChange()
{
	guard(UAnimEditProps::PostEditChange);
	WBrowserAnimation* Browser = (WBrowserAnimation*)WBrowserAnimationPtr;
	Browser->SaveAnimationProperties();
	unguard;		
}
IMPLEMENT_CLASS(UAnimEditProps);


//
// 	Animation object (not per-sequence ) editing support.
//
void WBrowserAnimation::RefreshAnimProperties()
{
	guard(WBrowserAnimation::RefreshAnimProperties);
	if( CurrentMeshAnim && EditAnimProps )
	{			
		AnimPropertyWindow->Root.SetObjects( NULL, 0 );
		CurrentMeshAnim->InitForDigestion();// Ensure DigestHelper presence.
		EditAnimProps->GlobalCompression = CurrentMeshAnim->DigestHelper->CompFactor;
		EditAnimProps->WBrowserAnimationPtr = (INT)(this);
		AnimPropertyWindow->Root.SetObjects( (UObject**)&EditAnimProps, 1 );
	}
	unguard;
}

void WBrowserAnimation::SaveAnimationProperties()
{
	guard(WBrowserAnimation::SaveAnimationProperties);
	if( CurrentMeshAnim && EditAnimProps )
	{
		CurrentMeshAnim->InitForDigestion(); // Ensure DigestHelper presence.
		CurrentMeshAnim->DigestHelper->CompFactor = EditAnimProps->GlobalCompression;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport rendering.
-----------------------------------------------------------------------------*/
void WBrowserAnimation::Draw( UViewport* Viewport )
{
	guard(Draw_Animation);

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
	
	FLOAT DeltaFrame = 0.f;
	
	if( !bPlaying && !bNotifyEditing ) 
	{
		// User forced a frame with the slider.
		FLOAT Frame = (NumFrames-1.f) * ScrubBar->GetPos() / SCRUBBARRANGE;
		FrameTime = Frame / NumFrames;

		DeltaFrame = FrameTime-OldFrameTime;
		MeshInstance->AnimForcePose( DemoAnimSequence, FrameTime, DeltaFrame, 0 );
	}
	else
	{
		// We're playing or editing a notify's properties.

		// Calculate deltatime
		static INT LastUpdateCycles;
		INT   CurrentCycles = appCycles();
		FLOAT DeltaTime = bPlayJustStarted ? 0.f : (CurrentCycles - LastUpdateCycles)*GSecondsPerCycle;

		// Tick the animation browser level	
		GTicks++;
		AnimBrowserLevel->Tick( LEVELTICK_All, DeltaTime );

		LastUpdateCycles = CurrentCycles;
		bPlayJustStarted = 0;

		// If we're playing, update FrameTime.
		DeltaFrame = 0.f;
		if( bPlaying )
		{
			FLOAT Rate = Seq ? CurrentMeshInstance()->AnimGetRate(Seq) / NumFrames : 1.0;
			DeltaFrame = Clamp<FLOAT>( (FLOAT)Rate*DeltaTime, 0.f, 1.f);
			FrameTime += DeltaFrame;
			if( bDoScrubLoop )
			{
				if( FrameTime >= 1.f )
					CleanupLevel(); // remove effects
				FrameTime -= appFloor(FrameTime); // Loop
			}
			else
			{
				if( FrameTime * NumFrames > NumFrames-1.f )
				{
					FrameTime = (NumFrames-1.f) / NumFrames;
					StopPlay();
				}
			}
		}
		MeshInstance->AnimForcePose( DemoAnimSequence, FrameTime, DeltaFrame, 0 );
		UpdateScrub();

		if( bNotifyEditing )
			CheckEditingNotifyActor();
	}


	// Attach/verify 'weapons' demo-attachments.
	if( CurrentSkelMesh()->TagAliases.Num() && MeshActor )
	{
		// Skeletal or staticmesh test-only-attachments - at specified socket/bone-alias locations.
		// Drawing involves spawning new actors (into the level) and attaching them,
		// every time the attachment (socket) list has been edited; 
		// TODO -  compare the required meshes to the actually attached ones 
		// to avoid slow editor reaction to changes in the mesh properties window..		

		UBOOL UpDateAttachments = false;
		INT   ExampleAttachCount = 0;

		for( INT i=0; i<CurrentSkelMesh()->TagAliases.Num(); i++)
		{
			if( EditMeshProps->Sockets(i).TestMesh )
				ExampleAttachCount++;
			if( EditMeshProps->Sockets(i).TestStaticMesh )
				ExampleAttachCount++;			
		}	
		
	    if(	(MeshActor->Attached.Num() != ExampleAttachCount) ||
			(EditMeshProps->ContinuousUpdate == true ) )		
		{
			UpDateAttachments = true;
		}

		if( UpDateAttachments )
		{
			// Delete all previously spawned & attached actors first.
			for (INT i=0; i<MeshActor->Attached.Num(); i++)
			{				
				if( MeshActor->Attached(i) )
					AnimBrowserLevel->DestroyActor( MeshActor->Attached(i) );
			}
			MeshActor->Attached.Empty();			

			for( INT i=0; i<CurrentSkelMesh()->TagAliases.Num(); i++)
			{
				// Draw if the EditProps->Sockets have anything in the relevant spot.
				if( EditMeshProps->Sockets.Num() > i )
				{					
					AActor* AttachmentActor = NULL;
					if( EditMeshProps->Sockets(i).TestMesh )
					{	
						AttachmentActor = AnimBrowserLevel->SpawnActor( AAnimBrowserMesh::StaticClass() );
						AttachmentActor->Mesh = EditMeshProps->Sockets(i).TestMesh;
						AttachmentActor->SetDrawType(DT_Mesh);
					}
					else if( EditMeshProps->Sockets(i).TestStaticMesh )
					{
						AttachmentActor = AnimBrowserLevel->SpawnActor( AAnimBrowserMesh::StaticClass() );
						AttachmentActor->StaticMesh = EditMeshProps->Sockets(i).TestStaticMesh;
						AttachmentActor->SetDrawType(DT_StaticMesh);
					}
					// else if( EditMeshProps->Sockets(i).TestActor != NULL )
					// {   // TestActor = UClass* ....
					//  	AttachmentActor = AnimBrowserLevel->SpawnActor( EditMeshProps->Sockets(i).TestActor );
					// }

					if( AttachmentActor )
					{					
						AttachmentActor->Location = FVector(0,0,0);
						AttachmentActor->Rotation = FRotator(0,0,0);						
						AttachmentActor->AmbientGlow = 255; 
						AttachmentActor->bUnlit = true;
						AttachmentActor->DrawScale =  (EditMeshProps->Sockets(i).Test_Scale != 0.0f) ? EditMeshProps->Sockets(i).Test_Scale : 1.0f;
						// Attach to either raw bone name or to alias, if given.
						if( EditMeshProps->Sockets(i).AttachAlias != NAME_None )
							MeshActor->AttachToBone(AttachmentActor, EditMeshProps->Sockets(i).AttachAlias);
						else if( EditMeshProps->Sockets(i).BoneName != NAME_None )
							MeshActor->AttachToBone(AttachmentActor, EditMeshProps->Sockets(i).BoneName);
					}				
				}
			}		
		}		
	}

	// Draw the level.
	MeshActor->ClearRenderData(); // Ensure vis/light bounding spheres etc are updated.
	SceneNode.Render(Viewport->RI);

	// Draw the axis indicator.
	if( GUnrealEd->UseAxisIndicator )
		GUnrealEd->edDrawAxisIndicator(&SceneNode);

	// Print the name of the static mesh at the top of the viewport.
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
	//FString Text = Mesh ? Mesh->GetPathName() : TEXT("No Animating Mesh");

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

	// Print out bone names.
	if( MeshInstance->bPrintBoneNames && Viewport->bShowBones ) 
	{
		FCanvasUtil	CanvasUtil( &Viewport->RenderTarget,Viewport->RI);
		UCanvas*	Canvas = Viewport->Canvas;
		
		USkeletalMesh* SkelMesh = (USkeletalMesh*)Mesh;
		
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
				Canvas->WrappedPrintf(Canvas->SmallFont, 0, *BoneString );
			}
		}
	}

	// Draw the test-only collision cylinder if nonzero.
	if( (TempCollisionRadius != 0.f) && (TempCollisionHeight != 0.f) )
	{
		FLineBatcher LineBatcher(Viewport->RI);
        Viewport->RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
		FColor Color(245,63,93,0);		
		LineBatcher.DrawCylinder(Viewport->RI,FVector(0,0,0),FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), Color, TempCollisionRadius, TempCollisionHeight, 16 );	
	}

	// If any actors share the same mesh as our current mesh, have their mesh-instances forced to the same pose.	
	if( bLevelAnim )
	{
		if( bRefpose )
		{
			ResetLevelMeshes();
		}
		else
		{		
			// Force same frame as is active in animbrowser for all valid identical-mesh-toting in-level actors..
			for( INT i=0; i<TempForcedActorList.Num(); i++)
			{
				AActor* SyncedActor = TempForcedActorList(i);
				if( !SyncedActor->bDeleteMe && SyncedActor->Mesh == CurrentSkelMesh() )
				{
					USkeletalMeshInstance* MInst = (USkeletalMeshInstance*)SyncedActor->Mesh->MeshGetInstance(SyncedActor);
					MInst->AnimForcePose( DemoAnimSequence, FrameTime, DeltaFrame, 0 );
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

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AAnimBrowserMesh);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

