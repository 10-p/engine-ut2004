/*=============================================================================

	UnVertMesh.cpp: Unreal vertex-animated mesh functions.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* March 2001 - Ported from the original Unreal vertex animation code.

=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	UVertMesh instance functions
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UVertMesh);

//
// Each mesh class now responsible for its own ticking and animation.
// VertMeshes are fairly close to how everything worked in UT, with only a few 
// animation/state variables for a single animation in Actor.
//

void UVertMeshInstance::Serialize( FArchive& Ar )
{
	guard(UVertMeshInstance::Serialize);

	Super::Serialize(Ar);
	if( !Ar.IsPersistent() )
	{
		Super::Serialize(Ar);
		Ar << CachedMeshPoints;
		Ar << CachedMeshNormals;
		Ar << CachedFrame;
		Ar << CachedSeq;
		Ar << CachedMesh;
		Ar << TweenIndicator;
		Ar << MeshScaleMax;
	}

	unguardobj;
}

void UVertMesh::PostLoad()
{
	guard(UVertMesh::PostLoad);
	Super::PostLoad();
	//OLDVER: convert old notifies to objects
	for( INT i=0;i<AnimSeqs.Num();i++ )
		AnimSeqs(i).UpdateOldNotifies(GetOuter());
	unguard;
}

// Legacy GetAnimSeq 
FMeshAnimSeq* UVertMeshInstance::GetAnimSeq( FName SeqName )
{
	guardSlow(UVertMeshInstance::GetAnimSeq);
	UVertMesh* Mesh = (UVertMesh*)GetMesh();

	for( INT i=0; i<Mesh->AnimSeqs.Num(); i++ )
		if( SeqName == Mesh->AnimSeqs(i).Name )
			return &Mesh->AnimSeqs(i);
	return NULL;

	unguardSlow;
}

void UVertMeshInstance::SetAnimFrame(INT Channel, FLOAT NewFrame, INT UnitFlag )
{
	guard(UVertMeshInstance::SetAnimFrame);

	if( UnitFlag == 0)
	{
		// NewFrame in 0-1 range		
		BaseChannel.AnimFrame = NewFrame;
	}
	else
	{
		// NewFrame in 0-(TotalFrames-1) range, where the interfalc (TotalFrames-2)  up to (TotalFrames-1) cycles the pose from the last frame to the first..
		FLOAT NumFrames = AnimGetFrameCount( GetAnimNamed( BaseChannel.AnimSequence ) );		
		BaseChannel.AnimFrame = (NumFrames >= 1.0f) ? NewFrame/(NumFrames-1.0f) : NewFrame ;		 
	}

	unguard;
}
	
UBOOL UVertMeshInstance::UpdateAnimation(FLOAT DeltaSeconds) 
{ 
	guard(UpdateAnimation);
	SetStatus( MINST_InUse );
	INT Iterations = 0;

	while
	(	OurActor->IsAnimating(0)
	&&	(DeltaSeconds>0.0f)
	&&	(++Iterations <= 4) 
	&&  !( GetStatus() & MINST_DeleteMe ) )
	{
		// Remember the old frame.
		FLOAT OldAnimFrame = BaseChannel.AnimFrame;

		// Update animation, and possibly overflow it.
		if( BaseChannel.AnimFrame >= 0.0f )
		{
			// Update regular or velocity-scaled animation.
			if( BaseChannel.AnimRate >= 0.0f )
				BaseChannel.AnimFrame += BaseChannel.AnimRate * DeltaSeconds;
			else
				BaseChannel.AnimFrame += ::Max( 0.3f, OurActor->Velocity.Size() * -BaseChannel.AnimRate ) * DeltaSeconds;
			//FIXME - instead of fixed 0.3, make it an animation attribute?

			// Handle all animation sequence notifies.
			if( BaseChannel.bAnimNotify && OurActor->Mesh ) //#debug
			{				
				HMeshAnim Seq = GetAnimNamed( BaseChannel.AnimSequence );
				if( Seq )
				{
					FLOAT BestElapsedFrames = 100000.0f;
					INT BestNotifyIdx = -1;

					for( INT i=0; i< AnimGetNotifyCount(Seq) ; i++ )
					{
						FLOAT NotifyTime = AnimGetNotifyTime( Seq, i);

						if( OldAnimFrame < NotifyTime && BaseChannel.AnimFrame >= NotifyTime )
						{
							FLOAT ElapsedFrames = NotifyTime - OldAnimFrame;
							if( BestNotifyIdx == -1 || ElapsedFrames<BestElapsedFrames )
							{
								BestElapsedFrames = ElapsedFrames;
								BestNotifyIdx        = i;
							}
						}
					}
					if( BestNotifyIdx > -1 )
					{
						DeltaSeconds = DeltaSeconds * (BaseChannel.AnimFrame - AnimGetNotifyTime( Seq, BestNotifyIdx)  ) / (BaseChannel.AnimFrame - OldAnimFrame);
						BaseChannel.AnimFrame = AnimGetNotifyTime( Seq, BestNotifyIdx);
						UAnimNotify* AnimNotify = AnimGetNotifyObject(Seq, BestNotifyIdx);
						if( AnimNotify )
							AnimNotify->Notify( this, OurActor );
						continue;
					}
				}
			}

			// Handle end of animation sequence.
			if( BaseChannel.AnimFrame<BaseChannel.AnimLast )
			{
				// We have finished the animation updating for this tick.
				break;
			}
			else if( BaseChannel.bAnimLoop )
			{
				if( BaseChannel.AnimFrame < 1.0f )
				{
					// Still looping.
					DeltaSeconds = 0.0f;
				}
				else
				{
					// Just passed end, so loop it.
					DeltaSeconds = DeltaSeconds * (BaseChannel.AnimFrame - 1.0f) / (BaseChannel.AnimFrame - OldAnimFrame);
					BaseChannel.AnimFrame = 0.0f;
				}
				if( OldAnimFrame < BaseChannel.AnimLast )
					OurActor->NotifyAnimEnd(0);
			}
			else 
			{
				// Just passed end-minus-one frame.
				DeltaSeconds = DeltaSeconds * (BaseChannel.AnimFrame - BaseChannel.AnimLast) / (BaseChannel.AnimFrame - OldAnimFrame);
				BaseChannel.AnimFrame	 = BaseChannel.AnimLast;
				BaseChannel.AnimRate      = 0.0f;
				OurActor->NotifyAnimEnd(0);
			}
		}
		else
		{
			// Update tweening.
			BaseChannel.AnimFrame += BaseChannel.TweenRate * DeltaSeconds;
			if( BaseChannel.AnimFrame >= 0.0f )
			{
				// Finished tweening.
				DeltaSeconds          = DeltaSeconds * (BaseChannel.AnimFrame-0) / (BaseChannel.AnimFrame - OldAnimFrame);
				BaseChannel.AnimFrame = 0.0f;
				if( BaseChannel.AnimRate == 0.0f )
					OurActor->NotifyAnimEnd(0);
			}
			else
			{
				// Finished tweening.
				break;
			}
		}
	}
	OurActor->ReplicateAnim(0,BaseChannel.AnimSequence, BaseChannel.AnimRate, BaseChannel.AnimFrame, BaseChannel.TweenRate, BaseChannel.AnimLast, BaseChannel.bAnimLoop);

	// Check deletion status.
	if( GetStatus() & MINST_DeleteMe )
	{
		//debugf(TEXT("Vertex mesh instance deleting itself: [%s]"),this->GetName()); 
		delete this;		
	}
	else
	{
		SetStatus(0);
	}

	return true;
	unguard;
}



UBOOL UVertMeshInstance::PlayAnim(INT channel,FName SequenceName, FLOAT InRate, FLOAT InTweenTime, UBOOL InLooping) 
{ 
	guard(UVertMeshInstance::PlayAnim);
	HMeshAnim InAnim = GetAnimNamed( SequenceName );
	if( !InAnim ) 
	{
		debugf( TEXT("PlayAnim: Sequence '%s' not found for mesh '%s'"), *SequenceName, OurMesh->GetName() );
		return false;
	}
	AActor* Actor = GetActor();
	if (!Actor )
		return false;

	// Regular playActor->Anim.
	if( !InLooping && (InRate > 0.f) )	
	{
		if( BaseChannel.AnimSequence == NAME_None )
			InTweenTime = 0.0f;

		FLOAT SeqFrameCount = AnimGetFrameCount( InAnim );
		BaseChannel.AnimSequence  = SequenceName;
		BaseChannel.OrigRate	  =	AnimGetRate( InAnim ) / SeqFrameCount;
		BaseChannel.AnimRate      = InRate * BaseChannel.OrigRate;
		BaseChannel.AnimLast      = 1.0f - 1.0f / SeqFrameCount;
		BaseChannel.bAnimNotify   = AnimGetNotifyCount( InAnim ) != 0;
		BaseChannel.bAnimFinished = 0;
		BaseChannel.bAnimLoop     = 0;
		if( BaseChannel.AnimLast == 0.0f )
		{
			BaseChannel.bAnimNotify   = 0;
			BaseChannel.OldAnimRate   = 0.0f;
			if( InTweenTime > 0.0f )
				BaseChannel.TweenRate = 1.0f / InTweenTime;
			else
				BaseChannel.TweenRate = 10.0f; //tween in 0.1 sec
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
			BaseChannel.AnimRate = 0.f;
		}
		else if( InTweenTime>0.0f )
		{
			BaseChannel.TweenRate = 1.0f / (InTweenTime * SeqFrameCount);
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
		}
		else if ( InTweenTime == -1.0f )
		{
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
			if ( BaseChannel.OldAnimRate > 0.0f )
				BaseChannel.TweenRate = BaseChannel.OldAnimRate;
			else if ( BaseChannel.OldAnimRate < 0.0f ) //was velocity based looping
				BaseChannel.TweenRate = ::Max(0.5f * BaseChannel.AnimRate, -1 * Actor->Velocity.Size() * BaseChannel.OldAnimRate );
			else
				BaseChannel.TweenRate =  1.0f/(0.025f * SeqFrameCount);
		}
		else
		{
			BaseChannel.TweenRate = 0.0f;
			BaseChannel.AnimFrame = 0.001f;
		}
		BaseChannel.OldAnimRate = BaseChannel.AnimRate;

		return 1;
	}

	// Regular LoopActor->Anim
	if( InLooping )	
	{
		FLOAT SeqFrameCount = AnimGetFrameCount( InAnim );
		FLOAT SeqRate = AnimGetRate( InAnim );

		if ( (BaseChannel.AnimSequence == SequenceName) && BaseChannel.bAnimLoop && Actor->IsAnimating(0) )
		{
			BaseChannel.OrigRate      = AnimGetRate( InAnim ) / SeqFrameCount;
			BaseChannel.AnimRate      = InRate * BaseChannel.OrigRate;
			BaseChannel.bAnimFinished = 0;
			BaseChannel.OldAnimRate   = BaseChannel.AnimRate;		
			return 1;
		}

		//if( BaseChannel.AnimSequence == NAME_None )
		//	InTweenTime = 0.0f;

		BaseChannel.AnimSequence  = SequenceName;
		BaseChannel.OrigRate      = SeqRate / SeqFrameCount;
		BaseChannel.AnimRate      = InRate * BaseChannel.OrigRate;		
		BaseChannel.AnimLast      = 1.0f - 1.0f / SeqFrameCount;
		BaseChannel.bAnimNotify   = AnimGetNotifyCount( InAnim ) != 0;
		BaseChannel.bAnimFinished = 0;
		BaseChannel.bAnimLoop     = 1;
		if ( BaseChannel.AnimLast == 0.0f )
		{
			BaseChannel.bAnimNotify   = 0;
			BaseChannel.OldAnimRate   = 0;
			if ( InTweenTime > 0.0f )
				BaseChannel.TweenRate = 1.0f / InTweenTime;
			else
				BaseChannel.TweenRate = 10.0f; //tween in 0.1 sec
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
			BaseChannel.AnimRate = 0;
		}
		else if( InTweenTime>0.0f )
		{
			BaseChannel.TweenRate = 1.0f / (InTweenTime * SeqFrameCount);
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
		}
		else if ( InTweenTime == -1.0f )
		{
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
			if ( BaseChannel.OldAnimRate > 0 )
				BaseChannel.TweenRate = BaseChannel.OldAnimRate;
			else if ( BaseChannel.OldAnimRate < 0 ) //was velocity based looping
				BaseChannel.TweenRate = ::Max(0.5f * BaseChannel.AnimRate, -1 * Actor->Velocity.Size() * BaseChannel.OldAnimRate );
			else
				BaseChannel.TweenRate =  1.0f/(0.025f * SeqFrameCount);
		}
		else
		{
			BaseChannel.TweenRate = 0.0f;
			BaseChannel.AnimFrame = 0.0001f;
		}
		BaseChannel.OldAnimRate = BaseChannel.AnimRate;

		return 1;
	}


	// Regular Actor->TweenActor->Anim.
	if( InRate == 0.0f )	
	{
		FLOAT SeqFrameCount = AnimGetFrameCount( InAnim );
		BaseChannel.AnimSequence  = SequenceName;
		BaseChannel.AnimLast      = 0.0f;
		BaseChannel.bAnimNotify   = 0;
		BaseChannel.bAnimFinished = 0;
		BaseChannel.bAnimLoop     = 0;
		BaseChannel.AnimRate      = 0;
		BaseChannel.OldAnimRate   = 0;
		if( InTweenTime>0.0f )
		{
			BaseChannel.TweenRate =  1.0f/(InTweenTime * SeqFrameCount);
			BaseChannel.AnimFrame = -1.0f/SeqFrameCount;
		}
		else
		{
			BaseChannel.TweenRate = 0.0f;
			BaseChannel.AnimFrame = 0.0f;
		}

		return 1;
	}

	// Unknown combination of rates and flags.
	return 0;
	unguard;
};


// Return number of animations supported by the mesh instance.
INT UVertMeshInstance::GetAnimCount()
{
	guardSlow(UVertMeshInstance::GetAnimCount);	
	return ((UVertMesh*)GetMesh())->AnimSeqs.Num();
	unguardSlow;
}
// Return animation for a given index.
HMeshAnim UVertMeshInstance::GetAnimIndexed(INT InIndex)
{
	guardSlow(UVertMeshInstance::GetAnimIndexed);
	return (HMeshAnim) &((UVertMesh*)GetMesh())->AnimSeqs( InIndex );
	unguardSlow;
}

// Return animation for a given name
HMeshAnim UVertMeshInstance::GetAnimNamed(FName InName)
{
	guardSlow(UVertMeshInstance::GetAnimSeq);
		for( INT i=0; i<((UVertMesh*)GetMesh())->AnimSeqs.Num(); i++ )
			if( InName == ((UVertMesh*)GetMesh())->AnimSeqs(i).Name )
				return (HMeshAnim) &(((UVertMesh*)GetMesh())->AnimSeqs(i)); // return a disguised FmeshAnimSeq pointer.
		return NULL;
	unguardSlow;
}

// Get the name of a given animation
FName UVertMeshInstance::AnimGetName(HMeshAnim InAnim)
{
	return ((FMeshAnimSeq*)InAnim)->Name;
}
// Get the group of a given animation - backward compatible: return only first group.
FName UVertMeshInstance::AnimGetGroup(HMeshAnim InAnim)
{
	if( ((FMeshAnimSeq*)InAnim)->Groups.Num() )
		return ((FMeshAnimSeq*)InAnim)->Groups(0);
	else
		return NAME_None;
}
// See if an animation has this particular group tag.
UBOOL UVertMeshInstance::AnimIsInGroup(HMeshAnim InAnim, FName Group)
{
	INT i=0;
	return ((FMeshAnimSeq*)InAnim)->Groups.FindItem(Group,i) ;
}
// Get the number of frames in an animation
FLOAT UVertMeshInstance::AnimGetFrameCount(HMeshAnim InAnim)
{
	return ((FMeshAnimSeq*)InAnim)->NumFrames;
}
// Get the play rate of the animation in frames per second
FLOAT UVertMeshInstance::AnimGetRate(HMeshAnim InAnim)
{
	return ((FMeshAnimSeq*)InAnim)->Rate;
}
// Get the number of notifications associated with this animation.
INT UVertMeshInstance::AnimGetNotifyCount(HMeshAnim InAnim)
{
	return ((FMeshAnimSeq*)InAnim)->Notifys.Num();
}
// Get the time of a particular notification.
FLOAT UVertMeshInstance::AnimGetNotifyTime(HMeshAnim InAnim, INT InNotifyIndex)
{
	return ((FMeshAnimSeq*)InAnim)->Notifys(InNotifyIndex).Time;
}
// Get text associated with a given notify.
const TCHAR* UVertMeshInstance::AnimGetNotifyText(HMeshAnim InAnim, INT InNotifyIndex)
{
	return *(((FMeshAnimSeq*)InAnim)->Notifys(InNotifyIndex).Function); // FName to string
}
// Get UAnimNotify object associated with a given notify.
UAnimNotify* UVertMeshInstance::AnimGetNotifyObject(HMeshAnim InAnim, INT InNotifyIndex)
{
	return ((FMeshAnimSeq*)InAnim)->Notifys(InNotifyIndex).NotifyObject;
}

UBOOL UVertMeshInstance::AnimForcePose( FName SeqName, FLOAT AnimFrame, FLOAT Delta, INT Channel)
{
	if( Channel != 0 )
	{
		debugf(TEXT("Invalid active channel: [%i] for MeshInstance [%s]"), Channel, GetName());
		return 0;
	}		
	BaseChannel.AnimFrame = AnimFrame;
	BaseChannel.AnimSequence = SeqName; 

	return 1;
}

//
// UVertMesh  vertex animated mesh data.
//
void UVertMesh::Serialize( FArchive& Ar )
{
	guard(UVertMesh::Serialize);

	// Serialize parent's variables
	Super::Serialize(Ar);
	
	// Serialize the additional UVertMesh variables.
	Ar <<  VertexStream;
	Ar <<  AnimVerts;
	Ar <<  FrameKeys;
	Ar <<  AnimSeqs;	

	Ar <<  AnimNormals;    // animated 16-bit compressed normals.
	Ar <<  FrameVerts;     // Total animation vertices (?)	
	Ar <<  AnimFrames;     // legacy frame count

	Ar <<  BoundingBoxes;
	Ar <<  BoundingSpheres;

	// Serialize - only for garbage collection.
	if( !Ar.IsPersistent() )
	{
		Ar << LODChunks[0];
		Ar << LODChunks[1];
		Ar << LODChunks[2];
		Ar << LODChunks[3];
	}

	unguardobj;
}



// Mesh implementations point to the MeshInstance's definition.
FBox UVertMesh::GetRenderBoundingBox( const AActor* Owner )
{
    guard(UVertMesh::GetRenderBoundingBox);
	return ((UVertMeshInstance*)MeshGetInstance(Owner))->GetRenderBoundingBox( Owner );
    unguard;
}

FSphere UVertMesh::GetRenderBoundingSphere( const AActor* Owner )
{
    guard(UVertMesh::GetRenderBoundingSphere);
	return ((UVertMeshInstance*)MeshGetInstance(Owner))->GetRenderBoundingSphere( Owner );
    unguard;
}


//
// Get the untransformed ( in mesh-space ) visibility bounding box for this primitive, as owned by Owner.
//
FBox UVertMeshInstance::GetRenderBoundingBox( const AActor* Owner )
{
	guard(UVertMeshInstance::GetRenderBoundingBox);
	FBox Bound;
	UVertMesh* Mesh = (UVertMesh*)GetMesh();

	// Get frame indices.
	INT iFrame1 = 0, iFrame2 = 0;
	const FMeshAnimSeq *Seq = GetAnimSeq( BaseChannel.AnimSequence );

	if( Seq && (BaseChannel.AnimFrame>=0.0f) )
	{
		// Animating, so use bound enclosing two frames' bounds.
		INT iFrame = appFloor((BaseChannel.AnimFrame+1.0f) * Seq->NumFrames);
		iFrame1    = Seq->StartFrame + ((iFrame + 0) % Seq->NumFrames);
		iFrame2    = Seq->StartFrame + ((iFrame + 1) % Seq->NumFrames);
		Bound      = Mesh->BoundingBoxes(iFrame1) + Mesh->BoundingBoxes(iFrame2);
	}
	else
	{
		// Interpolating, so be pessimistic and use entire-mesh bound.
		Bound = Mesh->BoundingBox;	
	}
	return Bound;
	unguard;
}

// Get the rendering bounding sphere.
FSphere UVertMeshInstance::GetRenderBoundingSphere( const AActor* Owner )
{
	guard(UVertMesh::GetRenderBoundingSphere);
	return GetMesh()->BoundingSphere;
	unguard;
}


void UVertMeshInstance::SetScale( FVector NewScale )
{
	guard(UVertMesh::SetScale);

	UVertMesh* Mesh = (UVertMesh*)GetMesh();

	Mesh->Scale = NewScale;
	// Maximum mesh scaling dimension for LOD gauging.
	Mesh->MeshScaleMax = (1.f/ 128.f) * Mesh->BoundingSphere.W * Max(Abs(Mesh->Scale.X), Max(Abs(Mesh->Scale.Y), Abs(Mesh->Scale.Z)));
	//debugf(TEXT("New MeshScaleMax %f "),MeshScaleMax);  

	unguard;
}

UBOOL UVertMeshInstance::IsAnimating(INT Channel)
{
	guard(UVertMeshInstance::IsAnimating);

	if( OurActor )
	{
		if ( Channel == 0 )
		{
				return	
					(BaseChannel.AnimSequence != NAME_None)
				&&	(BaseChannel.AnimFrame>=0 ? BaseChannel.AnimRate!=0.f : BaseChannel.TweenRate!=0.f);
		}
		else
		{
			return 0;
		}			
	}
	return 0;
	unguard;
}

UBOOL UVertMeshInstance::StopAnimating()
{
	guard(UVertMeshInstance::IsAnimating);
	if( OurActor )
	{
		if(1)//if ( Channel == 0 )
		{
			//#SKEL 
			BaseChannel.AnimSequence = NAME_None;
			BaseChannel.AnimFrame = 0.0f;
			BaseChannel.AnimRate = 0.0f;
			return 1;			
		}
		else
		{
			return 0;
		}			
	}
	return 0;
	unguard;
}

//#SKEL
UBOOL UVertMeshInstance::IsAnimTweening(INT Channel)
{ 
	return false; 
}

UBOOL UVertMeshInstance::IsAnimLooping(INT Channel)
{ 
	return BaseChannel.bAnimLoop; 
}


UBOOL UVertMeshInstance::IsAnimPastLastFrame(INT Channel)
{ 
	return ( BaseChannel.AnimFrame < BaseChannel.AnimLast );
}

UBOOL UVertMeshInstance::AnimStopLooping(INT Channel)
{ 	
	BaseChannel.bAnimLoop = 0;
	BaseChannel.bAnimFinished = 0;
	return true;
}

FName UVertMeshInstance::GetActiveAnimSequence(INT Channel)
{ 
	if( Channel==0)
		return( BaseChannel.AnimSequence );
	return NAME_None; 
}

FLOAT UVertMeshInstance::GetActiveAnimRate(INT Channel)
{ 
	if( Channel==0)
		return( BaseChannel.AnimRate );
	return 0.f; 
}
FLOAT UVertMeshInstance::GetActiveAnimFrame(INT Channel)
{ 
	if( Channel==0)
		return( BaseChannel.AnimFrame );
	return 0.f; 
}


//
// The vertex mesh instance contains the stored last-getframe-result-mesh (for tweening purposes).
//

IMPLEMENT_CLASS(UVertMeshInstance);

	
//
// Build bounding boxes for each animation frame of the mesh,
// and one bounding box enclosing all animation frames.
//
void UVertMeshInstance::MeshBuildBounds()
{
	guard(UVertMeshInstance::MeshBuildBounds);

	UVertMesh* Mesh = (UVertMesh*)GetMesh();

	GWarn->StatusUpdatef( 0, 0, TEXT("Bounding mesh") );

	// Bound all frames.
	TArray<FVector> AllFrames;

	for( INT i=0; i<Mesh->AnimFrames; i++ )
	{
		TArray<FVector> OneFrame;
		for( INT j=0; j<Mesh->FrameVerts; j++ )
		{
			FVector Vertex = Mesh->Verts( i * Mesh->FrameVerts + j ).Vector();
			OneFrame .AddItem( Vertex );
			AllFrames.AddItem( Vertex );
		}
		Mesh->BoundingBoxes  (i) = FBox   ( &OneFrame(0), OneFrame.Num() );
		Mesh->BoundingSpheres(i) = FSphere( &OneFrame(0), OneFrame.Num() );
	}

	Mesh->BoundingBox    = FBox   ( &AllFrames(0), AllFrames.Num() );
	Mesh->BoundingSphere = FSphere( &AllFrames(0), AllFrames.Num() );

	// Display bounds.
	debugf
	(
		NAME_Log,
		TEXT("Mesh: %s BoundingBox (%f,%f,%f)-(%f,%f,%f) BoundingSphere (%f,%f,%f) %f  animframes: %i") ,
		Mesh->GetName(),
		Mesh->BoundingBox.Min.X,
		Mesh->BoundingBox.Min.Y,
		Mesh->BoundingBox.Min.Z,
		Mesh->BoundingBox.Max.X,
		Mesh->BoundingBox.Max.Y,
		Mesh->BoundingBox.Max.Z,
		Mesh->BoundingSphere.X,
		Mesh->BoundingSphere.Y,
		Mesh->BoundingSphere.Z,
		Mesh->BoundingSphere.W,
		Mesh->AnimFrames
	);

	unguard;
}

//
// Pre-processing of a vertex mesh: builds the vertex buffer and per-material sections.
//
INT UVertMesh::RenderPreProcess()
{
	guard(UVertMesh::RenderPreProcess);

	// Check if already preprocessed
	if(	LODChunks[0].Sections.Num() )	
		return 0; 	
	// Use only the first of the 4 static index buffers.

	// Set Vertices from wedges;
	VertexStream.Vertices.Empty();
	VertexStream.Vertices.Add(Wedges.Num());
	VertexStream.Revision++;

	for( INT WedgeIdx=0; WedgeIdx < Wedges.Num(); WedgeIdx++ )
	{
		FAnimMeshVertex*	Vert = &(VertexStream.Vertices(WedgeIdx));

		// Both vertex and normals set dynamically in getframe..

		// INT PointIndex = Wedges(WedgeIdx).iVertex;
		// Vert.Position = Points(PointIndex);
		// Vert.Normal = Points(PointIndex) + ( Normals(PointIndex) * 0.1f);  
		// uses 'offset point', easier for skeletal combination of normals.
		Vert->U = (Wedges(WedgeIdx).TexUV.U);
		Vert->V = (Wedges(WedgeIdx).TexUV.V);
	}

	debugf(TEXT("Preprocessing:  Vertex stream total vertices: %i Orig wedges: %i"), VertexStream.Vertices.Num(), Wedges.Num());

	//
	// Create 'material' sections, with proper max/min & material indices, but the rest 
	// will be set dynamically;
	// NumTriangles could be changed/ added: StartFace/MaxFaces,
	// this'll give LOD an easy time (not having to look at materials etc)
	//

	// Only one level - dynamic LOD within that.
	INT LodIdx = 0;

	FLODMeshSection* Section = NULL;

	for(INT FaceIndex = 0;FaceIndex < Faces.Num(); FaceIndex++)
	{
		FMeshFace ThisFace = Faces(FaceIndex);
		INT  ThisMatIndex = ThisFace.MeshMaterialIndex;
			
		if( !Section || ( ThisMatIndex != Section->MaterialIndex ) )
		{
			//debugf(TEXT("New section %i for material index %i"),this->LODChunks[LodIdx].Sections.Num(),ThisMatIndex ); 
		
			// Create a new static mesh section.
			Section = new(this->LODChunks[LodIdx].Sections) FLODMeshSection;
			Section->FirstFace = FaceIndex;
			Section->MaterialIndex = ThisFace.MeshMaterialIndex;
			// Indices filled later, dynamically
			// Section->FirstIndex = LODChunks[LodIdx].IndexBuffer.Indices.Num(); 
			//Section->MinIndex = Wedges.Num();
			//Section->MaxIndex = 0; 

			Section->MinIndex = 0;
			Section->MaxIndex = Wedges.Num()-1; 

			Section->TotalFaces = 0;			
			Section->TotalFaces = 0;
		}

		// For each vertex in the triangle...
		// #ERROR: this does not properly take into account any LOD.
		/*
		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			INT ThisVertIdx = ThisFace.iWedge[VertexIndex]; 
			// Update the current section's minimum/maximum index hints.
			Section->MinIndex = Min<_WORD>(Section->MinIndex, ThisVertIdx );
			Section->MaxIndex = Max<_WORD>(Section->MaxIndex, ThisVertIdx );
		}
		*/

		// Update the section's triangle count.
		Section->TotalFaces++;
	}
	// debugf(TEXT("LODChunk [%i] IndexBuffer indices: %i"),LodIdx,LODChunks[LodIdx].IndexBuffer.Indices.Num());
	// #TODO - delete geometry data made redundant by having vertex/index buffers (too dangerous currently - game-time-only?)

	return 1;
	unguard;
}

