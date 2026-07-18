/*=============================================================================
	UnViewport.h: Unreal viewport object.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	UCanvas.
-----------------------------------------------------------------------------*/

// Constants.
enum {MAX_COLOR_BYTES=9}; // Maximum expected frame buffer depth = 8 bytes = 64-bit.

//
// Blitting types.
//
enum EViewportBlitFlags
{
	// Bitflags.
	BLIT_Fullscreen     = 0x0001, // Fullscreen.
	BLIT_Temporary      = 0x0002, // Temporary viewport.
	BLIT_Direct3D       = 0x0010, // Create Direct3D along with DirectDraw.
	BLIT_NoWindowChange = 0x0020, // Don't resize existing window frame.
	BLIT_NoWindowFrame  = 0x0040, // Turn off the window frame.
	BLIT_OpenGL			= 0x0080, // OpenGL rendering.
	BLIT_HardwarePaint  = 0x0100, // Window should be repainted in hardware when drawn.

	// Special.
	BLIT_ParameterFlags	= BLIT_NoWindowChange, // Only parameters to ResizeViewport, not permanent flags.
};

/*-----------------------------------------------------------------------------
	FHitCause.
-----------------------------------------------------------------------------*/

//
// Information about a hit-test cause.
//
struct ENGINE_API FHitCause
{
	class FHitObserver* Observer;
	UViewport* Viewport;
	DWORD Buttons;
	FLOAT MouseX;
	FLOAT MouseY;
	FHitCause( FHitObserver* InObserver, UViewport* InViewport, DWORD InButtons, FLOAT InMouseX, FLOAT InMouseY )
	: Observer( InObserver ), Viewport( InViewport ), Buttons( InButtons ), MouseX( InMouseX ), MouseY( InMouseY )
	{}
};

/*-----------------------------------------------------------------------------
	FHitObserver.
-----------------------------------------------------------------------------*/

//
// Hit observer class for receiving hit-test notification.
//
class ENGINE_API FHitObserver
{
public:
	virtual void Click( const struct FHitCause& Cause, const struct HHitProxy& Hit ) {}
};

/*-----------------------------------------------------------------------------
	UViewport.
-----------------------------------------------------------------------------*/

// Information for rendering the viewport (detail level settings).
enum ERenderType
{
	REN_None				= 0,	// Hide completely.
	REN_Wire				= 1,	// Wireframe of EdPolys.
	REN_Zones				= 2,	// Show zones and zone portals.
	REN_Polys				= 3,	// Flat-shaded Bsp.
	REN_PolyCuts			= 4,	// Flat-shaded Bsp with normals displayed.
	REN_DynLight			= 5,	// Illuminated texture mapping.
	REN_PlainTex			= 6,	// Plain texture mapping.
	REN_LightingOnly		= 7,	// Untextured, lit geometry
	REN_DepthComplexity		= 8,	// Show per-pixel depth complexity.
	REN_OrthXY				= 13,	// Orthogonal overhead (XY) view.
	REN_OrthXZ				= 14,	// Orthogonal XZ view.
	REN_OrthYZ				= 15,	// Orthogonal YZ view.
	REN_TexView				= 16,	// Viewing a texture (no actor).
	REN_TexBrowser			= 17,	// Viewing a texture browser (no actor).
	REN_StaticMeshBrowser	= 18,	// Viewing a static mesh browser (no actor).
	REN_MeshView			= 19,	// Viewing a mesh.
	REN_Prefab				= 20,	// Viewing a prefab.
	REN_PrefabCompiled		= 21,	// Viewing a prefab (after the prefab level has been compiled).
	REN_TerrainHeightmap	= 22,	// Terrain heightmap info
	REN_TerrainLayers		= 23,	// Terrain layers
	REN_TerrainDecoLayers	= 24,	// Terrain decoration Layers
	REN_Animation			= 26,	// Animations
	REN_MatineeScenes		= 27,
	REN_MatineeActions		= 28,
	REN_MatineeSubActions	= 29,
	REN_MatineePreview		= 30,
	REN_MaterialEditor		= 31,	// Material editor
	REN_TexBrowserUsed		= 32,	// Browses texture which are in use in the current level
	REN_TexBrowserMRU		= 33,	// Browses the most recently used textures
	REN_ScreenActor         = 34,	// sjs

#ifdef WITH_LIPSINC
	REN_LIPSinc				= 34,	// LIPSinc
	REN_MAX					= 35,
#else
	REN_MAX					= 34,
#endif
};

enum EMaterialViewType
{
	MVT_TEXTURE,		// Regular, flat texture view - fill the window
	MVT_PLANE,			// Map texture on a quad
	MVT_CUBE,			// Map texture on a cube
	MVT_SPHERE,			// Map texture on a sphere
};

// Viewport capabilities.
enum EViewportCaps
{
	CC_RGB565			= 1,	// RGB 565 format (otherwise 555).
	CC_Mask				= 0xff, // Caps mask which affects cached rendering info.
};

// ShowFlags for viewport.
enum EViewportShowFlags
{
	SHOW_Frame				= 0x00000001, 	// Show world bounding cube.
	SHOW_ActorRadii			= 0x00000002, 	// Show actor collision radii.
	SHOW_Backdrop			= 0x00000004, 	// Show background scene.
	SHOW_Actors				= 0x00000008,	// Show actors.
	SHOW_Coords				= 0x00000010,	// Show brush/actor coords.
	SHOW_ActorIcons			= 0x00000020,	// Show actors as icons.
	SHOW_Brush				= 0x00000040,	// Show the active brush.
	SHOW_StandardView		= 0x00000080,	// Viewport is a standard view.
	SHOW_EdViewport			= 0x00000100,	// This is a special editor viewport and shouldn't have it's level reset by level loads
	SHOW_ChildWindow		= 0x00000200,	// Show as true child window.
	SHOW_MovingBrushes		= 0x00000400,	// Show moving brushes.
	SHOW_PlayerCtrl			= 0x00000800,	// Player controls are on.
	SHOW_Paths				= 0x00001000,   // Show paths.
	SHOW_KarmaMassProps     = 0x00002000,   // Show Karma mass properties (centre-of-mass, inertia tensor)
	SHOW_RealTime			= 0x00004000,	// Update window in realtime.
	SHOW_StaticMeshes		= 0x00008000,	// Show static meshes.
	SHOW_EventLines			= 0x00010000,   // Show event lines (from triggers, etc)
	SHOW_SelectionHighlight	= 0x00020000,   // Show green highlights on selections?
	SHOW_Terrain			= 0x00040000,	// Show terrain.
	SHOW_DistanceFog		= 0x00080000,	// Show distance fog.
	SHOW_MatRotations		= 0x00100000,	// Show rotation indicators (matinee preview windows)
	SHOW_MatPaths			= 0x00200000,	// Show the path itself (matinee preview windows)
	SHOW_Coronas			= 0x00400000,	// Show coronas.
	SHOW_Volumes			= 0x00800000,	// Show volume brushes.
	SHOW_Particles			= 0x01000000,	// Show pahticuls.
	SHOW_BSP				= 0x02000000,	// Show BSP surfaces.
	SHOW_ActorInfo			= 0x04000000,	// Shows extended information about actors (class name, event, tag, etc)
	SHOW_KarmaPrimitives    = 0x08000000,   // Show Karma collision primitives.
	SHOW_FluidSurfaces		= 0x10000000,	// Show simulated fluid surfaces.
	SHOW_Projectors			= 0x20000000,	// Show projectors.
	SHOW_NoFallbackMaterials= 0x40000000,	// Don't evaluate fallback materials.
	SHOW_Collision          = 0x80000000	// Show simple collision for static meshes etc.
};

