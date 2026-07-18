/*=============================================================================
	BrowserAnimation : Browser window for animations
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	WBrowserAnimation.
-----------------------------------------------------------------------------*/

// Temp notify storage structure for cut/paste across animation objects.
struct NotifyStorage
{
	FName   SequenceName;
	TArray<FMeshAnimNotify> Notifications;
};

struct GroupStorage
{
	FName SequenceName;
	TArray<FName> Groups;
};

struct ToolTipStrings
{
	TCHAR ToolTip[64];
	INT ID;
};

class WBrowserAnimation : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserAnimation,WBrowser,Window);

	HWND hWndToolBar; // Main animation toolbar.

	FContainer Container;
	TMap<DWORD,FWindowAnchor> Anchors;

	// Combo box selectors at top.
	WComboBox *ComboPackage;
	WComboBox* MeshCombo;
	WComboBox* AnimCombo;

	WLabel*  PackageComboLabel;
	WLabel*  AnimComboLabel;
	WLabel*  MeshComboLabel;

	// Splitter which contains the viewport, scrub bar and properties tab.
	WSplitterContainer* SplitterContainer;

	// Viewport and Viewport Label.
	WLabel* ViewportLabel;
	UViewport *Viewport;
	ULevel* AnimBrowserLevel;
	AActor* MeshActor;
	USkeletalMesh* WorkMesh;

	// Sequence list alongside main browser 3d window.
	WListBox*  AnimSeqList;	

	// Sequence list sort buttons.	
	WCheckBox *ApplySequenceSort;
	WCheckBox *MoveSequenceUp;
	WCheckBox *MoveSequenceDown;	

	INT   SequenceSortMethod;	
	INT   SequenceSortDirection;

	// Individually placed buttons.
	WButton   *ScrubButtonPlay;
	WButton   *ScrubButtonBegin;
	WButton   *ScrubButtonEnd;
	WButton   *ScrubButtonForward;
	WButton   *ScrubButtonBackward;
	WButton   *ScrubButtonLoop;		

	// Button bitmaps.
	HBITMAP ScrubPlayBitmap;
	HBITMAP ScrubPauseBitmap;
	HBITMAP ScrubBeginBitmap;
	HBITMAP ScrubEndBitmap;
	HBITMAP ScrubForwardBitmap;
	HBITMAP ScrubBackwardBitmap;
	HBITMAP ScrubLoopBitmap;
	HBITMAP ScrubNoLoopBitmap;
	
	//
	WToolTip *ToolTipCtrl;
	MRUList* mrulist;
	WTrackBar* ScrubBar;
	WLabel* LeftListLabel; 

	WPropertySheet* PropSheet;
	WPropertyPage*  MeshPage;
	WPropertyPage*  AnimPage;
	WPropertyPage*  SeqPage;
	WPropertyPage*  NotifyPage;
	WPropertyPage*  PrefsPage;

	WObjectProperties* MeshPropertyWindow;
	WObjectProperties* AnimPropertyWindow;
	WObjectProperties* SeqPropertyWindow;
	WObjectProperties* NotifyPropertyWindow;
	WObjectProperties* PrefsPropertyWindow;

	UMeshEditProps *EditMeshProps;
	UAnimEditProps *EditAnimProps;
	USequEditProps *EditSequProps;
	UNotifyProperties *EditNotifyProperties;
	USkelPrefsEditProps *EditPrefsProps;

	UBOOL bPlaying;
	UBOOL bPlayJustStarted;
	UBOOL bDoScrubLoop;
	UBOOL bNotifyEditing;
	UBOOL bLevelAnim;
	INT EditingNotifyNum;

	FLOAT FrameTime;
	FLOAT OldFrameTime;

	UBOOL bForceFrame;	
	UBOOL bWireframe;
	UBOOL bRawOffset;
	UBOOL bPrintBones;
	UBOOL bRefpose;
	UBOOL bBackface;
	UBOOL bDrawCollision;
	UBOOL bDrawKarma;

	UBOOL bFilledCopiedMesh;

	//"CopiedMesh" vars
	FVector  CopiedOrigin;
	FPlane   CopiedScale;
	FRotator CopiedRotOrigin;	
	FVector  CopiedMinVisBound;
	FVector  CopiedMaxVisBound;
	FLOAT    CopiedLODStrength;
	FLOAT    CopiedVisSphereRadius;
	FLOAT    CopiedSkinTesselationFactor;
	TArray<FAttachSocket> CopiedSockets;
	TArray <FSkelBoneSphere>	CopiedCollisionSpheres;
	TArray <FSkelBoneBox>		CopiedCollisionBoxes;	

	// Debug/temp/test vars.
	FLOAT	TempCollisionRadius;
	FLOAT	TempCollisionHeight;

	UMeshAnimation* CurrentMeshAnim;
	FName           CurrentSequence;

	FLOAT           CurrentSeqFrames; 

	// Non-package/object specific animbrowser preferences.
	UBOOL		DoWeldDuplicateVertices;

	// Array for copy/pasting notifies by sequence-name.
	TArray <NotifyStorage> TempNotifies; 
	TArray <GroupStorage>  TempGroups;

	// Array with actors that need to synchronize animation to the animbrowser when in bLevelAnim mode.
	TArray<AActor*> TempForcedActorList;

	// Serialize function: needed to associate the custom objects with THIS window so they don't get GC'd at the wrong time; when a level is loaded etc.
	virtual void Serialize( FArchive& Ar );

	// Structors.
	WBrowserAnimation( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame );

	// WBrowser interface.
	void OpenWindow( UBOOL bChild );
	void AddToolTips(WToolTip* ToolTipWindow, HWND& hWndBar, ToolTipStrings* TipArray );
	void OnCreate();
	void SetCaption( void );
	void RefreshPackages( void );
	virtual void RefreshAll();
	FString GetCurrentMeshName();
	FString GetCurrentAnimName();
	void RefreshMeshList();
	void RefreshAnimObjList();
	void RefreshAnimSeqList( FName NewCurrent=NAME_None );
	void RefreshViewport();
	void InitViewActor();
	void OnDestroy();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void PositionChildControls();
	void OnCommand( INT Command );
	virtual USkeletalMeshInstance* CurrentMeshInstance();
	virtual USkeletalMesh* CurrentSkelMesh();
	virtual void UpdateMenu();
	void CleanupLevel();
	void StartPlay();
	void StopPlay();
	void OnPackageSelectionChange();
	void InitMeshDrawSettings();
	void OnMeshSelectionChange();
	void OnAnimObjectSelectionChange();
	void OnAnimSequenceSelectionChange();
	void OnAnimSequenceDoubleClick();
	void OnAnimSequenceRightClick();
	void OnApplySequenceSort();
	void OnMoveSequenceUp();
	void OnMoveSequenceDown();
	void OnSliderMove();
	void OnScrubPlay();
	void OnScrubBegin();
	void OnScrubEnd();
	void OnScrubBackward();
	void OnScrubForward();
	void OnScrubLoop();
	void RefreshMeshProperties();
	void SaveMeshProperties();
	void RefreshAnimProperties();
	void SaveAnimationProperties();	
	void RefreshPrefsEditProperties();
	void SavePrefsEditProperties();

	FName FindAnimSeqNameFromIndex( INT Index );
	FLOAT GetSeqFramesFromIndex( INT Index );
	UBOOL DeleteNamedSequence( FName SeqName );
	FMeshAnimSeq* FindAnimSeqByName( FName FindSeq );

	// Notify handling
	void RefreshNotifiesList();
	void AddNotify();
	UBOOL SaveNotifys();
	void SetNotifyTicks();
	void GotoNotify( INT Notify );
	void CheckEditingNotifyActor();

	// Sequence property handling
	void RefreshSequenceProperties();
	void SaveSequenceProperties();
	void OnSequenceRename();

	// Interface
	void OnMeshRename();
	void OnMeshDelete();
	void OnAnimRename();
	void OnAnimDelete();
	void OnCopyMeshProps();
	void OnPasteMeshProps();
	void OnImportMeshLOD();
	void OnMeshRedigestLOD();
	void OnClearNotifies();
	void OnCopyNotifies();
	void OnPasteNotifies();
	void OnCopyGroups();
	void OnPasteGroups();
	void UpdateScrub();
	void SaveBookmark();
	void ResetLevelMeshes();
	void Draw( UViewport* Viewport );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