//
// Do whatever is needed for this particular mesh class to use GetFrame to get 
// properly scaled vertices (3d vectors only, no normals) for conversion purposes (brushes)
//
void UVertMeshInstance::GetMeshVerts
( 
	AActor*		Owner,
	FVector*	ResultVerts,
	INT			Size,
	INT&		LODRequest	
)
{
	guard(UVertMeshInstance::GetMeshVerts);

	// Let Getframe know we only want vertices.	
	TArray <FVector> NewVerts;

	UVertMesh* Mesh = (UVertMesh*)GetMesh();
	NewVerts.AddZeroed(Mesh->ModelVerts);

	// GetFrame will return just vectors if size = sizeof(FVector).
	GetFrame( Owner, NULL, &NewVerts(0), sizeof(FVector), LODRequest, GF_RawVerts);
	// Now add the scaling which, for GetFrame, was left to the hardware...	
	FCoords MeshConversion;
	if(bIgnoreMeshOffset)
	{
		MeshConversion = GMath.UnitCoords *(Owner->Location)* Owner->Rotation * FScale( Mesh->Scale * Owner->DrawScale3D * Owner->DrawScale,0.0,SHEER_None);
	}
	else
	{
		MeshConversion = GMath.UnitCoords *(Owner->Location + Owner->PrePivot)* Owner->Rotation * Mesh->RotOrigin * FScale( Mesh->Scale * Owner->DrawScale3D * Owner->DrawScale,0.0,SHEER_None);
		MeshConversion.Origin += Mesh->Origin;
	}

	FVector* OutVert = ResultVerts;	
	for( INT v=0; v<LODRequest; v++)
	{
		*OutVert = (NewVerts(v)).TransformPointBy( MeshConversion );
		*(BYTE**)&OutVert += Size;		
	}

	unguard;
}