// Mouse buttons and commands.
enum EMouseButtons	
{
	MOUSE_Left				= 0x00000001,	// Left mouse button.
	MOUSE_Right				= 0x00000002,	// Right mouse button.
	MOUSE_Middle 			= 0x00000004,	// Middle mouse button.
	MOUSE_FirstHit			= 0x00000008,	// Sent when a mouse button is initially hit.
	MOUSE_LastRelease		= 0x00000010,	// Sent when last mouse button is released.
	MOUSE_SetMode			= 0x00000020,	// Called when a new viewport mode is first set.
	MOUSE_ExitMode			= 0x00000040,	// Called when the existing mode is changed.
	MOUSE_Ctrl				= 0x00000080,	// Ctrl is pressed.
	MOUSE_Shift				= 0x00000100,	// Shift is pressed.
	MOUSE_Alt				= 0x00000200,	// Alt is pressed.
	MOUSE_LeftDouble		= 0x00000400,	// Left double click.
	MOUSE_MiddleDouble		= 0x00000800,	// Middle double click.
	MOUSE_RightDouble		= 0x00001000,	// Right double click.
};

// Mouse control styles - used by the editor
enum EMouseControl
{
	MOUSEC_Locked		= 0,		// Mouse is locked in center of the captured window
	MOUSEC_Free			= 1,		// Mouse is free to roam around the captured window
};

//
//	FViewportRenderTarget
//

class FViewportRenderTarget : public FRenderTarget
{
public:

	UViewport*	Viewport;
	QWORD		CacheId;

	// Constructor.

	FViewportRenderTarget(UViewport* InViewport)
	{
		Viewport = InViewport;
		CacheId = MakeCacheID(CID_RenderTexture);
	}

	// FBaseTexture interface.

	virtual INT GetWidth();
	virtual INT GetHeight();
	virtual INT GetFirstMip() { return 0; }
	virtual INT GetNumMips() { return 1; }
	virtual ETextureFormat GetFormat();
	virtual ETexClampMode GetUClamp() { return TC_Wrap; }
	virtual ETexClampMode GetVClamp() { return TC_Wrap; }

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return 1; }
};

//
// A viewport object, which associates an actor (which defines
// most view parameters) with a Windows window.
//
class ENGINE_API UViewport : public UPlayer
{
	DECLARE_ABSTRACT_CLASS(UViewport,UPlayer,CLASS_Transient,Engine)
	DECLARE_WITHIN(UClient)

	// Referenced objects.
	class UCanvas*		 Canvas;	// Viewport's painting canvas.
	class UInput*		 Input;		// Input system.
	class URenderDevice* RenDev;	// Render device.

	// Normal variables.
	UObject*		MiscRes;		// Used in in modes like EM_TEXVIEW.
	FName			Group;			// Group for editing.
	DOUBLE			LastUpdateTime;	// Time of last update.
	INT				DirtyViewport;	// Count how many times the viewport needs to be repainted.
	INT				SizeX, SizeY;   // Buffer X & Y resolutions.
	FLOAT			ScaleX,ScaleY;	// Scale factor.
	INT				ColorBytes;		// Engine only runs in 32bit color.
	INT				FrameCount;		// Frame count, incremented when locked.
	DWORD			Caps;			// Capabilities (CC_).
	UBOOL			Current;		// If this is the current input viewport.
	UBOOL			Dragging;		// Dragging mouse.
	DWORD			Buttons;		// The current set of mouse buttons being held down (used in the editor)
	FVector			OrigCursor;		// The position where the mouse was originally clicked in the viewport.
	UBOOL			FullscreenOnly; // Engine requires desktop set to 32 bpp for windowed mode.	
	FVector			TerrainPointAtLocation;	// The 3D location currently being pointed at on the terrain
	UBOOL			UseSoftwareRasterizer;

