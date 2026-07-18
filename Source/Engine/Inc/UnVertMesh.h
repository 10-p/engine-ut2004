/*=============================================================================
	UnVertMesh.h: Unreal vertex-animated mesh objects.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
	    * An adaptation of Unreal's classic vertex animated mesh class.
		* Updated to use Chris Hargrove's mesh instancing concept.

=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

//
// TODO: 
//
// * Straigtforward frames structure, choice between compression methods, ability to
//   read the old U1 format and new ones with floating point UV's..
// * Nifty vertex-compression tricks ?
//
//

// Forward declarations.
class UVertMesh;
class UVertMeshInstance;


//
// A vertex-animated mesh instance.
//

class ENGINE_API UVertMeshInstance : public ULodMeshInstance
{
	DECLARE_CLASS(UVertMeshInstance, ULodMeshInstance, 0, Engine)

	TArray <FVector> CachedMeshPoints; // Cached vertex frame for tweening
	TArray <FVector> CachedMeshNormals;
	FLOAT	CachedFrame;
	FName	CachedSeq;	
	INT     CachedLodVerts;
	UVertMesh* CachedMesh;
	FLOAT   TweenIndicator;	
	FLOAT   MeshScaleMax; // Max of x/y/z mesh scale for LOD gauging (works on top of drawscale).

	// Animation blending (Vertex animation: for now only one channel supported.
	MeshAnimChannel BaseChannel;       // Animation blending.


	//  Object interface.
	void Serialize( FArchive& Ar );	
	// Constructor.
	UVertMeshInstance()
	{
		CachedMesh = NULL;
	}

	// UpdateAnimation moves task of animation advancement to the actor's mesh instance.
	UBOOL UpdateAnimation(FLOAT DeltaSeconds);
	// PlayAnim moves task of initializing animation to the actor's mesh instance.
	UBOOL PlayAnim(INT Channel,FName SequenceName, FLOAT InRate, FLOAT InTweenTime, UBOOL InLooping);

	// Return number of animations supported by the mesh instance.
	INT GetAnimCount();
	// Return whether any animation active for channel.
	UBOOL IsAnimating(INT Channel);
	// Stop animating all channels.
	UBOOL StopAnimating();
	// Return animation for a given index.
	HMeshAnim GetAnimIndexed(INT InIndex);
	// Return animation for a given name.
	HMeshAnim GetAnimNamed(FName InName);
	// Get the name of a given animation.
	FName AnimGetName(HMeshAnim InAnim);
	// Get the group of a given animation.
	FName AnimGetGroup(HMeshAnim InAnim);
	// See if an animation has this particular group tag.
	UBOOL AnimIsInGroup(HMeshAnim InAnim, FName Group);
	// Get the number of frames in an animation.
	FLOAT AnimGetFrameCount(HMeshAnim InAnim);
	// Get the play rate of the animation in frames per second.
	FLOAT AnimGetRate(HMeshAnim InAnim);
	// Get the number of notifications associated with this animation.
	INT AnimGetNotifyCount(HMeshAnim InAnim);
	// Get the time of a particular notification.
	FLOAT AnimGetNotifyTime(HMeshAnim InAnim, INT InNotifyIndex);
	// Get text associated with a given notify.
	const TCHAR* AnimGetNotifyText(HMeshAnim InAnim, INT InNotifyIndex);
	// Get function associated with given notify.
	UAnimNotify* AnimGetNotifyObject(HMeshAnim InAnim, INT InNotifyIndex);
	// change to void* => AnimGetNotifyObject() ? #debug

	UBOOL IsAnimTweening(INT Channel=0);
	UBOOL IsAnimLooping( INT Channel=0);
	UBOOL IsAnimPastLastFrame( INT Channel=0);
	UBOOL AnimStopLooping( INT Channel=0);

	FName GetActiveAnimSequence(INT Channel=0);
	FLOAT GetActiveAnimRate(INT Channel=0);
	FLOAT GetActiveAnimFrame(INT Channel=0);
	void SetAnimFrame(INT Channel, FLOAT NewFrame, INT UnitFlag=0 );

	UBOOL AnimForcePose( FName SeqName, FLOAT AnimFrame, FLOAT Delta, INT Channel = 0 );

	// TODO -clean up texture retrieval logic - some sources obsolete?
	UMaterial* GetMaterial( INT Count, AActor* Owner )
	{
		guardSlow(UMesh::GetMaterial);
		if( Owner && Owner->GetSkin( Count ) )
			return Owner->GetSkin( Count );
		else if( Count!=0 && ((ULodMesh*)GetMesh())->Materials.Num() > Count )
			return ((ULodMesh*)GetMesh())->Materials(Count);
		else if( Owner && Owner->Skins.Num() )
			return Owner->Skins(0);
		else if( Count>=0 &&((ULodMesh*)GetMesh())->Materials.Num() > Count )
			return ((ULodMesh*)GetMesh())->Materials(Count);
		else if( Count >= ((ULodMesh*)GetMesh())->Materials.Num() && ((ULodMesh*)GetMesh())->Materials.Num()>0 )
			return ((ULodMesh*)GetMesh())->Materials(0);
		else
			return NULL;
		unguardSlow;
	}
		
	FMeshAnimSeq* GetAnimSeq( FName SeqName );	

	void GetFrame(AActor* Owner, FLevelSceneNode* SceneNode, FVector*	ResultVerts, INT Size, INT& LODRequest, DWORD TaskFlag);
	void GetMeshVerts(AActor* Owner, FVector* ResultVerts, INT Size, INT& LODRequest);

	// Render support.
	virtual FMatrix MeshToWorld();	
	void Render(FDynamicActor* Owner,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI);
	
	// Helper(s) at construction time.
	void MeshBuildBounds();
	void SetScale( FVector NewScale );

	// Defined in both, but points to meshinstance implementation.
	FBox GetRenderBoundingBox( const AActor* Owner );
	FSphere GetRenderBoundingSphere( const AActor* Owner );

};

//
// A vertex animated mesh resource.
//

class ENGINE_API UVertMesh : public ULodMesh
{
	DECLARE_CLASS(UVertMesh, ULodMesh, 0, Engine)

	// Objects.
	TArray<FMeshVert>				AnimVerts;   // (animated) vertices; 	
	TArray<FMeshNorm>               AnimNormals; // Animated 16-bit compressed normals.
 	TArray<FLOAT>					FrameKeys;   // Variable-time frames: each frame its own time key.
	
	// Vertex-animated meshes include all their animation data and anim-sequences.
	TArray<FMeshAnimSeq>			AnimSeqs;	
	TArray<FBox>					BoundingBoxes;
	TArray<FSphere>					BoundingSpheres;//!!currently broken

	// Misc Internal.
	INT    FrameVerts;     // Total animation vertices (?)	
	INT    AnimFrames;     // legacy frame count
	
	FAnimMeshVertexStream VertexStream; // Vertex buffer for hardware rendering.
    FMeshLODChunk LODChunks[4];         // 4 levels of static LOD - Vertex uses only the [0]th with dynamic lod.
	

	//  Object interface.
	void Serialize( FArchive& Ar );	
	void PostLoad();

	// Constructor.
	UVertMesh()
	{
	}

	UClass* MeshGetInstanceClass() { return UVertMeshInstance::StaticClass(); }

	// Preprocessing.
	INT RenderPreProcess();

	// Defined in both, but points to meshinstance implementation.
	FBox GetRenderBoundingBox( const AActor* Owner );
	FSphere GetRenderBoundingSphere( const AActor* Owner );
	
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif
