/*=============================================================================
	Engine.h: Unreal engine public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_ENGINE
#define _INC_ENGINE

/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef ENGINE_API
	#define ENGINE_API DLL_IMPORT
#endif

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "Core.h"

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

ENGINE_API extern class FMemStack			GEngineMem;
ENGINE_API extern class FMemCache			GCache;
ENGINE_API extern class FStats				GStats;
ENGINE_API extern class FEngineStats		GEngineStats;
ENGINE_API extern class FMatineeTools		GMatineeTools;
ENGINE_API extern class UGlobalTempObjects* GGlobalTempObjects;
ENGINE_API extern class FStatGraph*			GStatGraph;
ENGINE_API extern class FTempLineBatcher*	GTempLineBatcher;

#define DECLARE_STATIC_UOBJECT( ObjectClass, ObjectName, ExtraJunk ) \
			static ObjectClass* ObjectName = NULL;	\
			if( !ObjectName )	\
			{ \
				ObjectName = ConstructObject<ObjectClass>(ObjectClass::StaticClass()); \
				GGlobalTempObjects->AddGlobalObject((UObject**)&ObjectName); \
				ExtraJunk; \
			}

/*-----------------------------------------------------------------------------
	Engine compiler specific includes.
-----------------------------------------------------------------------------*/

#if __GNUC__ || __MWERKS__
	#include "UnEngineGnuG.h"
#endif

/*-----------------------------------------------------------------------------
	Size of the world.
-----------------------------------------------------------------------------*/

#define WORLD_MAX		524288.0	/* Maximum size of the world */
#define HALF_WORLD_MAX	262144.0	/* Half the maximum size of the world */
#define HALF_WORLD_MAX1	262143.0	/* Half the maximum size of the world - 1*/
#define MIN_ORTHOZOOM	250.0		/* Limit of 2D viewport zoom in */
#define MAX_ORTHOZOOM	16000000.0	/* Limit of 2D viewport zoom out */

// Karma Includes
#if !defined(_XBOX) && !defined(__PSX2_EE__) && !defined(__GCN__) && !defined(__LINUX__) && !defined(MACOSX) && !defined (__FreeBSD__) && !defined(NO_KARMA)
#ifndef WITH_KARMA
#  define WITH_KARMA
#endif
#endif

#ifdef WITH_KARMA
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
    #include "MePrecision.h"
    #include "Mst.h"
    #include "McdTriangleList.h"
    #include "MeAssetDB.h"
    #include "MeAssetFactory.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

    // These need to be public for UnrealEd etc.
    const MeReal K_ME2UScale = (MeReal)50;
    const MeReal K_U2MEScale = (MeReal)0.02;

	const MeReal K_Rad2U = (MeReal)10430.2192;
	const MeReal K_U2Rad = (MeReal)0.000095875262;
#endif


#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

// LIPSinc Includes
#if !defined(__PSX2_EE__) && !defined(__GCN__) && !defined(__LINUX__)
#ifndef WITH_LIPSINC
#//  define WITH_LIPSINC
#endif
#endif

/*-----------------------------------------------------------------------------
	Editor callback codes.
-----------------------------------------------------------------------------*/
enum EUnrealEdCallbacks
{
	EDC_None						= 0,
	EDC_Browse						= 1,
	EDC_UseCurrent					= 2,
	EDC_CurTexChange				= 10,
	EDC_CurStaticMeshChange			= 11,
	EDC_SelPolyChange				= 20,
	EDC_SelChange					= 21,
	EDC_RtClickTexture				= 23,
	EDC_RtClickStaticMesh			= 24,
	EDC_RtClickPoly					= 26,
	EDC_RtClickActor				= 27,
	EDC_RtClickWindow				= 28,
	EDC_RtClickWindowCanAdd			= 29,
	EDC_MapChange					= 42,
	EDC_ViewportUpdateWindowFrame	= 43,
	EDC_SurfProps					= 44,
	EDC_SaveMap						= 45,
	EDC_SaveMapAs					= 46,
	EDC_LoadMap						= 47,
	EDC_PlayMap						= 48,
	EDC_CamModeChange				= 49,
	EDC_RedrawAllViewports			= 50,
	EDC_ViewportsDisableRealtime	= 51,
	EDC_ActorPropertiesChange		= 52,
	EDC_RtClickTerrainLayer			= 53,
	EDC_RtClickActorStaticMesh		= 54,
	EDC_RefreshEditor				= 55,
	EDC_DisplayLoadErrors			= 56,
	EDC_RtClickMatScene				= 57,
	EDC_RtClickAnimSeq              = 58,
	EDC_RedrawCurrentViewport		= 59,
	EDC_MaterialTreeClick			= 60,
	EDC_RtClickMatAction			= 61,

#ifdef WITH_LIPSINC
	EDC_LIPSincAnimListRtClick		= 62,
#endif
};

// Used with the EDC_RefreshEditor callback to selectively refresh parts of the editor.
enum ERefreshEditor
{
	ERefreshEditor_Misc					= 1,	// All other things besides browsers
	ERefreshEditor_ActorBrowser			= 2,
	ERefreshEditor_GroupBrowser			= 4,
	ERefreshEditor_MeshBrowser			= 8,
	ERefreshEditor_AnimationBrowser		= 16,
	ERefreshEditor_MusicBrowser			= 32,
	ERefreshEditor_PrefabBrowser		= 64,
	ERefreshEditor_SoundBrowser			= 128,
	ERefreshEditor_StaticMeshBrowser	= 256,
	ERefreshEditor_TextureBrowser		= 512,
	ERefreshEditor_Matinee				= 1024,
	ERefreshEditor_Terrain				= 2048,

#ifdef WITH_LIPSINC
	ERefreshEditor_LIPSincBrowser		= 4096,
#endif
};

#ifdef WITH_LIPSINC
#define ERefreshEditor_AllBrowsers		ERefreshEditor_GroupBrowser | ERefreshEditor_MeshBrowser | ERefreshEditor_AnimationBrowser | ERefreshEditor_MusicBrowser | ERefreshEditor_PrefabBrowser | ERefreshEditor_SoundBrowser | ERefreshEditor_StaticMeshBrowser | ERefreshEditor_LIPSincBrowser
#else
#define ERefreshEditor_AllBrowsers		ERefreshEditor_GroupBrowser | ERefreshEditor_MeshBrowser | ERefreshEditor_AnimationBrowser | ERefreshEditor_MusicBrowser | ERefreshEditor_PrefabBrowser | ERefreshEditor_SoundBrowser | ERefreshEditor_StaticMeshBrowser
#endif


// Camera orientations for Matinee (this is mirrored in Object.uc - keep all definitions in sync!)
enum ECamOrientation
{
	CAMORIENT_None,
	CAMORIENT_LookAtActor,
	CAMORIENT_FacePath,
	CAMORIENT_Interpolate,
	CAMORIENT_Dolly,
};

// Struct for storing Matinee camera orientations (this is mirrored in MatObject.uc and SceneManager.uc - keep all definitions in sync!)
class UMatSubAction;
struct ENGINE_API FOrientation
{
	// Exposed in editor
	BYTE	CamOrientation;
	AActor* LookAt GCC_PACK(4);
	AActor* DollyWith;
	FLOAT EaseInTime;
	INT bReversePitch;
	INT bReverseYaw;
	INT bReverseRoll;

	// Work varibles
	UMatSubAction* MA;		// Used entirely for comparison purposes (this is the subaction this orientation belongs to)
	FLOAT PctInStart, PctInEnd, PctInDuration;
	FRotator StartingRotation;		// The original rotation we were at when the change started

	FOrientation()
		: CamOrientation(CAMORIENT_FacePath), LookAt(NULL), DollyWith(NULL), EaseInTime(0), MA(NULL), bReverseYaw(0), bReversePitch(0), bReverseRoll(0)
	{}

	FOrientation& operator=( const FOrientation Other )
	{
		CamOrientation = Other.CamOrientation;
		LookAt = Other.LookAt;
		DollyWith = Other.DollyWith;
		EaseInTime = Other.EaseInTime;
		StartingRotation = Other.StartingRotation;
		PctInStart = Other.PctInStart;
		PctInEnd = Other.PctInEnd;
		PctInDuration = Other.PctInDuration;
		MA = Other.MA;
		bReverseYaw = Other.bReverseYaw;
		bReversePitch = Other.bReversePitch;
		bReverseRoll = Other.bReverseRoll;

		return *this;
	}
	UBOOL operator!=( const FOrientation& Other ) const
	{
		return (MA != Other.MA);
	}
};

// Ease In and Ease Out Interpolation is done by calculating the 
// acceleration which will cover exactly half the distance in half 
// the specified time. That acceleration is applied for the first 
// half, then the inverse is applied for the second half to slow 
// down so the data point stops as it reaches the target point.

class FInterpolator
{
public:
	FInterpolator()
	{
		bDone = 1;
		_value = 1.f;
	}
	~FInterpolator()
	{}
	UBOOL Start( FLOAT InTime )
	{
		bDone = 0;
		if(InTime <= 0)
		{
			bDone = 1;
			return 0;
		}
			_value = 0.f;
		_speed = 0.0f;
		_acceleration = 1.f/(InTime*InTime/4.f);
		_remainingTime = _totalTime = InTime;
		return true;
	}
	void Tick(float deltaTime)
	{
		if( IsDone() )	return;

		_remainingTime -= deltaTime;
		if(_remainingTime < _totalTime/2.f)
		{
			// Deceleration
			_speed -= _acceleration * deltaTime;
		}
		else
		{
			// Acceleration
			_speed += _acceleration * deltaTime;
		}
		_value += _speed*deltaTime;
		bDone = (_remainingTime < 0.f);
	}
	float GetValue()
	{
		return _value;
	}
	UBOOL IsDone()
	{
		return bDone;
	}

	UBOOL bDone;
	float _value;
	float _remainingTime;
	float _totalTime;
	float _speed;
	float _acceleration;
};

/*-----------------------------------------------------------------------------
	FRotatorF.
-----------------------------------------------------------------------------*/

// A floating point version of FRotator.  Used by Matinee so precision doesn't get lost in camera rotation calcs.
class ENGINE_API FRotatorF
{
public:
	// Variables.
	FLOAT Pitch; // Looking up and down (0=Straight Ahead, +Up, -Down).
	FLOAT Yaw;   // Rotating around (running in circles), 0=East, +North, -South.
	FLOAT Roll;  // Rotation about axis of screen, 0=Straight, +Clockwise, -CCW.

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FRotatorF& R )
	{
		return Ar << R.Pitch << R.Yaw << R.Roll;
	}

	// Constructors.
	FRotatorF() {}
	FRotatorF( FLOAT InPitch, FLOAT InYaw, FLOAT InRoll )
	:	Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	FRotatorF( FRotator InRotator )
		:	Pitch(InRotator.Pitch), Yaw(InRotator.Yaw), Roll(InRotator.Roll) {}

	FRotator Rotator()
	{
		return FRotator( (INT)Pitch, (INT)Yaw, (INT)Roll );
	}

	FRotatorF operator*( FLOAT In ) const
	{
		return FRotatorF( Pitch*In, Yaw*In, Roll*In );
	}
	FRotatorF operator+( FRotatorF In ) const
	{
		return FRotatorF( Pitch+In.Pitch, Yaw+In.Yaw, Roll+In.Roll );
	}
	FRotatorF operator-( FRotatorF In ) const
	{
		return FRotatorF( Pitch-In.Pitch, Yaw-In.Yaw, Roll-In.Roll );
	}

	FRotatorF operator*=( FLOAT In )
	{
		Pitch *= In;		Yaw *= In;		Roll *= In;
		return *this;
	}
	FRotatorF operator+=( FRotatorF In )
	{
		Pitch += In.Pitch;		Yaw += In.Yaw;		Roll += In.Roll;
		return *this;
	}
	FRotatorF operator-=( FRotatorF In )
	{
		Pitch -= In.Pitch;		Yaw -= In.Yaw;		Roll -= In.Roll;
		return *this;
	}

	inline FVector Vector()
	{
		return (GMath.UnitCoords / Rotator()).XAxis;
	}
};

typedef class FDynamicActor* FActorRenderDataPtr;
typedef class FDynamicLight* FLightRenderDataPtr;

typedef struct FProjectorRenderInfo* FProjectorRenderInfoPtr;
typedef struct FStaticProjectorInfo* FStaticMeshProjectorRenderInfoPtr;

struct FParticleProjectorInfo
{
	class UBitmapMaterial*	BitmapMaterial;
	FMatrix					Matrix;
	UBOOL					Projected;
	INT						BlendMode;
};

struct FBatchReference
{
	INT	BatchIndex,
		ElementIndex;
};

typedef QWORD KModelPairType;

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


/*-----------------------------------------------------------------------------
	Engine public includes.
-----------------------------------------------------------------------------*/