	FViewportRenderTarget	RenderTarget;	// A dummy render target that exposes the viewport's size to interfaces that take a FRenderTarget.

	// The current position of the mouse both on the screen, and within this viewports client area (using the X, Y ... Z is unused)
	FVector			MouseScreenPos, MouseClientPos;

	UBOOL			bLockOnSelectedActors;	// Update the location/rotation of selected actors when the camera is moved?
	AActor*			LockedActor;			// The actor we're locked to
	void LockOnActor( AActor* InActor )
	{
		if( !bLockOnSelectedActors ) return;

		LockedActor = InActor;
		if( InActor )
		{
			Actor->Location = InActor->Location;
			Actor->Rotation = InActor->Rotation;
		}
	}

	// Editor and debugging mode render effects, stat flags.
	UBOOL			bShowLargeVertices;	 // Show large vertices on brushes/meshes/etc?
	UBOOL           bShowBounds;     // Show visibility bounding boxes.
	UBOOL           bShowBones;      // Draw skeleton for skeletal meshes.
	UBOOL           bHideSkin;       // Hide/draw skin for meshes.
	UBOOL           bShowNormals;    // Draw bone-influence-number-color coded normals on mesh vertices.
	UBOOL			bShowGrid;		 // Draws grid for tweaking FOV/ distortion.
    UBOOL           bShowCollisionBounds; // amb

	// Cinematics.
	UBOOL			bRenderCinematics;
	FLOAT			CinematicsRatio;

	// Level traveling.
	ETravelType		TravelType;
	FStringNoInit	TravelURL;
	UBOOL			bTravelItems;

	// Frame buffer info; only valid when locked.
	DOUBLE			CurrentTime;	// Time when initially locked.
	BYTE*			ScreenPointer;	// Pointer to screen frame buffer, or NULL if none.
	INT				Stride;			// Stride in pixels.

	// Rendering data; only valid when locked.
	class FRenderInterface*	RI;
	class FSceneNode*		LodSceneNode;

	// Stencil mask; only valid when locked.  The next unused stencil bit.
	BYTE			NextStencilMask;

	// Precaching flag.
	UBOOL	Precaching,
			PendingFrame;

	// Hit testing.
	UBOOL			HitTesting;		// Whether hit-testing.
	INT				HitX, HitY;		// Hit rectangle top left.
	INT				HitXL, HitYL;	// Hit size.
	TArray<INT>		HitSizes;		// Currently pushed hit sizes.

	// Saved-actor parameters.
	FLOAT SavedOrthoZoom, SavedFovAngle;
	INT SavedShowFlags, SavedRendMap, SavedMisc1, SavedMisc2;

	// Dump Frames Command

	UBOOL bDumpingFrames;		// True if we are dumping each frame

	// Constructor.
	UViewport();

	// UObject interface.
	void Destroy();
	void Serialize( FArchive& Ar );

	// FArchive interface.
	void Serialize( const TCHAR* Data, EName MsgType );

	// UPlayer interface.
	void ReadInput( FLOAT DeltaSeconds );

	// UViewport interface.
	virtual UBOOL Lock( BYTE* HitData=NULL, INT* HitSize=NULL );
	virtual void Unlock();
	virtual void Present();

	virtual UBOOL SetDrag( UBOOL NewDrag );
	virtual UBOOL IsFullscreen()=0;
	virtual UBOOL ResizeViewport( DWORD BlitType, INT X=INDEX_NONE, INT Y=INDEX_NONE, UBOOL bSaveSize=true )=0;
	virtual void SetModeCursor()=0;
	virtual void UpdateWindowFrame()=0;
	virtual void OpenWindow( PTRINT ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY )=0;
	virtual void CloseWindow()=0;
	virtual void UpdateInput( UBOOL Reset, FLOAT DeltaSeconds )=0;
	virtual void* GetWindow()=0;
	virtual void* GetServer();
	virtual void UpdateMousePosition() {}
	virtual void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly=0 )=0;
	virtual void Repaint( UBOOL Blit )=0;
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual UBOOL TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, UBOOL Fullscreen )=0;
	virtual	TCHAR* GetLocalizedKeyName( EInputKey Key )=0;
	virtual void LoadSRGrammar( UBOOL ReloadCurrentGrammar, const FString& Grammar ) {}
	virtual void TextToSpeech( const FString& Text, FLOAT Volume ) {}
	
	// Movie encode functions.
	virtual void MovieEncodeStart( FString Filename, FLOAT Quality, INT Width, INT Height ) {}
	virtual void MovieEncodeFrame() {}
	virtual void MovieEncodeStop() {}

	// Functions.
	void ExecuteHits( const FHitCause& Cause, BYTE* HitData, INT HitSize, TCHAR* HitOverrideClass=NULL, FColor* HitColor=NULL, AActor** HitActor=NULL );
	void PushHit( const struct HHitProxy& Hit, INT Size );
	void PopHit( UBOOL bForce );
	UBOOL IsWire();
	void ExecMacro( const TCHAR* Filename, FOutputDevice& Ar=*GLog );

	// UViewport inlines.
	BYTE* _Screen( INT X, INT Y )
	{
		return ScreenPointer + (X+Y*Stride)*ColorBytes;
	}
	UBOOL IsOrtho()
	{
		return Actor && (Actor->RendMap==REN_OrthXY||Actor->RendMap==REN_OrthXZ||Actor->RendMap==REN_OrthYZ);
	}
	UBOOL IsPerspective()
	{
		return Actor && (Actor->RendMap==REN_Wire||Actor->RendMap==REN_Zones||Actor->RendMap==REN_Polys||Actor->RendMap==REN_PolyCuts||Actor->RendMap==REN_DynLight||Actor->RendMap==REN_PlainTex||Actor->RendMap==REN_LightingOnly||Actor->RendMap==REN_MatineePreview||(Actor->RendMap==REN_TexView&&Actor->Misc1!=MVT_TEXTURE));
	}
	UBOOL IsTopView()
	{
		return Actor && Actor->RendMap==REN_OrthXY;
	}
	UBOOL IsRealtime()
	{
		return Actor && (Actor->ShowFlags&(SHOW_RealTime | SHOW_PlayerCtrl));
	}
	UBOOL IsLit()
	{
		return Actor && (Actor->RendMap==REN_DynLight||Actor->RendMap==REN_LightingOnly||Actor->RendMap==REN_DepthComplexity||Actor->RendMap==REN_MatineePreview||(Actor->RendMap==REN_TexView&&Actor->Misc1!=MVT_TEXTURE));
	}
	UBOOL IsDepthComplexity()
	{
		return Actor && (Actor->RendMap == REN_DepthComplexity);
	}
	// Determines if the viewport is a viewport where the user can be editing the level (i.e. not a browser viewport)
	UBOOL IsEditing()
	{
		return Actor && ( IsOrtho() || Actor->RendMap==REN_Wire || Actor->RendMap==REN_Zones || Actor->RendMap==REN_Polys || Actor->RendMap==REN_PolyCuts || Actor->RendMap==REN_DynLight || Actor->RendMap==REN_PlainTex || Actor->RendMap==REN_LightingOnly || Actor->RendMap==REN_DepthComplexity );
	}
	// Refresh all viewports.
	static void RefreshAll()
	{
		for( TObjectIterator<UViewport> It; It; ++It )
			It->DirtyViewport = 1;
	}

	static const TCHAR* StaticConfigName() {return TEXT("User");}

	virtual UBOOL FireWallHack(INT Cmd) { return 1; }	// JMW - Hack for fixing the UT2004 Firewall support

};