// Stat drawing
void VertDrawStat(UCanvas* Canvas,FColor Color,INT X,const TCHAR* Format,...)
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

//
// Vertex animation 3D vertex setup. No transformations necessary (all delegated to hardware).
// Normals are compressed along with vertices, in a special compact 15-bit format.
// If memory space is a concern (consoles) and you can do without tweening,
// it could all be done without the stored CachedVerts-per-instance.
//
void UVertMeshInstance::GetFrame
(
	AActor*		Owner,
	FLevelSceneNode* SceneNode,
	FVector*	ResultVerts,
	INT			Size,
	INT&		LODRequest,
	DWORD       TaskFlag
)
{
	guard(UVertMeshInstance::GetFrame);

	UVertMesh& Mesh = *(UVertMesh*)GetMesh();
	
	AActor*	AnimOwner = NULL;

	// Check to see if bAnimByOwner
	if ((Owner->bAnimByOwner) && (Owner->Owner != NULL))
		AnimOwner = Owner->Owner;
	else
		AnimOwner = Owner;

	// Determine how many vertices to lerp; in case of tweening, we're limited 
	// by the previous cache size also.
	INT VertsRequested = Min(LODRequest, Mesh.ModelVerts);	
	INT VertexNum = VertsRequested;

	// Create or get cache memory.
	UBOOL WasCached  = 1;

	if( CachedMeshPoints.Num() == 0 || CachedMesh != GetMesh() ) 
	{
		if( CachedMesh != GetMesh() )
		{
			// Actor's mesh changed.
			CachedMeshPoints.Empty();
			CachedMeshNormals.Empty();
		}
		// Full size cache (for now.) We don't want to have to realloc every time our LOD scales up a bit...
		CachedMeshPoints.Add(Mesh.ModelVerts);
		CachedMeshNormals.Add(Mesh.ModelVerts);
		WasCached = 0;
		TweenIndicator = 1.0f;
	}

	if( !WasCached )
	{
		CachedMesh  =(UVertMesh*) GetMesh();
		CachedSeq   = NAME_None;
		CachedFrame = 0.f;
	}

	FVector* CachedVerts    = &CachedMeshPoints(0);
	FVector* CachedNorms    = &CachedMeshNormals(0);
	const FMeshAnimSeq* Seq = GetAnimSeq( BaseChannel.AnimSequence );

	// Coords                  = Coords * (Owner->Location + Owner->PrePivot) * Owner->Rotation * Mesh.RotOrigin * FScale(Mesh.Scale * Owner->DrawScale3D * Owner-> DrawScale,0.f,SHEER_None);
	// FCoords LocalCoords = Coords;
	// LocalCoords.Origin += Mesh.Origin;

	if( BaseChannel.AnimFrame>=0.f || !WasCached )
	{		
		LODRequest = VertexNum; // How many regular vertices returned.
		CachedLodVerts = VertexNum;  //

		// Compute interpolation numbers.
		FLOAT Alpha=0.f;
		INT iFrameOffset1=0, iFrameOffset2=0;
		if( Seq )
		{
			FLOAT Frame   = ::Max( BaseChannel.AnimFrame,0.f) * Seq->NumFrames;
			INT iFrame    = appFloor(Frame);
			Alpha         = Frame - iFrame;
			iFrameOffset1 = (Seq->StartFrame + ((iFrame + 0) % Seq->NumFrames)) * Mesh.ModelVerts;
			iFrameOffset2 = (Seq->StartFrame + ((iFrame + 1) % Seq->NumFrames)) * Mesh.ModelVerts;

			//debugf(TEXT(" ModelVerts %i FrameOffset 1, 2  %i %i  Total vertices: %i "),Mesh.ModelVerts,iFrameOffset1,iFrameOffset2,Mesh.Verts.Num() ) ; 
		}
		else 
		{
			//debugf(TEXT(" Static ModelVerts %i FrameOffset 1, 2  %i %i  Total vertices: %i "),Mesh.ModelVerts,iFrameOffset1,iFrameOffset2,Mesh.Verts.Num() ) ; 
		}

		// Special case Alpha 0. 
		if( Alpha <= 0.f )
		{
			// Initialize a single frame.
			FMeshVert* MeshVertex = &Mesh.Verts( iFrameOffset1 );
			FMeshNorm* MeshNormal = &Mesh.AnimNormals( iFrameOffset1 );

			for( INT i=0; i<VertexNum; i++ )
			{
				// Expand new vector from stored compact integers.
				CachedVerts[i] = FVector( MeshVertex[i].X,  MeshVertex[i].Y,  MeshVertex[i].Z );
				CachedNorms[i] = FVector( MeshNormal[i].X-512.0f , MeshNormal[i].Y-512.0f , MeshNormal[i].Z-512.0f  );  
				// Transform all points into screenspace.
			}	
		}
		else
		{	
			// Interpolate two frames.
			FMeshVert* MeshVertex1 = &Mesh.Verts( iFrameOffset1 );
			FMeshVert* MeshVertex2 = &Mesh.Verts( iFrameOffset2 );

			FMeshNorm* MeshNormal1 = &Mesh.AnimNormals( iFrameOffset1 );
			FMeshNorm* MeshNormal2 = &Mesh.AnimNormals( iFrameOffset2 );
			
			for( INT i=0; i<VertexNum; i++ )
			{
				FVector V1( MeshVertex1[i].X, MeshVertex1[i].Y, MeshVertex1[i].Z );
				FVector V2( MeshVertex2[i].X, MeshVertex2[i].Y, MeshVertex2[i].Z );
				CachedVerts[i] = V1 + (V2-V1)*Alpha;

				FVector N1( MeshNormal1[i].X-512.0f,  MeshNormal1[i].Y-512.0f , MeshNormal1[i].Z-512.0f  );
				FVector N2( MeshNormal2[i].X-512.0f , MeshNormal2[i].Y-512.0f , MeshNormal2[i].Z-512.0f  );
				CachedNorms[i] =( N1 + (N2-N1)*Alpha);  
			}
		}	
	}
	else // Tween: cache present, and starting from Animframe < 0.f
	{
		
		// Any requested number within CACHE limit is ok, since 
		// we cannot tween more than we have in the cache.
		VertexNum  = Min(VertexNum,CachedLodVerts);
		CachedLodVerts = VertexNum;
		LODRequest = VertexNum; // how many regular vertices returned.

		// Compute tweening numbers.
		FLOAT StartFrame = Seq ? (-1.f / Seq->NumFrames) : 0.f;
		INT iFrameOffset = Seq ? Seq->StartFrame * Mesh.ModelVerts : 0;
		FLOAT Alpha = 1.f - BaseChannel.AnimFrame / CachedFrame;

		if( CachedSeq!=BaseChannel.AnimSequence )
		{
			TweenIndicator = 0.f;
		}
		
		// Original:
		if( CachedSeq!=BaseChannel.AnimSequence || Alpha<0.f || Alpha>1.f)
		{
			CachedFrame = StartFrame; 
			Alpha       = 0.f;
			CachedSeq = BaseChannel.AnimSequence;
		}
				
		// Tween indicator says destination has been (practically) reached ?
		TweenIndicator += (1.f - TweenIndicator) * Alpha;
		if( TweenIndicator > 0.97f ) 
		{
			// We can set Alpha=0 (faster).
			Alpha = 0.f;

			// LOD fix: if the cache has too little vertices, 
			// now is the time to fill it out to the requested number.
			if (VertexNum < VertsRequested )
			{
				FMeshVert* MeshVertex = &Mesh.Verts( iFrameOffset );
				FMeshNorm* MeshNormal = &Mesh.AnimNormals( iFrameOffset );

				for( INT i=VertexNum; i<VertsRequested; i++ )
				{
					CachedVerts[i]= FVector( MeshVertex[i].X, MeshVertex[i].Y, MeshVertex[i].Z );
					//CachedNorms[i]= FVector( MeshNormal[i].X ,MeshNormal[i].Y , MeshNormal[i].Z  );
					CachedNorms[i]= FVector( MeshNormal[i].X - 512.0f, MeshNormal[i].Y - 512.0f, MeshNormal[i].Z - 512.0f );
				}
				VertexNum = VertsRequested;
				LODRequest = VertexNum; 
				CachedLodVerts = VertexNum;   
			}
		}
		
		// Special case Alpha 0.
		if (Alpha <= 0.f)
		{

		}
		else
		{
			// Tween all points between cached value and new one.
			FMeshVert* MeshVertex = &Mesh.Verts( iFrameOffset );
			FMeshNorm* MeshNormal = &Mesh.AnimNormals( iFrameOffset );

			for( INT i=0; i<VertexNum; i++ )
			{
				FVector V2( MeshVertex[i].X, MeshVertex[i].Y, MeshVertex[i].Z );
				CachedVerts[i] += (V2 - CachedVerts[i]) * Alpha;

				//FVector N2( MeshNormal[i].X , MeshNormal[i].Y , MeshNormal[i].Z );
				FVector N2( MeshNormal[i].X-512.0f , MeshNormal[i].Y-512.0f , MeshNormal[i].Z-512.0f );
				CachedNorms[i] += (N2 - CachedNorms[i]) * Alpha;
			}
		}
		// Update cached frame.
		CachedFrame = BaseChannel.AnimFrame;
	}

	// Do we want vertices only ? To accomodate GetMeshVerts.
	if( TaskFlag == GF_RawVerts )
	{
		for( INT v=0; v<LODRequest; v++)
		{				
			*ResultVerts = CachedVerts[v];			
			*(BYTE**)&ResultVerts += Size;
		}
	}
	else
	{
		// Final: output vertices and normals, wedge-wise (for vertex buffer).
		for( INT v=0; v<Mesh.Wedges.Num(); v++)
		{
			if( Mesh.Wedges(v).iVertex < VertexNum ) // LOD check, only transform necessary ones.
			{
				INT Idx = Mesh.Wedges(v).iVertex;
				*ResultVerts       = CachedVerts[Idx]; // .TransformPointBy(LocalCoords);
				*(ResultVerts + 1) = CachedNorms[Idx];
				// Normals: not normalized, hardware renderer automatically normalizes.
			}
			*(BYTE**)&ResultVerts += Size;
		}
	}

	// STAT ANIM
	// ">Stat anim" logging  - assume only one mesh being animated (player in behindview ?) 
	// Optional extra animation statistics - print single line for every channel of any animated mesh.
	if( SceneNode && SceneNode->Viewport && SceneNode->Viewport->Actor->GetLevel()->Engine->bShowAnimStats )
	{
		if( SceneNode->Viewport->Canvas )
		{
			UCanvas* Canvas = SceneNode->Viewport->Canvas;
			FString	AnimString;			
			AnimString = FString::Printf(TEXT(" Animation - mesh:%s actor:%s  instance:%s channels:"),Mesh.GetName(),  AnimOwner->GetName(), this->GetName() );
			//SceneNode->Viewport->AnimStats.IntStat( *AnimString,TEXT(" ")) = Blends.Num();
			Canvas->CurY += 6;
			VertDrawStat(Canvas, FColor(255,255,0),4,TEXT("%s"),*AnimString); // Title string
			Canvas->CurY += 3;

			if( Owner->bAnimByOwner )
			{
				VertDrawStat(Canvas, FColor(255,255,0),4,TEXT(" AnimByOwner: %s"),AnimOwner->GetName() ); 
				Canvas->CurY += 3;
			}
			if( BaseChannel.AnimSequence != NAME_None )
			{
				HMeshAnim Seq = GetAnimNamed( BaseChannel.AnimSequence );
				FLOAT NumFrames = Seq ? AnimGetFrameCount(Seq) : 1.0;
			
				AnimString = FString::Printf(TEXT("(%s) Seq:{%s} animframe: %4.2f frames: %i rate %f" ), AnimOwner->GetName(), *BaseChannel.AnimSequence, BaseChannel.AnimFrame,(INT)NumFrames,BaseChannel.AnimRate );																
				
				VertDrawStat(Canvas,FColor(255,255,0),4,TEXT(" Anim: [%s]"),*AnimString);
			}
		}
	}

	unguard;
}