#include "UnRebuildTools.h"		// Tools used by UnrealEd for rebuilding the level.
#include "Bezier.h"				// Class for computing bezier curves
#include "UnStats.h"			// Performance stat gathering.
#include "UnObj.h"				// Standard object definitions.
#include "UnPrim.h"				// Primitive class.
#include "UnConvexVolume.h"		// Convex volume primitive.
#include "UnRenderResource.h"	// Render resource objects.
#include "UnRenDev.h"			// Rendering interface definition.
#include "UnRenderUtil.h"		// Rendering utilities.
#include "UnMaterial.h"			// Materials
#include "UnTex.h"				// Texture and palette.
#include "UnModel.h"			// Model class.
#include "UnAnim.h"				// Animation.
#include "UnComponents.h"		// Forward declarations of object components of actors
#include "UnParticleSystem.h"	// Particle System.
#include "UnVolume.h"			// Volume brushes.
#include "UnVoiceChat.h"		// Voice chat declarations.
#include "UStream.h"            // Streaming file high-level support.
#include "KTypes.h"             // Public Karma integration types.
#include "EngineClasses.h"		// All actor classes.
#include "UnPhysic.h"			// Physics constants
#include "UnReach.h"			// Reach specs.
#include "UnURL.h"				// Uniform resource locators.
#include "UnLevel.h"			// Level object.
#include "UnIn.h"				// Input system.
#include "UnPlayer.h"			// Player class.
#include "UnPackageCheckInfo.h"	// Package MD5 info for validation
#include "UnEngine.h"			// Unreal engine.
#include "UnGame.h"				// Unreal game engine.
#include "UnCanvas.h"			// Canvas interface.
#include "UnCamera.h"			// Viewport subsystem.
#include "UnCameraEffects.h"	// Camera effects.
#include "UnRender.h"			// High-level rendering definitions.
#include "UnProjector.h"		// Projected textures and Decals.
#include "UnMesh.h"				// Mesh base.
#include "UnLodMesh.h"			// LOD mesh base class.
#include "UnSkeletalMesh.h"		// Skeletal animated mesh.
#include "UnVertMesh.h"			// Vertex animated mesh.
#include "UnActor.h"			// Actor inlines.
#include "UnAudio.h"			// Audio code.
#include "UnTerrain.h"			// Terrain objects.
#include "UnTerrainTools.h"		// Terrain tools.
#include "UnStaticMesh.h"		// Static T&L meshes.
#include "UnMatineeTools.h"		// Matinee tools.
#include "UnFluidSurface.h"		// Fluid Surface
#include "UnStatGraph.h"		// Stat drawing utility.
#include "UnCDKey.h"			// CD key validation.
#include "UnCacheManager.h"		// Cache management.


#if __STATIC_LINK
#define NAMES_ONLY
#define NATIVE_DEFS_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVE_DEFS_ONLY
#undef NAMES_ONLY
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	Hit proxies.
-----------------------------------------------------------------------------*/

// Hit an axis indicator on a gizmo
struct HGizmoAxis : public HHitProxy
{
	DECLARE_HIT_PROXY(HGizmoAxis,HHitProxy)
	AActor* Actor;
	INT Axis;
	HGizmoAxis( AActor* InActor, INT InAxis ) : Actor(InActor), Axis(InAxis) {}
	virtual AActor* GetActor()
	{
		return Actor;
	}

};

// Hit an actor vertex.
struct HActorVertex : public HHitProxy
{
	DECLARE_HIT_PROXY(HActorVertex,HHitProxy)
	AActor* Actor;
	FVector Location;
	HActorVertex( AActor* InActor, FVector InLocation ) : Actor(InActor), Location(InLocation) {}
	virtual AActor* GetActor()
	{
		return Actor;
	}
};

// Hit a bezier control point
struct HBezierControlPoint : public HHitProxy
{
	DECLARE_HIT_PROXY(HBezierControlPoint,HHitProxy)
	UMatAction* MA;
	UBOOL bStart;		// Is this the starting(=0) or ending(=1) control point?
	HBezierControlPoint( UMatAction* InMA, UBOOL InStart ) : MA(InMA), bStart(InStart) {}

	UBOOL operator==( const HBezierControlPoint& BCP ) const
	{
		return (MA==BCP.MA && bStart==BCP.bStart);
	}
};

/*-----------------------------------------------------------------------------
	Terrain editing brush types.
-----------------------------------------------------------------------------*/

enum ETerrainBrush
{
	TB_None				= -1,
	TB_VertexEdit		= 0,	// Select/Drag Vertices on heightmap
	TB_Paint			= 1,	// Paint on selected layer
	TB_Smooth			= 2,	// Does a filter on the selected vertices
	TB_Noise			= 3,	// Adds random noise into the selected vertices
	TB_Flatten			= 4,	// Flattens the selected vertices to the height of the vertex which was initially clicked
	TB_TexturePan		= 5,	// Pans the texture on selected layer
	TB_TextureRotate	= 6,	// Rotates the texture on selected layer
	TB_TextureScale		= 7,	// Scales the texture on selected layer
	TB_Select			= 8,	// Selects areas of the terrain for copying, generating new terrain, etc
	TB_Visibility		= 9,	// Toggles terrain sectors on/off
	TB_Color			= 10,	// Paints color into the RGB channels of layers
	TB_EdgeTurn			= 11,	// Turns edges of terrain triangulation
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

