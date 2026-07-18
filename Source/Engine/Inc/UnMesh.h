/*=============================================================================

	UnMesh.h: Unreal Engine mesh instance / abstract base object.
	Copyright 2001-2002 Epic Games, Inc. All Rights Reserved.

    Pseudo-abstract mesh base.

=============================================================================*/

#ifndef _INC_UNMESH
#define _INC_UNMESH

#include "UnForcePacking_begin.h"

// Forward declaration
class UMesh;
class UMeshInstance;

// Flags
enum MInstanceFlags
{
	MINST_InUse        = 0x00000001,
	MINST_DeleteMe     = 0x00000002
};

enum GetFrameFlags
{
	GF_FullSkin  = 0,
	GF_RawVerts  = 1,
	GF_BonesOnly = 2,
	GF_RootOnly  = 3,
};


/*-----------------------------------------------------------------------------
	UMesh - pseudo-abstract mesh class.
-----------------------------------------------------------------------------*/
class ENGINE_API UMesh : public UPrimitive
{
	DECLARE_CLASS(UMesh, UPrimitive, 0, Engine)

protected:
    UMeshInstance* DefMeshInstance; // Default instance, used with null actor GetInstance calls.

public:
    // constructor
    UMesh()
	{
		DefMeshInstance=NULL;
	}
	
    // Retrieve instance class associated with this mesh class.
    virtual UClass* MeshGetInstanceClass() { return NULL; }
	
    // Get a mesh instance for a particular actor (default implementation exists but may be overridden)
    virtual UMeshInstance* MeshGetInstance( const AActor* InActor );	

    virtual class USkeletalMesh* GetUSkeletalMesh() { return NULL; } // sjs
	// Serialization ( garbage collection! )
	void Serialize( FArchive& Ar );		

	

};

/*----------------------------------------------------------------------------
  UMeshInstance - pseudo-abstract mesh instance class.
----------------------------------------------------------------------------*/
class ENGINE_API UMeshInstance : public UPrimitive
{
	DECLARE_CLASS( UMeshInstance, UPrimitive, 0, Engine)

    virtual class USkeletalMeshInstance* GetUSkeletalMeshInstance() { return NULL; } // sjs
	// Get or assign the owner for this meshinstance.
	virtual AActor* GetActor() { return NULL; }
	virtual void SetActor(AActor* InActor) {}

	// Get/set the mesh associated with the mesh instance.
	virtual UMesh* GetMesh() { return NULL; }
	virtual void SetMesh(UMesh* InMesh) {}

	// Status queries
	virtual void SetStatus(INT Flags){}
	virtual int  GetStatus() {return 0;}

	// Animation methods

	// UpdateAnimation moves task of animation advancement to the actor's mesh instance.
	virtual UBOOL UpdateAnimation(FLOAT DeltaSeconds) { return 0; }
	// PlayAnim moves task of initializing animation to the actor's mesh instance.
	virtual UBOOL PlayAnim(INT Channel,FName SequenceName, FLOAT InRate, FLOAT InTweenTime, UBOOL InLooping) { return 0; }

	// Return number of animations supported by the mesh instance.
	virtual INT GetAnimCount() { return 0; }
	// Return animation for a given index.
	virtual HMeshAnim GetAnimIndexed(INT InIndex) { return NULL; }
	// Return animation for a given name
	virtual HMeshAnim GetAnimNamed(FName InName) { return NULL; }
	// Get the name of a given animation
	virtual FName AnimGetName(HMeshAnim InAnim) { return NAME_None; }
	// Get the group of a given animation
	virtual FName AnimGetGroup(HMeshAnim InAnim) { return NAME_None; }
	// Find if animation has certain group tag.
	virtual UBOOL AnimIsInGroup(HMeshAnim InAnim, FName Group) { return false; }
	// Get the number of frames in an animation
	virtual FLOAT AnimGetFrameCount(HMeshAnim InAnim) { return 0.f; }
	// Get the play rate of the animation in frames per second
	virtual FLOAT AnimGetRate(HMeshAnim InAnim) { return 15.f; }
	// Get the number of notifications associated with this animation.
	virtual INT AnimGetNotifyCount(HMeshAnim InAnim) { return 0; }
	// Get the time of a particular notification.
	virtual FLOAT AnimGetNotifyTime(HMeshAnim InAnim, INT InNotifyIndex) { return 0.f; }
	// Get text associated with a given notify.
	virtual const TCHAR* AnimGetNotifyText(HMeshAnim InAnim, INT InNotifyIndex) { return TEXT(""); }
	// Get function associated with given notify.
	virtual UAnimNotify* AnimGetNotifyObject(HMeshAnim InAnim, INT InNotifyIndex){ return NULL; }
	// change to void* => AnimGetNotifyObject() ? #debug
	virtual UBOOL IsAnimating(INT Channel=0){ return false; }
	// Stop all animation.
	virtual UBOOL StopAnimating(){ return false; }
	virtual UBOOL FreezeAnimAt( FLOAT Time, INT Channel ){ return false; }