// Mesh-to-world matrix - for rendering and visibility bounding boxes.
FMatrix UVertMeshInstance::MeshToWorld() 
{ 
	FMatrix NewMatrix;
	
	if(bIgnoreMeshOffset)
		NewMatrix = //FTranslationMatrix( OurMesh->Origin  ) *
		    FScaleMatrix( OurActor->DrawScale3D * OurActor->DrawScale * OurMesh->Scale ) * 
			FRotationMatrix( OurActor->Rotation ) *
			FTranslationMatrix( OurActor->Location );
			//FTranslationMatrix( OurMesh->Origin  );  
	else
		NewMatrix = //FTranslationMatrix( OurMesh->Origin  ) *
		         FScaleMatrix( OurActor->DrawScale3D * OurActor->DrawScale * OurMesh->Scale ) * 
				 FRotationMatrix( OurMesh->RotOrigin ) *			 
				 FRotationMatrix( OurActor->Rotation ) *
				 FTranslationMatrix( OurActor->Location + OurActor->PrePivot );
				 //FTranslationMatrix( OurMesh->Origin  );  

	FVector XAxis( NewMatrix.M[0][0], NewMatrix.M[1][0], NewMatrix.M[2][0] );
	FVector YAxis( NewMatrix.M[0][1], NewMatrix.M[1][1], NewMatrix.M[2][1] );
	FVector ZAxis( NewMatrix.M[0][2], NewMatrix.M[1][2], NewMatrix.M[2][2] );
	if(!bIgnoreMeshOffset)
	{
		NewMatrix.M[3][0] += - OurMesh->Origin | XAxis;
		NewMatrix.M[3][1] += - OurMesh->Origin | YAxis;
		NewMatrix.M[3][2] += - OurMesh->Origin | ZAxis;
	}

	return NewMatrix;
};