// Viewport hit-testing macros.
#define PUSH_HIT(Viewport,expr) if( (Viewport)->HitTesting ) (Viewport)->PushHit(expr,sizeof(expr));
#define POP_HIT(Viewport)       if( (Viewport)->HitTesting ) (Viewport)->PopHit(0);
#define POP_HIT_FORCE(Viewport) if( (Viewport)->HitTesting ) (Viewport)->PopHit(1);

/*-----------------------------------------------------------------------------
	Base hit proxy.
-----------------------------------------------------------------------------*/

// Hit proxy declaration macro.
#define DECLARE_HIT_PROXY(cls,parent) \
	const TCHAR* GetName() const \
		{ return TEXT(#cls); } \
	UBOOL IsA( const TCHAR* Str ) const \
		{ return appStricmp(TEXT(#cls),Str)==0 || parent::IsA(Str); }

// Base class for detecting user-interface hits.
struct ENGINE_API HHitProxy
{
	union
	{
		mutable INT Size;
		HHitProxy* Parent;
	};
	FColor HitColor;
	virtual const TCHAR* GetName() const
	{
		return TEXT("HHitProxy");
	}
	virtual UBOOL IsA( const TCHAR* Str ) const
	{
		return appStricmp(TEXT("HHitProxy"),Str)==0;
	}
	virtual void Click( const FHitCause& Cause )
	{
		Cause.Observer->Click( Cause, *this );
	}
	virtual AActor* GetActor()
	{
		return NULL;
	}
};

/*-----------------------------------------------------------------------------
	UClient.
-----------------------------------------------------------------------------*/

//
// Client, responsible for tracking viewports.
//
class ENGINE_API UClient : public UObject
{
	DECLARE_ABSTRACT_CLASS(UClient,UObject,CLASS_Config,Engine)

	// Variables.
	UEngine*			Engine;
	TArray<UViewport*>	Viewports;

	// Configurable.
	BITFIELD	CaptureMouse;
	BITFIELD	ScreenFlashes;
	BITFIELD	NoLighting;
	BITFIELD	Decals;
	BITFIELD	NoDynamicLights;
	BITFIELD    NoFractalAnim;
	BITFIELD	Coronas;
	BITFIELD	DecoLayers;
	BITFIELD	Projectors;
	BITFIELD	ReportDynamicUploads;
	BITFIELD	WeatherEffects;
	INT			WindowedViewportX;
	INT			WindowedViewportY;
	INT			FullscreenViewportX;
	INT			FullscreenViewportY;
	INT			MenuViewportX;
	INT			MenuViewportY;
	FLOAT		Brightness;
	FLOAT		Contrast;
	FLOAT		Gamma;
	INT			TextureLODSet[LODSET_MAX];
	FLOAT		MinDesiredFrameRate;
	UViewport*	LastCurrent;		// The viewport that was last current
	FLOAT		ScaleHUDX;
	FLOAT		AnimMeshDynamicLOD; //Maximum framerate-induced animating mesh LOD reduction.
	FLOAT		DrawDistanceLOD;

    UEnum *PTextureDetail; // gam

	class UInteractionMaster* InteractionMaster;	// Input router

	// Constructors.
	UClient();
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();
	void PostEditChange();

	// UClient interface.
	virtual void Init( UEngine* InEngine )=0;
	virtual void Flush( UBOOL AllowPrecache );
	virtual void UpdateGamma();
	virtual void RestoreGamma();
	virtual void ShowViewportWindows( DWORD ShowFlags, int DoShow )=0;
	virtual void EnableViewportWindows( DWORD ShowFlags, int DoEnable )=0;
	virtual void Tick()=0;
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog )=0;
	virtual class UViewport* NewViewport( const FName Name )=0;
	virtual void MakeCurrent( UViewport* NewViewport )=0;
	virtual UViewport* GetLastCurrent()=0;
	virtual void TeardownSR()=0;

    // gam ---
    INT GetTextureLODBias (ELODSet LODSet);
    // --- gam
};

#include "UnForcePacking_end.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