	virtual UBOOL IsAnimTweening(INT Channel=0){ return false; }
	virtual UBOOL IsAnimLooping( INT Channel=0){ return false; }
	virtual UBOOL IsAnimPastLastFrame( INT Channel=0){ return false; }
	virtual UBOOL AnimStopLooping( INT Channel=0){ return false; }	

	virtual FName GetActiveAnimSequence(INT Channel=0){ return NAME_None; }
	virtual FLOAT GetActiveAnimRate(INT Channel=0){ return 0.f; }
	virtual FLOAT GetActiveAnimFrame(INT Channel=0){ return 0.f; }

	// sjs --- adding these in this interface to avoid casting hell for deca
	virtual UBOOL SetBlendParams( INT Channel, FLOAT BlendAlpha, FLOAT InTime, FLOAT OutTime, FName StartBoneName, UBOOL bGlobalPose ) { return 1; }
    virtual UBOOL GetAttachMatrix( FName boneName, FMatrix& mat ) { return false; }
    virtual INT GetAttachCount() { return 0; }
    virtual UBOOL GetAttachIdx( int idx, FName& outName, FMatrix& outMat ) { return false; }
    virtual UBOOL CheckAnimFinished(INT channel) { return true; }
    virtual UBOOL SetBoneLocation( FName boneName, FVector boneTrans, float alpha ) { return false; }
    virtual UBOOL SetBoneRotation( FName boneName, FRotator boneRot, INT Space, float alpha ) { return false; }
    virtual UBOOL SetBoneScale( INT slot, FLOAT scale, FName boneName ) { return false; }
    virtual INT   MatchRefBone( FName StartBoneName ) { return -1; }
    virtual void DrawVolume( AActor* Owner, FRenderInterface* RI, FVector volDir ) {}
	virtual bool InitSkeletalPhysics(void) { return false; }
	virtual bool TickSkeletalPhysics(float delta) { return false; }
	// --- sjs


	// PostNetReceive animation state network reconstruction.
	virtual void SetAnimFrame(INT Channel, FLOAT NewFrame, INT UnitFlag = 0 ) {}
	virtual UBOOL AnimForcePose( FName SeqName, FLOAT AnimFrame, FLOAT Delta, INT Channel = 0 ){ return false; };

	// UPrimitive interface. Default implementations point back to source mesh.

	virtual UBOOL PointCheck(FCheckResult& Result, AActor* Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags)
	{
		return GetMesh()->PointCheck(Result, Owner, Location, Extent, ExtraNodeFlags);
	}
	virtual UBOOL LineCheck(FCheckResult& Result, AActor* Owner, const FVector& End, const FVector& Start, const FVector& Extent, DWORD ExtraNodeFlags, DWORD TraceFlags)
	{
		return GetMesh()->LineCheck(Result, Owner, End, Start, Extent, ExtraNodeFlags, TraceFlags);
	}
	virtual FBox GetRenderBoundingBox(const AActor* Owner)
	{
		return GetMesh()->GetRenderBoundingBox(Owner);
	}
	virtual FSphere GetRenderBoundingSphere( const AActor* Owner)
	{
		return GetMesh()->GetRenderBoundingSphere(Owner);
	}
	virtual FBox GetCollisionBoundingBox(const AActor* Owner)
	{
		return GetMesh()->GetCollisionBoundingBox(Owner);
	}



	virtual UMaterial* GetMaterial( INT Count, AActor* Owner ){ return NULL; }

	virtual void GetFrame(AActor* Owner, FLevelSceneNode* SceneNode, FVector*	ResultVerts, INT Size, INT& LODRequest, DWORD TaskFlag){};		

	// Render support.
	virtual FMatrix MeshToWorld(){ return FMatrix::Identity; }
	virtual void Render(FDynamicActor* Owner,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI){};

	// Mesh creation methods. Usually point back to the mesh.
	// Set drawing scale.
	virtual void SetScale( FVector NewScale ) {};
	// Bounds generation. 
	virtual void MeshBuildBounds(){};
	
};

#include "UnForcePacking_end.h"

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
#endif