//	Render
void UVertMeshInstance::Render(FDynamicActor* Owner,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI)
{
	guard(UVertMesh::Render);

	AActor*		Actor = Owner->Actor;
	UVertMesh*	VertMesh = (UVertMesh*)Actor->Mesh;

	// jij -- temp (bHidden wasn't being honored in the rendering pipeline for vert meshes)
    if (Actor->bHidden)
        return;
    // --- jij

	AActor* AnimOwner = NULL;
	// Check to see if bAnimByOwner
	if ((Actor->bAnimByOwner) && (Actor->Owner != NULL))
		AnimOwner = Actor->Owner;
	else
		AnimOwner = Actor;		

	if( VertMesh->LODChunks[0].Sections.Num() == 0 )
		VertMesh->RenderPreProcess();	 // pre-process for software or hardware skinning.

	FMatrix NewMatrix = FMatrix::Identity;


    if (Owner->Actor->bAlwaysFaceCamera)
    {
        FVector toEye = (SceneNode->ViewOrigin - Owner->Actor->Location).SafeNormal();
        FVector up = FVector(0,0,1);
        FVector xAxis = (toEye ^ up).SafeNormal();
        FVector yAxis = (toEye ^ xAxis).SafeNormal();
        FVector zAxis = toEye;
        FVector trans = Owner->Actor->Location;
        FMatrix localWorld;
        localWorld.M[0][0] = xAxis.X; localWorld.M[0][1] = xAxis.Y;
        localWorld.M[0][2] = xAxis.Z; localWorld.M[0][3] = 0.0f;
        localWorld.M[1][0] = yAxis.X; localWorld.M[1][1] = yAxis.Y;
        localWorld.M[1][2] = yAxis.Z; localWorld.M[1][3] = 0.0f;
        localWorld.M[2][0] = zAxis.X; localWorld.M[2][1] = zAxis.Y;
        localWorld.M[2][2] = zAxis.Z; localWorld.M[2][3] = 0.0f;
        localWorld.M[3][0] = trans.X; localWorld.M[3][1] = trans.Y;
        localWorld.M[3][2] = trans.Z; localWorld.M[3][3] = 1.0f;

        if(bIgnoreMeshOffset)
			localWorld = 
				FScaleMatrix( OurActor->DrawScale3D * OurActor->DrawScale * OurMesh->Scale ) * 
				FRotationMatrix( OurActor->Rotation ); 
		else
        localWorld = 
            FScaleMatrix( OurActor->DrawScale3D * OurActor->DrawScale * OurMesh->Scale ) * 
            FRotationMatrix( OurMesh->RotOrigin ) *			 
            FRotationMatrix( OurActor->Rotation ) *
            FTranslationMatrix( OurMesh->Origin  ) * localWorld; 

            /*FRotationMatrix(OurActor->Rotation) * FRotationMatrix(OurMesh->RotOrigin)
            * FScaleMatrix(OurActor->DrawScale3D * OurActor->DrawScale * OurMesh->Scale)
            * FTranslationMatrix( OurMesh->Origin )
            * localWorld;*/

        RI->SetTransform(TT_LocalToWorld,localWorld);
    }
    else
    {
	    NewMatrix = MeshToWorld();

	    RI->SetTransform(TT_LocalToWorld,NewMatrix);
    }

	// Determine desired VertexSubset for LOD
	INT VertexSubset = VertMesh->ModelVerts;
	FLOAT FovBias = (SceneNode->Viewport && SceneNode->Viewport->Actor) ? SceneNode->Viewport->Actor->FOVBias : 1.f;  // calculated in RenderLevel
	
	// Global framerate dependent LOD factor.
	FLOAT GlobalLODFactor = GIsEditor ? 1.0f : ( Actor->GetLevel() ? Actor->GetLevel()->GetLevelInfo()->AnimMeshGlobalLOD : 1.0f );

	// Complexity bias: effectively, stronger LOD for more complex meshes.
	FLOAT CpxBias =  0.25f + 0.75f*VertMesh->ModelVerts/250.f; // about even for 250-vertex meshes.
	FLOAT ResolutionBias = 0.3f + 0.7f*SceneNode->Viewport->SizeX/640.f; // Moderated resolution influence.
	// Z coordinate :in units. 60 units is about the player's height. 
	FLOAT Z = SceneNode->Project(Actor->Location).W - VertMesh->LODZDisplace;
	FLOAT DetailDiv = VertMesh->LODStrength * /*GlobalShapeLOD * GlobalShapeLODAdjust **/ FovBias * Max(1.f,Z) * CpxBias * GlobalLODFactor;
	FLOAT MeshVertLOD  = 530.f * ResolutionBias * Actor->DrawScale3D.GetAbsMax() * Abs(Actor->DrawScale) * Actor->LODBias * VertMesh->MeshScaleMax / DetailDiv;  	

	FLOAT TargetSubset    = Max( (FLOAT)VertMesh->LODMinVerts, (FLOAT)VertMesh->ModelVerts * MeshVertLOD  );
	VertexSubset          = Min( appRound(TargetSubset), VertMesh->ModelVerts );

	// Safety - do we even have this LOD level ?
	if( VertMesh->LODChunks[0].Sections.Num() == 0)
		return;

	UMaterial*	EnvironmentMap = NULL;

	if( Actor->Texture )
		EnvironmentMap = Actor->Texture;
	else if( Actor->Region.Zone && Actor->Region.Zone->EnvironmentMap )
		EnvironmentMap = Actor->Region.Zone->EnvironmentMap;
	else if( Actor->Level->EnvironmentMap )
		EnvironmentMap = Actor->Level->EnvironmentMap;
	if( EnvironmentMap==NULL )
		return;
	check(EnvironmentMap);
	
	// Wireframe shading ?
	UBOOL	Wireframe = SceneNode->Viewport->IsWire();
	DWORD	ExtraFlags = 0;	
	if (Wireframe )
		ExtraFlags |= (PF_Wireframe | PF_FlatShaded);
	
	// Setup a vertex stream, expected to change every frame ( if not, use a static mesh! ) 
	INT	RequestedVerts = VertexSubset; // VertMesh->VertexBuffer->Vertices.Num();
	
	// GETFRAME
	// This initializes the VertMesh animated 3d points.
	GetFrame( Actor, SceneNode, (FVector*)&(VertMesh->VertexStream.Vertices(0)), sizeof(FAnimMeshVertex), RequestedVerts, GF_FullSkin ); //#SKEL:SizeOf?

	INT LodIdx = 0;
	
	FMeshLODChunk& Chunk = VertMesh->LODChunks[0]; // Dynamic LOD, use LODChunks[0].

	// Build the index buffer.
	FRawIndexBuffer		IndexBuffer;

	IndexBuffer.Indices.Empty(VertMesh->Faces.Num() * 3);

	// Record dynamic chunk sizes at current LOD for rendering.
	TArray<INT> DynSectionFaces;

	// Set up faces, appropriate for this LOD level.
	for( INT SectIndex = 0; SectIndex< Chunk.Sections.Num(); SectIndex++)
	{		
		INT LodFaces = 0;
		// current index..
		Chunk.Sections(SectIndex).FirstIndex = IndexBuffer.Indices.Num();
		INT FaceStart = Chunk.Sections(SectIndex).FirstFace;

		for( INT FaceIdx = 0; FaceIdx < Chunk.Sections(SectIndex).TotalFaces; FaceIdx ++)
		{
			//  Use face only if FaceLevel indicates it falls within our vertex budget.
			if( VertMesh->FaceLevel( FaceStart+FaceIdx ) <= VertexSubset ) 
			{			
				FMeshFace Face = VertMesh->Faces(FaceStart+FaceIdx);  
				for( INT v=0; v<3; v++ )
				{
					INT WedgeIndex = Face.iWedge[2-v]; // Handedness flip 2-v?
					INT LODVertIndex = VertMesh->Wedges(WedgeIndex).iVertex;						

					// Go down LOD wedge collapse list until below the current LOD vertex count.
					while( LODVertIndex >= VertexSubset ) 
					{
						WedgeIndex = VertMesh->CollapseWedgeThus(WedgeIndex);
						LODVertIndex = VertMesh->Wedges(WedgeIndex).iVertex;
					}					
					IndexBuffer.Indices.AddItem( WedgeIndex );
				}
				LodFaces++; 
			}
		}   
		DynSectionFaces.AddItem(LodFaces);		
	}

	
	// Set the lights.
	// AZoneInfo*	ZoneInfo = SceneNode->Level->GetZoneActor(Owner->Actor->Region.ZoneNumber);
	// RI->SetAmbientLight(FColor(FGetHSV(ZoneInfo->AmbientHue,ZoneInfo->AmbientSaturation,ZoneInfo->AmbientBrightness)));

	if( Wireframe )
	{
		RI->EnableLighting(1,0,0,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly,Owner->BoundingSphere);
		// Determine the wireframe color.
		UEngine*	Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
		FColor		WireColor = Engine->C_AnimMesh;
		if( Actor->bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
			RI->SetAmbientLight(FColor(120,255,75,255)); // higlight wire with a yellowish green..
		else
			RI->SetAmbientLight(FColor(WireColor.Plane())); 
	}	
	else if( Actor->bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
	{
		// Highlight the static mesh with green marker color.			
		RI->EnableLighting(1,0,0,NULL,1);
		RI->SetAmbientLight(FColor(128,255,128,255));  
	}
	else if( SceneNode->Viewport->Actor && ( SceneNode->Viewport->Actor->RendMap == REN_ScreenActor ) ) // sjs
	{
		// Do nothing
	}
	else if( Actor->GetAmbientLightingActor()->bUnlit || !SceneNode->Viewport->IsLit() )
	{
		RI->SetAmbientLight(FColor(255,255,255,255));  
	}
	else
	{
		INT LightsLimit = Actor->MaxLights; 
		INT	NumHardwareLights = 0;
		// Set lights.
		RI->EnableLighting(1,0,1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly,Owner->BoundingSphere);
		for(TList<FDynamicLight*>* LightList = Lights;LightList && NumHardwareLights < LightsLimit;LightList = LightList->Next)
			RI->SetLight(NumHardwareLights++,LightList->Element);			
		RI->SetAmbientLight(Owner->AmbientColor);		
	}

	// Set buffers
	INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&(VertMesh->VertexStream)),
	BaseIndex = RI->SetDynamicIndexBuffer(&IndexBuffer,BaseVertexIndex);

	UBOOL bRegularTexturing = true;

	if( SceneNode->Viewport->IsWire() )
	{
		bRegularTexturing = false;
		DECLARE_STATIC_UOBJECT(
			UShader,
			MeshWireframeShader,
			{
				MeshWireframeShader->Wireframe = 1;
			}
			);

		RI->SetMaterial(MeshWireframeShader);
	}
	else if (SceneNode->Viewport->Actor->RendMap==REN_LightingOnly ) // Flatshaded.
	{
		bRegularTexturing = false;
		DECLARE_STATIC_UOBJECT(
			UShader,
			MeshFlatframeShader,
			{							
			}
			);
		RI->SetMaterial(MeshFlatframeShader);
	}

	// jij --- umm, ok...
    //if(!SceneNode->Viewport->IsWire() && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Projectors))
	//{
    /// --- jij

	// Now finally draw the stuff.
	for( INT SectionIndex = 0; SectionIndex < VertMesh->LODChunks[LodIdx].Sections.Num(); SectionIndex++ )
	{
		FLODMeshSection& Section = Chunk.Sections(SectionIndex);
	
		// Draw if the section has any visible triangles at this LOD level.		
		if( VertMesh->LODChunks[LodIdx].Sections(SectionIndex).TotalFaces ) //#DEBUG 
		{									
				UMaterial*	Material = VertMesh->MeshGetInstance(Actor)->GetMaterial( VertMesh->MeshMaterials(Section.MaterialIndex).MaterialIndex, Actor );
				if( SceneNode->Viewport->IsWire() || bForceWireframe ) // Wireframe.
				{
					DECLARE_STATIC_UOBJECT(
						UShader,
						MeshWireframeShader,
						{
							MeshWireframeShader->Wireframe = 1;
						}
						);

					RI->SetMaterial(MeshWireframeShader);
				}
				else // Regular textured.
				{				
			        // jij ---
                    if (Section.MaterialIndex < Owner->Actor->Skins.Num())
                        RI->SetMaterial(Owner->Actor->GetSkin(Section.MaterialIndex));
                    else
                        RI->SetMaterial(Material);
                    // --- jij
				}
			
			RI->DrawPrimitive
			( 
				PT_TriangleList,
				BaseIndex + Section.FirstIndex,
				DynSectionFaces(SectionIndex), 
				Section.MinIndex, 
				Section.MaxIndex
			);			

			}
		}
	// jij ---
    //}
    // --- jij

	// TODO: projected shadows ?	

	// Debug visibility bounding box drawing 
	if( SceneNode->Viewport->bShowBounds )
	{
		FLineBatcher LineBatcher(RI);
		FBox BoundBox = Owner->BoundingBox;      //GetRenderBoundingBox((AActor*)Actor); // Bounding box in world space;
		RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
		LineBatcher.DrawBox( BoundBox, FColor(72,90,255) );
	}
	
	unguard;
}

