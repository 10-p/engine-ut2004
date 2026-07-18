/*=============================================================================
	UnRenderVisibility.cpp: Rendering visibility determination code.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"

//
//	Current global visibility tag.  Incremented for every scene node.
//

static DWORD	CurrentVisibilityTag = 0;

//WD: added xmmintrin.h for SSE/SSE2 intrinsics
#if __HAS_SSE__
#include <xmmintrin.h>
#endif

static FORCEINLINE float BoxDistanceSqr( FDynamicActor* DynamicActor, FVector& Origin ) // sjs
{
    FBox box = DynamicActor->BoundingBox;
	float s, d = 0.0f; 
	//find the square of the distance
	//from the sphere to the box
	for( long i=0 ; i<3 ; i++ ) 
	{ 
		if( Origin[i] < box.Min[i] )
		{	
			s = Origin[i] - box.Min[i];
			d += s*s; 	
		}
		else if( Origin[i] > box.Max[i] )
		{ 
			s = Origin[i] - box.Max[i];
			d += s*s; 
		}
    }
    return d;
}

static FLOAT CullDistanceFOVBias;
static FORCEINLINE UBOOL CheckCullDistance( FVector& ViewOrigin, FDynamicActor* DynamicActor, UViewport* pVP ) // sjs
{
    if( DynamicActor->Actor->CullDistance <= 0.0f || pVP->Precaching == 1)
        return 1;
	else
	    return (Square(DynamicActor->Actor->CullDistance) >= BoxDistanceSqr(DynamicActor, ViewOrigin) * Square(CullDistanceFOVBias) );
}

//
//  ENodePass
//

enum ENodePass
{
	PASS_Front = 0,
	PASS_Plane = 1,
};

//
//  FNodeStack
//

struct FNodeStack
{
	FNodeStack*	Prev;
	INT			iNode,
				Outside,
				iFarNode,
				FarOutside,
				iFarLeaf;
	ENodePass	Pass;

	FProjectorRenderInfo**	DynamicProjectors;
	FDynamicLight**			DynamicLights;
	INT						NumDynamicLights,
							NumDynamicProjectors;

	static FNodeStack* Allocate(INT NumDynamicLights,INT NumDynamicProjectors)
	{
		FNodeStack*	NodeStack = (FNodeStack*) New<BYTE>(
				GSceneMem,
				sizeof(FNodeStack) +
				sizeof(FDynamicLight*) * NumDynamicLights +
				sizeof(FProjectorRenderInfo*) * NumDynamicProjectors
				);

		NodeStack->DynamicLights = (FDynamicLight**)&NodeStack[1];
		NodeStack->NumDynamicLights = NumDynamicLights;
		NodeStack->DynamicProjectors = (FProjectorRenderInfo**)(NodeStack->DynamicLights + NumDynamicLights);
		NodeStack->NumDynamicProjectors = NumDynamicProjectors;

		return NodeStack;
	}
};

//
//  FZoneInfo
//

class FZoneInfo : public FVisibilityInterface
{
public:

	TList<FConvexVolume*>*	Portals;
	TList<FConvexVolume*>*	AntiPortals;

	// Constructor/destructor.

	FZoneInfo() { Portals = AntiPortals = NULL; }
	~FZoneInfo() {}

	// FVisibilityInterface implementation.

	UBOOL Visible(const FBox& BoundingBox)
	{
		FVector	Origin = BoundingBox.GetCenter(),
				Extent = BoundingBox.Max - Origin;
		UBOOL	Visible = 0;

		for(TList<FConvexVolume*>* Portal = Portals;Portal;Portal = Portal->Next)
		{
			if(Portal->Element->BoxCheck(Origin,Extent) & CF_Inside)
			{
				Visible = 1;
				break;
			}
		}

		if(Visible)
		{
			for(TList<FConvexVolume*>* AntiPortal = AntiPortals;AntiPortal;AntiPortal = AntiPortal->Next)
			{
				if(!(AntiPortal->Element->BoxCheck(Origin,Extent) & CF_Outside))
				{
					Visible = 0;
					break;
				}
			}
		}

		return Visible;
	}

	UBOOL Visible(const FSphere& BoundingSphere)
	{
		UBOOL	Visible = 0;

		for(TList<FConvexVolume*>* Portal = Portals;Portal;Portal = Portal->Next)
		{
			if(Portal->Element->SphereCheck(BoundingSphere) & CF_Inside)
			{
				Visible = 1;
				break;
			}
		}

		if(Visible)
		{
			for(TList<FConvexVolume*>* AntiPortal = AntiPortals;AntiPortal;AntiPortal = AntiPortal->Next)
			{
				if(!(AntiPortal->Element->SphereCheck(BoundingSphere) & CF_Outside))
				{
					Visible = 0;
					break;
				}
			}
		}

		return Visible;
	}
};

//
//	FTranslucentDrawItem
//

struct FTranslucentDrawItem
{
	UBOOL	BSP;

// !!! FIXME: rcg06012002 This don't fly on GCC.
//	union
//	{
//		struct
//		{

			INT						iNode;
			FDynamicLight**			DynamicLights;
			INT						NumDynamicLights;
			FProjectorRenderInfo**	DynamicProjectors;
			INT						NumDynamicProjectors;
//		};
		FDynamicActor*	DynamicActor;
//	};
};

//
//  FRenderState
//

struct FRenderState
{
	FLevelSceneNode*				SceneNode;				// The scene node that's rendering the level.
	FRenderInterface*				RI;						// The renderer.
	ULevel*							Level;					// The level being rendered.
	UModel*							Model;					// The level's BSP tree.
	UClient*						Client;					// UClient for detail options.

	TList<FDynamicActor*>**			LeafActors;				// Maps a leaf index to a list of actors in the leaf.
	TList<FDynamicActor*>*			OutsideActors;			// A list of actors that are entirely outside of the level.

	FZoneInfo						Zones[64];				// The visibility volumes for each zone.
	TList<INT>*						ActiveZones;			// The zones that are visible.
	QWORD							ActiveZoneMask;			// A visible zone mask.
	BYTE*							RenderedPortals;		// Contains a bitmask for [anti]portal surfaces that have been rendered.

	TList<FLevelSceneNode*>*		ChildSceneNodes;		// Child scene nodes(mirrors, skyboxes, etc)

	FDynamicLight***				LeafLights;				// A list of dynamic lights in each leaf.
	FProjectorRenderInfo***			LeafProjectors;			// A list of dynamic projectors in each leaf.

	TList<FDynamicActor*>*			ActorDrawList;			// A list of actors to render.
	TList<FTranslucentDrawItem>*	TranslucentDrawList;	// A draw list for translucent surfaces and actors.

	FStaticMeshBatchList			StaticMeshBatchList;	// A list of visible static mesh batches.

	FBspDrawList**					BspDrawLists;			// An array of BSP draw list pointers.
	TList<INT>*						SectionDrawList;		// A list of BSP sections to draw.

	TList<FBspStencilDrawList*>*	StencilDrawLists;		// A list of BSP stencil draw lists.

	DWORD							SkyStencilMask;			// The stencil mask to initialize untouched pixels to before drawing the sky.
};

TList<FProjectorRenderBatch*>*	GProjectorBatchList;

//
// CoronaRender - actually not global // sjs - it is now!
//

FCoronaRender GCoronaRender;

//
//	ExtrudeAntiPortal
//

FConvexVolume* ExtrudeAntiPortal(FVector ViewOrigin,FDynamicActor* Actor)
{
	UConvexVolume*	ConvexVolume = Actor->Actor->AntiPortal;
	FConvexVolume*	Result = new(GSceneMem) FConvexVolume;
	FLOAT*			DotProducts = New<FLOAT>(GSceneMem,ConvexVolume->Faces.Num());

	for(INT FaceIndex = 0;FaceIndex < ConvexVolume->Faces.Num() && Result->NumPlanes < FConvexVolume::MAX_VOLUME_PLANES;FaceIndex++)
	{
		FConvexVolumeFace*	Face = &ConvexVolume->Faces(FaceIndex);
		FPlane				Plane = Face->Plane.TransformBy(Actor->LocalToWorld);
		FLOAT				Dot = Plane.PlaneDot(ViewOrigin);

		if(Dot > 0.0f)
			Result->BoundingPlanes[Result->NumPlanes++] = Plane;

		DotProducts[FaceIndex] = Dot;
	}

	for(INT EdgeIndex = 0;EdgeIndex < ConvexVolume->Edges.Num() && Result->NumPlanes < FConvexVolume::MAX_VOLUME_PLANES;EdgeIndex++)
	{
		FConvexVolumeEdge*	Edge = &ConvexVolume->Edges(EdgeIndex);

		if(DotProducts[Edge->Faces[0]] * DotProducts[Edge->Faces[1]] < 0.0f)
		{
			INT	FrontFace = DotProducts[Edge->Faces[1]] > 0.0f;

			Result->BoundingPlanes[Result->NumPlanes++] = FPlane(
				ViewOrigin,
				Actor->LocalToWorld.TransformFVector(ConvexVolume->Faces(Edge->Faces[FrontFace]).Vertices(Edge->Vertices[FrontFace])),
				Actor->LocalToWorld.TransformFVector(ConvexVolume->Faces(Edge->Faces[1 - FrontFace]).Vertices(Edge->Vertices[1 - FrontFace]))
				);
		}
	}

	return Result;
}

//
//	BuildAntiPortals
//

static void BuildAntiPortals(FRenderState& RenderState,INT ZoneIndex)
{
	if(!RenderState.SceneNode->Viewport->Precaching && ZoneIndex < RenderState.Level->ZoneRenderInfo.Num())
	{
		for(INT ActorIndex = 0;ActorIndex < RenderState.Level->ZoneRenderInfo(ZoneIndex).AntiPortals.Num();ActorIndex++)
		{
			FDynamicActor*	DynamicActor = RenderState.Level->ZoneRenderInfo(ZoneIndex).AntiPortals(ActorIndex)->GetActorRenderData();

			if(DynamicActor->VisibilityTag != CurrentVisibilityTag && RenderState.Zones[ZoneIndex].Visible(DynamicActor->BoundingBox) && CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport))
			{
				FConvexVolume*	AntiPortalVolume = ExtrudeAntiPortal(RenderState.SceneNode->ViewOrigin,DynamicActor);

				RenderState.Zones[ZoneIndex].AntiPortals = new(GSceneMem) TList<FConvexVolume*>(AntiPortalVolume,RenderState.Zones[ZoneIndex].AntiPortals);

				if(RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_Volumes)
					RenderState.ActorDrawList = new(GSceneMem) TList<FDynamicActor*>(DynamicActor,RenderState.ActorDrawList);

				DynamicActor->VisibilityTag = CurrentVisibilityTag;
			}
		}
	}
}

// sjs - invert exclude tag for special fun
static inline bool CheckSpecialInclude(AActor* actor, AActor* light)
{
    for( int i=0; i<ARRAY_COUNT(actor->ExcludeTag); i++ )
    {
        if( actor->ExcludeTag[i] == NAME_None )
        {
            return false;
        }

        // check tag matches light caster's tag
        if( actor->ExcludeTag[i] == light->Tag )
        {
            return true;
        }

        // check tag matches light caster's class name
        UClass* TempClass = NULL;
        for( TempClass=light->GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
        {
            if( actor->ExcludeTag[i] == TempClass->GetFName() )
            {
                return true;
            }
        }
    }
    return false;
}

//
//	FActorLightCache
//

struct FActorLightCache
{
	struct FActorLightInfluence
	{
		AActor*	Actor;
		INT		Index;
        UClass* Class; // sjs

		DOUBLE	TraceTime;

		FLOAT	FudgeFactor;

		UBOOL	Considered,
				Visible,
				LastVisible;
	};

	FActorLightInfluence	Lights[16];
	INT						NumLights;
};

static inline INT Compare(const FDynamicLight* A,const FDynamicLight* B)
{
	return B->SortKey - A->SortKey;
}

static inline INT Compare(FActorLightCache::FActorLightInfluence& A,FActorLightCache::FActorLightInfluence& B)
{
	FDynamicLight*	LightA = A.Actor->GetLightRenderData();
	FDynamicLight*	LightB = B.Actor->GetLightRenderData();

	return LightB->SortKey - LightA->SortKey;
}

#define LIGHT_UPDATE_TIME 0.35f

//
//	CalcSortKey
//

static void CalcSortKey(FDynamicLight* DynamicLight, const FSphere& RealBoundingSphere)
{
	AActor*	LightActor = DynamicLight->Actor;

	if(LightActor->LightEffect == LE_Sunlight)
	{
		DynamicLight->SortKey = MAXINT - 1;  
	}
	else if(LightActor->LightEffect == LE_Spotlight || LightActor->LightEffect == LE_StaticSpot)
	{		
		FLOAT SpotDot = (RealBoundingSphere - DynamicLight->Position).SafeNormal() | DynamicLight->Direction;
		if( SpotDot > 0 && SpotDot > Square(1.f-LightActor->LightCone/256.f) ) 
		{	
			DynamicLight->SortKey = appRound((1.0f - (DynamicLight->Position - RealBoundingSphere).SizeSquared() / Square(DynamicLight->Radius+RealBoundingSphere.W)) * LightActor->LightBrightness * 1024);			
		}
		else
		{
			DynamicLight->SortKey = 0;
		}
	}
	else
	{
		DynamicLight->SortKey = appRound((1.0f - (DynamicLight->Position - RealBoundingSphere).SizeSquared() / Square(DynamicLight->Radius+RealBoundingSphere.W)) * LightActor->LightBrightness * 1024);
	}
}

//
//	GetRelevantLights
//

void _DrawStat(UCanvas* Canvas,FColor Color,INT X,const TCHAR* Format,...)
{
	guard(DrawStat);
	TCHAR	TempStr[4096];
	GET_VARARGS(TempStr,ARRAY_COUNT(TempStr),Format,Format);
	Canvas->Color = Color;
	Canvas->CurX = X;
	Canvas->WrappedPrintf(Canvas->SmallFont,0,TempStr);
	Canvas->CurY -= 4;	
	unguard;
}

TList<FDynamicLight*>* GetRelevantLights(FSceneNode* SceneNode,FDynamicActor* DynamicActor,FSphere RealBoundingSphere,FDynamicLight** Consider,INT NumConsider)
{
	guard(GetRelevantLights);

	clock(GStats.DWORDStats(GEngineStats.STATS_Visibility_MeshLightCycles));

	AActor*					RenderActor				= DynamicActor->Actor;
	QWORD					CacheId					= MakeCacheID(CID_ActorLightCache,RenderActor);
	FCacheItem*				CacheItem				= NULL;
	FActorLightCache*		LightCache				= NULL;
	TList<FDynamicLight*>*	ActorLights				= NULL;
	UBOOL					DisableFadeIn			= 0;
	FLOAT					DramaticLightingScale	= RenderActor->Region.Zone ? RenderActor->Region.Zone->DramaticLightingScale : 1.f;

	LightCache = (FActorLightCache*)GCache.Get(CacheId,CacheItem);

	if(!LightCache)
	{
		LightCache = (FActorLightCache*)GCache.Create(CacheId,CacheItem,sizeof(FActorLightCache));
		LightCache->NumLights = 0;
		DisableFadeIn = 1;
	}

	// Remove old lights.

	INT	NumLights = 0;
	guard(RemoveOldLights);
	for(INT LightIndex = 0;LightIndex < LightCache->NumLights;LightIndex++)
	{
		FActorLightCache::FActorLightInfluence&	Light = LightCache->Lights[LightIndex];

		// sjs - added good ole bDeleteMe check to hasten removal as well as cached the class to guard against
		// non-actor objects being allocated in the same object table index and same heap location (it happens often!)
		if( Light.Actor && (Light.Actor == UObject::GetIndexedObject(Light.Index)) && (Light.Class == Light.Actor->GetClass()) && !Light.Actor->bDeleteMe && Light.Actor->GetLightRenderData() )
		{
			if( NumLights != LightIndex )
				LightCache->Lights[NumLights] = LightCache->Lights[LightIndex];

			LightCache->Lights[NumLights].Considered = 0;

			FDynamicLight*	DynamicLight = Light.Actor->GetLightRenderData();

			CalcSortKey(DynamicLight,RealBoundingSphere);

			if(DynamicLight->SortKey > 0)
				NumLights++;
		}
	}
	unguard;

	LightCache->NumLights = NumLights;

	// Find the strongest light influences.
	guard(FindStrongest);

	for(INT ConsiderIndex = 0;ConsiderIndex < NumConsider;ConsiderIndex++)
		CalcSortKey(Consider[ConsiderIndex],RealBoundingSphere);

	Sort(Consider,NumConsider);

	for(INT ConsiderIndex = 0;ConsiderIndex < NumConsider;ConsiderIndex++)
	{
		FDynamicLight*	DynamicLight = Consider[ConsiderIndex];
		INT				ExistingLightIndex = INDEX_NONE;

		for(INT LightIndex = 0;LightIndex < LightCache->NumLights;LightIndex++)
			if(LightCache->Lights[LightIndex].Actor == DynamicLight->Actor)
			{
				ExistingLightIndex = LightIndex;
				break;
			}

		if(ExistingLightIndex == INDEX_NONE && DynamicLight->SortKey > 0)
		{
			if(LightCache->NumLights < 16)
				ExistingLightIndex = LightCache->NumLights++;
			else
			{
				INT	WeakestLightIndex = INDEX_NONE,
					WeakestSortKey = DynamicLight->SortKey;

				for(INT LightIndex = 0;LightIndex < LightCache->NumLights;LightIndex++)
				{
					FDynamicLight*	OtherLight = LightCache->Lights[LightIndex].Actor->GetLightRenderData();

					check(OtherLight);

					if(OtherLight->SortKey < WeakestSortKey)
					{
						WeakestLightIndex = LightIndex;
						WeakestSortKey = OtherLight->SortKey;
					}
				}

				if(WeakestLightIndex != INDEX_NONE)
					ExistingLightIndex = WeakestLightIndex;
			}

			if(ExistingLightIndex != INDEX_NONE)
			{
				LightCache->Lights[ExistingLightIndex].Actor				= DynamicLight->Actor;
				LightCache->Lights[ExistingLightIndex].Index				= DynamicLight->Actor->GetIndex();
				LightCache->Lights[ExistingLightIndex].Class				= DynamicLight->Actor->GetClass(); // sjs
				LightCache->Lights[ExistingLightIndex].TraceTime			= -LIGHT_UPDATE_TIME;
				LightCache->Lights[ExistingLightIndex].FudgeFactor			= 1.f;
				LightCache->Lights[ExistingLightIndex].Visible				= 0;
				LightCache->Lights[ExistingLightIndex].LastVisible			= 0;
			}
		}

		if(ExistingLightIndex != INDEX_NONE)
			LightCache->Lights[ExistingLightIndex].Considered = 1;
	}
	unguard;

	// Sort the light cache.

	Sort(LightCache->Lights,LightCache->NumLights);

	// Update light visibility.

	guard(LightVisibility);
	for(INT LightIndex = 0;LightIndex < LightCache->NumLights;LightIndex++)
	{
		FActorLightCache::FActorLightInfluence&	Light = LightCache->Lights[LightIndex];
		FDynamicLight*							DynamicLight = Light.Actor->GetLightRenderData();

		check(DynamicLight);

		if(Light.TraceTime <= SceneNode->Viewport->CurrentTime - LIGHT_UPDATE_TIME)
		{
			FVector			TraceStart = RealBoundingSphere,
							TraceEnd = (DynamicLight->Actor->LightEffect == LE_Sunlight) ? TraceStart - DynamicLight->Direction * 65536.0f : DynamicLight->Position;
			FCheckResult	Hit;

			Light.LastVisible = Light.Visible;
			Light.Visible = Light.Considered && (GIsEditor || !RenderActor->bLightingVisibility || !Light.Actor->bStatic || SceneNode->Viewport->Actor->XLevel->SingleLineCheck(Hit,RenderActor,TraceEnd,TraceStart,TRACE_ShadowCast | TRACE_Level | TRACE_StopAtFirstHit));
			Light.TraceTime = SceneNode->Viewport->CurrentTime - LIGHT_UPDATE_TIME * DisableFadeIn;
		}
	}
	unguard;

	INT	MaxLights = Min<INT>(RenderActor->bDramaticLighting ? 4 : 8,RenderActor->MaxLights),
		NumOutputLights = 0;

	static FDynamicLight	FudgedLights[4];
	static FLOAT			FudgeFactors[4] = { 2.0f, 1.5f, 1.0f, 0.5f };

	guard(FinishRelevantLights);
	for(INT LightIndex = 0;LightIndex < LightCache->NumLights;LightIndex++)
	{
		FActorLightCache::FActorLightInfluence&	Light = LightCache->Lights[LightIndex];
		FDynamicLight*							DynamicLight = Light.Actor->GetLightRenderData();

		check(DynamicLight);

		if(NumOutputLights < MaxLights && (Light.Visible || Light.LastVisible))
		{
			FLOAT	Start = Light.LastVisible ? 1.0f : 0.0f,
					End = Light.Visible ? 1.0f : 0.0f;

			DynamicLight->Alpha = Lerp(Start,End,(SceneNode->Viewport->CurrentTime - Light.TraceTime) / LIGHT_UPDATE_TIME);

			if( RenderActor->bDramaticLighting )
			{
				appMemcpy(&FudgedLights[NumOutputLights],DynamicLight,sizeof(FDynamicLight));
			
				if( GDeltaTime < 1.f )
					Light.FudgeFactor = Lerp( Light.FudgeFactor, FudgeFactors[NumOutputLights], GDeltaTime );
				else
					Light.FudgeFactor = FudgeFactors[NumOutputLights];

				FLOAT FudgeFactor = Light.FudgeFactor;
				if( !DynamicLight->Dynamic )
					FudgeFactor *= DramaticLightingScale;

				FudgedLights[NumOutputLights].Radius *= FudgeFactor;
				FudgedLights[NumOutputLights].Color  *= FudgeFactor;
				
				ActorLights = new(GSceneMem) TList<FDynamicLight*>(&FudgedLights[NumOutputLights],ActorLights);
			}
			else
				ActorLights = new(GSceneMem) TList<FDynamicLight*>(DynamicLight,ActorLights);

			NumOutputLights++;
		}
	}

	CacheItem->Unlock();
    unguard;

	// Actor's light debugging

	if(SceneNode->Viewport->Actor->GetLevel()->Engine->bShowLightStats && RenderActor->IsA(APawn::StaticClass()) && SceneNode && SceneNode->Viewport && SceneNode->Viewport->Canvas && SceneNode->Viewport->LodSceneNode == SceneNode)
	{
		UCanvas* Canvas = SceneNode->Viewport->Canvas;
		FString	LightString;			
		LightString = FString::Printf(TEXT(" Lighting - actor:%s sphereradius %f "), *RenderActor->GetName(), DynamicActor->BoundingSphere.W );
		
		Canvas->CurY += 16; //skip line

		_DrawStat(Canvas, FColor(255,255,0),4,TEXT("%s"),*LightString); //Title string

		Canvas->CurY += 3;
		INT Number = 0;

		for(INT LightIndex = 0;LightIndex < LightCache->NumLights;LightIndex++)
		{
			FActorLightCache::FActorLightInfluence&	Light = LightCache->Lights[LightIndex];
			FDynamicLight*							DynamicLight = Light.Actor->GetLightRenderData();

			if( DynamicLight->Actor )
			{
				FLOAT OurDistance =  FDist( DynamicLight->Position, DynamicActor->BoundingSphere ); // Actor->Location );

				LightString = FString::Printf(TEXT("[%i]{%2i}(%s) Rad %4.4f Col %3.2f %3.2f %3.2f Alph %4.4f Dst %4.5f Ky %i br %3.2f v=%u lv=%u"), 
						    Number, 
							DynamicLight->Actor->LightEffect,
							DynamicLight->Actor->GetName(), 
							DynamicLight->Radius,
							DynamicLight->Color.X,
							DynamicLight->Color.Y,
							DynamicLight->Color.Z,
							DynamicLight->Alpha,
							OurDistance,								
							DynamicLight->SortKey,
							DynamicLight->Actor->LightBrightness,
							Light.Visible,
							Light.LastVisible
							);

				_DrawStat(Canvas,FColor(255,255,0),4,TEXT("%s"),*LightString);
			}
			Number++;
		}			
	}

	unclock(GStats.DWORDStats(GEngineStats.STATS_Visibility_MeshLightCycles));

	return ActorLights;

	unguard;
}

//
//  RenderActor
//
DWORD GConsideredTag = 0;
static void RenderActor(FRenderState& RenderState,FDynamicActor* DynamicActor)
{
	AActor*							RenderActor = DynamicActor->Actor;
	TList<FDynamicLight*>*			ActorLights = NULL;
	TList<FProjectorRenderInfo*>*	ActorProjectors = NULL;

    // sjs --- case where mesh should not be rendered unless lit, todo: generalize
    bool skipRendering = false;
    if( !GIsEditor && RenderActor->bSpecialLit && RenderActor->ExcludeTag[0] != NAME_None )
        skipRendering = true;
    // --- sjs

	// Offset the bounding sphere to account for root motion.

	FSphere	RealBoundingSphere = DynamicActor->BoundingSphere;

	if(RenderActor->DrawType == DT_Mesh && RenderActor->Mesh && RenderActor->Mesh->IsA(USkeletalMesh::StaticClass()))
	{
		USkeletalMeshInstance*	MeshInstance = Cast<USkeletalMeshInstance>(RenderActor->Mesh->MeshGetInstance(RenderActor));

		check(MeshInstance);
		RealBoundingSphere += (MeshInstance->GetBoneCoords(0).Origin - RenderActor->Location);
	}

    if(RenderActor->DrawType == DT_StaticMesh && RenderActor->bSpecialLit )
    {
        RealBoundingSphere.W *= 10.5f;
    }

	if(!RenderActor->GetAmbientLightingActor()->bUnlit)
	{
		FDynamicLight*	Consider[256];
		INT				NumConsider = 0;
		UBOOL			NoStaticLights = RenderActor->DrawType == DT_StaticMesh && ((Cast<AMover>(RenderActor) && !Cast<AMover>(RenderActor)->bDynamicLightMover) || (RenderActor->bStatic && !RenderActor->bLightChanged));

		GConsideredTag++;

		for(INT LeafIndex = 0;LeafIndex < RenderActor->Leaves.Num();LeafIndex++)
		{
			INT		iLeaf = RenderActor->Leaves(LeafIndex);
			FLeaf&	Leaf = RenderState.Model->Leaves(iLeaf);

			// Find lights that affect this actor.
				
			clock(GStats.DWORDStats(GEngineStats.STATS_Visibility_MeshLightCycles));

			AActor**		StaticLightActors = Leaf.iPermeating != INDEX_NONE ? &RenderState.Model->Lights(Leaf.iPermeating) : NULL;
			FDynamicLight**	DynamicLightActors = RenderState.LeafLights[iLeaf];

			for(INT Pass = NoStaticLights;Pass < 2;Pass++)
			{
				if(	NumConsider >= 256 || (Pass == 0 && Leaf.iPermeating == INDEX_NONE) || (Pass == 1 && (!DynamicLightActors || !RenderActor->bUseDynamicLights)))
					continue;

				while(1)
				{
					AActor*			LightActor;
					FDynamicLight*	DynamicLight;

					if(Pass == 0)
					{
						LightActor = *StaticLightActors++;

						if(LightActor)
							DynamicLight = LightActor->GetLightRenderData();
						else
							break;
					}
					else
					{
						DynamicLight = *DynamicLightActors++;

						if(DynamicLight)
							LightActor = DynamicLight->Actor;
						else
							break;
					}

					if(DynamicLight)
					{
						// sjs --- invert exclude tag for extended speciallit behaviors
						if( RenderActor->bSpecialLit && RenderActor->ExcludeTag[0] != NAME_None ) 
						{
							if(!CheckSpecialInclude(RenderActor, LightActor ))
								continue;
							skipRendering = false;
						}
						// --- sjs
						else if(LightActor->bSpecialLit != RenderActor->bSpecialLit)
							continue;

						if( GConsideredTag == DynamicLight->ConsideredTag )
							continue;

						if(LightActor->LightEffect != LE_Sunlight && (DynamicLight->Position - RealBoundingSphere).SizeSquared() > Square(DynamicLight->Radius + RealBoundingSphere.W))
							continue;

						DynamicLight->ConsideredTag = GConsideredTag;

						Consider[NumConsider++] = DynamicLight;

						if(NumConsider >= 256)
							break;
					}
				}
			}

			unclock(GStats.DWORDStats(GEngineStats.STATS_Visibility_MeshLightCycles));

			// Find dynamic projectors that affect this actor.

			if(RenderActor->bAcceptsProjectors)
			{
				for(FProjectorRenderInfo** ProjectorList = RenderState.LeafProjectors[iLeaf];ProjectorList && *ProjectorList;ProjectorList++)
				{
					FProjectorRenderInfo*	ProjectorInfo = *ProjectorList;

					if(!ProjectorInfo->Projector->bProjectActor && RenderActor->Mesh)
						continue;

					if(!ProjectorInfo->Projector->bProjectStaticMesh && RenderActor->StaticMesh)
						continue;

					TList<FProjectorRenderInfo*>* ExistingProjectorList;
					for(ExistingProjectorList = ActorProjectors;ExistingProjectorList;ExistingProjectorList = ExistingProjectorList->Next)
						if(ExistingProjectorList->Element == ProjectorInfo)
							break;

					if(ExistingProjectorList)
						continue;

					UBOOL	Inside = 1;
					FVector	ActorCenter = DynamicActor->BoundingBox.GetCenter(),
							ActorExtent = DynamicActor->BoundingBox.GetExtent();

					for(INT PlaneIndex = 0;PlaneIndex < 6;PlaneIndex++)
					{
						FLOAT	PushOut = FBoxPushOut(ProjectorInfo->FrustumPlanes[PlaneIndex],ActorExtent),
								Dist = ProjectorInfo->FrustumPlanes[PlaneIndex].PlaneDot(ActorCenter);

						if(Dist < -PushOut)
						{
							Inside = 0;
							break;
						}
					}

					if(Inside)
						ActorProjectors = new(GSceneMem) TList<FProjectorRenderInfo*>(ProjectorInfo,ActorProjectors);
				}
			}
		}

		if(NumConsider)
			ActorLights = GetRelevantLights(RenderState.SceneNode,DynamicActor,RealBoundingSphere,Consider,NumConsider);
	}

	 if( skipRendering ) // sjs
		return;

	UBOOL RenderBatched = 1;

	if(	RenderActor->DrawType != DT_StaticMesh 
	|| !RenderActor->bStatic 
	|| RenderActor->UV2Texture 
	|| RenderActor->Projectors.Num() 
	|| RenderState.SceneNode->Viewport->IsWire() 
	|| GIsEditor 
	|| ActorProjectors 
	|| ActorLights 
	|| !RenderState.Level->Engine->UseStaticMeshBatching 
	|| !RenderState.Level->UseStaticMeshBatching
	)
		RenderBatched = 0;

	if( RenderBatched )
		RenderBatchedStaticMesh(RenderState.StaticMeshBatchList,DynamicActor,RenderState.SceneNode,RenderState.RI);
	else
	{
		RenderState.RI->PushState();
		RenderState.RI->SetCullMode(DynamicActor->Determinant * RenderState.SceneNode->Determinant < 0.0f ? CM_CCW : CM_CW);
		DynamicActor->Render(RenderState.SceneNode,ActorLights,ActorProjectors,RenderState.RI);
		RenderState.RI->PopState();
	}
}


//
//  ProcessLeaf - Process the contents of a leaf.
//
static void ProcessLeaf(FRenderState& RenderState,INT iLeaf)
{
	guard(ProcessLeaf);

	INT		iZone = RenderState.SceneNode->ViewZone ? RenderState.Model->Leaves(iLeaf).iZone : 0;

	if(iZone != RenderState.SceneNode->InvisibleZone)
	{
		if( GIsEditor )
		{
			// Add anti-portals in this leaf to the list of visibility volumes.

			for(TList<FDynamicActor*>* ActorList = RenderState.LeafActors[iLeaf];ActorList;ActorList = ActorList->Next)
			{
				FDynamicActor*	DynamicActor = ActorList->Element;

				if(DynamicActor->Actor->DrawType == DT_AntiPortal && DynamicActor->Actor->AntiPortal && (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
				{
					if(DynamicActor->VisibilityTag != CurrentVisibilityTag && RenderState.Zones[iZone].Visible(DynamicActor->BoundingBox) && CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport))
					{
						FConvexVolume*	AntiPortalVolume = ExtrudeAntiPortal(RenderState.SceneNode->ViewOrigin,DynamicActor);
						INT				Zones[4] = { -1, -1, -1, -1 },
										NumZones = 0;

						for(INT LeafIndex = 0;LeafIndex < DynamicActor->Actor->Leaves.Num();LeafIndex++)
						{
							INT	ZoneIndex = RenderState.Model->Leaves(DynamicActor->Actor->Leaves(LeafIndex)).iZone,
								OtherZoneIndex = 0;

							for(OtherZoneIndex = 0;OtherZoneIndex < NumZones;OtherZoneIndex++)
								if(Zones[OtherZoneIndex] == ZoneIndex)
									break;

							if(OtherZoneIndex < 4 && Zones[OtherZoneIndex] != ZoneIndex)
							{
								RenderState.Zones[ZoneIndex].AntiPortals = new(GSceneMem) TList<FConvexVolume*>(AntiPortalVolume,RenderState.Zones[ZoneIndex].AntiPortals);
								Zones[OtherZoneIndex] = ZoneIndex;
							}
						}

						DynamicActor->VisibilityTag = CurrentVisibilityTag;
					}
				}
			}
		}
		else
		{
			// Render static actors in this leaf.

			FLeafRenderInfo&	LeafRenderInfo = RenderState.Level->LeafRenderInfo(iLeaf);

			for(INT ActorIndex = 0;ActorIndex < LeafRenderInfo.RenderActors.Num();ActorIndex++)
			{
				AActor*	Actor = LeafRenderInfo.RenderActors(ActorIndex);

				// Update the cached filter state for the static actor.

				if(Actor->StaticFilterState == FS_Maybe)
				{
					if(RenderState.SceneNode->FilterActor(Actor))
						Actor->StaticFilterState = FS_Yes;
					else
						Actor->StaticFilterState = FS_No;
				}

				if(Actor->StaticFilterState == FS_Yes)
				{
					FDynamicActor*	DynamicActor = Actor->GetActorRenderData();
					if(DynamicActor->VisibilityTag != CurrentVisibilityTag && ((RenderState.Zones[iZone].Visible(DynamicActor->BoundingBox) && CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport)) || RenderState.SceneNode->Viewport->Precaching))
					{
						if(DynamicActor->Translucent)
						{
							TList<FTranslucentDrawItem>**	DrawTail = &RenderState.TranslucentDrawList;
							FLOAT							DistanceSquared = (DynamicActor->BoundingSphere - RenderState.SceneNode->ViewOrigin).SizeSquared();

							while(*DrawTail)
							{
								if(!(*DrawTail)->Element.BSP && ((*DrawTail)->Element.DynamicActor->BoundingSphere - RenderState.SceneNode->ViewOrigin).SizeSquared() > DistanceSquared)
									DrawTail = &(*DrawTail)->Next;
								else if((*DrawTail)->Element.BSP)
								{
									FBspNode&	Node = RenderState.Model->Nodes((*DrawTail)->Element.iNode);

									if(Node.Plane.PlaneDot(DynamicActor->BoundingSphere) * Node.Plane.PlaneDot(RenderState.SceneNode->ViewOrigin) > 0.0f)
										DrawTail = &(*DrawTail)->Next;
									else
										break;
								}
								else
									break;
							};

							FTranslucentDrawItem	TranslucentActorItem;
							TranslucentActorItem.BSP = 0;
							TranslucentActorItem.DynamicActor = DynamicActor;

							*DrawTail = new(GSceneMem) TList<FTranslucentDrawItem>(TranslucentActorItem,*DrawTail);
						}
						else
							RenderState.ActorDrawList = new(GSceneMem) TList<FDynamicActor*>(DynamicActor,RenderState.ActorDrawList);

						if( RenderState.Client->Coronas
						&&	DynamicActor->Actor->bCorona 
						&&  (FDistSquared(RenderState.SceneNode->ViewOrigin, DynamicActor->Actor->Location) < Square(DynamicActor->Actor->WorldLightRadius()) || DynamicActor->Actor->LightRadius == 0 )
						&&  RenderState.Zones[iZone].Visible(FBox(DynamicActor->Actor->Location,DynamicActor->Actor->Location)) 
						&&  CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport)   // sjs
						)
							GCoronaRender.AddVisibleLight( DynamicActor->Actor );

						DynamicActor->VisibilityTag = CurrentVisibilityTag;
					}
				}
				else
					LeafRenderInfo.RenderActors.Remove(ActorIndex);
			}
		}

		// Render dynamic actors in this leaf.

		for(TList<FDynamicActor*>* ActorList = RenderState.LeafActors[iLeaf];ActorList;ActorList = ActorList->Next)
		{
			FDynamicActor*	DynamicActor = ActorList->Element;
			if(DynamicActor->VisibilityTag != CurrentVisibilityTag && ((RenderState.Zones[iZone].Visible(DynamicActor->BoundingBox) && CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport)) || RenderState.SceneNode->Viewport->Precaching) && (DynamicActor->Actor->DrawType != DT_AntiPortal || !(RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl)))
			{
				if(DynamicActor->Translucent)
				{
					TList<FTranslucentDrawItem>**	DrawTail = &RenderState.TranslucentDrawList;
					FLOAT							DistanceSquared = (DynamicActor->BoundingSphere - RenderState.SceneNode->ViewOrigin).SizeSquared();

					while(*DrawTail)
					{
						if(!(*DrawTail)->Element.BSP && ((*DrawTail)->Element.DynamicActor->BoundingSphere - RenderState.SceneNode->ViewOrigin).SizeSquared() > DistanceSquared)
							DrawTail = &(*DrawTail)->Next;
						else if((*DrawTail)->Element.BSP)
						{
							FBspNode&	Node = RenderState.Model->Nodes((*DrawTail)->Element.iNode);

							if(Node.Plane.PlaneDot(DynamicActor->BoundingSphere) * Node.Plane.PlaneDot(RenderState.SceneNode->ViewOrigin) > 0.0f)
								DrawTail = &(*DrawTail)->Next;
							else
								break;
						}
						else
							break;
					};

					FTranslucentDrawItem	TranslucentActorItem;
					TranslucentActorItem.BSP = 0;
					TranslucentActorItem.DynamicActor = DynamicActor;

					*DrawTail = new(GSceneMem) TList<FTranslucentDrawItem>(TranslucentActorItem,*DrawTail);
				}
				else
					RenderState.ActorDrawList = new(GSceneMem) TList<FDynamicActor*>(DynamicActor,RenderState.ActorDrawList);

				if( RenderState.Client->Coronas
				&&	DynamicActor->Actor->bCorona 
				&&  (FDistSquared(RenderState.SceneNode->ViewOrigin, DynamicActor->Actor->Location) < Square(DynamicActor->Actor->WorldLightRadius()) || DynamicActor->Actor->LightRadius == 0 )
				&&  RenderState.Zones[iZone].Visible(FBox(DynamicActor->Actor->Location,DynamicActor->Actor->Location)) 
				&&  CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport)   // sjs
				)
					GCoronaRender.AddVisibleLight( DynamicActor->Actor );

				DynamicActor->VisibilityTag = CurrentVisibilityTag;
			}
		}
	}

	unguard;
}

//
//  ExtrudePolygon - Extrudes a polygon into a convex volume.
//

static FConvexVolume* ExtrudePolygon(FRenderState& RenderState,FPoly& Polygon,UBOOL FarClip = 1)
{
	FConvexVolume*	Volume = new(GSceneMem) FConvexVolume;
	FPlane			PolygonPlane = FPlane(Polygon.Vertex[0],Polygon.Normal);

	Volume->BoundingPlanes[Volume->NumPlanes++] = PolygonPlane;

	for(INT EdgeIndex = 0;EdgeIndex < Polygon.NumVertices && Volume->NumPlanes < FConvexVolume::MAX_VOLUME_PLANES - 1;EdgeIndex++)
	{
		FPlane	SidePlane = FPlane(RenderState.SceneNode->ViewOrigin,Polygon.Vertex[EdgeIndex],Polygon.Vertex[(EdgeIndex + 1) % Polygon.NumVertices]);

		Volume->BoundingPlanes[Volume->NumPlanes++] = SidePlane;
	}

	if(FarClip)
	{
		AZoneInfo*	ViewZoneInfo = RenderState.Level->GetZoneActor(RenderState.SceneNode->ViewZone);

		if(ViewZoneInfo->bDistanceFog && (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_DistanceFog))
		{
			FLOAT		FarClip = ViewZoneInfo->DistanceFogEnd;
			FVector		Z = (RenderState.SceneNode->Deproject(FPlane(0,0,0,NEAR_CLIPPING_PLANE)) - RenderState.SceneNode->ViewOrigin).SafeNormal();

			Volume->BoundingPlanes[Volume->NumPlanes++] = FPlane(RenderState.SceneNode->ViewOrigin + Z * FarClip,Z);
		}
	}

	return Volume;
}

//
//  ProcessNode - Render a node and it's coplanars.
//

static void ProcessNode(FRenderState& RenderState,INT iNode,FDynamicLight** DynamicLights,INT NumDynamicLights,FProjectorRenderInfo** DynamicProjectors,INT NumDynamicProjectors)
{
	guard(ProcessNode);

	FBspNode&	Node = RenderState.Model->Nodes(iNode);
	FBspSurf&	Surf = RenderState.Model->Surfs(Node.iSurf);
	FLOAT		PlaneDot = Node.Plane.PlaneDot(RenderState.SceneNode->ViewOrigin);
	INT			IsFront = PlaneDot > 0.0f;

	// Backface cull.

	if(IsFront || (Surf.PolyFlags & (PF_TwoSided | PF_Portal)) || !RenderState.SceneNode->ViewZone)
	{
		// Test for visibility with view frustum/portal volumes.

		if(RenderState.Zones[RenderState.SceneNode->ViewZone ? Node.iZone[IsFront] : 0].Visible(Node.ExclusiveSphereBound))
		{
			DWORD	StencilMask = 0;
			UBOOL	RenderNode = 0;

			if(Surf.PolyFlags & PF_FakeBackdrop)
			{
				if(!(Surf.PolyFlags & PF_Invisible))
					RenderNode = 1;

				if((RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_Backdrop) && RenderState.SceneNode->Recursion < MAX_RECURSION_DEPTH - 1 && RenderState.SceneNode->Viewport->NextStencilMask)
				{
					// Find the corresponding sky zone.

					ASkyZoneInfo*	SkyZoneInfo = NULL;

					if(GIsEditor && RenderState.SceneNode->Viewport->IsPerspective())
					{
						for(INT ZoneIndex = 0;ZoneIndex < RenderState.Model->NumZones;ZoneIndex++)
							if(Cast<ASkyZoneInfo>(RenderState.Level->GetZoneActor(ZoneIndex)))
								SkyZoneInfo = Cast<ASkyZoneInfo>(RenderState.Level->GetZoneActor(ZoneIndex));
					}
					else
						SkyZoneInfo = RenderState.Level->GetZoneActor(Node.iZone[IsFront])->SkyZone;

					AZoneInfo* TestZone = RenderState.Level->GetZoneActor(RenderState.SceneNode->ViewZone);
					if( (SkyZoneInfo) && (!TestZone->bLonelyZone) )
					{
						SkyZoneInfo->LastRenderTime = SkyZoneInfo->Level->TimeSeconds;

						// Create a child scene node for the sky box.
						FSkySceneNode*	SkySceneNode = NULL;

						for(TList<FLevelSceneNode*>* Child = RenderState.ChildSceneNodes;Child;Child = Child->Next)
						{
							if(Child->Element->ViewZone == SkyZoneInfo->Region.ZoneNumber)
								SkySceneNode = (FSkySceneNode*) Child->Element;
						}

						if(!SkySceneNode)
						{
							SkySceneNode = new FSkySceneNode(RenderState.SceneNode,SkyZoneInfo->Region.ZoneNumber);
							RenderState.ChildSceneNodes = new(GSceneMem) TList<FLevelSceneNode*>(SkySceneNode,RenderState.ChildSceneNodes);
						}

						RenderState.SkyStencilMask = SkySceneNode->StencilMask;
						RenderNode = 0;
					}
				}
			}
			else if(Surf.PolyFlags & PF_Mirrored)
			{
				RenderNode = 1;

				if((!GIsEditor || (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl)) && RenderState.SceneNode->Recursion < MAX_RECURSION_DEPTH - 1 && RenderState.SceneNode->Viewport->NextStencilMask)
				{
					FMirrorSceneNode*	MirrorSceneNode = NULL;

					for(TList<FLevelSceneNode*>* Child = RenderState.ChildSceneNodes;Child;Child = Child->Next)
					{
						FMirrorSceneNode*	OtherMirrorSceneNode = Child->Element->GetMirrorSceneNode();

						if(OtherMirrorSceneNode && OtherMirrorSceneNode->MirrorSurface == Node.iSurf && OtherMirrorSceneNode->ViewZone == Node.iZone[IsFront])
							MirrorSceneNode = OtherMirrorSceneNode;
					}

					if(!MirrorSceneNode)
					{
						MirrorSceneNode = new FMirrorSceneNode(RenderState.SceneNode,Node.Plane,Node.iZone[IsFront],Node.iSurf);
						RenderState.ChildSceneNodes = new(GSceneMem) TList<FLevelSceneNode*>(MirrorSceneNode,RenderState.ChildSceneNodes);
					}

					StencilMask = MirrorSceneNode->StencilMask;
				}
			}
			else if(RenderState.SceneNode->ViewZone && (Surf.PolyFlags & PF_Portal))
			{
				RenderNode = 1;

				if(!GIsEditor || (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
				{
					AWarpZoneInfo*	WarpZone = Cast<AWarpZoneInfo>(RenderState.Level->GetZoneActor(Node.iZone[1 - IsFront]));

					if(Surf.Actor && Surf.Actor->Brush && Surf.Actor->Brush->Polys)
					{
						if(!(RenderState.RenderedPortals[Node.iSurf >> 3] & (1 << (Node.iSurf & 7))) && Node.iZone[0] != Node.iZone[1])
						{
							// Build a FPoly containing the portal's vertices.

							FPoly	Polygon = Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly);
							INT		iOppositeZone = Node.iZone[1 - IsFront];

							FMatrix&	LocalToWorld = Surf.Actor->GetActorRenderData()->LocalToWorld;

							Polygon.Normal = LocalToWorld.TransformNormal(Polygon.Normal);

							for(INT VertexIndex = 0;VertexIndex < Polygon.NumVertices;VertexIndex++)
								Polygon.Vertex[VertexIndex] = LocalToWorld.TransformFVector(Polygon.Vertex[VertexIndex]);

							if(!IsFront)
								Polygon.Reverse();

							if(!WarpZone)
							{

								// Quick cull out for Lonely Zones

								AZoneInfo* TestZone = RenderState.Level->GetZoneActor(iOppositeZone);
								AZoneInfo* ViewZone = RenderState.Level->GetZoneActor(RenderState.SceneNode->ViewZone);
 
								if ( (!TestZone->bLonelyZone) && (!ViewZone->bLonelyZone) )
								{
	
									UBOOL	Clipped = 0;

									// Test for manual excludes

									for (int indx=0;indx<ViewZone->ManualExcludes.Num();indx++)
									{
											if (TestZone == ViewZone->ManualExcludes(indx) )
											{
												Clipped = 1;
												break;
											}
									}

									if (!Clipped)
									{

								// Add the portal to the visibility volumes for the opposite zone.

								FLOAT	Area = Polygon.Area();

								for(TList<FConvexVolume*>* AntiPortal = RenderState.Zones[Node.iZone[IsFront]].AntiPortals;AntiPortal;AntiPortal = AntiPortal->Next)
								{
									FPoly	ClippedPolygon = AntiPortal->Element->ClipPolygon(Polygon);
									FLOAT	ClippedArea = ClippedPolygon.Area();

									if(ClippedArea >= Area)
									{
										Clipped = 1;
										break;
									}
									else if(ClippedArea > 0.0f)
										RenderState.Zones[iOppositeZone].AntiPortals = new(GSceneMem) TList<FConvexVolume*>(AntiPortal->Element,RenderState.Zones[iOppositeZone].AntiPortals);
								}

								if(!Clipped)
								{
									for(TList<FConvexVolume*>* Portal = RenderState.Zones[Node.iZone[IsFront]].Portals;Portal;Portal = Portal->Next)
									{
										if(PlaneDot > 1.0f || PlaneDot < -1.0f)
										{
										    FPoly	ClippedPolygon = Portal->Element->ClipPolygon(Polygon);
    
										    if(ClippedPolygon.NumVertices)
										    {
											    FConvexVolume*	PortalVolume = ExtrudePolygon(RenderState,ClippedPolygon);
    
											    RenderState.Zones[iOppositeZone].Portals = new(GSceneMem) TList<FConvexVolume*>(PortalVolume,RenderState.Zones[iOppositeZone].Portals);
										    }
										}
										else
										{
											// The viewer is very close to the portal, simply use the view frustums for the current zone as the visibility volume.

											FConvexVolume*	PortalVolume = Portal->Element;

											RenderState.Zones[iOppositeZone].Portals = new(GSceneMem) TList<FConvexVolume*>(PortalVolume,RenderState.Zones[iOppositeZone].Portals);
										}
									}

									QWORD	OppositeZoneMask = (QWORD) 1 << Node.iZone[1 - IsFront];

									if(!(RenderState.ActiveZoneMask & OppositeZoneMask))
									{
										RenderState.ActiveZones = new(GSceneMem) TList<INT>(iOppositeZone,RenderState.ActiveZones);
										RenderState.ActiveZoneMask |= OppositeZoneMask;

										if(!GIsEditor)
											BuildAntiPortals(RenderState,iOppositeZone);
									}
								}
									}
								}

								RenderNode = 0;
							}
							else
							{
								if(!WarpZone->OtherSideLevel || !WarpZone->OtherSideActor)
								{
									WarpZone->OtherSideLevel = NULL;
									WarpZone->eventGenerate();
								}

								if(WarpZone->OtherSideLevel && WarpZone->OtherSideActor && RenderState.SceneNode->Recursion < MAX_RECURSION_DEPTH - 1 && RenderState.SceneNode->Viewport->NextStencilMask)
								{
									FWarpZoneSceneNode*	WarpZoneSceneNode = NULL;

									for(TList<FLevelSceneNode*>* Child = RenderState.ChildSceneNodes;Child;Child = Child->Next)
									{
										if(Child->Element->ViewZone == WarpZone->iWarpZone)
											WarpZoneSceneNode = (FWarpZoneSceneNode*) Child->Element;
									}

									if(!WarpZoneSceneNode)
									{
										WarpZoneSceneNode = new FWarpZoneSceneNode(RenderState.SceneNode,WarpZone);
										RenderState.ChildSceneNodes = new(GSceneMem) TList<FLevelSceneNode*>(WarpZoneSceneNode,RenderState.ChildSceneNodes);
									}

									StencilMask = WarpZoneSceneNode->StencilMask;
								}
							}

							RenderState.RenderedPortals[Node.iSurf >> 3] |= (1 << (Node.iSurf & 7));
						}
						else
							RenderNode = 0;
					}
				}

				if(!(Surf.PolyFlags & PF_Invisible))
					RenderNode = 1;
			}
			else if(Surf.PolyFlags & PF_AntiPortal)
			{
				if(!GIsEditor || (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
				{
					// Add the antiportal to the visibility volumes for the antiportal's zone.

					if(Surf.Actor && Surf.Actor->Brush && Surf.Actor->Brush->Polys)
					{
						if(!(RenderState.RenderedPortals[Node.iSurf >> 3] & (1 << (Node.iSurf & 7))))
						{
							FPoly	Polygon = Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly);
							FMatrix&	LocalToWorld = Surf.Actor->GetActorRenderData()->LocalToWorld;

							Polygon.Normal = LocalToWorld.TransformNormal(Polygon.Normal);

							for(INT VertexIndex = 0;VertexIndex < Polygon.NumVertices;VertexIndex++)
								Polygon.Vertex[VertexIndex] = LocalToWorld.TransformFVector(Polygon.Vertex[VertexIndex]);

							if(((RenderState.SceneNode->ViewOrigin - Polygon.Vertex[0]) | Polygon.Normal) < 0.0f)
								Polygon.Reverse();

							FConvexVolume*	AntiPortalVolume = ExtrudePolygon(RenderState,Polygon,0);

							RenderState.Zones[Node.iZone[IsFront]].AntiPortals = new(GSceneMem) TList<FConvexVolume*>(AntiPortalVolume,RenderState.Zones[Node.iZone[IsFront]].AntiPortals);

							RenderState.RenderedPortals[Node.iSurf >> 3] |= (1 << (Node.iSurf & 7));
						}
					}

					if(!(Surf.PolyFlags & PF_Invisible))
						RenderNode = 1;
				}
				else
					RenderNode = 1;
			}
			else if(!(Surf.PolyFlags & PF_Invisible) || !(RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
				RenderNode = 1;

			if(RenderNode && Node.iZone[IsFront] != RenderState.SceneNode->InvisibleZone && Node.iSection != INDEX_NONE && (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_BSP))
			{
				if(StencilMask)
				{
					// Add the node to a stencil draw list.

					FBspStencilDrawList*	StencilDrawList = NULL;

					for(TList<FBspStencilDrawList*>* List = RenderState.StencilDrawLists;List;List = List->Next)
					{
						if(List->Element->StencilMask == StencilMask && List->Element->SectionIndex == Node.iSection)
						{
							StencilDrawList = List->Element;
							break;
						}
					}

					if(!StencilDrawList)
					{
						StencilDrawList = new(GSceneMem) FBspStencilDrawList(RenderState.Model,Node.iSection,StencilMask);
						RenderState.StencilDrawLists = new(GSceneMem) TList<FBspStencilDrawList*>(StencilDrawList,RenderState.StencilDrawLists);
					}

					StencilDrawList->AddNode(iNode);
				}

				if(!(Surf.PolyFlags & PF_Mirrored) || !(Surf.PolyFlags & PF_Invisible))
				{
					if(Surf.Material && Surf.Material->RequiresSorting())
					{
						// Add the node to the translucent draw list.

						FTranslucentDrawItem	TranslucentNodeItem;
						TranslucentNodeItem.BSP = 1;
						TranslucentNodeItem.iNode = iNode;
						TranslucentNodeItem.NumDynamicLights = NumDynamicLights;
						TranslucentNodeItem.NumDynamicProjectors = NumDynamicProjectors;

						if(NumDynamicLights)
						{
							TranslucentNodeItem.DynamicLights = New<FDynamicLight*>(GSceneMem,NumDynamicLights);
							appMemcpy(TranslucentNodeItem.DynamicLights,DynamicLights,NumDynamicLights * sizeof(FDynamicLight*));
						}
						else
							TranslucentNodeItem.DynamicLights = NULL;

						if(NumDynamicProjectors)
						{
							TranslucentNodeItem.DynamicProjectors = New<FProjectorRenderInfo*>(GSceneMem,NumDynamicProjectors);
							appMemcpy(TranslucentNodeItem.DynamicProjectors,DynamicProjectors,NumDynamicProjectors * sizeof(FProjectorRenderInfo*));
						}
						else
							TranslucentNodeItem.DynamicProjectors = NULL;

						RenderState.TranslucentDrawList = new(GSceneMem) TList<FTranslucentDrawItem>(TranslucentNodeItem,RenderState.TranslucentDrawList);
					}
					else
					{
						// Add the node to it's section's draw list.

						if(!RenderState.BspDrawLists[Node.iSection])
						{
							RenderState.BspDrawLists[Node.iSection] = new(GSceneMem) FBspDrawList(RenderState.Model,Node.iSection);
							RenderState.SectionDrawList = new(GSceneMem) TList<INT>(Node.iSection,RenderState.SectionDrawList);
						}

						RenderState.BspDrawLists[Node.iSection]->AddNode(iNode,DynamicLights,NumDynamicLights,DynamicProjectors,NumDynamicProjectors,RenderState.SceneNode);

						if(Node.iLightMap != INDEX_NONE && (UPDATE_CHANGED_LIGHTMAPS || UPDATE_DYNAMIC_LIGHTMAPS))
						{
							FLightMap*	LightMap = &RenderState.Model->LightMaps(Node.iLightMap);

							for(INT LightIndex = 0;LightIndex < NumDynamicLights;LightIndex++)
							{
								FDynamicLight*	DynamicLight = DynamicLights[LightIndex];

								if((DynamicLight->Changed && UPDATE_CHANGED_LIGHTMAPS) || (DynamicLight->Dynamic && UPDATE_DYNAMIC_LIGHTMAPS))
								{
									UBOOL	Update = 1;

									for(INT LightIndex = 0;LightIndex < LightMap->DynamicLights.Num();LightIndex++)
										if(LightMap->DynamicLights(LightIndex) == DynamicLight->Actor)
										{
											Update = 0;
											break;
										}

									if(Update)
									{
										if(!LightMap->DynamicLights.Num())
											RenderState.Model->DynamicLightMaps.AddItem(Node.iLightMap);

										LightMap->DynamicLights.AddItem(DynamicLight->Actor);

										LightMap->Revision++;
										RenderState.Model->LightMapTextures(LightMap->iTexture).Revision++;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(Node.iPlane != INDEX_NONE)
		ProcessNode(RenderState,Node.iPlane,DynamicLights,NumDynamicLights,DynamicProjectors,NumDynamicProjectors);

	unguard;
}

//
//  RenderLevel
//

static FRenderState*			RootRenderState = NULL;
static FDynamicLight**			RootDynamicLights = NULL;
static FProjectorRenderInfo**	RootDynamicProjectors = NULL;
static INT						RootNumDynamicLights,
								RootNumDynamicProjectors;

#ifdef WITH_LIPSINC
// The 'Talker' list.
extern TLIPSincTalkerList GLIPSincTalkerList;
#endif

void RenderLevel(FLevelSceneNode* SceneNode,FRenderInterface* RI)
{
	guard(RenderLevel);

	ULevel*	Level = SceneNode->Level;
	UModel*	Model = SceneNode->Model;

	INT	SetupStartTime = appCycles(),
		InitialByteCount = (INT) GSceneMem.GetByteCount();

	// Setup the render state.

	FRenderState	RenderState;

	RenderState.SceneNode	= SceneNode;
	RenderState.RI			= RI;

	RenderState.Level		= Level;
	RenderState.Model		= Model;
	RenderState.Client		= SceneNode->Viewport->GetOuterUClient();

	CurrentVisibilityTag++;

	guard(ResetVisibilityTag);
	if(!CurrentVisibilityTag)
	{
		// The visibility tag wrapped around, reset all actor's visibility tags.

		CurrentVisibilityTag++;

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AActor*	Actor = Level->Actors(ActorIndex);

			if(Actor && Actor->ActorRenderData)
				Actor->ActorRenderData->VisibilityTag = 0;
		}
	}
	unguard;

	// sjs - clamp upper fov bound so carefully crafted cull distances don't break for higher fovs
    
	CullDistanceFOVBias = appTan(Clamp<FLOAT>(SceneNode->Viewport->Actor->FovAngle, 10.0f, 85.0f)*(PI/360.f));
	SceneNode->Viewport->Actor->FOVBias = CullDistanceFOVBias;

	// Corona initialization.

	if(!SceneNode->Parent)
	{
	    GCoronaRender.iVisible	= 0;
	    appMemzero(GCoronaRender.VisibleLights, sizeof(GCoronaRender.VisibleLights));
	}

	// Initialize temporary memory stack.

	FMemMark	MemMark(GSceneMem);

	// Set the view and projection matrices.

	if( SceneNode->Viewport->Actor->UseFixedVisibility )
		RI->SetTransform(TT_WorldToCamera,SceneNode->Viewport->Actor->RenderWorldToCamera);
	else
		RI->SetTransform(TT_WorldToCamera,SceneNode->WorldToCamera);
	RI->SetTransform(TT_CameraToScreen,SceneNode->CameraToScreen);

	// Set the cull mode.		

	RI->SetCullMode(SceneNode->Determinant < 0.0f ? CM_CCW : CM_CW);

	// Setup actors for rendering.

	FDynamicLight*			DynamicLights[256];
	FProjectorRenderInfo*	DynamicProjectors[256];
	INT						NumDynamicLights = 0,
							NumDynamicProjectors = 0;

#ifdef WITH_LIPSINC	
	// Force an update on all currently 'talking' skeletal mesh instances, even if they are not
	// currently visible.
	GLIPSincTalkerList.ForceUpdate();
#endif

	if(!RootRenderState)
	{
		// Initialize lists.

		RenderState.OutsideActors	= NULL;
		RenderState.LeafActors		= NewZeroed<TList<FDynamicActor*>*>(GSceneMem,Model->Leaves.Num());

		// Iterate through all actors, except for the level info and builder brush.
		UBOOL	IsLit = SceneNode->Viewport->IsLit(),
				IsDynamicLit = !SceneNode->Viewport->GetOuterUClient()->NoDynamicLights;

		INT ActorIndex;

		if(GIsEditor)
			ActorIndex = 2;
		else
			ActorIndex = Level->iFirstDynamicActor;

		guard(IterateOverActors);
		for(;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AActor*	Actor = Level->Actors(ActorIndex);
#if __HAS_SSE__
				//WD: **NOTE**: the order of prefetching is directly dependent on the data layout in EngineClasses.h (defined by Actor.uc).
				if( GIsSSE )
				{
					// pull the first cache-line of Actor into cache hierarchy
					_mm_prefetch((char*)&Actor->LightBrightness, _MM_HINT_NTA);       
				}
#endif

			if( Actor )
			{
				if(SceneNode->FilterActor(Actor))
				{
					FDynamicActor*	DynamicActor = Actor->GetActorRenderData();

					// Add the actor to LeafActors for leaves it is in.
					if(SceneNode->ViewZone && Actor->Leaves.Num())
					{
						for(INT LeafIndex = 0;LeafIndex < Actor->Leaves.Num();LeafIndex++)
							RenderState.LeafActors[Actor->Leaves(LeafIndex)] = new(GSceneMem) TList<FDynamicActor*>(DynamicActor,RenderState.LeafActors[Actor->Leaves(LeafIndex)]);
					}
					else if(!SceneNode->ViewZone)
						RenderState.OutsideActors = new(GSceneMem) TList<FDynamicActor*>(DynamicActor,RenderState.OutsideActors);
				}

				if(Actor->LightType != LT_None && IsLit)
				{
					// Ensure the cached FDynamicLight is up to date.
					FDynamicLight*	CachedLight = Actor->GetLightRenderData();

					if(!SceneNode->Parent && (((Actor->bDynamicLight || CachedLight->Dynamic) || ((Actor->bLightChanged || Actor->bDeleteMe) || CachedLight->Changed))))
						CachedLight->Update();

					if((CachedLight->Dynamic || CachedLight->Changed) && NumDynamicLights < 256 && IsDynamicLit)
						DynamicLights[NumDynamicLights++] = CachedLight;
				}
			}
		}
		unguard;

		// Find dynamic projectors we need to render.

		for(INT ProjectorIndex = 0;ProjectorIndex < Level->DynamicProjectors.Num();ProjectorIndex++)
		{
			FProjectorRenderInfo*	ProjectorInfo = Level->DynamicProjectors(ProjectorIndex);

			if(!ProjectorInfo->Render( Level->GetLevelInfo()->TimeSeconds ))
			{
				// Replace this projector with the last dynamic projector, and remove an item from the end of the array.
				Level->DynamicProjectors(ProjectorIndex--) = Level->DynamicProjectors(Level->DynamicProjectors.Num() - 1);
				Level->DynamicProjectors.Remove(Level->DynamicProjectors.Num() - 1);
				continue;
			}

			check(ProjectorInfo->Projector); // There should never be a dynamic projector without an attached actor!

			if(SceneNode->FilterProjector(ProjectorInfo->Projector))
			{
				DynamicProjectors[NumDynamicProjectors++] = ProjectorInfo;

				if(NumDynamicProjectors == 256)
					break;
			}
		}

		// Clear dynamic light lists.

		if(!SceneNode->Parent && IsLit)
		{
			for(INT LightMapIndex = 0;LightMapIndex < Model->DynamicLightMaps.Num();LightMapIndex++)
			{
				FLightMap*	LightMap = &Model->LightMaps(Model->DynamicLightMaps(LightMapIndex));

				LightMap->DynamicLights.Empty();

				LightMap->Revision++;
				Model->LightMapTextures(LightMap->iTexture).Revision++;
			}

			Model->DynamicLightMaps.Empty();
		}

		RootRenderState = &RenderState;
		RootDynamicLights = DynamicLights;
		RootDynamicProjectors = DynamicProjectors;
		RootNumDynamicLights = NumDynamicLights;
		RootNumDynamicProjectors = NumDynamicProjectors;
	}
	else
	{
		RenderState.LeafActors = RootRenderState->LeafActors;
		RenderState.OutsideActors = RootRenderState->OutsideActors;
		appMemcpy(DynamicLights,RootDynamicLights,sizeof(FDynamicLight*) * RootNumDynamicLights);
		appMemcpy(DynamicProjectors,RootDynamicProjectors,sizeof(FProjectorRenderInfo*) * RootNumDynamicProjectors);
		NumDynamicLights = RootNumDynamicLights;
		NumDynamicProjectors = RootNumDynamicProjectors;
	}

	// Initialize rendering variables.

	RenderState.LeafLights		= NewZeroed<FDynamicLight**>(GSceneMem,Model->Leaves.Num());
	RenderState.LeafProjectors	= NewZeroed<FProjectorRenderInfo**>(GSceneMem,Model->Leaves.Num());

	RenderState.RenderedPortals		= NewZeroed<BYTE>(GSceneMem,(Model->Surfs.Num() + 7) / 8);

	RenderState.ActorDrawList		= NULL;
	RenderState.TranslucentDrawList = NULL;

	RenderState.StaticMeshBatchList.Level = Level;
	RenderState.StaticMeshBatchList.Batches = New<INT>(GSceneMem,Level->StaticMeshBatches.Num());
	RenderState.StaticMeshBatchList.VisibleBatchElements = NewZeroed<INT*>(GSceneMem,Level->StaticMeshBatches.Num());
	RenderState.StaticMeshBatchList.NumVisibleBatchElements = New<INT>(GSceneMem,Level->StaticMeshBatches.Num());
	RenderState.StaticMeshBatchList.NumBatches = 0;

	TList<FProjectorRenderBatch*>*	SavedGProjectorBatchList = GProjectorBatchList;
	GProjectorBatchList = NULL;

	RenderState.BspDrawLists = NewZeroed<FBspDrawList*>(GSceneMem,Model->Sections.Num());
	RenderState.SectionDrawList = NULL;
	RenderState.StencilDrawLists = NULL;

	RenderState.SkyStencilMask		= 0;
	RenderState.ChildSceneNodes		= NULL;

	// Setup the view frustum.

	if(SceneNode->Viewport->Precaching)
	{
		RenderState.ActiveZones = NULL;
		for(INT ZoneIndex = 1;ZoneIndex < Model->NumZones;ZoneIndex++)
			RenderState.ActiveZones = new(GSceneMem) TList<INT>(ZoneIndex,RenderState.ActiveZones);

		RenderState.ActiveZoneMask = ((QWORD) 1 << Model->NumZones) - 1;

		for(INT ZoneIndex = 0;ZoneIndex < Model->NumZones;ZoneIndex++)
			RenderState.Zones[ZoneIndex].Portals = new(GSceneMem) TList<FConvexVolume*>(new(GSceneMem) FConvexVolume());
	}
	else
	{
		RenderState.Zones[SceneNode->ViewZone].Portals = new(GSceneMem) TList<FConvexVolume*>(new(GSceneMem) FConvexVolume(SceneNode->GetViewFrustum()));
		RenderState.ActiveZones = new(GSceneMem) TList<INT>(SceneNode->ViewZone);
		RenderState.ActiveZoneMask = (QWORD) 1 << SceneNode->ViewZone;
	}

	if(!GIsEditor)
		BuildAntiPortals(RenderState,SceneNode->ViewZone);

	// Clear the color buffer if outside the level.

	if( !GIsEditor && !SceneNode->Parent && (SceneNode->ViewZone == 0) )
		RI->Clear( 1, FColor(0,0,0,0), 0, 0, 0 );

	// Clear the color buffer to the distance fog color if wanted.

	AZoneInfo* ViewZoneInfo = SceneNode->Level->GetZoneActor(SceneNode->ViewZone);
	if( !GIsEditor && !SceneNode->Parent && ViewZoneInfo->bClearToFogColor && ViewZoneInfo->bDistanceFog )
		RI->Clear( 1, ViewZoneInfo->DistanceFogColor, 0, 0, 0 );

	GStats.DWORDStats( GEngineStats.STATS_Visibility_SetupCycles ) += (appCycles() - SetupStartTime);

	// Traverse the BSP tree.

	INT	TraverseStartTime	= appCycles();

	// Push the root BSP node.

	FNodeStack*	NodeStack = NULL;

	if(!(SceneNode->Viewport->IsWire() && GIsEditor) && Model->Nodes.Num())
	{
		FNodeStack*	NodeStackBase = FNodeStack::Allocate(NumDynamicLights,NumDynamicProjectors);

		appMemcpy(NodeStackBase->DynamicLights,DynamicLights,NumDynamicLights * sizeof(FDynamicLight*));
		appMemcpy(NodeStackBase->DynamicProjectors,DynamicProjectors,NumDynamicProjectors * sizeof(FProjectorRenderInfo*));
		NodeStackBase->Pass = PASS_Front;
		NodeStackBase->iNode = 0;
		NodeStackBase->Outside = Model->RootOutside;
		NodeStackBase->Prev = NULL;

		NodeStack = NodeStackBase;
	}

	// Handle outside antiportals.

	for(TList<FDynamicActor*>* ActorList = RenderState.OutsideActors;ActorList;ActorList = ActorList->Next)
	{
		FDynamicActor*	DynamicActor = ActorList->Element;

		if(DynamicActor->Actor->DrawType == DT_AntiPortal && DynamicActor->Actor->AntiPortal && (RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
		{
			if(RenderState.Zones[0].Visible(DynamicActor->BoundingBox) && CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport)) // sjs
			{
				FConvexVolume*	AntiPortalVolume = ExtrudeAntiPortal(RenderState.SceneNode->ViewOrigin,DynamicActor);
				RenderState.Zones[0].AntiPortals = new(GSceneMem) TList<FConvexVolume*>(AntiPortalVolume,RenderState.Zones[0].AntiPortals);
			}
		}	
	}

	// Process the BSP tree.

	while(NodeStack)
	{
		FBspNode&	Node = Model->Nodes(NodeStack->iNode);

		if(NodeStack->Pass == PASS_Front)
		{
			// Zone mask rejection.

			GStats.DWORDStats( GEngineStats.STATS_Visibility_MaskTests )++;

			if(!(Node.ZoneMask & RenderState.ActiveZoneMask) && SceneNode->ViewZone )
			{
				GStats.DWORDStats( GEngineStats.STATS_Visibility_MaskRejects )++;
				NodeStack = NodeStack->Prev;
				continue;
			}

			// Bound rejection.

			if(Node.iRenderBound != INDEX_NONE)
			{
				FBox&	BoundingBox = Model->Bounds(Node.iRenderBound);
				UBOOL	Visible = 0;

				GStats.DWORDStats( GEngineStats.STATS_Visibility_BoxTests )++;

				if(SceneNode->ViewZone)
				{
					for(TList<INT>* Zone = RenderState.ActiveZones;Zone;Zone = Zone->Next)
					{
						if((Node.ZoneMask & ((QWORD) 1 << Zone->Element)) && RenderState.Zones[Zone->Element].Visible(BoundingBox))
						{
							Visible = 1;
							break;
						}
					}
				}
				else
					Visible = RenderState.Zones[0].Visible(BoundingBox);

				if(!Visible)
				{
					GStats.DWORDStats( GEngineStats.STATS_Visibility_BoxRejects )++;
					NodeStack = NodeStack->Prev;
					continue;
				}
			}

			// Filter dynamic lights through the BSP tree.

			FDynamicLight*			ChildLights[2][256];
			INT						NumChildLights[2] = {0, 0};

			for(INT LightIndex = 0;LightIndex < NodeStack->NumDynamicLights;LightIndex++)
			{
				FDynamicLight*	DynamicLight = NodeStack->DynamicLights[LightIndex];

				if(DynamicLight->Actor->LightEffect == LE_Sunlight)
				{
					ChildLights[1][NumChildLights[1]++] = DynamicLight;
					ChildLights[0][NumChildLights[0]++] = DynamicLight;
				}
				else
				{
					FLOAT	Dist = Node.Plane.PlaneDot(DynamicLight->Position);

					if(Dist > -DynamicLight->Radius)
						ChildLights[1][NumChildLights[1]++] = DynamicLight;

					if(Dist < DynamicLight->Radius)
						ChildLights[0][NumChildLights[0]++] = DynamicLight;
				}
			}

			// Filter dynamic projectors through the BSP tree.

			FProjectorRenderInfo*	ChildProjectors[2][256];
			INT						NumChildProjectors[2] = {0, 0};

			for(INT ProjectorIndex = 0;ProjectorIndex < NodeStack->NumDynamicProjectors;ProjectorIndex++)
			{
				FProjectorRenderInfo*	ProjectorInfo = NodeStack->DynamicProjectors[ProjectorIndex];
				FLOAT					PushOut = FBoxPushOut(Node.Plane,ProjectorInfo->BoundingBoxExtent),
										Dist = Node.Plane.PlaneDot(ProjectorInfo->BoundingBoxCenter);

				if(Dist > -PushOut)
					ChildProjectors[1][NumChildProjectors[1]++] = ProjectorInfo;

				if(Dist < PushOut)
					ChildProjectors[0][NumChildProjectors[0]++] = ProjectorInfo;
			}

			// Push the current node onto the stack for the second pass.

			INT	IsFront = Node.Plane.PlaneDot(SceneNode->ViewOrigin) > 0.0f;

			NodeStack->Pass = PASS_Plane;
			NodeStack->iFarNode = Node.iChild[1 - IsFront];
			NodeStack->FarOutside = Node.ChildOutside(1 - IsFront,NodeStack->Outside);
			NodeStack->iFarLeaf = Node.iLeaf[1 - IsFront];

			appMemcpy(NodeStack->DynamicLights,ChildLights[1 - IsFront],NumChildLights[1 - IsFront] * sizeof(FDynamicLight*));
			NodeStack->NumDynamicLights = NumChildLights[1 - IsFront];

			appMemcpy(NodeStack->DynamicProjectors,ChildProjectors[1 - IsFront],NumChildProjectors[1 - IsFront] * sizeof(FProjectorRenderInfo*));
			NodeStack->NumDynamicProjectors = NumChildProjectors[1 - IsFront];

			if(Node.iChild[IsFront] != INDEX_NONE)
			{
				// Push the front child onto the stack for the first pass.

				FNodeStack*	FrontNodeStack = FNodeStack::Allocate(NumChildLights[IsFront],NumChildProjectors[IsFront]);

				appMemcpy(FrontNodeStack->DynamicLights,ChildLights[IsFront],NumChildLights[IsFront] * sizeof(FDynamicLight*));
				appMemcpy(FrontNodeStack->DynamicProjectors,ChildProjectors[IsFront],NumChildProjectors[IsFront] * sizeof(FProjectorRenderInfo*));
				FrontNodeStack->iNode = Node.iChild[IsFront];
				FrontNodeStack->Outside = Node.ChildOutside(IsFront,NodeStack->Outside);
				FrontNodeStack->Pass = PASS_Front;
				FrontNodeStack->Prev = NodeStack;

				NodeStack = FrontNodeStack;
			}
			else if(Node.iLeaf[IsFront] != INDEX_NONE)
			{
				if(NumChildLights[IsFront])
				{
					ChildLights[IsFront][NumChildLights[IsFront]] = NULL;
					RenderState.LeafLights[Node.iLeaf[IsFront]] = New<FDynamicLight*>(GSceneMem,NumChildLights[IsFront] + 1);
					appMemcpy(RenderState.LeafLights[Node.iLeaf[IsFront]],ChildLights[IsFront],sizeof(FDynamicLight*) * (NumChildLights[IsFront] + 1));
				}

				if(NumChildProjectors[IsFront])
				{
					ChildProjectors[IsFront][NumChildProjectors[IsFront]] = NULL;
					RenderState.LeafProjectors[Node.iLeaf[IsFront]] = New<FProjectorRenderInfo*>(GSceneMem,NumChildProjectors[IsFront] + 1);
					appMemcpy(RenderState.LeafProjectors[Node.iLeaf[IsFront]],ChildProjectors[IsFront],sizeof(FProjectorRenderInfo*) * (NumChildProjectors[IsFront] + 1));
				}

				ProcessLeaf(RenderState,Node.iLeaf[IsFront]);
			}

			continue;
		}
		else
		{
			// Process the node and it's coplanars.

			ProcessNode(RenderState,NodeStack->iNode,NodeStack->DynamicLights,NodeStack->NumDynamicLights,NodeStack->DynamicProjectors,NodeStack->NumDynamicProjectors);

			// Push the back child onto the stack for the first pass.

			if(NodeStack->iFarNode != INDEX_NONE)
			{
				NodeStack->iNode = NodeStack->iFarNode;
				NodeStack->Outside = NodeStack->FarOutside;
				NodeStack->Pass = PASS_Front;
				continue;
			}
			else if(NodeStack->iFarLeaf != INDEX_NONE)
			{
				if(NodeStack->NumDynamicLights)
				{
					RenderState.LeafLights[NodeStack->iFarLeaf] = New<FDynamicLight*>(GSceneMem,NodeStack->NumDynamicLights + 1);
					appMemcpy(RenderState.LeafLights[NodeStack->iFarLeaf],NodeStack->DynamicLights,sizeof(FDynamicLight*) * NodeStack->NumDynamicLights);
					RenderState.LeafLights[NodeStack->iFarLeaf][NodeStack->NumDynamicLights] = NULL;
				}

				if(NodeStack->NumDynamicProjectors)
				{
					RenderState.LeafProjectors[NodeStack->iFarLeaf] = New<FProjectorRenderInfo*>(GSceneMem,NodeStack->NumDynamicProjectors + 1);
					appMemcpy(RenderState.LeafProjectors[NodeStack->iFarLeaf],NodeStack->DynamicProjectors,sizeof(FProjectorRenderInfo*) * NodeStack->NumDynamicProjectors);
					RenderState.LeafProjectors[NodeStack->iFarLeaf][NodeStack->NumDynamicProjectors] = NULL;
				}

				ProcessLeaf(RenderState,NodeStack->iFarLeaf);
			}
			
			NodeStack = NodeStack->Prev;
		}
	};
	
	// If the camera is outside the world, render actors that are outside.
	UBOOL Precaching = RenderState.SceneNode->Viewport->Precaching;
	for(TList<FDynamicActor*>* ActorList = RenderState.OutsideActors;ActorList;ActorList = ActorList->Next)
	{
		FDynamicActor*	DynamicActor = ActorList->Element;

		if(DynamicActor->Actor->DrawType != DT_AntiPortal || !DynamicActor->Actor->AntiPortal || !(RenderState.SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
		{
		    if(Precaching || (RenderState.Zones[0].Visible(DynamicActor->BoundingBox) && CheckCullDistance(RenderState.SceneNode->ViewOrigin,DynamicActor,RenderState.SceneNode->Viewport))) // sjs
			    RenderState.ActorDrawList = new(GSceneMem) TList<FDynamicActor*>(DynamicActor,RenderState.ActorDrawList);
		}
	}

	GStats.DWORDStats( GEngineStats.STATS_Visibility_TraverseCycles ) += (appCycles() - TraverseStartTime);


	// Render child scene nodes first if not using stencil.
	if( !(GIsPixomatic && SceneNode->Viewport->RenDev->SkyboxHack) && !(GIsEditor || SceneNode->Viewport->RenDev->UseStencil ) )
	{
		UBOOL RequiresClear = 0;
		for(TList<FLevelSceneNode*>* Child = RenderState.ChildSceneNodes;Child;Child = Child->Next)
		{
			RequiresClear = 1;
			Child->Element->Render(RI);
			delete Child->Element;
		}
		if( RequiresClear )
			RI->Clear(0,FColor(0,0,0),1,1.0f,0,0);
	}

	// Render terrain.
	if(SceneNode->Viewport->Actor->ShowFlags & SHOW_Terrain)
	{
		INT	TerrainStartTime = appCycles();

		for(INT ZoneIndex = 0;ZoneIndex < Model->NumZones;ZoneIndex++)
		{
			AZoneInfo*	ZoneActor = Level->GetZoneActor(ZoneIndex);

			if(ZoneActor->bTerrainZone && (!SceneNode->ViewZone || (RenderState.ActiveZoneMask & ((QWORD) 1 << ZoneIndex))) && ZoneIndex != SceneNode->InvisibleZone)
			{
				for(INT TerrainIndex = 0;TerrainIndex < ZoneActor->Terrains.Num();TerrainIndex++) 
				{
					ATerrainInfo*	TerrainInfo = ZoneActor->Terrains(TerrainIndex);
					
					if((TerrainInfo->bHighDetail && TerrainInfo->Level->DetailMode == DM_Low) || (TerrainInfo->bSuperHighDetail && TerrainInfo->Level->DetailMode != DM_SuperHigh))
						continue;
						
					if( GIsPixomatic && (TerrainInfo->bHighDetail || TerrainInfo->bSuperHighDetail) )
						continue;

					if( !TerrainInfo->CollapsedLayers && GIsPixomatic && SceneNode->Viewport->RenDev->LowQualityTerrain )
					{					
						SceneNode->Viewport->RenDev->TerrainLOD = Clamp<DWORD>( SceneNode->Viewport->RenDev->TerrainLOD, 3, 11 );
						INT TextureSize = 1 << SceneNode->Viewport->RenDev->TerrainLOD;

						FAuxRenderTarget TempRenderTarget(TextureSize,TextureSize,TEXF_RGBA8);

						RI->PushState();
						if( RI->SetRenderTarget(&TempRenderTarget) )
						{
							RI->Clear(1,FColor(0,0,0,255));
							RI->SetDistanceFog(0,0,0,0);
							RI->SetCullMode( CM_None );
											
							FVector Extent = TerrainInfo->Vertices( TerrainInfo->GetGlobalVertex( TerrainInfo->HeightmapX-1, TerrainInfo->HeightmapY-1 ) )
											-TerrainInfo->Vertices( TerrainInfo->GetGlobalVertex( 0, 0 ) );
							Extent *= 0.5f;

							FVector ViewOrigin		= TerrainInfo->Location + FVector( 0, 0, 20000 );
							FMatrix WorldToCamera	= FTranslationMatrix(-ViewOrigin) * FMatrix(FPlane( Extent.X > 0 ? 1 : -1,			0,								0,							0),
																								FPlane(0,								Extent.Y < 0 ? 1 : -1,			0,							0),
																								FPlane(0,								0,								-1,							0),
																								FPlane(0,								0,								-ViewOrigin.Z,				1));
							FMatrix	CameraToScreen	= FOrthoMatrix( Abs(Extent.X), Abs(Extent.Y), 0.5f / HALF_WORLD_MAX, HALF_WORLD_MAX );

							RI->SetTransform( TT_WorldToCamera, WorldToCamera );
							RI->SetTransform( TT_CameraToScreen, CameraToScreen );

							RI->PixoSetHint( PIXOHINT_Bilinear );
							TerrainInfo->Render(
								SceneNode,
								RI,
								NULL,
								NULL,
								0,
								NULL,
								0
							);
							RI->PixoResetHint( PIXOHINT_Bilinear );
						}
						RI->PopState();
						
						TerrainInfo->CollapsedLayers = RI->PixoCreateTexture( &TempRenderTarget, 1 );
					}

					RI->PushState();

					if( TerrainInfo->Inverted )
						RI->SetCullMode(SceneNode->Determinant < 0.0f ? CM_CW : CM_CCW);

					TerrainInfo->Render(
						SceneNode,
						RI,
						SceneNode->ViewZone ? &RenderState.Zones[ZoneIndex] : &RenderState.Zones[0],
						DynamicLights,
						NumDynamicLights,
						DynamicProjectors,
						NumDynamicProjectors
						);

					RI->PopState();
				}
			}
		}

		GStats.DWORDStats( GEngineStats.STATS_Terrain_RenderCycles ) += (appCycles() - TerrainStartTime);
	}

	// Render solid BSP sufaces.
	RI->PushState();
	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);

	for(TList<INT>* SectionList = RenderState.SectionDrawList;SectionList;SectionList = SectionList->Next)
		RenderState.BspDrawLists[SectionList->Element]->Render(RenderState.SceneNode,RI);

	// Render solid actors.
	for(TList<FDynamicActor*>* ActorDrawList = RenderState.ActorDrawList;ActorDrawList;ActorDrawList = ActorDrawList->Next)
		RenderActor(RenderState,ActorDrawList->Element);

	RenderState.StaticMeshBatchList.Render(SceneNode,RI);

	if(!SceneNode->Viewport->HitTesting)
		for(TList<FProjectorRenderBatch*>* ProjectorBatchList = GProjectorBatchList;ProjectorBatchList;ProjectorBatchList = ProjectorBatchList->Next)
			ProjectorBatchList->Element->Render(SceneNode,RI);

	GProjectorBatchList = NULL;

	RI->PopState();

	if( GIsEditor || SceneNode->Viewport->RenDev->UseStencil )
	{
		// Render stencil surfaces.
		for(TList<FBspStencilDrawList*>* StencilDrawList = RenderState.StencilDrawLists;StencilDrawList;StencilDrawList = StencilDrawList->Next)
			StencilDrawList->Element->Render(SceneNode,RI);

		// Write the sky stencil mask to untouched pixels.
		if(RenderState.SkyStencilMask)
		{
			INT	SkyStencilStartCycle = appCycles();

			RI->PushState();

			DECLARE_STATIC_UOBJECT(
				UProxyBitmapMaterial,
				HackMaterial,
				{
					static FSolidColorTexture	BlackTexture(FColor(0,0,0));
					HackMaterial->SetTextureInterface(&BlackTexture);
				}
				);

			DECLARE_STATIC_UOBJECT(
				UFinalBlend,
				SkyStencilFinalBlend,
				{
					SkyStencilFinalBlend->Material = HackMaterial;
					SkyStencilFinalBlend->FrameBufferBlending = FB_Invisible;
					SkyStencilFinalBlend->ZWrite = 0;
					SkyStencilFinalBlend->TwoSided = 1;
				}
				);

			RI->SetStencilOp(CF_Always,~RenderState.SkyStencilMask,~0,SO_Keep,SO_Keep,SO_Replace,RenderState.SkyStencilMask);
			FCanvasUtil(&SceneNode->Viewport->RenderTarget,RI).DrawTile(
				0,0,
				SceneNode->Viewport->SizeX,SceneNode->Viewport->SizeY,
				0,0,
				0,0,
				1,
				SkyStencilFinalBlend,
				FColor(0,0,0)
				);

			RI->PopState();

			GStats.DWORDStats( GEngineStats.STATS_Stencil_RenderCycles ) += (appCycles() - SkyStencilStartCycle);
		}

		// Render child scene nodes.
		for(TList<FLevelSceneNode*>* Child = RenderState.ChildSceneNodes;Child;Child = Child->Next)
		{
			Child->Element->Render(RI);
			delete Child->Element;
		}
	}

	// The below assumes that there is only one level of child scenenodes, but then again, we already assume that 
	// there are no mirrors and warp portals so it's not really a restriction.
	if( (GIsPixomatic && SceneNode->Viewport->RenDev->SkyboxHack) && !SceneNode->Parent )
	{
		RI->PixoSetHint( PIXOHINT_Skybox );
		for(TList<FLevelSceneNode*>* Child = RenderState.ChildSceneNodes;Child;Child = Child->Next)
		{
			Child->Element->Render(RI);
			delete Child->Element;
		}
		RI->PixoResetHint( PIXOHINT_Skybox );
	}

	// Render translucent actors.

    TArray<FDynamicActor*> deferredActors; // sjs

	for(TList<FTranslucentDrawItem>* TranslucentDrawList = RenderState.TranslucentDrawList;TranslucentDrawList;TranslucentDrawList = TranslucentDrawList->Next)
	{
		if(TranslucentDrawList->Element.BSP)
	    {
			FBspDrawList			ScratchList(Model);
			FTranslucentDrawItem&	Item = TranslucentDrawList->Element;
			FBspNode&				Node = Model->Nodes(Item.iNode);

			ScratchList.SectionIndex = Node.iSection;
			ScratchList.Nodes = &Item.iNode;
			ScratchList.AddNode(Item.iNode,Item.DynamicLights,Item.NumDynamicLights,Item.DynamicProjectors,Item.NumDynamicProjectors,SceneNode);
			ScratchList.Render(SceneNode,RI);
	    }
		else
        {
            if( (TranslucentDrawList->Element.DynamicActor->Actor->DrawType==DT_Particle || TranslucentDrawList->Element.DynamicActor->Actor->Style == STY_Additive) 
			&&  TranslucentDrawList->Element.DynamicActor->Actor->bDeferRendering )
            {
                deferredActors.AddItem(TranslucentDrawList->Element.DynamicActor);
            }
            else
			{
                RenderActor(RenderState,TranslucentDrawList->Element.DynamicActor);

				if( RenderState.StaticMeshBatchList.NumBatches )
					RenderState.StaticMeshBatchList.Render(SceneNode,RI);
			}
        }

		if(!SceneNode->Viewport->HitTesting)
			for(TList<FProjectorRenderBatch*>* ProjectorBatchList = GProjectorBatchList;ProjectorBatchList;ProjectorBatchList = ProjectorBatchList->Next)
				ProjectorBatchList->Element->Render(SceneNode,RI);

		GProjectorBatchList = NULL;
	}

	GProjectorBatchList = SavedGProjectorBatchList;

	// Render terrain decorations.
	if( SceneNode->Viewport->GetOuterUClient()->DecoLayers && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Terrain) )
	{
		INT	TerrainDecoStartTime = appCycles();

		RI->PushState();

		for(INT ZoneIndex = 0;ZoneIndex < Model->NumZones;ZoneIndex++)
		{
			AZoneInfo*	ZoneActor = Level->GetZoneActor(ZoneIndex);

			if(ZoneActor->bTerrainZone && (!SceneNode->ViewZone || (RenderState.ActiveZoneMask & ((QWORD) 1 << ZoneIndex))) && ZoneIndex != SceneNode->InvisibleZone)
			{
				for(INT TerrainIndex = 0;TerrainIndex < ZoneActor->Terrains.Num();TerrainIndex++) 
				{
					ATerrainInfo*	TerrainInfo = ZoneActor->Terrains(TerrainIndex);

					if( TerrainInfo->Inverted )
						RI->SetCullMode(SceneNode->Determinant < 0.0f ? CM_CW : CM_CCW);

					TerrainInfo->RenderDecorations(SceneNode,RI,SceneNode->ViewZone ? &RenderState.Zones[ZoneIndex] : &RenderState.Zones[0]);

					if( TerrainInfo->Inverted )
						RI->SetCullMode(SceneNode->Determinant < 0.0f ? CM_CCW : CM_CW);
				}
			}
		}

		RI->PopState();
		GStats.DWORDStats( GEngineStats.STATS_DecoLayer_RenderCycles ) += (appCycles() - TerrainDecoStartTime);
	}

    // sjs ---
    for( int i=0; i<deferredActors.Num(); i++ )
        RenderActor(RenderState,deferredActors(i));
    // --- sjs

	// Render coronas.
	if( !SceneNode->Parent && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Coronas) )
	{
		RI->PushState();

		GCoronaRender.RenderCoronas( SceneNode, RI );

		RI->PopState();
	}

	// Render the builder brush.
	if( GIsEditor && SceneNode->FilterActor(Level->Brush()) )
	{
		FDynamicActor*	DynamicActor = Level->Brush()->GetActorRenderData();
		FConvexVolume	Frustum(SceneNode->GetViewFrustum());
		if(Frustum.BoxCheck(DynamicActor->BoundingBox.GetCenter(), DynamicActor->BoundingBox.GetExtent()) & CF_Inside)
			DynamicActor->Render(SceneNode,NULL,NULL,RI);
	}

#if 0
	// Render antiportal volumes.

	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
	for(INT ZoneIndex = 0;ZoneIndex < Model->NumZones;ZoneIndex++)
		for(TList<FConvexVolume*>* AntiPortalList = RenderState.Zones[ZoneIndex].AntiPortals;AntiPortalList;AntiPortalList = AntiPortalList->Next)
			FLineBatcher(RI,0).DrawConvexVolume(*AntiPortalList->Element,FColor(255,0,0));
#endif

	// Cleanup.
	GStats.DWORDStats( GEngineStats.STATS_Visibility_ScratchBytes ) += (DWORD) (GSceneMem.GetByteCount() - InitialByteCount);

	if(RootRenderState == &RenderState)
		RootRenderState = NULL;

	MemMark.Pop();

	unguardf((TEXT("%s"),SceneNode->Level->GetPathName()));
}

