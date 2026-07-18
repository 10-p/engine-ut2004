/*=============================================================================
	UnSkeletalMesh.cpp: Unreal mesh animation and preprocessing functions.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Erik de Neve	 
	    * Remarks
		  - New methods: the explicit ctor, and GetFrame()
		  - Distinguishing Mesh from SkeletalMesh : if( Mesh->IsA(USkeletalMesh::StaticClass()) )
		  - No specific amd3d support.
        * Updated to use Chris Hargrove's mesh instancing concept.

	    *  __PSX2_EE__ : Most PSX2 code temporarily removed from this file to avoid confusion.

		* May 2001
		  GetFrame Uses GTicks to determine if the bone state is current 
		  ( multiple GetFrame's may be done by shadown-drawing routines etc )
		* Feb 2002
		  Static LOD, static-part rendering optimizations, faster GetTrackRotPos key matching.
	    * 2002 
		  Features and fixes by Steve Sinclair, Glen Miner et al. for UT2003.
	    * Sept 2002
		  Overlay material simplifications (James Golding) and re-complications (Erik).
	    * Oct 2003
		  Simplified spherical interpolation (no more explicit inter-quaternion alignment necessary.)

 Work notes:

   Mesh built with full floating point coordinates.        
   TODO:convert ApplyPivot into assembly for efficiency ?

=============================================================================*/ 

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"

// Additional /experimental / special-case math code.

// Corners-cutting spherical interpolation and normalization.
// Assumes aligned quaternions.
#define NORMDELTA (0.00001f)

/*-----------------------------------------------------------------------------
	UMeshAnimation object implementation.
-----------------------------------------------------------------------------*/

INT MotionChunk::CalculateMemFootprint( UBOOL RenderDataOnly )
{
	guard(MotionChunk::CalculateMemFootprint);
	INT TotalMem = 0;

	for(INT j=0;j< AnimTracks.Num(); j++)
	{
		TotalMem += sizeof(DWORD); // Flags
		TotalMem += sizeof(FQuat)* AnimTracks(j).KeyQuat.Num();
		TotalMem += sizeof(FVector)* AnimTracks(j).KeyPos.Num();
		TotalMem += sizeof(FLOAT)* AnimTracks(j).KeyTime.Num();				
	}
	TotalMem += sizeof(FQuat)* RootTrack.KeyQuat.Num();
	TotalMem += sizeof(FVector)* RootTrack.KeyPos.Num();
	TotalMem += sizeof(FLOAT)* RootTrack.KeyTime.Num();

	TotalMem += sizeof(AnalogTrack)* AnimTracks.Num();

	return TotalMem;
	unguard;
}

UMeshAnimation::UMeshAnimation()
{
	guard(UMeshAnimation::UMeshAnimation);
	
	unguardobj;
}

void UMeshAnimation::PostLoad()
{
	guard(UMeshAnimation::PostLoad);
	Super::PostLoad();
	//OLDVER: convert old notifies to objects
	for( INT i=0;i<AnimSeqs.Num();i++ )
		AnimSeqs(i).UpdateOldNotifies(GetOuter());
	unguard;
}

// Memory footprint 
INT UMeshAnimation::MemFootprint()
{
	guard(UMeshAnimation::MemFootprint);

	INT TotalMem = 0;
	for(INT i=0;i<Moves.Num();i++)
	{
		TotalMem += Moves(i).CalculateMemFootprint();
	}
	return TotalMem;
	unguard;
}

// Footprint for one particular sequence (if it can be found)
INT UMeshAnimation::SequenceMemFootprint( FName SeqName)
{
	guard(UMeshAnimation::SequenceMemFootprint);

	INT TotalMem = 0;
	INT SeqIdx= -1;

	// Find sequence.
	for( INT i=0; i<AnimSeqs.Num(); i++ )
	{
		if( SeqName == AnimSeqs(i).Name )
		{
			SeqIdx = i;
		}
	}
	if( SeqIdx >= 0 )
	{
		TotalMem = Moves(SeqIdx).CalculateMemFootprint(0);
	}	
	return TotalMem;
	unguard;
}

// Modify the root orientation/translation for a certain sequence in this UMeshAnimation
UBOOL UMeshAnimation::AdjustMovement( FName SeqName, FCoords AdjustCoords )
{
	guard(UMeshAnimation::AdjustMovement);
	MotionChunk* MoveData = NULL;
	for( INT i=0; i<AnimSeqs.Num(); i++ )
	{
		if( SeqName == AnimSeqs(i).Name )
			MoveData = &( Moves(i) );
	}	
	if( MoveData )
	{
		// Apply the AdjustCoordsto all the root's keys.
		for( INT i=0; i< MoveData->AnimTracks(0).KeyTime.Num(); i++)
		{
			// Add the offset, if applicable.
			if( AdjustCoords.Origin != FVector(0,0,0) )
				MoveData->AnimTracks(0).KeyPos(i) += AdjustCoords.Origin;

			// Add the rotation to the quaternion by 'appending' the quaternion...?			
			FQuat& MoveQuat = MoveData->AnimTracks(0).KeyQuat(i);
			if( MoveQuat.W != 0.f )
			{			
				FCoords OrigRot = FQuaternionCoords( MoveQuat);
				OrigRot *= AdjustCoords;
				MoveQuat = FCoordsQuaternion(OrigRot);
			}
		}
		return true;
	}
	return false;
	unguard;
}

// Conforminging - detect bone matches, re-order bones accordingly, patch missing ones in from the reference skeleton if available.								
void UMeshAnimation::ConformBones( UMeshAnimation* DestAnimObject, USkeletalMesh* ReferenceMesh )
{
	guard(UMeshAnimation::ConformBones)
	UBOOL BonesChanged = false;

	if(!DestAnimObject)
		return;

	if( DestAnimObject->RefBones.Num() != RefBones.Num() )
		BonesChanged = true;

	INT OriginalBoneCount = RefBones.Num();

	TArray <INT> BoneRemap;
	BoneRemap.AddZeroed( DestAnimObject->RefBones.Num());

	INT LinkedBoneCount = 0;
	for( INT b=0; b<BoneRemap.Num(); b++)
	{
		BoneRemap(b) = -1;
		for( INT j=0; j< RefBones.Num(); j++)
		{
			if( DestAnimObject->RefBones(b).Name == RefBones(j).Name )
			{
				BoneRemap(b) = j;
				LinkedBoneCount++;
				if( b != j )
				{
					BonesChanged = true;
				}
			}
		}				
	}

	if( LinkedBoneCount != RefBones.Num() )
		BonesChanged = true;
		
	// Any difference in total bone numbers or non-perfect bonenames-match triggers animation reconstruction.
	if( !BonesChanged )
		return;

	// With newly mapped bones, recreate all animation tracks.
	TArray<MotionChunk> NewMoves;	

	// Remap Moves.Num() sequences.
	for( INT i=0; i< Moves.Num(); i++)
	{
		INT NewIdx = NewMoves.AddZeroed();
		MotionChunk* NewMove = &NewMoves(NewIdx);		

		NewMove->AnimTracks.AddZeroed( BoneRemap.Num() );

		NewMove->RootSpeed3D = Moves(i).RootSpeed3D;
		NewMove->StartBone = Moves(i).StartBone;
		NewMove->TrackTime = Moves(i).TrackTime;
		NewMove->Flags = Moves(i).Flags;
		
		// Fill all animtracks in new-bone order.
		for( INT b=0; b< NewMove->AnimTracks.Num(); b++)
		{
			INT SourceBoneIdx = BoneRemap(b);
			// Try get reference skeleton position/rotation if remapping was not possible.			
			if( SourceBoneIdx < 0 )
			{
				FQuat RefQuat = FQuat(0,0,0,0);
				FVector RefVector = FVector(0,0,0);
				// If the bone exists in our currently liked mesh, retrieve the angle/offset from it.
				if( ReferenceMesh )
				{
					INT RefBoneIdx = ReferenceMesh->MeshGetInstance(NULL)->MatchRefBone( DestAnimObject->RefBones(b).Name );
					if( RefBoneIdx > 0 )
					{
						RefQuat = ReferenceMesh->RefSkeleton(RefBoneIdx).BonePos.Orientation;
						RefVector = ReferenceMesh->RefSkeleton(RefBoneIdx).BonePos.Position;
					}				
				}
				// Add first and only item - static track.
				NewMove->AnimTracks(b).KeyQuat.AddItem(RefQuat);
				NewMove->AnimTracks(b).KeyPos.AddItem(RefVector);
				NewMove->AnimTracks(b).KeyTime.AddItem( 0.0f );			
			}
			else
			{
				//Get the motion data for this track however many keys there are - from the source BoneRemap(b) bone.
				for( int m=0; m< Moves(i).AnimTracks( SourceBoneIdx ).KeyQuat.Num(); m++)
					NewMove->AnimTracks(b).KeyQuat.AddItem( Moves(i).AnimTracks( SourceBoneIdx ).KeyQuat(m) );				
				for( int m=0; m< Moves(i).AnimTracks( SourceBoneIdx ).KeyPos.Num(); m++)
					NewMove->AnimTracks(b).KeyPos.AddItem(  Moves(i).AnimTracks( SourceBoneIdx ).KeyPos(m) );				
				for( int m=0; m< Moves(i).AnimTracks( SourceBoneIdx ).KeyTime.Num(); m++)
					NewMove->AnimTracks(b).KeyTime.AddItem( Moves(i).AnimTracks( SourceBoneIdx ).KeyTime(m) );				
			}
		}
	}

	// Replace 'refbones'.
	RefBones.Empty();
	RefBones.Add( DestAnimObject->RefBones.Num());
	for( INT b=0; b< DestAnimObject->RefBones.Num(); b++)
	{
		RefBones(b)= DestAnimObject->RefBones(b);
	}

	// Replace moves with 'newmoves'.
	if( NewMoves.Num() > 0 )
	{
		for( INT m=0; m< Moves.Num(); m++)
			Moves(m).Erase();

		Moves.Empty();
		Moves.AddZeroed( NewMoves.Num() );

		for( INT i=0; i< NewMoves.Num(); i++)
		{			
			Moves(i) = NewMoves(i);
			NewMoves(i).Erase();
			
			// BoneIndices (never used..)
			Moves(i).BoneIndices.Empty();
			Moves(i).BoneIndices.Add(RefBones.Num());
			for(INT b=0; b< RefBones.Num(); b++)
			{
				Moves(i).BoneIndices(b) = b;
			}
		}
		debugf(TEXT("Conformed %i animation sequence(s). Original bone count: %i conformed to %i bones."), NewMoves.Num(), OriginalBoneCount, RefBones.Num() );
	}	

	unguard;
}


void UMeshAnimation::Serialize( FArchive& Ar )
{
	guard(UMeshAnimation::Serialize);
	Super::Serialize(Ar);
	Ar << InternalVersion;
	Ar << RefBones;	
	Ar << Moves;
	Ar << AnimSeqs;
	if( !Ar.IsPersistent())
	{
	}
	unguardobj;
}
IMPLEMENT_CLASS(UMeshAnimation);

// Stub legacy object
void UAnimation::Serialize( FArchive& Ar )
{
	guard(UAnimation::Serialize);
	//Super::Serialize(Ar);
	unguardobj;
}
IMPLEMENT_CLASS(UAnimation);




//
// Skin stream functions. Functions for rigid parts as well as for software-skinned vertex-filler callbacks. 
// 
void FSkinVertexStream::GetStreamData(void* Dest)
{
	guard(FSkinVertexStream::GetStreamData);
	if( bStreamCallback && MeshInstance )
		((USkeletalMeshInstance*)MeshInstance)->MeshSkinVertsCallback( Dest );	
	else
		appMemcpy( Dest, &Vertices(0), Vertices.Num() * sizeof(FAnimMeshVertex) );		
	unguard;
}

void FSkinVertexStream::GetRawStreamData(void ** Dest, INT FirstVertex )
{
	if( bStreamCallback)
	{
		//debugf(TEXT("WARNING!!!! Getrawstreamdata called. "));
		*Dest = NULL;
	}
	else
		*Dest = &Vertices(FirstVertex);	
}

INT FSkinVertexStream::GetSize()
{	
	if( bStreamCallback )
	{
		INT CurrentVertexBufferSize = ((USkeletalMeshInstance*)MeshInstance)->ActiveVertStreamSize();
		//debugf(TEXT("Vertex size reporting - %i"),CurrentVertexBufferSize);
		return sizeof(FAnimMeshVertex) * CurrentVertexBufferSize;	
	}
	else
	{
		//debugf(TEXT("Vertex size reporting (regular stream) - %i"), Vertices.Num());
		return sizeof(FAnimMeshVertex) * Vertices.Num(); 
	}
}

INT FSkinVertexStream::GetStride()
{
	return sizeof(FAnimMeshVertex);	
}


/*-----------------------------------------------------------------------------
	USkeletalMesh /-Instance implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USkeletalMesh);
IMPLEMENT_CLASS(USkeletalMeshInstance);


//
//	Authentication key functions.
//
DWORD USkeletalMesh::CreateAuthenticationKey( INT AuxCode )
{
#ifdef OBFKEYREQUIRED
	// Generates an obfuscated one-way 15-bit 'hash' key unique to
	// this mesh, dependent on the number of triangles, vertices.
	DWORD ContentKey = 0;
	
	RawVerts.Load();	
	RawWedges.Load();
	RawFaces.Load();
	
	// Key based on easily accessible LOD-independent characteristics..
	if( RawVerts.Num())
    {
        ContentKey +=  RawWedges.Num() * 3 + RawVerts.Num() * 7 + RawFaces.Num() + 79 * RefSkeleton.Num();
	}
	// 'Auxiliary Code' gets encoded into the key. Assume internal vertex/face buffers have been intialized at this point.
	ContentKey = (0x7fff & ContentKey ) + ( AuxCode > 0 ? AuxCode : 0 );	
	// -1: requested non-obfuscated key.
	if( AuxCode == -1 ) return( ContentKey );
	// 15-bit time-dependent random number, always >= 2^14 
	DWORD ObfuscationMask = 0x7fff & ( ( ((DWORD)(appCycles())<<7) ^ (DWORD)(appCycles()) ^ (QWORD)(appRand()) ^ ((QWORD)(appRand())<<8) ) | 0x4000 );
	return OBFCREATEKEY( ObfuscationMask, ContentKey ); 
#else
	return 0;
#endif
}

UBOOL USkeletalMesh::ValidateAuthenticationKey()
{	
#ifdef OBFKEYREQUIRED
	DWORD OurKey = OBFUNWRAPKEY(AuthenticationKey);	
	return( CreateAuthenticationKey( -1 ) == OurKey );  // Offset rolls out.
#else
	return 1;
#endif
}


// Forward decl. for local functions.
void ComputeSkinVerts( USkeletalMesh* Mesh, USkeletalMeshInstance* MInst, void* Destination,INT Size, INT& LODRequest );

void USkeletalMesh::Serialize( FArchive& Ar )
{
	guard(USkeletalMesh::Serialize);
	Super::Serialize(Ar);

	// On saving, LOD mesh's Serialize (our Super::Serialize) will have stamped the object witht he current internal mesh version number.

	// Mesh specific data - fully floating point UV and vertex vectors.
	Ar << Points;
	// Skeletal specific data
	Ar << RefSkeleton;    //  Reference skeleton	
	Ar << DefaultAnim;    //
	Ar << SkeletalDepth;  //
	Ar << MultiBlends;    // Always...
	Ar << Weights;

	Ar << TagAliases;	  // Attachment bone aliases
	Ar << TagNames;		  // real bone names
	Ar << TagCoords;	  // reserved	
	
	if( InternalVersion < 2 ) 
	{
		// Dummy equivalents.
		TArray<FLODMeshSection> SmoothSections;
		TArray<FLODMeshSection> StaticSections;

		Ar << SmoothSections;   
		Ar << StaticSections;    

		TArray<_WORD> CollapseRemap;
		Ar << CollapseRemap;    
	}
	
	if( InternalVersion >= 2 ) 
	{	
		// Explicitly enable Lazyloading for the raw data in LODModels and the Raw* arrays below.
		UBOOL SavedLazyLoad = GLazyLoad;
		GLazyLoad = 1;
		Ar << LODModels;
		Ar << DefaultRefMesh;	
		Ar << RawVerts;
		Ar << RawWedges;
		Ar << RawFaces;		
		Ar << RawInfluences;
		Ar << RawCollapseWedges;
		Ar << RawFaceLevel;
		GLazyLoad = SavedLazyLoad;
	}
	
	// Serialize - only for garbage collection.
	if( !Ar.IsPersistent() )
	{		
		Ar << InfluenceIndex;
		Ar << LODChunks[0]; // FMeshLODChunk fixed-size array.
		Ar << LODChunks[1];
		Ar << LODChunks[2];
		Ar << LODChunks[3];
		Ar << RefBasesInverse;
	}	

	// Content authentication.
	if( Ar.Ver() >= 120)
	{
		Ar << AuthenticationKey;
	}

	// Collision
	if( Ar.Ver() >= 122 )
	{
		Ar << KPhysicsProps;
		Ar << BoneCollisionSpheres;
		Ar << BoneCollisionBoxes;
		Ar << BoneCollisionBoxModels;
	}

    if ( Ar.Ver() >= 127 )
		Ar << CollisionStaticMesh;

	unguardobj;
}


//
// Postload - check version number, backward compatibility tasks.
//
void USkeletalMesh::PostLoad()
{
	guard(UStaticMesh::PostLoad);

	Super::PostLoad();

	// Only if it wasn't already present, reconstruct the raw mesh...
	if( this->InternalVersion < 2 )  // Version 2 starts using static LOD's and Raw-data lazyarrays.
	{		
		// debugf(TEXT("Converting from older mesh data for skeletal mesh [%s] internal version [%i]"),this->GetName(),this->InternalVersion); 
		ReconstructRawMesh();
	}		

	//
	// Now if necessary, create the LOD levels from existing (old-style) 
	// continuous-LOD data. This is a special case of automatic LOD 
	// conversion. 
	// Proper, size/distance specific LOD importing/recomputation 
	// will be done at UCC time / in the editor.
	//	

	if( LODModels.Num() == 0)
	{
		debugf(TEXT("Postload-generating LOD models for mesh [%s]"),this->GetName()); 
		GenerateLodModel( 0, 1.00f, 1.00f, 4, false );  
		GenerateLodModel( 1, 0.70f, 0.50f, 1, false );
		GenerateLodModel( 2, 0.35f, 0.30f, 1, false );
		GenerateLodModel( 3, 0.10f, 0.17f, 1, false );		
	}

#ifdef WITH_LIPSINC	

	// debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Looking up controller for mesh %s"), this->GetName());

	LIPSincController = GLIPSincDB.FindControllerByName( FString(this->GetName()) );

	if(LIPSincController)
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: NumAnimations = %i"), LIPSincController->NumAnimations());

		if( !LinkupLIPSincBones() )
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: LIPSinc bone linkup failed!"));
		}
	}

#endif

	// Any (older) versions that didn't have a key, get one now. 
	if( AuthenticationKey == 0 )
		AuthenticationKey = CreateAuthenticationKey();
	
	unguard;
}

#ifdef WITH_LIPSINC

extern TLIPSincTalkerList GLIPSincTalkerList;

void USkeletalMeshInstance::PlayLIPSincAnim ( FName LIPSincAnimName, FLOAT Volume, FLOAT Radius, FLOAT Pitch )
{
	guard(USkeletalMeshInstance::PlayLIPSincAnim);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: PlayLIPSincAnim for %s"), this->GetName());

	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

#ifdef _XBOX

	// due to linear loading on xbox, most skeletal meshes are loaded before the GLIPSincInitGame
	// call -- therefore the controllers aren't linked up properly at that time.  Just try to
	// find a link the first time a LIPSinc animation is played on this mesh -- only try once.
	if( !Mesh->LIPSincController && !Mesh->bAlreadyLookedForController )
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Looking up controller for mesh %s"), Mesh->GetName());

		Mesh->LIPSincController = GLIPSincDB.FindControllerByName( FString(Mesh->GetName()) );

		if(Mesh->LIPSincController)
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: NumAnimations = %i"), Mesh->LIPSincController->NumAnimations());
		}

		if( !Mesh->LinkupLIPSincBones() )
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: LIPSinc bone linkup failed!"));
		}

		Mesh->bAlreadyLookedForController = 1;
	}

#endif

	if( Mesh->LIPSincController )
	{
		UBOOL bAlreadyPlaying = IsPlayingLIPSincAnim();

		if( bAlreadyPlaying )
		{
			// If we are the editor, we should ignore m_bInterruptible
			if( GIsEditor )
			{
				Mesh->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->Stop( GetActor() );
			}
			else if( !Mesh->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->IsInterruptible() )
			{
				// If this animation shouldn't be interrupted, simply return
				return;
			}
			else
			{
				// We're not the editor and we can be interrupted, so stop this animation and continue
				Mesh->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->Stop( GetActor() );
			}
		}

		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Looking up animation %s"), (*(LIPSincAnimName)));

		// look up anim for LIPSincAnimName
		m_nActiveLIPSincAnim = Mesh->LIPSincController->FindAnimIndex(FString(*(LIPSincAnimName)));

		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Found at %i"), m_nActiveLIPSincAnim );

		if ( m_nActiveLIPSincAnim >= 0 )
		{
			LIPSincBlendInfo.Reset();

			if( !Mesh->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->GetUSound() )
			{
				LIPSincBlendInfo.bNoSound = 1;
			}

			if( bAlreadyPlaying )
			{
				LIPSincBlendInfo.bWasAlreadyPlaying = 1;
			}
			else
			{
				// Don't want to add things into the list unless they weren't previously playing.
				// Otherwise we could end up with two copies in the list.
				GLIPSincTalkerList.Add( this );

				//GLIPSincTalkerList.PrintList();
			}

			LIPSincBlendInfo.Volume = Volume;
			LIPSincBlendInfo.Radius = Radius;
			LIPSincBlendInfo.Pitch  = Pitch;

			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Just started at %f for Actor %s and MeshInstance %s: Volume: %f Radius: %f Pitch: %f"), appSeconds(),
				GetActor()->GetName(), this->GetName(), Volume, Radius, Pitch);
		}
	}
	unguard;
}

UBOOL USkeletalMeshInstance::StopLIPSincAnim( void )
{
	guard(USkeletalMeshInstance::StopLIPSincAnim);

	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

	if( Mesh->LIPSincController )
	{
		if( IsPlayingLIPSincAnim() )
		{
			INT AnimIndex = m_nActiveLIPSincAnim;
			m_nActiveLIPSincAnim = -1;
			return (Mesh->LIPSincController->GetAnimation(AnimIndex)->Stop( GetActor() ));
		}
	}

	return 0;

	unguard;
}

UBOOL USkeletalMeshInstance::PauseLIPSincAnim( void )
{
	guard(USkeletalMeshInstance::PauseLIPSincAnim);

	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

	if( Mesh->LIPSincController )
	{
		if( IsPlayingLIPSincAnim() )
		{
			return (Mesh->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->Pause( GetActor() ));
		}
	}

	return 0;

	unguard;
}

UBOOL USkeletalMeshInstance::ResumeLIPSincAnim( void )
{
	guard(USkeletalMeshInstance::ResumeLIPSincAnim);

	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

	if( Mesh->LIPSincController )
	{
		if( IsPlayingLIPSincAnim() )
		{
			return (Mesh->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->Resume( GetActor() ));
		}
	}

	return 0;

	unguard;
}

UBOOL USkeletalMeshInstance::HasLIPSincAnim( FName LIPSincAnimName )
{
	guard(USkeletalMeshInstance::HasLIPSincAnim);

	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

	if( Mesh->LIPSincController )
	{
		if( Mesh->LIPSincController->FindAnimIndex(FString(*(LIPSincAnimName))) >= 0 )
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}

	unguard;
}

UBOOL USkeletalMeshInstance::IsPlayingLIPSincAnim( void )
{
	guard(USkeletalMeshInstance::IsPlayingLIPSincAnim);

	if( m_nActiveLIPSincAnim > -1 )
	{
		return 1;
	}
	else
	{
		return 0;
	}

	unguard;
}

FString USkeletalMeshInstance::CurrentLIPSincAnim( void )
{
	guard(USkeletalMeshInstance::CurrentLIPSincAnim);

	if( IsPlayingLIPSincAnim() )
	{
		return (((USkeletalMesh*)GetMesh())->LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->Name());
	}
	else
	{
		return FString(TEXT("LIPSinc_NONE"));
	}

	unguard;
}

// Force an update for this mesh, whether it is visible or not.
void USkeletalMeshInstance::ForceUpdate( void )
{
	guard(USkeletalMeshInstance::ForceUpdate);

	// Only do this update once per frame.
	if( !SpaceBases.Num() || (LastGTicks < GTicks) )
	{
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly);
		}		
	}

	unguard;
}

UBOOL USkeletalMesh::LinkupLIPSincBones( void )
{
	guard(USkeletalMesh::LinkupLIPSincBones);

	if( LIPSincController )
	{
		for( INT i = 0; i < LIPSincController->NumExpressions(); ++i )
		{
			LIPSincController->GetExpression(i)->BuildBoneMap( this, NULL );
		}

		LIPSincController->GetLookAtInfo()->BuildBoneMap( this );

		return ( LIPSincController->BonePoseInfo()->BuildBoneMap( this, NULL ) );
	}
	else
	{
		return 0;
	}

	unguard;
}

#endif

//
// Destroy.
//
void USkeletalMesh::Destroy()
{
	guard(UStaticMesh::Destroy);
	//
	//
	//

	// Destroy any inner's of this skeletal mesh. This should only happen in the editor i think.
	if(!GIsGarbageCollecting)
	{
		if(BoneCollisionBoxModels.Num() > 0)
		{
			for(INT i=0; i<BoneCollisionBoxModels.Num(); i++)
				delete BoneCollisionBoxModels(i);

			BoneCollisionBoxModels.Empty();
		}

		if(KPhysicsProps)
		{
			delete KPhysicsProps;
			KPhysicsProps = NULL;
		}
	}

	Super::Destroy();	
	unguard;
}



#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
/* Destroy dynamics/collision for this Skeletal Mesh instance. */
void USkeletalMeshInstance::Destroy()
{
	guard(USkeletalMeshInstance::Destroy);

    KTermSkeletonKarma(this);
    Super::Destroy();

	unguard;
}
#endif




//
//  Checks bone hierarchy of one mesh against another.
//  TODO: expand to cull dummies/ reorder bones.
//
UBOOL USkeletalMesh::ConformSkeletonTo( USkeletalMesh* SrcMesh, UBOOL Replace )
{
	guard(USkeletalMesh::ConformSkeletonTo);

	TArray<INT> NewBoneIndices;
	NewBoneIndices.Add( RefSkeleton.Num() );
	
	// Only writes for now.
	debugf(TEXT(" ConformSkeletonTo:  %s to %s "),this->GetName(),SrcMesh->GetName());
	for( INT p=0; p< RefSkeleton.Num(); p++)			
	{
		NewBoneIndices(p) = -1;
		for( INT b=0; b< SrcMesh->RefSkeleton.Num(); b++)
		{				
			if( SrcMesh->RefSkeleton(b).Name == RefSkeleton(p).Name ) 
			{
				NewBoneIndices(p) = b;				
				debugf(TEXT(" Matched bone [%s], mesh index %i source mesh index %i "),*(RefSkeleton(p).Name),p,b);
				break;
			}
		}	
		if( NewBoneIndices(p) == -1 )
		{
			debugf( TEXT(" Unmatched bone [%s] index %i not found in mesh [%s]"),*(RefSkeleton(p).Name),p,SrcMesh->GetName() );
		}
	}

	// Replace reference skeleton of this mesh with that of the source mesh - including the hierarchy.
	if( Replace )
	{
		RefSkeleton.Empty();
		for( INT b=0; b<SrcMesh->RefSkeleton.Num(); b++ )
		{
			RefSkeleton.AddItem( SrcMesh->RefSkeleton(b));
		}
		debugf( TEXT(" Completely replaced %i bones."), RefSkeleton.Num() );
		//TODO - Use NewBoneIndices to ensure weights still point to the correct bones.
	}

    return true;
	unguard;
}



void USkeletalMeshInstance::Serialize( FArchive& Ar )
{
	guard(USkeletalMeshInstance::Serialize);
	Super::Serialize(Ar);
	// Serialize for garbage collection only.
	if( !Ar.IsPersistent() )
	{
		Ar << Blends;
		Ar << Scalers;
		Ar << Directors;
		Ar << CurrentLODLevel;
		
		Ar << CachedOrientations;
		Ar << CachedPositions;
		Ar << CachedLinks;

#ifdef WITH_LIPSINC

		Ar << CachedLIPSincValues;
		Ar << CachedLIPSincBones;
		Ar << LIPSincBlendInfo;

#endif

		Ar << DebugPivots;
		Ar << DebugParents;

		Ar << SpaceBases;
	}
	unguard;
}

//
// Match up startbone by name. Also allows the attachment tag aliases.
//
INT USkeletalMeshInstance::MatchRefBone( FName StartBoneName)
{
	guardSlow(USkeletalMeshInstance::MatchRefBone);
	USkeletalMesh& Mesh = *(USkeletalMesh*)GetMesh();
	INT Bone = -1;
	if ( StartBoneName != NAME_None) 
	{
		// Match possible 'attachtag' bone names first.
		for( INT t=0; t< Mesh.TagAliases.Num(); t++)
		{
			if( StartBoneName == Mesh.TagAliases(t) )
			{
				StartBoneName = Mesh.TagNames(t);
				break;
			}
		}	
		// Search though regular bone names.
		for( INT p=0; p< ((USkeletalMesh*)GetMesh())->RefSkeleton.Num(); p++)
		{	
			if( ((USkeletalMesh*)GetMesh())->RefSkeleton(p).Name == StartBoneName )
			{
				Bone = p;
				break;
			}
		}	
	}
	return Bone;
	unguardSlow;
}

void USkeletalMeshInstance::SetScale( FVector NewScale )
{
	guard(USkeletalMeshInstance::SetScale);
	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

	Mesh->Scale = NewScale;
	// debugf(TEXT("Setting scale for mesh [%s]  %f %f %f"),Mesh->GetName(),NewScale.X,NewScale.Y,NewScale.Z );
	// Maximum mesh scaling dimension for LOD gauging. Somewhat arbitrary.
	Mesh->MeshScaleMax = ( 5.0f * 1.0f / 128.0f ) * Mesh->BoundingSphere.W * Max(Abs(Mesh->Scale.X), Max(Abs(Mesh->Scale.Y), Abs(Mesh->Scale.Z)));
	unguard;
}

UBOOL USkeletalMeshInstance::ValidateAnimChannel( INT TestChannel )
{
	guardSlow(USkeletalMeshInstance::ValidateAnimChannel);
	// Note: channel 0 is default, channel 1 is Blends(0) 
	if( TestChannel > MAXSKELANIMCHANNELS || TestChannel < 0 ) return false; // Arbitrary limit. 

	// Make sure up to TestChannel channels are allocated but no more.
	if( Blends.Num() <= TestChannel )
	{
		while( Blends.Num() <= TestChannel ) 
		{
			Blends.AddZeroed(1);
		}
		Blends.Shrink();
	}
	return true;
	unguardSlow;
}

INT USkeletalMeshInstance::GetAnimChannelCount()
{
	guardSlow(USkeletalMeshInstance::GetAnimChannelCount);
	return ( Blends.Num() ); // Blends plus the default channel..
	unguardSlow;
}


UBOOL USkeletalMeshInstance::IsAnimating(INT Channel)
{
	guardSlow(USkeletalMeshInstance::IsAnimating);
	// Status of nonzero channel:
	if( (Blends.Num() > Channel) && ( Channel >= 0) )
	{
		
		return( Blends(Channel).AnimSequence != NAME_None )
			&&( Blends(Channel).AnimFrame >=0 ? Blends(Channel).AnimRate!=0.f : Blends(Channel).TweenRate!=0.f );
	}
	return 0;
	unguardSlow;
}

//
// End all animation on all channels - optionally clean up all controllers.
//
UBOOL USkeletalMeshInstance::StopAnimating( UBOOL ClearAll )
{
	guardSlow(USkeletalMeshInstance::StopAnimating);

	// Freeze status of channel 0 - note: no animend callbacks..
	for( INT BlendIdx = 0; BlendIdx<Blends.Num(); BlendIdx++)
	{
		//Blends(BlendIdx).AnimSequence = NAME_None;
		//Blends(BlendIdx).AnimFrame = 0.0f;
		Blends(BlendIdx).AnimRate = 0.0f;				
		Blends(BlendIdx).TweenRate = 0.0f;
	}	

	// Disable controllers and nozero channels.
	if( ClearAll )
	{
		// Clear controllers.
		Directors.Empty();
		WorldSpacers.Empty();

		// Don't erase scalers - not necessary for Ragdoll, and they
		// are needed for rendering while in ragdoll mode.
	 	// Scalers.Empty();

		// Clear activity all nonzero channels too.
		for( INT BlendIdx = 1; BlendIdx<Blends.Num(); BlendIdx++)
		{
			Blends(BlendIdx).BlendAlpha = 0.0f;
			Blends(BlendIdx).AnimSequence = NAME_None;
		}
	}
	return 1;

	unguardSlow;
}


UBOOL USkeletalMeshInstance::FreezeAnimAt( FLOAT Time, INT Channel )
{
	guardSlow( USkeletalMeshInstance::FreezeAnimAt);

	// Freeze frame in channel - note: no animend callbacks..
	if( (Channel >= 0) && (Channel < Blends.Num()) )
	{

		// Scale with frame count for this animation..
		HMeshAnim InAnim = GetAnimNamed( Blends(Channel).AnimSequence );
		FLOAT FrameCount = AnimGetFrameCount( InAnim );

		if( FrameCount > 0.0001f )
			Time = Time / FrameCount;

		// Whatever current animsequence is, freeze it at 'Time'.
		Time = Max( Min( Time, 1.0f ), 0.0f );

		Blends(Channel).AnimFrame = Time;
		Blends(Channel).AnimRate  = 0.0f;				
		Blends(Channel).TweenRate = 0.0f;
		//Blends(Channel).bAnimLoop = false;
		return 1;
	}
	return 0;

	unguardSlow;
}


UBOOL USkeletalMeshInstance::IsAnimTweening(INT Channel)
{ 
	guard(USkeletalMeshInstance::IsAnimTweening);
	if( (Blends.Num() > Channel) && (Channel >= 0) )
	{
		if( (Blends(Channel).AnimFrame < 0.0)  && IsAnimating(Channel) )
			return true;
	}
	return false; 
	unguard;
}

UBOOL USkeletalMeshInstance::IsAnimLooping(INT Channel)
{ 
	guard(USkeletalMeshInstance::IsAnimLooping);
	if( (Blends.Num() > Channel) && (Channel >= 0) )
	{		
		return Blends(Channel).bAnimLoop; 
	}
	return false; 
	unguard;
}

UBOOL USkeletalMeshInstance::IsAnimPastLastFrame(INT Channel)
{ 
	guard(USkeletalMeshInstance::IsAnimPastLastFrame);
	if( (Blends.Num() > Channel) && (Channel >= 0) )
	{		
		return ( Blends(Channel).AnimFrame >= Blends(Channel).AnimLast );
	}
	return false; 
	unguard;
}

UBOOL USkeletalMeshInstance::AnimStopLooping( INT Channel )
{ 
	guard(USkeletalMeshInstance::AnimStopLooping);
	if( (Blends.Num() > Channel) && (Channel >= 0) )
	{
		Blends(Channel).bAnimLoop = 0;
		Blends(Channel).bAnimFinished = 0;
		return true;
	}
	return false; 
	unguard;
}


FName USkeletalMeshInstance::GetActiveAnimSequence(INT Channel)
{ 
	guard(USkeletalMeshInstance::GetActiveAnimSequence);
	if( (Blends.Num() > Channel) && (Channel >= 0))
	{
		return Blends(Channel).AnimSequence;
	}	
	return NAME_None; 
	unguard;
}

FLOAT USkeletalMeshInstance::GetActiveAnimRate(INT Channel)
{ 
	guard(USkeletalMeshInstance::GetActiveAnimRate);
	if( (Blends.Num() > Channel) && (Channel >= 0))
	{
		return Blends(Channel).AnimRate;
	}		
	return 0.f; 
	unguard;
}

FLOAT USkeletalMeshInstance::GetActiveAnimFrame(INT Channel)
{ 
	guard(USkeletalMeshInstance::GetActiveAnimFrame);
	if( (Blends.Num() > Channel) && (Channel >= 0))
	{
		return Blends(Channel).AnimFrame;
	}		
	return 0.f; 
	unguard;
}


UBOOL USkeletalMeshInstance::SetBoneScale( INT Slot, FLOAT BoneScale, FName BoneName )
{
	guard(USkeletalMeshInstance::SetBoneScale);

	if( (Slot<0) || (Slot > MAXSKELBONESCALERS) ) return false; // arbitrary limit
	
	if( Scalers.Num() <= Slot )
	{
		while( Scalers.Num() <= Slot)
		{
			// Add empty scalers.
			Scalers.AddZeroed(1);
			Scalers(Scalers.Num()-1).Bone = -1;
		}
		Scalers.Shrink();
	}
	
	// Set or clear the bone scaler:
	if( (BoneScale == 1.0f) || (BoneName == NAME_None) )
	{
		// Erase scaler...
		Scalers(Slot).Bone = -1;
		Scalers(Slot).BoneName = NAME_None;
	}
	else 
	{
		INT StartBone = MatchRefBone( BoneName );		
		if( StartBone >= 0 )
		{
			// Located bone.
			Scalers(Slot).Bone = StartBone;
			Scalers(Slot).BoneName = BoneName;
			Scalers(Slot).ScaleUniform = BoneScale;
		}
	}

	return true;
	unguard;
}

UBOOL USkeletalMeshInstance::SetBoneLocation( FName BoneName, FVector BoneTrans, FLOAT Alpha )
{
	guardSlow(USkeletalMeshInstance::SetBoneLocation);

	if( Directors.Num() >= MAXSKELBONEMOVERS ) return false; // arbitrary limit

	INT StartBone = MatchRefBone( BoneName );
	if( StartBone >= 0 )
	{
		INT ThisDir = -1;
		for(INT t=0; t< Directors.Num(); t++)
		{
			if( Directors(t).BoneName == BoneName )
			{
				ThisDir=t;
				break;
			}
		}
		if( ThisDir < 0)
		{
			// New bone controller.
			ThisDir = Directors.Num();
			Directors.Add();			
			Directors(ThisDir).BoneName = BoneName;
			Directors(ThisDir).TurnAlpha = 0.0f;
			Directors(ThisDir).Bone = StartBone;
		}
		
		Directors(ThisDir).TransAlpha = Alpha;
		Directors(ThisDir).Trans      = BoneTrans;		
		return true;
	}
	return false;
	unguardSlow;
}

UBOOL USkeletalMeshInstance::SetBoneRotation( FName BoneName, FRotator BoneTurn, INT Space, FLOAT Alpha )
{
	guardSlow(USkeletalMeshInstance::SetBoneRotation);

	if( Directors.Num() >= MAXSKELBONEMOVERS ) return false; // arbitrary limit

	INT StartBone = MatchRefBone( BoneName );
	if( StartBone >= 0 )
	{
		INT ThisDir = -1;
		for(INT t=0; t< Directors.Num(); t++)
		{
			if( Directors(t).BoneName == BoneName )
			{
				ThisDir=t;
				break;
			}
		}
		if( ThisDir < 0)
		{
			// New bone controller.
			ThisDir = Directors.Num();
			Directors.Add();			
			Directors(ThisDir).BoneName = BoneName;
			Directors(ThisDir).TransAlpha = 0.0f;
			Directors(ThisDir).Bone = StartBone;			
		}

		Directors(ThisDir).Flags = Space; // 0=local 1=global 2=refpose  space.
		Directors(ThisDir).TurnAlpha = Alpha;
		Directors(ThisDir).Turn      = BoneTurn;
		return true;
	}
	return false;
	unguardSlow;
}

// Set a bone direction in worldspace.
UBOOL USkeletalMeshInstance::SetBoneDirection( FName BoneName, FRotator BoneTurn, FVector BoneTrans, FLOAT Alpha, INT Space )
{
	guardSlow(USkeletalMeshInstance::SetBoneDirection);

	if( WorldSpacers.Num() >= MAXSKELBONEMOVERS ) return false; // arbitrary limit

	INT StartBone = MatchRefBone( BoneName );
	if( StartBone >= 0 )
	{
		INT ThisDir = -1;
		// Existing controller?
		for(INT t=0; t< WorldSpacers.Num(); t++)
		{
			if( WorldSpacers(t).BoneName == BoneName )
			{
				ThisDir=t;
				break;
			}
		}

		if( ThisDir < 0)
		{
			// New bone controller.
			ThisDir = WorldSpacers.Num();
			WorldSpacers.Add();			
			WorldSpacers(ThisDir).BoneName = BoneName;
			WorldSpacers(ThisDir).TransAlpha = 0.0f;
			WorldSpacers(ThisDir).Bone = StartBone;
		}
		
		WorldSpacers(ThisDir).Flags = Space;         
		WorldSpacers(ThisDir).TurnAlpha  = Alpha;
		WorldSpacers(ThisDir).TransAlpha = Alpha; 
		WorldSpacers(ThisDir).Turn      = BoneTurn;
		WorldSpacers(ThisDir).Trans     = BoneTrans;
		return true;
	}	
	return false;
	unguardSlow;
}


INT LinkAnimBonesToMesh( USkeletalMesh* Mesh,const UMeshAnimation* MeshAnim, TArray<INT>& CachedLinks)
{
	guardSlow( USkeletalMeshInstance::LinkAnimBonesToMesh );
	INT LinksFound = 0;
	CachedLinks.Empty();
	CachedLinks.Add(Mesh->RefSkeleton.Num());

	for( INT p=0; p< Mesh->RefSkeleton.Num(); p++)			
	{
		CachedLinks(p) = -1;
		for( INT b=0; b< MeshAnim->RefBones.Num(); b++)
		{				
			if( Mesh->RefSkeleton(p).Name == MeshAnim->RefBones(b).Name ) 
			{
				CachedLinks(p) = b;
				LinksFound++;				
				 //debugf(TEXT(" Linked up %s with %s  index %i "),*MeshAnim->RefBones(b).Name,*(Mesh->RefSkeleton(p).Name), MeshAnim->RefBones(b).Name.GetIndex() ); 
				break;
			}
		}		

		//if(CachedLinks(p) == -1)
		//	debugf(TEXT(" Unmatched link %s index %i "),*(Mesh->RefSkeleton(p).Name),p);
	}
	if( LinksFound == 0)
	{
		debugf(TEXT("Warning: Unable to match any animation tracks from [%s] to skeletal mesh [%s]."),MeshAnim->GetName(),Mesh->GetName());
	}
	
	return LinksFound;
	unguardSlow;
}


INT LinkMeshBonesToMesh( USkeletalMesh& Mesh,const USkeletalMesh& OwnerMesh, MeshAnimLinkup* AnimLinkup)
{
	guard( USkeletalMeshInstance::LinkMeshBonesToMesh );
	INT LinksFound = 0;

	guard(TESTLINKS);
	AnimLinkup->CachedLinks.Empty();
	unguard;
	guard(TESTLINKS2);
	AnimLinkup->CachedLinks.Add(Mesh.RefSkeleton.Num());
	unguard;
	

	for( INT p=0; p< Mesh.RefSkeleton.Num(); p++)			
	{
		AnimLinkup->CachedLinks(p) = -1;
		for( INT b=0; b< OwnerMesh.RefSkeleton.Num(); b++)
		{				
			if( OwnerMesh.RefSkeleton(b).Name == Mesh.RefSkeleton(p).Name ) 
			{
				AnimLinkup->CachedLinks(p) = b;
				LinksFound++;
				break;
			}
		}		        
		//if( AnimLinkup->CachedLinks(p) == -1 )
		//	debugf(TEXT(" Unmatched link %s index %i "),*(Mesh.RefSkeleton(p).Name),p);
	}

	if( LinksFound == 0)
	{
		debugf(TEXT("Warning: Unable to match any reference pose bones from mesh [%s] to mesh [%s]."),Mesh.GetName(),OwnerMesh.GetName());
	}

	return LinksFound;

	unguard;
}


INT FindLinkedAnimIndex(TArray<MeshAnimLinkup>& AnimLinkups,UMeshAnimation* OurAnimObj )
{
	for(INT i=0; i< AnimLinkups.Num(); i++)
	{
		if( AnimLinkups(i).Anim == OurAnimObj )
			return i;
	}
	return -1;
}


//
// PlayAnim lets the meshinstance take over control of animation from the actor.
//
UBOOL USkeletalMeshInstance::PlayAnim(INT channel,FName SequenceName, FLOAT InRate, FLOAT InTweenTime, UBOOL InLooping) 
{
	//debugf(TEXT("PlayAnim: Gticks %i Sequence[%s] Channel %i InTweenTime %f  InRate %f "),(INT)GTicks, *SequenceName, channel, InTweenTime, InRate); 

	guard( USkeletalMeshInstance::PlayAnim );
	if ( (channel < 0) || !ValidateAnimChannel( channel ) )
	{
		debugf(TEXT("Invalid active channel: [%i] for MeshInstance [%s]"), channel, GetName());
		return false;
	}

	HMeshAnim InAnim = GetAnimNamed( SequenceName );
	if( !InAnim ) 
	{
		debugf( TEXT("PlayAnim: Sequence '%s' not found for mesh '%s'"), *SequenceName, GetMesh()->GetName() );
		return false;
	}

	AActor* Actor = GetActor();
	if ( !Actor )
		return 0;
	
	// NOTE - There are several cases of some animation-driven non-visible meshes, like the 'virtual' 
	// first-person weapons of 3rd-person players.

	MeshAnimChannel* Blend = &Blends( channel );		
	UMeshAnimation* OurAnimObj = FindAnimObjectForSequence( SequenceName );
	if( !OurAnimObj )
	{
		debugf(TEXT("No animation object found which contains sequence [%s]"),*SequenceName );
		return 0;
	}
		
	Blend->MeshAnimIndex = FindLinkedAnimIndex( AnimLinkups, OurAnimObj );	

	// Regular playBlend->Anim.
	if( !InLooping && (InRate > 0.f) )	
	{
		if( Blend->AnimSequence == NAME_None )
			InTweenTime = 0.0f;

		FLOAT SeqFrameCount = AnimGetFrameCount( InAnim );
		if ( SeqFrameCount <= 0.f  ) // Valid frame count ?
			return 0;		
		Blend->AnimSequence  = SequenceName;
		Blend->OrigRate      = AnimGetRate( InAnim ) / SeqFrameCount;
		Blend->AnimRate      = InRate * Blend->OrigRate;		
		Blend->AnimLast      = 1.0f - 1.0f / SeqFrameCount; // AnimFrame at the last frame.
		Blend->bAnimNotify   = AnimGetNotifyCount( InAnim ) != 0;
		Blend->bAnimFinished = 0;
		Blend->bAnimLoop     = 0;

		if( Blend->AnimLast == 0.0f )
		{
			Blend->bAnimNotify   = 0;
			Blend->OldAnimRate   = 0.0f;
			
			if( InTweenTime > 0.0f )
				Blend->TweenRate = 1.0f / InTweenTime;
			else
				Blend->TweenRate = 10.0f; //tween in 0.1 sec
			Blend->AnimFrame = -1.0f/SeqFrameCount;
			Blend->AnimRate = 0.f;
		}
		else if( InTweenTime>0.0f )
		{
			Blend->TweenRate = 1.0f / (InTweenTime * SeqFrameCount);
			Blend->AnimFrame = -1.0f/SeqFrameCount;
		}
		else if ( InTweenTime == -1.0f )
		{
			Blend->AnimFrame = -1.0f/SeqFrameCount;
			if ( Blend->OldAnimRate > 0.0f )
				Blend->TweenRate = Blend->OldAnimRate;
			else if ( Blend->OldAnimRate < 0.0f ) //was velocity based looping
				Blend->TweenRate = ::Max(0.5f * Blend->AnimRate, -1 * GetActor()->Velocity.Size() * Blend->OldAnimRate );
			else
				Blend->TweenRate =  1.0f/(0.025f * SeqFrameCount);
		}
		else
		{
			Blend->TweenRate = 0.0f;
			Blend->AnimFrame = 0.001f;
		}

		Blend->OldAnimRate = Blend->AnimRate;

		return 1;
	}

	// Regular LoopBlend->Anim
	if( InLooping )	
	{
		FLOAT SeqFrameCount = AnimGetFrameCount( InAnim );
		if ( SeqFrameCount <= 0.f  ) // Valid frame count ?
			return 0;		

		FLOAT SeqRate = AnimGetRate( InAnim ); // Xbox compiler appeasing..

		if ( (Blend->AnimSequence == SequenceName) && Blend->bAnimLoop && GetActor()->IsAnimating(channel) )
		{
			Blend->OrigRate      = SeqRate / SeqFrameCount;
			Blend->AnimRate      = InRate * Blend->OrigRate;			
			Blend->bAnimFinished = 0;
			Blend->OldAnimRate   = Blend->AnimRate;		
			return 1;
		}

		//if( Blend->AnimSequence == NAME_None )
		//	InTweenTime = 0.0f;

		Blend->AnimSequence  = SequenceName;
		Blend->OrigRate      = SeqRate / SeqFrameCount;
		Blend->AnimRate      = InRate * Blend->OrigRate;		
		Blend->AnimLast      = 1.0f - 1.0f / SeqFrameCount;
		Blend->bAnimNotify   = AnimGetNotifyCount( InAnim ) != 0;
		Blend->bAnimFinished = 0;
		Blend->bAnimLoop     = 1;
		if ( Blend->AnimLast == 0.0f )
		{
			Blend->bAnimNotify   = 0;
			Blend->OldAnimRate   = 0;
			if ( InTweenTime > 0.0f )
				Blend->TweenRate = 1.0f / InTweenTime;
			else
				Blend->TweenRate = 10.0f; //tween in 0.1 sec
			Blend->AnimFrame = -1.0f/SeqFrameCount;
			Blend->AnimRate = 0;
		}
		else if( InTweenTime>0.0f )
		{
			Blend->TweenRate = 1.0f / (InTweenTime * SeqFrameCount);
			Blend->AnimFrame = -1.0f/SeqFrameCount;
		}
		else if ( InTweenTime == -1.0f )
		{
			Blend->AnimFrame = -1.0f/SeqFrameCount;
			if ( Blend->OldAnimRate > 0 )
				Blend->TweenRate = Blend->OldAnimRate;
			else if ( Blend->OldAnimRate < 0 ) //was velocity based looping
				Blend->TweenRate = ::Max(0.5f * Blend->AnimRate, -1 * GetActor()->Velocity.Size() * Blend->OldAnimRate );
			else
				Blend->TweenRate =  1.0f/(0.025f * SeqFrameCount);
		}
		else
		{
			Blend->TweenRate = 0.0f;
			Blend->AnimFrame = 0.0001f;
		}		
		Blend->OldAnimRate = Blend->AnimRate;
		return 1;
	}

	// Regular Blend->TweenBlend->Anim.
	if( InRate == 0.0f )	
	{
		FLOAT SeqFrameCount = AnimGetFrameCount( InAnim );
		FLOAT SeqRate = AnimGetRate( InAnim );

		if ( SeqFrameCount <= 0.f  ) // Valid frame count ?
			return 0;		
		Blend->AnimSequence  = SequenceName;
		Blend->AnimLast      = 0.0f;
		Blend->bAnimNotify   = 0;
		Blend->bAnimFinished = 0;
		Blend->bAnimLoop     = 0;
		Blend->AnimRate      = 0.f;
		Blend->OldAnimRate   = 0.f;
		Blend->OrigRate      = SeqRate / SeqFrameCount;
		if( InTweenTime>0.0f )
		{
			Blend->TweenRate =  1.0f/(InTweenTime * SeqFrameCount);
			Blend->AnimFrame = -1.0f/SeqFrameCount;
		}
		else
		{
			Blend->TweenRate = 0.0f;
			Blend->AnimFrame = 0.0f;
		}		
		return 1;
	}

	// Unknown combination of rates and flags.
	return 0;

	unguard;
};


//
// Refresh mesh skeleton - to - animdata skeleton linkups-by-name if required.
//
void USkeletalMeshInstance::ActualizeAnimLinkups()
{		
	for( int a=0; a<AnimLinkups.Num(); a++)
	{
		if( AnimLinkups(a).Anim )
		{
			if( AnimLinkups(a).Mesh != GetMesh()  )
			{
				LinkAnimBonesToMesh((USkeletalMesh*)GetMesh(), AnimLinkups(a).Anim, AnimLinkups(a).CachedLinks );
				AnimLinkups(a).Mesh = GetMesh();				
			}
		}
	}
}

//
// Ensure bones have been updated/set up at least once using GetFrame ( useful for first->3rd person switches on death or end-of-game that used to still be in T-poses )
//
void USkeletalMeshInstance::ForceBoneRefresh()
{
	guard(USkeletalMeshInstance::ForceBoneRefresh);
	AActor* Owner = GetActor();		
	if( Owner )
	{
		INT DummyVerts;
		GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly); 		
	}		
	unguard;
}

//
// Allow setting of mesh without erasing current animation channel states.
//
void USkeletalMeshInstance::SetMesh(UMesh* InMesh)
{	
	OurMesh = (USkeletalMesh*)InMesh;

	// Mesh-specific persistent animation arrays have to be refreshed - i.e., not possible to swap meshes during a tween.
	CachedOrientations.Empty();
	CachedPositions.Empty();	
	SpaceBases.Empty();	

	Scalers.Empty();
	Directors.Empty();
	WorldSpacers.Empty();		

	// Clear bone-to-bone linkups (so animation states can continue on new mesh.)
	ActualizeAnimLinkups();	
}

//
// Setting channel-specific animation object.
//

UBOOL USkeletalMeshInstance::SetSkelAnim( UMeshAnimation* NewAnimation, USkeletalMesh* NewMesh )
{
	guardSlow( USkeletalMeshInstance::SetSkelAnim);

	if( NewAnimation )
	{
		INT Index = FindLinkedAnimIndex( AnimLinkups, NewAnimation );
		if (Index == -1) // add new one
		{
			Index = AnimLinkups.Num();
			AnimLinkups.AddZeroed();
			AnimLinkups(Index).Anim = NewAnimation;
			AnimLinkups(Index).Mesh = NewMesh;
		}		
		ActualizeAnimLinkups();
	}	

	return true;
	unguardSlow;
}

void USkeletalMeshInstance::ClearSkelAnims()
{
	guardSlow( USkeletalMeshInstance::ClearSkelAnims);	

	for(INT i=0; i<AnimLinkups.Num(); i++)
	{
		AnimLinkups(i).CachedLinks.Empty();
		//AnimLinkups(i).RefBoneAdjusts.Empty();
	}
	AnimLinkups.Empty();
	unguardSlow;
}


void VerifyAnimationPresent(USkeletalMeshInstance* MInst)
{
	guardSlow( UnSkeletalMesh_VerifyAnimationPresent );
	// Linking up default animation necessary ?  
	if( MInst && MInst->AnimLinkups.Num() == 0  && ((USkeletalMesh*)MInst->GetMesh())->DefaultAnim )
	{
		MInst->SetSkelAnim( ((USkeletalMesh*)MInst->GetMesh())->DefaultAnim, ((USkeletalMesh*)MInst->GetMesh())->DefaultRefMesh );
	}
	unguardSlow;
}

//
// Setting channel-specific animation parameters.
//
UBOOL USkeletalMeshInstance::SetBlendParams(INT Channel, FLOAT BlendAlpha, FLOAT InTime, FLOAT OutTime, FName StartBoneName, UBOOL bGlobalPose )
{
	guardSlow( USkeletalMeshInstance::SetBlendParams );
	// Ensure the channel is allocated.
	if( !ValidateAnimChannel(Channel) ) 
	{
		return false;
	}

	if( Channel == 0 )
	{
		debugf(TEXT("Warning: attempt to set blending parameters for base stage in meshinstance [%s]"),GetName());
		return false;
	}

	INT StartBone = MatchRefBone( StartBoneName );
	if( StartBone < 0 ) // Default to the rootbone.
	{
		//debugf(TEXT("Warning: blend at unknown bone [%s] - reverting to root bone."),StartBoneName);
		StartBone = 0;
	}
	
	MeshAnimChannel& Blend = Blends( Channel );
	// Set parameters.
	Blend.BlendAlpha   = BlendAlpha;
	Blend.BlendInTime  = Min(InTime,1.0f);
	Blend.BlendOutTime = Min(OutTime,1.0f);
	Blend.StartBone =    StartBone;
	Blend.bGlobalPose = bGlobalPose?1:0;
	// Disable any timed blending in progress.  
	Blend.bTimedBlend = false;

	return true;
	unguardSlow;
}

// Return current animation object active for a channel.
UMeshAnimation* USkeletalMeshInstance::CurrentSkelAnim( INT channel )
{
	guardSlow( USkeletalMeshInstance::CurrentSkelAnim);

	VerifyAnimationPresent(this);	
	
	if( (channel >= 0) && (channel < Blends.Num())  )
	{
		if( ( Blends(channel).MeshAnimIndex > -1 ) && 
			( AnimLinkups.Num() > Blends(channel).MeshAnimIndex ) &&  
			  AnimLinkups( Blends(channel).MeshAnimIndex ).Anim )			  
			return AnimLinkups( Blends(channel).MeshAnimIndex ).Anim;
		else 
			return ((USkeletalMesh*)GetMesh())->DefaultAnim;		
	}
	else 
	{	
		
		return ((USkeletalMesh*)GetMesh())->DefaultAnim;
	}
	unguardSlow;
}

// Return number of animations supported by the mesh instance.
INT USkeletalMeshInstance::GetAnimCount()
{
	guardSlow( USkeletalMeshInstance::GetAnimCount );

	VerifyAnimationPresent(this);	
	
	// Get all animation sequences from ALL linked-up animation objects.
	INT TotalSequences = 0;
	for(INT i=0; i< AnimLinkups.Num(); i++)
	{
		if( AnimLinkups(i).Anim )
			TotalSequences += AnimLinkups(i).Anim->AnimSeqs.Num();
	}
	return TotalSequences;

	unguardSlow;
}


FLOAT USkeletalMeshInstance::GetAnimRateOnChannel(INT Channel)
{
	guardSlow( USkeletalMeshInstance::GetAnimRateOnChannel );
	if( !ValidateAnimChannel( Channel ))
		return 30.f;
	FLOAT Result = AnimGetRate(GetAnimNamed(GetAnimSequence(Channel)));
	if ( Result == 0.f )
		return 30.f;
	return Result;
	unguardSlow;
}

// Return animation for a given index.
HMeshAnim USkeletalMeshInstance::GetAnimIndexed(INT InIndex)
{
	guardSlow( USkeletalMeshInstance::GetAnimIndexed );

	//#FIXME - allow indexing beyond SkelAnim(0)!!
    if( CurrentSkelAnim(0) && (CurrentSkelAnim(0)->AnimSeqs.Num() > InIndex) )
	{
		return (HMeshAnim) &(CurrentSkelAnim(0)->AnimSeqs( InIndex));
	}
	else
	return (HMeshAnim) NULL;

	unguardSlow;
}

// Return animation for a given name
HMeshAnim USkeletalMeshInstance::GetAnimNamed(FName InName)
{
	guardSlow(USkeletalMeshInstance::GetAnimNamed);

	VerifyAnimationPresent(this);	
	
	for(INT a=0; a< AnimLinkups.Num(); a++)
	{
		if( AnimLinkups(a).Anim )
		{
			for( INT i=0; i<AnimLinkups(a).Anim->AnimSeqs.Num(); i++ )
				if( InName == AnimLinkups(a).Anim->AnimSeqs(i).Name )
					return (HMeshAnim) &(AnimLinkups(a).Anim->AnimSeqs(i)); // return a disguised FmeshAnimSeq pointer.		
		}
	}
	return NULL;
	unguardSlow;
}

// Get the name of a given animation
FName USkeletalMeshInstance::AnimGetName(HMeshAnim InAnim)
{
	guardSlow( USkeletalMeshInstance::AnimGetName );
	if( InAnim) 
		return ((FMeshAnimSeq*)InAnim)->Name;
	else
		return NAME_None;
	unguardSlow;
}
// Get the group of a given animation
FName USkeletalMeshInstance::AnimGetGroup(HMeshAnim InAnim)
{
	guardSlow( USkeletalMeshInstance::AnimGetGroup );
	if( ((FMeshAnimSeq*)InAnim)->Groups.Num() )
		return ((FMeshAnimSeq*)InAnim)->Groups(0);
	else
		return NAME_None;
	unguardSlow;
}

// See if an animation has this particular group tag.
UBOOL USkeletalMeshInstance::AnimIsInGroup(HMeshAnim InAnim, FName Group)
{
	guardSlow( USkeletalMeshInstance::AnimIsInGroup );
	INT i=0;
	if( InAnim )
		return ((FMeshAnimSeq*)InAnim)->Groups.FindItem(Group,i) ;
	else
		return false;
	unguardSlow;
}
// Get the number of frames in an animation
FLOAT USkeletalMeshInstance::AnimGetFrameCount(HMeshAnim InAnim)
{
	guardSlow( USkeletalMeshInstance::AnimGetFrameCount );
	if( InAnim )
		return ((FMeshAnimSeq*)InAnim)->NumFrames;
	else
		return 0.0f;
	unguardSlow;
}
// Get the play rate of the animation in frames per second
FLOAT USkeletalMeshInstance::AnimGetRate(HMeshAnim InAnim)
{
	guardSlow( USkeletalMeshInstance::AnimGetRate );
	if( InAnim )
		return ((FMeshAnimSeq*)InAnim)->Rate;
	else
		return 0.0f;
	unguardSlow;
}
// Get the number of notifications associated with this animation.
INT USkeletalMeshInstance::AnimGetNotifyCount(HMeshAnim InAnim)
{
	guardSlow( USkeletalMeshInstance::AnimGetNotifyCount );
	if( InAnim )
		return ((FMeshAnimSeq*)InAnim)->Notifys.Num();
	else
		return 0;
	unguardSlow;
}
// Get the time of a particular notification.
FLOAT USkeletalMeshInstance::AnimGetNotifyTime(HMeshAnim InAnim, INT InNotifyIndex)
{
	guardSlow( USkeletalMeshInstance::AnimGetNotifyTime );
	if( InAnim )
		return ((FMeshAnimSeq*)InAnim)->Notifys(InNotifyIndex).Time;
	else
		return 0.0f;
	unguardSlow;
}
// Get text associated with a given notify.
const TCHAR* USkeletalMeshInstance::AnimGetNotifyText(HMeshAnim InAnim, INT InNotifyIndex)
{
	guardSlow( USkeletalMeshInstance::AnimGetNotifyText );
	if( InAnim )
		return *(((FMeshAnimSeq*)InAnim)->Notifys(InNotifyIndex).Function); // FName to string
	else
		return NULL;
	unguardSlow;
}
// Get UAnimNotify function associated with a given notify.
UAnimNotify* USkeletalMeshInstance::AnimGetNotifyObject(HMeshAnim InAnim, INT InNotifyIndex)
{
	guardSlow( USkeletalMeshInstance::AnimGetNotifyObject );
	if( InAnim )
		return ((FMeshAnimSeq*)InAnim)->Notifys(InNotifyIndex).NotifyObject;
	else
		return NULL;
	unguardSlow;
}

UMeshAnimation* USkeletalMeshInstance::FindAnimObjectForSequence( FName SeqName )
{
	guardSlow( USkeletalMeshInstance::FindAnimObjectForSequence);

	VerifyAnimationPresent(this);	

	// Go over all linked animations to find one containing SeqName..
	for(INT a=0; a< AnimLinkups.Num(); a++)
	{
		//FMeshAnimSeq* ResultSeq = AnimLinkups(a).Anim->GetAnimSeq( SeqName );
		FMeshAnimSeq* ResultSeq = NULL;

		if( AnimLinkups(a).Anim )
			ResultSeq = AnimLinkups(a).Anim->GetAnimSeq( SeqName );					
		if( ResultSeq )		
			return  AnimLinkups(a).Anim;
	}
	return NULL;
	unguardSlow;
}

UBOOL USkeletalMeshInstance::AnimForcePose( FName SeqName, FLOAT AnimFrame, FLOAT Delta, INT Channel)
{
	guardSlow( USkeletalMeshInstance::AnimForcePose);
	// Typically called from Editor.
	if( !ValidateAnimChannel( Channel ))
	{
		return 0;
	}

	if( (Blends.Num() > Channel) && (Channel >= 0) )
	{
		UMeshAnimation* FoundAnimObject = FindAnimObjectForSequence( SeqName );
		if (FoundAnimObject )
		{
			// process notifies
			FMeshAnimSeq* Seq = FoundAnimObject->GetAnimSeq( SeqName );
			if( Seq )
			{
				for( INT i=0; i< AnimGetNotifyCount(Seq) ; i++ )
				{
					FLOAT NotifyTime = AnimGetNotifyTime(Seq, i);

					FLOAT OldFrame = AnimFrame-Delta;
					if( (Delta > 0.f && ((NotifyTime <= AnimFrame && NotifyTime > OldFrame) || (NotifyTime > OldFrame+1.f))) ||
						(Delta < 0.f && NotifyTime >= AnimFrame && NotifyTime < OldFrame) )
					{
						UAnimNotify* AnimNotify = AnimGetNotifyObject(Seq, i);
						if( AnimNotify )					
							AnimNotify->Notify( this, GetActor() );
					}
				}
			}

			//Blends(Channel).SkelAnim  = FoundAnimObject;			
			Blends(Channel).MeshAnimIndex = FindLinkedAnimIndex ( AnimLinkups, FoundAnimObject );
			Blends(Channel).AnimFrame = AnimFrame;
			Blends(Channel).AnimSequence = SeqName; 
			return 1;	
		}			
		else
		{
			// debugf(TEXT("Animation sequence not found in any of the linked animation objects."));
			return 0;
		}
	}	
	else
	{
		debugf(TEXT("Invalid active channel: [%i] for MeshInstance [%s]"), Channel, this->GetName());
		return 0;
	}
	unguardSlow;
}

//
// UpdateAnimation. Moves forward all animation channels, and calls notifies tied to the active sequences.
//
UBOOL USkeletalMeshInstance::UpdateAnimation(FLOAT DeltaSeconds) 
{ 
	guard(UpdateAnimation);

	if( GetActor()->bAnimByOwner && GetActor()->Owner && GetActor()->Owner->Mesh && GetActor()->Owner->Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		// All animation for this mesh(-instance) including notifications are all left to its owner.
		if( GetStatus() & MINST_DeleteMe )
			delete this;				
		return true;
	}

	SetStatus( MINST_InUse );	
	FLOAT WorkSeconds = DeltaSeconds;
	bRootTrafoStale = 1;
	INT Iterations = 0;

	// Verify instance validity.
	if( !( GetStatus() & MINST_DeleteMe ) )
	{
		// Ticking of all channels.
		for( INT channel =0; channel< Blends.Num(); channel++)
		{
			Iterations = 0;
			WorkSeconds = DeltaSeconds;
			MeshAnimChannel* Blend = &Blends( channel );

			// Update blend-to-target alpha.
			if (   Blend->bTimedBlend 
				&& (Blend->MeshAnimIndex > -1)
				//&&  GetActor()->IsAnimating(channel)
				&&	(WorkSeconds>0.0f))
			{				
				// Timed blend: proceed from current alpha to Blend->BlendTargetAlpha over  Blend->BlendTargetInterval.
				FLOAT TimeDelta = Min( 1.0f,  DeltaSeconds / Blend->BlendTargetInterval );
				FLOAT AlphaDelta = (Blend->BlendTargetAlpha - Blend->BlendAlpha) * TimeDelta;
				Blend->BlendAlpha += AlphaDelta;
				Blend->BlendTargetInterval = Max( Blend->BlendTargetInterval - DeltaSeconds, 0.f );
				// Done with target-alpha blending ?
				if( Blend->BlendTargetInterval == 0.f )
					Blend->bTimedBlend = false;					
			}
			
			// Original AnimUpdate
			while
			(   (Blend->MeshAnimIndex > -1)
			&&  GetActor()->IsAnimating(channel)
			&&	(WorkSeconds>0.0f)
			&&	(++Iterations <= 4) )
			{			
				// Remember the old frame.
				FLOAT OldAnimFrame = Blend->AnimFrame;

				// Update animation, and possibly overflow it.
				if( Blend->AnimFrame >= 0.0f )
				{
					// Update regular or velocity-scaled animation.
					if( Blend->AnimRate >= 0.0f )
						Blend->AnimFrame += Blend->AnimRate * WorkSeconds;
					else
						Blend->AnimFrame += ::Max( 0.3f, GetActor()->Velocity.Size() * -Blend->AnimRate ) * WorkSeconds;
					//FIXME - instead of fixed 0.3, make it an animation attribute?

					// Handle all animation sequence notifys.
					if( Blend->bAnimNotify && GetActor()->Mesh && !Blend->bNotifyIgnore ) 
					{				
						HMeshAnim Seq = GetAnimNamed( Blend->AnimSequence );
						if( Seq )
						{
							FLOAT BestElapsedFrames = 100000.0f;
							INT BestNotifyIdx = -1;

							for( INT i=0; i< AnimGetNotifyCount(Seq) ; i++ )
							{
								FLOAT NotifyTime = AnimGetNotifyTime( Seq, i);

								if( OldAnimFrame < NotifyTime && Blend->AnimFrame >= NotifyTime )
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
								// Update workseconds: remaining stretch of seconds we're still evaluating.
								WorkSeconds = WorkSeconds * ( Blend->AnimFrame - AnimGetNotifyTime( Seq, BestNotifyIdx)  ) / (Blend->AnimFrame - OldAnimFrame);
								
								// Enter script with an intermediate animframe - the actual notify time.
								Blend->AnimFrame = AnimGetNotifyTime( Seq, BestNotifyIdx); 

								UAnimNotify* AnimNotify = AnimGetNotifyObject(Seq, BestNotifyIdx);
								if( AnimNotify )
								{
									LastNotifyStage = channel;
									AnimNotify->Notify( this, GetActor() );
								}
								continue;
							}
						}
					}

					// Handle end of animation sequence.
					if( Blend->AnimFrame<Blend->AnimLast )
					{
						// We have finished the animation updating for this tick.
						break;
					}
					else if( Blend->bAnimLoop )
					{
						if( Blend->AnimFrame < 1.0f )
						{
							// Still looping.
							WorkSeconds = 0.0f;
						}
						else
						{
							// Just passed end, so loop it.
							WorkSeconds = WorkSeconds * (Blend->AnimFrame - 1.0f) / (Blend->AnimFrame - OldAnimFrame);
							Blend->AnimFrame = 0.0f;
						}
						if( OldAnimFrame < Blend->AnimLast )
						{
							LastAnimEndStage = channel;
							if( !Blend->bNotifyIgnore ) 
							{
								//debugf(TEXT("End-anim notify 1 (looping animation) for mesh %s  anim %s channel %i"),GetActor()->Mesh->GetName(), *Blend->AnimSequence, channel );
								GetActor()->NotifyAnimEnd(LastAnimEndStage);														
							}
						}
					}
					else 
					{
						// Just passed end-minus-one frame, not-looping.
						WorkSeconds = WorkSeconds * (Blend->AnimFrame - Blend->AnimLast) / (Blend->AnimFrame - OldAnimFrame);						
						Blend->AnimFrame	 = Blend->AnimLast; 
						LastAnimEndStage = channel;

						if( !Blend->bNotifyIgnore && (Blend->AnimRate > 0.0f) ) // No notifies for zero animrate either.
						{
							//debugf(TEXT("End-anim notify 2 for mesh %s  anim %s channel %i"),GetActor()->Mesh->GetName(), *Blend->AnimSequence, channel );
							Blend->AnimRate = 0.0f; 
							GetActor()->NotifyAnimEnd(LastAnimEndStage);												
						}
						else						
						{
							Blend->AnimRate = 0.0f; 
						}								
					}
				}
				else   
				//
				//	Update tweening.
				//
				{					
					Blend->AnimFrame += Blend->TweenRate * WorkSeconds;
					if( Blend->AnimFrame >= 0.0f )
					{
						// Finished tweening.
						WorkSeconds          = WorkSeconds * (Blend->AnimFrame - 0.0f) / (Blend->AnimFrame - OldAnimFrame);
						Blend->AnimFrame = 0.0f;
						if( Blend->AnimRate == 0.0f )
						{
							LastAnimEndStage = channel;
							if( !Blend->bNotifyIgnore ) 
							{
								// debugf(TEXT("End-anim notify 3 for mesh %s  anim %s channel %i"),GetActor()->Mesh->GetName(), *Blend->AnimSequence, channel );
								GetActor()->NotifyAnimEnd(LastAnimEndStage);														
							}
						}
					}
					else
					{
						// Still tweening. Done with this channel's update.
						break;
					}
				}
			} // Iteration.
			
						
			if( channel == 0 )
			{   
				FLOAT NewRate = 0.0;

				if (Blend->OrigRate > 0.0)
					NewRate = Blend->AnimRate / Blend->OrigRate;

				GetActor()->ReplicateAnim(0,Blend->AnimSequence, NewRate, Blend->AnimFrame, Blend->TweenRate, Blend->AnimLast, Blend->bAnimLoop);
			}
		}				
	}

	// Check deletion status.
	if( GetStatus() & MINST_DeleteMe )
	{
		//debugf(TEXT("Skeletal mesh instance deleting itself: [%s]"),this->GetName()); 
		delete this;		
	}
	else
	{
		SetStatus(0);
	}
	return true;
	unguard;
}



UBOOL USkeletalMesh::SetAttachAlias( FName TagName, FName BoneName, FCoords& AdjustCoords )
{
	guard(USkeletalMesh::SetAttachAlias);
	// Add to or replace in list of bone aliases.
	if( (TagName != NAME_None) && (BoneName != NAME_None ))
	{
		INT TagNum = TagAliases.Num();
		INT TagNewIdx = TagAliases.AddUniqueItem(TagName);
		if( TagNum == TagAliases.Num() ) // no change -> replace item.
		{
			// Replace.
			if( (TagCoords.Num() == TagAliases.Num()) && (TagNames.Num() == TagAliases.Num()) )
			{
				TagNames(TagNewIdx) = BoneName;
				TagCoords(TagNewIdx) = AdjustCoords;
			}
		}
		else
		{
			// Add.
			TagNames.AddItem(BoneName);		
			TagCoords.AddItem(AdjustCoords);
		}
		return 1;
	}
	return 0;
	unguard;
}

//
// Match up startbone by name. Also allows the attachment tag aliases.
// Pretty much the same as USkeletalMeshInstance::MatchRefBone
//
INT USkeletalMesh::MatchRefBone( FName StartBoneName)
{
	guardSlow(USkeletalMesh::MatchRefBone);
	INT Bone = -1;
	if ( StartBoneName != NAME_None) 
	{
		// Match possible 'attachtag' bone names first.
		for( INT t=0; t< TagAliases.Num(); t++)
		{
			if( StartBoneName == TagAliases(t) )
			{
				StartBoneName = TagNames(t);
				break;
			}
		}	
		// Search though regular bone names.
		for( INT p=0; p< RefSkeleton.Num(); p++)
		{	
			if( RefSkeleton(p).Name == StartBoneName )
			{
				Bone = p;
				break;
			}
		}	
	}
	return Bone;
	unguardSlow;
}

FSkelBoneSphere* USkeletalMesh::FindBoneCollisionSphere(FName BoneName)
{
	guard(USkeletalMesh::FindBoneCollisionSphere);

	for(INT i=0; i<BoneCollisionSpheres.Num(); i++)
	{
		if(BoneCollisionSpheres(i).BoneName == BoneName)
			return &BoneCollisionSpheres(i);
	}

	return NULL;

	unguard;
}


// Mesh implementations point to the MeshInstance's definition.

FBox USkeletalMesh::GetRenderBoundingBox( const AActor* Owner )
{
	guardSlow(USkeletalMesh::GetRenderBoundingBox );
	return ((USkeletalMeshInstance*)MeshGetInstance(Owner))->GetRenderBoundingBox( Owner );
	unguardSlow;
}

FSphere USkeletalMesh::GetRenderBoundingSphere( const AActor* Owner )
{
	guardSlow(USkeletalMesh::GetRenderBoundingSphere );
	return ((USkeletalMeshInstance*)MeshGetInstance(Owner))->GetRenderBoundingSphere( Owner );
	unguardSlow;
}

//
// Get the untransformed ( in mesh-space ) visibility bounding box for this primitive, as owned by Owner.
//
FBox USkeletalMeshInstance::GetRenderBoundingBox( const AActor* Owner )
{
	guardSlow(USkeletalMeshInstance::GetRenderBoundingBox);	

	// NOTE: Avoid confusion with the Instance's own boundingbox .
	//return ((USkeletalMesh*)GetMesh())->BoundingBox;
	return GetMesh()->BoundingBox;

	unguardSlow;
}

//
// Get the rendering bounding sphere for this primitive.
//
FSphere USkeletalMeshInstance::GetRenderBoundingSphere( const AActor* Owner )
{
	guardSlow(USkeletalMeshInstance::GetRenderBoundingSphere);
	return GetMesh()->BoundingSphere;
	unguardSlow;
}

void USkeletalMeshInstance::ForceGetFrame()
{
	guardSlow(USkeletalMeshInstance::ForceGetFrame);

	// Evaluate skeleton if not already present / updated #debug - use instance's last game GTicks
	if(! SpaceBases.Num() || (LastGTicks < GTicks) )
	{
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly); 
		}		
	}
	unguardSlow;
}

// Getbonerotation/location
FCoords USkeletalMeshInstance::GetBoneCoords( DWORD BoneIdx )
{
	guardSlow(USkeletalMeshInstance::GetBoneCoords);

	// Evaluate skeleton if not already present / updated #debug - use instance's last game GTicks
	if(! SpaceBases.Num() || (LastGTicks < GTicks) )
	{
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly); 
		}		
	}

	if( SpaceBases.Num() && BoneIdx < (DWORD)SpaceBases.Num() )
	{
#if 0 // JAG - seems to work
		return (SpaceBases(BoneIdx) * CachedMeshTrafo);
#else
		const FMatrix& MeshToWorldMatrix = MeshToWorld();	
		const FMatrix& JointMatrix		 = SpaceBases(BoneIdx).Matrix() * MeshToWorldMatrix;

		FVector Axis = FVector(1.0f,0.0f,0.0f);
		const FVector& XAxis = JointMatrix.TransformNormal( Axis ).SafeNormal();

		Axis = FVector(0.0f,1.0f,0.0f);
		const FVector& YAxis = JointMatrix.TransformNormal( Axis ).SafeNormal();				

		Axis = FVector(0.0f,0.0f,1.0f);
		const FVector& ZAxis = JointMatrix.TransformNormal( Axis ).SafeNormal();				

		const FVector& Origin = MeshToWorldMatrix.TransformFVector( SpaceBases(BoneIdx).Origin );

		return FCoords(Origin, XAxis, YAxis, ZAxis);
#endif
	}
	else
		return GMath.UnitCoords;
	unguardSlow;
}


// Rotation: separate since it needs special handling that's cumbersome in script.
FRotator USkeletalMeshInstance::GetBoneRotation( DWORD BoneIdx, INT Space )
{
	 guardSlow(USkeletalMeshInstance::GetBoneRotation);
	 // Evaluate skeleton if not already present / updated #debug - use instance's last game GTicks
	 if(! SpaceBases.Num() || (LastGTicks < GTicks ) )
	 {
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly); 
		}		
	 }
	 if( SpaceBases.Num() && BoneIdx < (DWORD)SpaceBases.Num() )
	 {
		// Construct global
		//FCoords Cached = SpaceBases(BoneIdx).Transpose();
		//FCoords LocalSpace = Cached * CachedMeshTrafo;
		//FRotator Orientation = LocalSpace.OrthoRotation();
		//return Orientation;
		return (SpaceBases(BoneIdx).Transpose() * CachedMeshTrafo).OrthoRotation();
	 }
	 else
	 {
		return FRotator(0,0,0);		
	 }
	 unguardSlow;
}

FRotator USkeletalMeshInstance::GetBoneRotation( FName BoneName, INT Space )
{
	 guardSlow(USkeletalMeshInstance::GetBoneRotation);
	 INT BoneIdx = MatchRefBone( BoneName);
	 if( BoneIdx < 0 )
	 {
		 debugf(TEXT("GetBoneRotation: Bone [%s] not found in skeleton."),*BoneName);
		 return FRotator(0,0,0);
	 }
	 return GetBoneRotation( BoneIdx, Space );
	 unguardSlow;
}


// General - get coords for tagname.
FCoords USkeletalMeshInstance::GetTagCoords( FName TagAlias )
{
	guardSlow(USkeletalMeshInstance::GetTagCoords);
	USkeletalMesh& Mesh = *(USkeletalMesh*)GetMesh();

	FName BoneName = NAME_None;

	for(INT i=0;i<Mesh.TagAliases.Num();i++)
	{
		if( Mesh.TagAliases(i) ==  TagAlias )
		{
			BoneName = Mesh.TagNames(i);
			break;
		}
	}	
	INT BoneIdx = MatchRefBone( BoneName );

	if( (BoneIdx > -1) && SpaceBases.Num() && BoneIdx < SpaceBases.Num() )
	{
		return (SpaceBases(BoneIdx) * CachedMeshTrafo);
	}
	else
		return GMath.UnitCoords;
	unguardSlow;
}


// Retrieve relative rotation/location since last lock directly from the root bone animation.
FVector USkeletalMeshInstance::GetRootLocation()
{
	guardSlow(USkeletalMeshInstance::GetRootLocation);
	// Update RootLocation directly from root track via calling GetFrame
	if( bRootTrafoStale )
	{		
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RootOnly); 
		}		
	}
	return RootLocation;
	unguardSlow;
}

FRotator USkeletalMeshInstance::GetRootRotation()
{
	guardSlow(USkeletalMeshInstance::GetRootRotation);
	// Update RootRotation directly from root track via calling GetFrame
	if( bRootTrafoStale )
	{		
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RootOnly); 
		}		
	}
	return RootRotation;
	unguardSlow;
}

// Retrieve location since last retrieval OR lock.
FVector  USkeletalMeshInstance::GetRootLocationDelta()
{	
	guard(USkeletalMeshInstance::GetRootLocationDelta);
	// Update RootLocation directly from root track via calling GetFrame
	if( bRootTrafoStale )
	{
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RootOnly); 
		}						
	}
	
	FVector OutLoc = RootLocation - LastRootLocation;
	LastRootLocation = RootLocation;

	if( bRelockRoot )
	{
		LockRootMotion(1);
	}

	return OutLoc;
	unguard;
}

// Retrieve roration since last retrieval or lock.
FRotator USkeletalMeshInstance::GetRootRotationDelta()
{
	guard(USkeletalMeshInstance::GetRootRotationDelta);
	// Update RootRotation directly from root track via calling GetFrame
	if( bRootTrafoStale )
	{
		AActor* Owner = GetActor();		
		if( Owner )
		{
			INT DummyVerts;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RootOnly); 
		}						
	}
	
	// Assume always called in tandem with GetRootLocationDelta()?
	// #TODO: both rotation and location need to be actor-centric.
	/*
	FRotator OutRot = LastRootRotation - RootRotation; //RootRotation = NewRoot.OrthoRotation() - LockedRootTrafo.OrthoRotation();
	LastRootRotation = RootRotation;

	OutRot.Yaw = 65535 - OutRot.Yaw;
	OutRot.Pitch = 0; 
	OutRot.Roll = 0;
	*/

	FRotator OutRot = FRotator(0,0,0);
	return OutRot;
	unguard;
}


// Output mesh vertices for conversion to other primitives..
void USkeletalMeshInstance::GetMeshVerts
( 	
	AActor*		Owner,
	FVector*	ResultVerts,
	INT			Size,
	INT&		LODRequest	
)
{
	guard(USkeletalMeshInstance::GetMeshVerts);
	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();		

	if( Mesh->LODModels.Num() )
	{
		if( Mesh->LODModels(0).SmoothStreamWedges > 0)
		{		
			TArray <FAnimMeshVertex> NewVerts;
			NewVerts.AddZeroed( Mesh->LODModels(0).SmoothStreamWedges );

			// GetFrame & ComputeSkinVerts will return a renderable vertex buffer from which we'll pick the first LODRequest number of 3d vertices (not the normals) to store into ResultVerts.
			INT DummyVerts = 0;
			GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RawVerts ); // Position the bones.

			LODRequest = Min( LODRequest, NewVerts.Num());
			CurrentLODLevel = 0; 
			ComputeSkinVerts( Mesh, this, &NewVerts(0), sizeof(FAnimMeshVertex), DummyVerts );		
			
			FVector* OutVert = ResultVerts;
			for( INT v=0; v<LODRequest; v++)
			{
				*OutVert = NewVerts(v).Position; // Copy vector.			
				*(BYTE**)&OutVert += Size;
			}			
		}
	}
	else
	{
		LODRequest = 0;
		debugf(TEXT("Error - no prepared LOD model data available to extract vertices from."));
	}
	unguard;
}



// Get Verts & Normals for the current pose (interleaved.) 
INT USkeletalMeshInstance::GetMeshVertsAndNormals( AActor* Owner, TArray<FVector>* Result )
{
	guard(USkeletalMeshInstance::GetMeshVertsAndNormals);
	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();				
	if( (!Result) || ( !Mesh) || ( CurrentLODLevel >= Mesh->LODModels.Num() )  )
	{
		return 0;	
	}

	FStaticLODModel* LODModel = &Mesh->LODModels( CurrentLODLevel);	
	INT DummyVerts = 0;
	GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RawVerts ); 

	TArray <FAnimMeshVertex> NewVerts;
	NewVerts.Add( LODModel->SmoothStreamWedges  );
	
	// Use whatever's the current LOD level. 
	ComputeSkinVerts( Mesh, this, &NewVerts(0), sizeof(FAnimMeshVertex), DummyVerts );	
	
	if(  Result->Num() )
		Result->Empty();
	Result->Add( LODModel->SmoothVerts.Num() * 2 );

	FVector*		OutPtr		= (FVector*) Result->GetData();
	DWORD*		InStream = (DWORD*) LODModel->SkinningStream.GetData();		
	DWORD		InDW = *InStream;
	INT              VertCount = 0;
	FAnimMeshVertex* NewVertsPtr =(FAnimMeshVertex*) NewVerts.GetData();

	while( (InDW < 0xFFFFFFFF) && ( VertCount < Result->Num())  ) // End marker, verify enough space in output.
	{				
		InStream += 3;
		if( InDW < (DWORD)0xF0000000 ) // Skip if  upper 4 bits full.
		{							
			*(OutPtr+0) = NewVertsPtr->Position;
			*(OutPtr+1) = NewVertsPtr->Normal;
			OutPtr+=2;		// Vertex and normal.
			VertCount+=2;			
			INT InfluenceCount = ( (InDW & 0x70000000) >> (32-4)) ; //  0-7: means 1-8 influences queued up in the DWORD stream.			
			InStream += InfluenceCount;  // add one extra for each influence above 1			
		}
		NewVertsPtr++;
		InDW = *InStream; // advance 'instruction' stream.		 		
	}
	return VertCount/2;
	
	unguard;
}

// Get Bones & Normals for the current pose (interleaved.) - skips root in output.
INT USkeletalMeshInstance::GetMeshJointsAndNormals( AActor* Owner, TArray<FVector>* Result, INT BoneDepth)
{
	guard(USkeletalMeshInstance::GetMeshJointsAndNormals);
	USkeletalMesh* Mesh = (USkeletalMesh*)GetMesh();

	if( Mesh->LODModels.Num() )
	{			
		// GetFrame will return the skeleton in its current pose.
		INT DummyVerts = 0;
		GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_RawVerts );

		INT TotalBones = Min( Mesh->RefSkeleton.Num() , SpaceBases.Num() );

		INT StartBone = 0;
		// Skip root bone but only if more available.
		if( TotalBones > 1 )
		{			
			TotalBones--;
			StartBone = 1;
		}		
		Result->Empty( TotalBones*2 );
		Result->Add( TotalBones*2 );
		for( INT b=0; b< TotalBones; b++)
		{			
			INT DestIdx = b*2;
			INT SourceIdx = b+StartBone;
			(*Result)(DestIdx + 0) = SpaceBases(SourceIdx).Origin;
			// get bone normal-'direction' vector by going down to child; otherwise, forced normal.
			INT ParentIndex = Mesh->RefSkeleton(SourceIdx).ParentIndex;
			if( ParentIndex != SourceIdx)
			{
				(*Result)(DestIdx + 1) = SpaceBases(ParentIndex).Origin - SpaceBases(SourceIdx).Origin;
				(*Result)(DestIdx + 1).Normalize();
			}
			else
			{
				(*Result)(DestIdx + 1) = FVector( 1.0f,0.0f,0.0f );
			}
			
		}
		return( TotalBones );
	}
	else
	{
		Result->Empty();
		debugf(TEXT("Error - no prepared LOD model data available to extract joint positions from."));
		return 0;
	}

	unguard;
}


UBOOL USkeletalMeshInstance::LockRootMotion( INT Lock )
{
	guard(USkeletalMeshInstance::LockRootMotion);
	
	bRootTrafoStale = 1;
	RootMotionMode = Lock;
	PrevFrame = 0.0f;

	// Lock current setup of turning raw motion into world motion, and keep with it.	
	USkeletalMesh& Mesh = *(USkeletalMesh*)GetMesh();
	AActor* Owner = GetActor();
	if( !Owner )
		return 0;

	FScale SkeletalScale	= FScale( Mesh.Scale * Owner->DrawScale * Owner->DrawScale3D, 0.0f, SHEER_None);		
	if(bIgnoreMeshOffset)
		LockedLocalTrafo = GMath.UnitCoords * ( Owner->Location ) * Owner->Rotation * SkeletalScale;
	else
		LockedLocalTrafo = GMath.UnitCoords * ( Owner->Location + Owner->PrePivot) * Owner->Rotation * Mesh.RotOrigin * SkeletalScale;

	// Get and lock current location.
	if( 1 )
	{
		GetRootLocation();
		LastRootLocation = RootLocation; // current actor pos is 'relative 0' again.
		LockedRootTrafo = NewRootRaw;    // raw root bone FCoords
		PrevRootTrafo = NewRootTrafo;	 // Cooked root trafo		
		LastRootRotation = RootRotation; // rotation update
	}
	// Lock cinematic rootmotion.
	if( 2 )
	{
		// LOCKEDlocal trafo: now also includes the move-actor-back-to-root.
		// Cinematics mode: needs subtracting the relative-to-actor root location too, since it might be locked at any time in a cinematic
		GetRootLocation(); // location of root, using GetFrame.		
		LastRootLocation = GetActor()->Location;		
		LockedRootTrafo = NewRootRaw;    // raw root bone FCoords  - MESH space.
		PrevRootTrafo   = NewRootTrafo;	 // Cooked root trafo - same as newrootraw but in world space..		
	}

	bRelockRoot = 0;
	return 1;
	unguard;
}

UBOOL USkeletalMeshInstance::EnableChannelNotify( INT Channel, INT Switch )
{
	guard(USkeletalMeshInstance::EnableChannelNotify);
	if( !ValidateAnimChannel(Channel) ) 
	{
		return false;
	}
	
	Blends( Channel ).bNotifyIgnore = !Switch;

	return true;
	unguard;
}


//
// Get properly interpolated rotation/position pair for a track and time.
//
void GetTrackRotPos(const MotionChunk* ThisMove, INT Bone, FLOAT Time, FQuat& OutRot, FVector& OutPos , USkeletalMeshInstance* Inst, INT bRootTrafoOnly )
{
	guardSlow(USkeletalMeshInstance::GetTrackRotPos);

	AnalogTrack& Track = (AnalogTrack&) ThisMove->AnimTracks( Bone );

	INT KeyA,KeyB,Wrap=0,WrapTime=0;

	// Find the right time into our animation.
	// Time should be no larger than Track.TrackTime.
	// If the time overruns the last timed key, wrap if necessary.
	//
	// Optimizations:
	// - Dec 2001: added 'binary' search.
	// - Other optimizations: caching the key or even the to/from quats or quat increment ?
	//

	INT TotalKeys = Track.KeyTime.Num();
	INT LastKey   = Track.KeyTime.Num()-1;

	// Single key case:
	if( TotalKeys <= 1)
	{
		KeyA = 0;
	}
	else if( TotalKeys > 30 )  // Somewhat arbitrary breakeven point.
	{
		// Important cycle-saver: binary search for key position.
		// First check for overrun.
		if( SmallerPositiveFloat( Time, Track.KeyTime(1) ) )//( Time < Track.KeyTime(1) ) // Time usually == 0.f or so
		{
			KeyA = 0;
		}
		else
		{
			// Looking for a KeyA so that KeyTime(KeyA)<= Time < KeyTime(KeyA+1)
			
			// First look whether it's the end key, otherwise start in the middle.
			INT Estimate = LastKey;
			
			if( SmallerEqPositiveFloat( Track.KeyTime(Estimate), Time) )//if( Track.KeyTime(Estimate) <= Time )
			{
				KeyA=Estimate;
			}
			else
			{
				// Start at approximate middle.
				Estimate = LastKey >> 1;
				INT SearchStep = LastKey >> 1;
				INT SearchIter = 0;
				UBOOL FoundKey = false;

				do 
				{
					SearchIter++;
					if( SearchStep > 1)
					{
						SearchStep = (SearchStep >> 1);
					}
					
					// Looking for a KeyA so that KeyTime(KeyA)<= Time < KeyTime(KeyA+1)
					if ( Estimate >= LastKey) 
					{
						Estimate = LastKey;
						FoundKey= true;
					}
					else
					if( SmallerPositiveFloat(Time,Track.KeyTime(Estimate) ) ) //( Time < Track.KeyTime(Estimate) )
					{
						Estimate -= SearchStep;
						//debugf(TEXT("Down: SearchStep:%i  Estimate: %i Totalkeys: %i "),SearchStep,Estimate,TotalKeys);
						if( Estimate < 0)
						{
							Estimate = 0;
						}
					}
					else
					if( SmallerEqPositiveFloat( Track.KeyTime(Estimate+1),Time) )//( Time >= Track.KeyTime(Estimate+1) )
					{
						Estimate +=SearchStep;
						//debugf(TEXT("Up: SearchStep:%i  Estimate: %i Totalkeys: %i "),SearchStep,Estimate,TotalKeys);
						if( Estimate > LastKey )
						{
							Estimate = LastKey;
						}
					}
					else
					{
						FoundKey = true;
					}
				}
				while( (!FoundKey) && (SearchIter < 32) );

				KeyA = Estimate;							
			}
		}
	}
	else
	{
		// Linear searches.
		KeyA = -1;
		for( INT t=1;t< TotalKeys; t++)
		{			
			if( SmallerPositiveFloat(Time,Track.KeyTime(t)) )//( Time < Track.KeyTime(t) )
			{
				KeyA = t-1;
				break;
			}
		}
		// KeyA wraps ?
		if( KeyA == -1 )
		{
			// if( Time < ThisMove->TrackTime ) //- implied..
			KeyA = TotalKeys - 1;
		}		
	}
	
	// KeyB
	KeyB = KeyA+1;
	if( KeyB >= TotalKeys )
	{
		KeyB = 0;
		Wrap = 1;
	}			

	if( bRootTrafoOnly)
	{
		if (Inst->PrevFrame > Time)
		{
			WrapTime = 1;
		}
		Inst->PrevFrame = Time;			
	}

	// Construct new Alpha if necessary.
	FLOAT IntervalSize;
	FLOAT KeyAlpha = 0.0f;
	if( KeyA != KeyB)
	{
		IntervalSize = Wrap ? ThisMove->TrackTime - Track.KeyTime(KeyA) : Abs(Track.KeyTime(KeyB)-Track.KeyTime(KeyA));
		KeyAlpha = (IntervalSize > 0.0001f) ? (Time - Track.KeyTime(KeyA)) / IntervalSize : 1.0f;
	}


	// Schedule re-locking to help append root translations over loop.
	if ( bRootTrafoOnly && WrapTime ) 
	{
		Inst->bRelockRoot = 1; 
	}

	// Early-out: for majority of cases.
	if( (KeyA == KeyB) || (KeyAlpha == 0.0f) )
	{
		OutRot = Track.KeyQuat(KeyA);
		if(Track.KeyPos.Num() <= 1) // Default case - single position.
			OutPos = Track.KeyPos(0);
		else
			OutPos = Track.KeyPos(KeyA);

		return;
	}
		
	FQuat* pQuat1 = &Track.KeyQuat(KeyA);
	FQuat* pQuat2 = &Track.KeyQuat(KeyB);	

	if( bRootTrafoOnly )
	{
		if( Wrap )
		{
			pQuat2 = &(Track.KeyQuat(Track.KeyQuat.Num()-1));
		}
		else
		if( WrapTime ) // BOTH keys must've wrapped.
		{
			pQuat1 = &(Track.KeyQuat(Track.KeyQuat.Num()-1));
			pQuat2 = &(Track.KeyQuat(Track.KeyQuat.Num()-1));			
		}
	}

	// Slerp the orientations.
	FastSlerpNormQuat( pQuat1,pQuat2,KeyAlpha,&OutRot );
	
	// Lerp the positions - default case: single position.
	if ( Track.KeyPos.Num() <= 1  )
	{
		OutPos = Track.KeyPos(0);						
		return;
	}
	else
	{
		FVector ThisPos1 = Track.KeyPos(KeyA);
		FVector ThisPos2 = Track.KeyPos(KeyB);

		if( bRootTrafoOnly )
		{
			if ( Wrap )
			{
				ThisPos2 +=  Track.KeyPos(Track.KeyPos.Num()-1) - Track.KeyPos(0) ;   // Don't jump back; keep the start-end position delta.
			}
			else
			if (WrapTime) // BOTH keys must've wrapped..
			{
				ThisPos1 += Track.KeyPos(Track.KeyPos.Num()-1) - Track.KeyPos(0); // Don't jump back..
				ThisPos2 += Track.KeyPos(Track.KeyPos.Num()-1) - Track.KeyPos(0); // Don't jump back..
			}
		}
		OutPos = ThisPos1 + (ThisPos2 - ThisPos1)*KeyAlpha;
	}
	
	unguardSlow;
}



// Set up a single-channel pose - to get one particular mesh-space rotation (as opposed to parent-bone space) 
// for more useful blends at the start-blend-bone.

void SetupChannelPose( USkeletalMesh& Mesh, FCoords& PivotJoint, INT PivotIndex, TArray<FQuat>& ChannelSkelRot, TArray<FVector>& ChannelSkelPos, const MotionChunk* ThisMove, MeshAnimLinkup* ThisLinkup, FLOAT Time,  USkeletalMeshInstance* MInst )
{
	//debugf(TEXT("Evaluating channel-specific pose for mesh: [%s]"),Mesh.GetName() ); 

	INT TotalBones = ThisLinkup->CachedLinks.Num();

	ChannelSkelRot.Empty(TotalBones);
	ChannelSkelRot.Add(  TotalBones);
	ChannelSkelPos.Empty(TotalBones);
	ChannelSkelPos.Add(  TotalBones);

	TArray<FCoords> SkelCoords;
	SkelCoords.Empty(TotalBones);
	SkelCoords.Add(TotalBones);

	// Get all keys
	for( INT b = 0; b<TotalBones; b++ )
	{		
		// CachedLinks contains actual anim bone num for the bone-index - OR:
		if( ThisLinkup->CachedLinks(b) < 0 )
		{
			// Use mesh's reference pose.
			ChannelSkelPos(b) = Mesh.RefSkeleton(b).BonePos.Position;
			ChannelSkelRot(b) = Mesh.RefSkeleton(b).BonePos.Orientation;
		}
		else
		{
			GetTrackRotPos( ThisMove, ThisLinkup->CachedLinks(b), Time, ChannelSkelRot(b), ChannelSkelPos(b), MInst, 0);	
		}
	}

	//
	// Build hierarchy (#skel up to startbone only necessary ) - and put resulting global rotation in StartBone.
	//
	// Digest bones into the spacebases.
	for( INT bone=0; bone < Mesh.RefSkeleton.Num(); bone ++ )
	{			
		SkelCoords(bone) =  FQuaternionCoords( ChannelSkelRot(bone) );
		SkelCoords(bone).Origin = ChannelSkelPos(bone);
	}
	for(INT s=1; s<TotalBones; s++ )
	{					
		SkelCoords(s) = SkelCoords(s).ApplyPivot(SkelCoords(Mesh.RefSkeleton(s).ParentIndex));
	}		

	// Final global 'pivot' = rotation to use when grafting the animation onto the skeleton.
	PivotJoint = SkelCoords(PivotIndex);	
}




//
// SSE equivalent of FPU based skinning.
//
/*

#if !__PSX2_EE__ && !__GCN__ && !__LINUX__
void SSESkeletalSkinning( USkeletalMesh* Mesh, TArray<FCoords>& SpaceBases,INT VertexNum, FVector* ResultVerts,INT Size, DWORD TaskFlag )
{	
	guard(SSESkeletalSkinning);

	DWORD Cycles=0;clock(Cycles);

	// SpaceBases are left intact for fast recalculation of these; the raw spacebases themselves no longer have a 'hardware bone' function. 
	TArray<FMatrix> FinalMatrices;
	FinalMatrices.Add(SpaceBases.Num()+1); // One extra to allow for alignment
	FMatrix* FinalMatrixStart = (FMatrix*)( (DWORD) &(FinalMatrices(1)) & (DWORD) 0xFFFFFFF0); // Alignment hack - 16-byte aligned...

	// Prepare transforms to deform skin vertices directly from reference pose into end pose. - make sure it works for SSE...
	for( INT s=0; s<SpaceBases.Num(); s++ ) 
	{
		// Perform the transform recombine that effectively accomplishes: NewCopy = SpaceBases(s).ApplyPivot(Mesh->RefBases(s).PivotInverse());
		FCoords& SpaceBaseRef   = SpaceBases(s);
		FCoords& SpaceCopy      = Mesh->RefBasesInverse(s);   
		FMatrix& FinalCopy      = FinalMatrixStart[s];
				
		FVector XPlane = SpaceCopy.XAxis * SpaceBaseRef.XAxis.X + SpaceCopy.YAxis * SpaceBaseRef.XAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.XAxis.Z;
		FVector YPlane = SpaceCopy.XAxis * SpaceBaseRef.YAxis.X + SpaceCopy.YAxis * SpaceBaseRef.YAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.YAxis.Z;
		FVector ZPlane = SpaceCopy.XAxis * SpaceBaseRef.ZAxis.X + SpaceCopy.YAxis * SpaceBaseRef.ZAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.ZAxis.Z;		
		// Copy as transpose => Todo: do this implied by altering the math above.
		FinalCopy.M[0][0]=XPlane.X;
		FinalCopy.M[1][0]=XPlane.Y;
		FinalCopy.M[2][0]=XPlane.Z;
		FinalCopy.M[0][1]=YPlane.X;
		FinalCopy.M[1][1]=YPlane.Y;
		FinalCopy.M[2][1]=YPlane.Z;
		FinalCopy.M[0][2]=ZPlane.X;
		FinalCopy.M[1][2]=ZPlane.Y;
		FinalCopy.M[2][2]=ZPlane.Z;
		
		FinalCopy.M[3][0] = SpaceBaseRef.Origin.X;
		FinalCopy.M[3][1] = SpaceBaseRef.Origin.Y;
		FinalCopy.M[3][2] = SpaceBaseRef.Origin.Z;
		FinalCopy.M[3][3] = 0.f;
		FinalCopy.M[3][0] += ( SpaceBaseRef.XAxis | SpaceCopy.Origin );
		FinalCopy.M[3][1] += ( SpaceBaseRef.YAxis | SpaceCopy.Origin );
		FinalCopy.M[3][2] += ( SpaceBaseRef.ZAxis | SpaceCopy.Origin );

		// "PivotTransform" - works as:
		//  Coords.Origin + FVector( *this | Coords.XAxis, *this | Coords.YAxis, *this | Coords.ZAxis );
	}
	


	// Final multi-bone vertex deformation. Ordering: vertices and normals interleaved.
	TArray <FPlane> OutVerts;
	OutVerts.Add(VertexNum*2+1);
	FPlane* OutVertsAligned = (FPlane*)( (DWORD) &(OutVerts(2)) & (DWORD) 0xFFFFFFF0); // Alignment hack - 16-byte aligned...
	//
	// Skinning.
	//
	for( INT m=0; m< Mesh->MultiBlends.Num(); m++ )
	{
		if( m==0 ) // All single-influence vertices.
		{
			_WORD* PointsIdx = &(Mesh->MultiBlends(m).PointIndices(0));
			INT    PointNum  = Mesh->MultiBlends(m).PointIndices.Num();
			VBoneInfluence* WeightStruc = &Mesh->Weights(Mesh->MultiBlends(m).WeightBase);

			for( INT v=0; v<PointNum; v++) // All points with a single influence.
			{
				INT VIndex = *PointsIdx++;
				if( VIndex < VertexNum )
				{					
					// INT BoneIdx        =  Mesh->Weights(WeightIdx).BoneIndex;					
					// OutVerts  (VIndex) =  Mesh->Points (VIndex).PivotTransform(FinalTrafos(BoneIdx));					
					// OutNormals(VIndex) =  Mesh->Normals(VIndex).PivotTransform(FinalTrafos(BoneIdx)) - OutVerts(VIndex);
					// WeightIdx++;

					FMatrix* MatSSE = &( FinalMatrixStart[ WeightStruc->BoneIndex ] );										
					__declspec(align(16)) FVector VectSSEIn = Mesh->Points (VIndex);  //#DEBUG - convert to nonaligned SSE loads ?
					__declspec(align(16)) FVector NormSSEIn = Mesh->Normals(VIndex);
			
					// Note: don't us EBX, EBP.
					__asm
					{//|regs|Vert|Norm											
							movaps xmm0,VectSSEIn 	  //
								movaps xmm4,NormSSEIn //						
						mov ecx, MatSSE     // Matrix address
							movaps xmm1,xmm0		  //						
								movaps xmm5,xmm4	  //
							movaps xmm2,xmm0		  //													
								movaps xmm6,xmm4   //						 

							shufps xmm0,xmm2,0x00  // expand all 0th members to xmm0
							shufps xmm1,xmm2,0x55  // expand all 1st members to xmm1						
							shufps xmm2,xmm2,0xAA  // expand all 2nd members to xmm2																		

								shufps xmm4,xmm6,0x00  //
							mulps  xmm0,[ecx]    // xmm4 //  result in xmm0: FinalMatrix.XPlane X,Y,Z times input  XXX 										    
								shufps xmm5,xmm6,0x55  // 							
							mulps  xmm1,[ecx+16] // xmm5 //  result in xmm1: FinalMatrix.YPlane X,Y,Z times input  YYY
								shufps xmm6,xmm6,0xAA  //												
							mulps  xmm2,[ecx+32] // xmm6 //  result in xmm20: FinalMatrix.ZPlane X,Y,Z times input ZZZ												

								mulps  xmm4,[ecx] //xmm4 //	
								mulps  xmm5,[ecx+16] //xmm5 //
								mulps  xmm6,[ecx+32] //xmm6 //
											
							addps xmm0,xmm1 //  																			
						mov esi,OutVertsAligned  // Output address
								addps  xmm4,xmm5 											
							addps xmm0,xmm2 //
						mov eax, VIndex				
							addps xmm0,[ecx+48]// xmm7 // First float holds: X* Xplane.X+ Y * Yplane.X + Z * ZPlane.X => ik wil: Xaxis.X*X + Xaxis.Y*Y + XAxis.Z*Z etc							
								addps  xmm4,xmm6 //													
						shl eax,5 // *32 - 4 dwords per vector/ 4dwords per normal.
								addps  xmm4,[ecx+48] 					
							movaps xmm7,xmm0 // copy to subtract from normal later								
								subps  xmm4, xmm7    // minus VectSSEOut -> normal was calculated with a displaced vertex.																								
							movaps [esi+eax],xmm0																									
								movaps [esi+eax+16],xmm4
					}
					WeightStruc++;
				}
				else break; // Early out
			}
		}
		//
		// Todo - special-case for 2 influences ?
		//
		else if (m>0) // Multiple weights. For speed, inner loop can be written out for specific cases (m=1,2,3) - or just keep the vert/norm in xmm registers.
		{
			_WORD* PointsIdx = &(Mesh->MultiBlends(m).PointIndices(0));
			INT PointNum  = Mesh->MultiBlends(m).PointIndices.Num();
			VBoneInfluence* WeightStruc = &Mesh->Weights(Mesh->MultiBlends(m).WeightBase);

			for( INT v=0; v<PointNum; v++)
			{					
				INT VIndex = *PointsIdx++;
				if( VIndex < VertexNum ) // This particular VIndex repeated m times..
				{
					__declspec(align(16)) FVector OutVert = FVector(0.f,0.f,0.f);
					__declspec(align(16)) FVector OutNorm = FVector(0.f,0.f,0.f);
					__declspec(align(16)) FVector VectSSEIn = Mesh->Points (VIndex);  //#DEBUG - convert to nonaligned SSE loads ?
					__declspec(align(16)) FVector NormSSEIn = Mesh->Normals(VIndex);		
					
					for( INT n=0; n<=m; n++) // 2,3 or more influences.
					{
						//INT BoneIdx  = Mesh->Weights(WeightIdx).BoneIndex;
						//FLOAT Weight = (FLOAT) Mesh->Weights(WeightIdx).BoneWeight*( 1.0f/65535.f );
						//OutVert  += Weight * Mesh->Points(VIndex).PivotTransform(FinalTrafos(BoneIdx));
						//OutNorm  += Weight * Mesh->Normals(VIndex).PivotTransform(FinalTrafos(BoneIdx));
						//WeightIdx++;					

						FLOAT  Weight = (1.0f/65535.f)*(FLOAT) WeightStruc->BoneWeight;
						FMatrix* MatSSE = &( FinalMatrixStart[ WeightStruc->BoneIndex ] );					
						// Note: don't us EBX, EBP.
						__asm
						{//|regs|Vert|Norm											

								movaps xmm0,VectSSEIn 	  //
									movaps xmm4,NormSSEIn //						
							mov ecx, MatSSE     // Matrix address
								movaps xmm1,xmm0		  //						
									movaps xmm5,xmm4	  //
								movaps xmm2,xmm0		  //													
									movaps xmm6,xmm4   //						 

								shufps xmm0,xmm2,0x00  // expand all 0th members to xmm0
								shufps xmm1,xmm2,0x55  // expand all 1st members to xmm1						
								shufps xmm2,xmm2,0xAA  // expand all 2nd members to xmm2																		

									shufps xmm4,xmm6,0x00  //
								mulps  xmm0,[ecx]    // xmm4 //  result in xmm0: FinalMatrix.XPlane X,Y,Z times input  XXX 										    
									shufps xmm5,xmm6,0x55  // 							
								mulps  xmm1,[ecx+16] // xmm5 //  result in xmm1: FinalMatrix.YPlane X,Y,Z times input  YYY
									shufps xmm6,xmm6,0xAA  //												
								mulps  xmm2,[ecx+32] // xmm6 //  result in xmm20: FinalMatrix.ZPlane X,Y,Z times input ZZZ												

									mulps  xmm4,[ecx] //xmm4 //	
							movss  xmm3,Weight  // Single float
									mulps  xmm5,[ecx+16] //xmm5 //
							shufps xmm3,xmm3,0x00    // Weight - expand 0th member 4 times into xmm4												
									mulps  xmm6,[ecx+32] //xmm6 //
												
								addps xmm0,xmm1 //  																			
									addps  xmm4,xmm5 											
								addps xmm0,xmm2 //
								addps xmm0,[ecx+48]// xmm7 // First float holds: X* Xplane.X+ Y * Yplane.X + Z * ZPlane.X => ik wil: Xaxis.X*X + Xaxis.Y*Y + XAxis.Z*Z etc							
									
									addps  xmm4,xmm6 //						
								mulps xmm0,xmm3    // weight
									addps  xmm4,[ecx+48] 					
								movaps xmm7,xmm0 // copy to subtract from normal later													
									mulps  xmm4, xmm3    // mul by weights..

									subps  xmm4, xmm7    // minus VectSSEOut -> normal was calculated with a displaced vertex.																		
								addps xmm0, OutVert
									addps  xmm4,OutNorm
								movaps OutVert,xmm0
									movaps OutNorm,xmm4									
						}
						WeightStruc++; 
					} // repeat m+1 times.
					OutVertsAligned[VIndex*2  ] = OutVert;
					OutVertsAligned[VIndex*2+1] = OutNorm;					
				}//valid LOD.
				else break; // Early out
			}// all with M+1 influences.
		}//all with M>0.
	}

	unclock(Cycles);
	GStats.DWORDStats( GEngineStats.STATS_Mesh_SkinCycles ) += Cycles;

	//
	// Update the vertex buffer with the results.
	//
	if( TaskFlag == GF_RawVerts )  
	{
		// Called from UnStaticMesh - only 3d FVectors requested. Oblivious to sections and vertex buffers.
		for( INT v=0; v<OutVerts.Num(); v++)
		{				
			*ResultVerts = OutVertsAligned[v*2];			
			*(BYTE**)&ResultVerts += Size;
		}
	}
	else		
	{	// Duplicate vertices and normals into their vertex stream locations, per material section.

		// Full-LOD special case:
		if( VertexNum == Mesh->Points.Num() )
		{
			for( INT s=0; s<Mesh->SmoothSections.Num();s++)
			{				
				FVector* StreamPtr = &(Mesh->SmoothSections(s).VertexStream.Vertices(0).Position);
				INT WedgeBase = Mesh->SmoothSections(s).MinIndex;			
				INT VertCount = Mesh->SmoothSections(s).VertexStream.Vertices.Num();								
				FMeshWedge*  IVertPtr = &(Mesh->Wedges(WedgeBase));
				for( INT v=0; v<VertCount; v++)
				{															
					INT Index = IVertPtr->iVertex;
					StreamPtr[0] = OutVertsAligned[Index*2+0];
					StreamPtr[1] = OutVertsAligned[Index*2+1];
					IVertPtr++;
					*(BYTE**)&StreamPtr += sizeof( FAnimMeshVertex );							
				}				
			}
		}
		else
		{
		// LOD-aware copy.
			for( INT s=0; s<Mesh->SmoothSections.Num();s++)
			{				
				FVector* StreamPtr = &(Mesh->SmoothSections(s).VertexStream.Vertices(0).Position);
				INT WedgeBase = Mesh->SmoothSections(s).MinIndex;				
				INT VertCount = Mesh->SmoothSections(s).VertexStream.Vertices.Num();
				FMeshWedge*  IVertPtr = &(Mesh->Wedges(WedgeBase));				
				for( INT v=0; v<VertCount; v++)
				{		
					INT Index = IVertPtr->iVertex;
					if( Index < VertexNum ) //LOD check
					{							
						StreamPtr[0] = OutVertsAligned[Index*2+0];
						StreamPtr[1] = OutVertsAligned[Index*2+1];						
						IVertPtr++;
						*(BYTE**)&StreamPtr += sizeof( FAnimMeshVertex );
					}
					else break;
				}				
			}
		}
	}	

	unguard;
}
#endif

*/

/*-----------------------------------------------------------------------------
	USkeletalMesh animation interface.
-----------------------------------------------------------------------------*/

struct Twitch
{
	FQuat   Rot;
	FVector Pos;
	INT     Bone;
	INT     Channel;
	DWORD   Flags;
};

struct Bend
{
	FCoords Coords;	
	INT     Bone;
	INT     Channel;
	DWORD   Flags;
	FLOAT   Alpha;
};

enum TwitchFlags
{
	TWITCH_Normal = 0,
	TWITCH_Global = 1,
};
enum BendFlags
{
	PH_Blend = 0,
	PH_Tween = 1,
};

// Enumeration for finding name.
enum BBoneWants
{	
	BBONE_RefPose    = 0,		// Bone wants to be initialized from the reference skeletal pose.
	BBONE_Normal     = 1,		// Default - properly assigned with rotation/translation.
	BBONE_CachedPose = 2,		// Bone wants to be initialized from cached _previous_ pose
};


// Return vertex stream size of currently active LOD model.
INT USkeletalMeshInstance::ActiveVertStreamSize()
{
	guardSlow(USkeletalMeshInstance::ActiveVertStreamSize);
	return( ((USkeletalMesh*)GetMesh())->LODModels(CurrentLODLevel).SmoothStreamWedges );
	unguardSlow;
}

//Callback from GetStreamData to transform and copy skin vertices; after GetFrame has been called.
void USkeletalMeshInstance::MeshSkinVertsCallback( void* Dest )
{	
	guardSlow( MeshSkinVertsCallback );

	INT Dummy = 0;
	ComputeSkinVerts( (USkeletalMesh*)GetMesh(), this, Dest, 0, Dummy);

	unguardSlow;
}


// Transform Normal and vertex simultaneously with the same matrix.
FORCEINLINE void TransformSkinnedVertex( FCoords& Bone, FSkinPoint* Vertex, FVector& VertexOut, FVector& NormalOut )
{
	// Allows the compiler to combine the matrix row loads.		
	FVector TempNorm;
	TempNorm.X = ((INT)Vertex->Normal.X - 511);
	TempNorm.Y = ((INT)Vertex->Normal.Y - 511);				
	TempNorm.Z = ((INT)Vertex->Normal.Z - 511);
	VertexOut.X = (Vertex->Point | Bone.XAxis) + Bone.Origin.X;			
	NormalOut.X = (TempNorm | Bone.XAxis);		
	VertexOut.Y = (Vertex->Point | Bone.YAxis) + Bone.Origin.Y;	
	NormalOut.Y = (TempNorm | Bone.YAxis);	
	VertexOut.Z = (Vertex->Point | Bone.ZAxis) + Bone.Origin.Z;
	NormalOut.Z = (TempNorm | Bone.ZAxis);	
}

FORCEINLINE FVector TransformPackedNormal( FMeshNorm& Norm, FCoords& Coords )
{	
	FVector NewNorm( (INT)Norm.X -511, (INT)Norm.Y-511, (INT)Norm.Z-511 );  // ANY other conversion is slower...
	return FVector(	NewNorm | Coords.XAxis, NewNorm | Coords.YAxis, NewNorm | Coords.ZAxis );		
}

// Bones: assumed premultiplied with 6 ; *8 = 48 = FCoords size.
#define BONEMATRIX(b) ( *(FCoords*) ( *(BYTE**)&BoneBase + ((b)*8) )  ) 
// Weight: when converting from integers, problems with unnecessary Quadword loading by compiler ??!
#define CREATEWEIGHT(w)  ( ((FLOAT)(1.f/(65535.f * 4096.0f))) * ((INT)(w) & (INT)0x0FFFFFFF) )  // Ignore the lower bits; does the >>12 shift in the fp factor.

//
// Local function to compute the smooth skinned mesh vertex positions when rendering, using bones set by GetFrame.
//
// Get a single, complete LOD level (MInst->CurrentLODIdx) to the vertex buffer.
//
void ComputeSkinVerts( USkeletalMesh* Mesh, USkeletalMeshInstance* MInst, void* Destination,INT Size, INT& LODRequest )
{
	guardSlow(ComputeSkinVerts);
	DWORD Cycles=0; clock(Cycles);
		
	//
	// MInst->SpaceBases contain the current state of the skeleton. 
	// FinalTrafos are needed so that we can use vertices directly from the reference 
	// pose (as opposed from the local-bone-space, an approach we don't use.)
	//	

	TArray<FLOAT> Duplicates; // Temp buffer for all 3d points to be copied into multiple vertices.
	TArray<FCoords> FinalTrafos;	
	
	FStaticLODModel* LODModel = &Mesh->LODModels( MInst->CurrentLODLevel );	 // Draw this particular level... 
	FinalTrafos.Add( MInst->SpaceBases.Num() );   // Special trafo's allowing skin to be transformed directly from the reference pose.
	Duplicates.Add( LODModel->DupVertCount * 2 * 3 ); // Duplicates-buffer - for vertices that need to be expanded into multiple 'wedges' (vertices with different UV's)

	// Prepare transforms to deform skin vertices directly from reference pose.
	for( INT s=0; s<MInst->SpaceBases.Num(); s++ ) 
	{
		// debugf( TEXT(" Mesh: %s  Bone # %3i [%19s]  Parent # %3i "),Mesh->GetName(),s,*(Mesh->RefSkeleton(s).Name),Mesh->RefSkeleton(s).ParentIndex);

		// Perform the transform recombine that effectively accomplishes: NewCopy = MInst->SpaceBases(s).ApplyPivot(Mesh->RefBases(s).PivotInverse());
		FCoords& SpaceBaseRef   = MInst->SpaceBases(s);
		FCoords& SpaceCopy      = Mesh->RefBasesInverse(s);   
		FCoords& FinalCopy      = FinalTrafos(s);
		FinalCopy.Origin = SpaceBaseRef.Origin;
		FinalCopy.Origin.X += ( SpaceBaseRef.XAxis | SpaceCopy.Origin );
		FinalCopy.Origin.Y += ( SpaceBaseRef.YAxis | SpaceCopy.Origin );
		FinalCopy.Origin.Z += ( SpaceBaseRef.ZAxis | SpaceCopy.Origin );
		FinalCopy.XAxis = SpaceCopy.XAxis * SpaceBaseRef.XAxis.X + SpaceCopy.YAxis * SpaceBaseRef.XAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.XAxis.Z;
		FinalCopy.YAxis = SpaceCopy.XAxis * SpaceBaseRef.YAxis.X + SpaceCopy.YAxis * SpaceBaseRef.YAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.YAxis.Z;
		FinalCopy.ZAxis = SpaceCopy.XAxis * SpaceBaseRef.ZAxis.X + SpaceCopy.YAxis * SpaceBaseRef.ZAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.ZAxis.Z;
	}

	//
	// Efficient transformation of vertices and packed normals.
	//
	//  Future optimizations:
	//  - Taking UV's out of stream into separate, -static- vertex stream ?
	//
	
	FVector*	StreamPtr		= (FVector*)Destination;		
	FCoords*	BoneBase		= &FinalTrafos(0);
	FSkinPoint* SkinVertPtr		= &(LODModel->SmoothVerts(0));
	DWORD*		InStream		= &(LODModel->SkinningStream(0)); 	
	FVector*    DupDestination	= (FVector*)&Duplicates(0);
	DWORD		InDW = *InStream;	

	while( InDW < 0xFFFFFFFF ) // End marker...
	{ 			
		
		if(!( InDW & (DWORD)0xF0000000)) // No marker- default transform.
		{	
			FCoords& Bone = BONEMATRIX(InDW & 0x0FFF);
			//StreamPtr[0] = SkinVertPtr->Point.PivotTransform( Bone ); // Does the compiler catch this though ???
			//StreamPtr[1] = TransformPackedNormal( SkinVertPtr->Normal, Bone );			
			FVector TempNorm;			
			TempNorm.X = ((INT)SkinVertPtr->Normal.X - 511);			
			StreamPtr[0].X  = (SkinVertPtr->Point | Bone.XAxis) + Bone.Origin.X;																		
			TempNorm.Y = ((INT)SkinVertPtr->Normal.Y - 511);
			StreamPtr[0].Y  = (SkinVertPtr->Point | Bone.YAxis) + Bone.Origin.Y;															
			TempNorm.Z = ((INT)SkinVertPtr->Normal.Z - 511);
			StreamPtr[0].Z  = (SkinVertPtr->Point | Bone.ZAxis) + Bone.Origin.Z;												
			StreamPtr[1].X  = (TempNorm | Bone.XAxis);
			StreamPtr[1].Y  = (TempNorm | Bone.YAxis);
			StreamPtr[1].Z  = (TempNorm | Bone.ZAxis);			
			SkinVertPtr++; // Advance vertex.
			
			((FLOAT*)StreamPtr)[6] = ((FLOAT*)InStream)[1]; // U texture coordinate.
			((FLOAT*)StreamPtr)[7] = ((FLOAT*)InStream)[2]; // V texture coordinate.
			*(BYTE**)&StreamPtr += sizeof(FAnimMeshVertex); // Advance Output stream pointer.
			InStream += 3; // Advance input stream pointer.				
		}
		else 		
		if( InDW >= (DWORD)0xF0000000 ) // Copy from duplicates buffer.
		{				
			// Copy back from vertex indicated by BoneFlags.		
			INT DupIndex = InDW & 0x0FFFFFFF;
			StreamPtr[0] = *(FVector*)&(Duplicates( DupIndex   ));
			StreamPtr[1] = *(FVector*)&(Duplicates( DupIndex+3 ));
			((FLOAT*)StreamPtr)[6] = ((FLOAT*)InStream)[1];  // U texture coordinate.
			((FLOAT*)StreamPtr)[7] = ((FLOAT*)InStream)[2];  // V texture coordinate.						
			*(BYTE**)&StreamPtr += sizeof(FAnimMeshVertex);  // Advance output stream pointer.
			InStream += 3;   // Advance input stream pointer.			
		}
		else
		if(!(InDW & (DWORD)0xE0000000)) // 2 influences special case.
		{
			DWORD InDW2 = ((DWORD*)InStream)[1];
			FLOAT Weight1  = CREATEWEIGHT(InDW);
			FLOAT Weight2  = CREATEWEIGHT(InDW2);			
			FCoords& Bone1 = BONEMATRIX(InDW & 0x0FFF);
			FCoords& Bone2 = BONEMATRIX(InDW2 & 0x0FFF);
			//FVector NewVertex = Weight1 * SkinVertPtr->Point.PivotTransform( Bone1 );
			//FVector NewNormal = Weight1 * TransformPackedNormal( SkinVertPtr->Normal, Bone1 );						
			//StreamPtr[0] = NewVertex + Weight2 * SkinVertPtr->Point.PivotTransform( Bone2 ); 
			//StreamPtr[1] = NewNormal + Weight2 * TransformPackedNormal( SkinVertPtr->Normal, Bone2 ); 			 

			FVector TempNorm;
			TempNorm.X = ((INT)SkinVertPtr->Normal.X - 511);
			TempNorm.Y = ((INT)SkinVertPtr->Normal.Y - 511);
			TempNorm.Z = ((INT)SkinVertPtr->Normal.Z - 511);
			FVector NewVertex;
			FVector NewNormal;			
			NewVertex.X  = Weight1*((Bone1.XAxis | SkinVertPtr->Point) + Bone1.Origin.X);
			NewVertex.Y  = Weight1*((Bone1.YAxis | SkinVertPtr->Point) + Bone1.Origin.Y);
			NewVertex.Z  = Weight1*((Bone1.ZAxis | SkinVertPtr->Point) + Bone1.Origin.Z);														
			NewNormal.X  = Weight1*(TempNorm | Bone1.XAxis);
			NewNormal.Y  = Weight1*(TempNorm | Bone1.YAxis);
			NewNormal.Z  = Weight1*(TempNorm | Bone1.ZAxis);											
			StreamPtr[0].X = NewVertex.X + Weight2*((SkinVertPtr->Point | Bone2.XAxis) + Bone2.Origin.X);
			StreamPtr[0].Y = NewVertex.Y + Weight2*((SkinVertPtr->Point | Bone2.YAxis) + Bone2.Origin.Y);
			StreamPtr[0].Z = NewVertex.Z + Weight2*((SkinVertPtr->Point | Bone2.ZAxis) + Bone2.Origin.Z);
			StreamPtr[1].X = NewNormal.X + Weight2*(TempNorm | Bone2.XAxis);
			StreamPtr[1].Y = NewNormal.Y + Weight2*(TempNorm | Bone2.YAxis);
			StreamPtr[1].Z = NewNormal.Z + Weight2*(TempNorm | Bone2.ZAxis);			
			SkinVertPtr++; // Advance vertex.
									
			((FLOAT*)StreamPtr)[6] = ((FLOAT*)InStream)[2]; // U texture coordinate.
			((FLOAT*)StreamPtr)[7] = ((FLOAT*)InStream)[3]; // V texture coordinate.						
			*(BYTE**)&StreamPtr += sizeof(FAnimMeshVertex); // Advance Output stream pointer.
			InStream += 4;	
		}
		else // Single-influence+storage case.
		if(( InDW & (DWORD)0xF0000000) == 0x80000000 ) // default transform.
		{	
			FCoords& Bone = BONEMATRIX(InDW & 0x0FFF);
			//StreamPtr[0] = DupDestination[0] = SkinVertPtr->Point.PivotTransform(Bone); // Does the compiler catch this though ???
			//StreamPtr[1] = DupDestination[1] = TransformPackedNormal( SkinVertPtr->Normal, Bone); 									
			FVector TempNorm;						
			TempNorm.X = ((INT)SkinVertPtr->Normal.X - 511);
			TempNorm.Y = ((INT)SkinVertPtr->Normal.Y - 511);				
			TempNorm.Z = ((INT)SkinVertPtr->Normal.Z - 511);
			StreamPtr[0].X = DupDestination[0].X = (SkinVertPtr->Point | Bone.XAxis) + Bone.Origin.X;						
			StreamPtr[0].Y = DupDestination[0].Y = (SkinVertPtr->Point | Bone.YAxis) + Bone.Origin.Y;				
			StreamPtr[0].Z = DupDestination[0].Z = (SkinVertPtr->Point | Bone.ZAxis) + Bone.Origin.Z;
			StreamPtr[1].X = DupDestination[1].X = (TempNorm | Bone.XAxis);		
			StreamPtr[1].Y = DupDestination[1].Y = (TempNorm | Bone.YAxis);	
			StreamPtr[1].Z = DupDestination[1].Z = (TempNorm | Bone.ZAxis);	
			DupDestination+=2;			
			SkinVertPtr++; // Advance vertex.								

			((FLOAT*)StreamPtr)[6] = ((FLOAT*)InStream)[1]; // U texture coordinate.
			((FLOAT*)StreamPtr)[7] = ((FLOAT*)InStream)[2]; // V texture coordinate.									
			*(BYTE**)&StreamPtr += sizeof(FAnimMeshVertex);   // Advance Output stream pointer.
			InStream += 3; // Advance input stream pointer.				
		}
		else // 2-influences-and-storage case.
		if( (InDW & (DWORD)0xF0000000) == 0x90000000 ) // 2 influences special case.
		{
			DWORD InDW2 = ((DWORD*)InStream)[1];
			FCoords& Bone1 = BONEMATRIX(InDW  & 0x0FFF);
			FCoords& Bone2 = BONEMATRIX(InDW2 & 0x0FFF);
			FLOAT Weight1  = CREATEWEIGHT(InDW);
			FLOAT Weight2  = CREATEWEIGHT(InDW2);
			//FVector NewVertex = Weight1 * SkinVertPtr->Point.PivotTransform( Bone1 );
			//FVector NewNormal = Weight1 * TransformPackedNormal( SkinVertPtr->Normal, Bone1 );
			//StreamPtr[0] = DupDestination[0] = NewVertex + Weight2 * SkinVertPtr->Point.PivotTransform( Bone2 ); 
			//StreamPtr[1] = DupDestination[1] = NewNormal + Weight2 * TransformPackedNormal( SkinVertPtr->Normal, Bone2 ); 

			FVector TempNorm;			
			TempNorm.X = ((INT)SkinVertPtr->Normal.X - 511);						
			TempNorm.Y = ((INT)SkinVertPtr->Normal.Y - 511);				
			TempNorm.Z = ((INT)SkinVertPtr->Normal.Z - 511);
			FVector NewVertex;
			FVector NewNormal;			
			NewVertex.X  = Weight1*((SkinVertPtr->Point | Bone1.XAxis) + Bone1.Origin.X);												
			NewVertex.Y  = Weight1*((SkinVertPtr->Point | Bone1.YAxis) + Bone1.Origin.Y);												
			NewVertex.Z  = Weight1*((SkinVertPtr->Point | Bone1.ZAxis) + Bone1.Origin.Z);															
			NewNormal.Z  = Weight1*(TempNorm | Bone1.ZAxis);	
			NewNormal.X  = Weight1*(TempNorm | Bone1.XAxis);
			NewNormal.Y  = Weight1*(TempNorm | Bone1.YAxis);				
			StreamPtr[0].X = DupDestination[0].X = NewVertex.X + Weight2*((SkinVertPtr->Point | Bone2.XAxis) + Bone2.Origin.X);
			StreamPtr[0].Y = DupDestination[0].Y = NewVertex.Y + Weight2*((SkinVertPtr->Point | Bone2.YAxis) + Bone2.Origin.Y);
			StreamPtr[0].Z = DupDestination[0].Z = NewVertex.Z + Weight2*((SkinVertPtr->Point | Bone2.ZAxis) + Bone2.Origin.Z);
			StreamPtr[1].X = DupDestination[1].X = NewNormal.X + Weight2*(TempNorm | Bone2.XAxis);
			StreamPtr[1].Y = DupDestination[1].Y = NewNormal.Y + Weight2*(TempNorm | Bone2.YAxis);
			StreamPtr[1].Z = DupDestination[1].Z = NewNormal.Z + Weight2*(TempNorm | Bone2.ZAxis);
			DupDestination+=2;
			SkinVertPtr++; // Advance vertex.									

			((FLOAT*)StreamPtr)[6] = ((FLOAT*)InStream)[2]; // U texture coordinate.
			((FLOAT*)StreamPtr)[7] = ((FLOAT*)InStream)[3]; // V texture coordinate.						
			*(BYTE**)&StreamPtr += sizeof(FAnimMeshVertex); // Advance Output stream pointer.
			InStream += 4;	
		}
		else 
		//if( (InDW & (DWORD)0x70000000) > 0x10000000 ) // 3-8 influences queued up in the DWORD stream - with OR without storage.
		{	
			INT InfluenceCount = ( (InDW & 0x70000000) >> (32-4)) + 1;				
			FCoords& Bone = BONEMATRIX(InDW & 0x0FFF);
			FLOAT Weight  = CREATEWEIGHT(InDW);
			//FVector NewVertex = Weight * SkinVertPtr->Point.PivotTransform( Bone );
			//FVector NewNormal = Weight * TransformPackedNormal( SkinVertPtr->Normal, Bone );
			FVector TempNorm;			
			TempNorm.X = ((INT)SkinVertPtr->Normal.X - 511);						
			TempNorm.Y = ((INT)SkinVertPtr->Normal.Y - 511);				
			TempNorm.Z = ((INT)SkinVertPtr->Normal.Z - 511);			
			FVector NewVertex;
			FVector NewNormal;
			NewNormal.X  = Weight*(TempNorm | Bone.XAxis);
			NewVertex.X  = Weight*((SkinVertPtr->Point | Bone.XAxis) + Bone.Origin.X);												
			NewNormal.Y  = Weight*(TempNorm | Bone.YAxis);	
			NewVertex.Y  = Weight*((SkinVertPtr->Point | Bone.YAxis) + Bone.Origin.Y);									
			NewNormal.Z  = Weight*(TempNorm | Bone.ZAxis);	
			NewVertex.Z  = Weight*((SkinVertPtr->Point | Bone.ZAxis) + Bone.Origin.Z);												

			
			for( INT i=1; i<InfluenceCount; i++)
			{
				DWORD InDW2 = ((DWORD*)InStream)[i];
				FCoords& Bone2 = BONEMATRIX(InDW2 & 0x0FFF);
				FLOAT Weight2  = CREATEWEIGHT(InDW2);
				//NewVertex += Weight2 * SkinVertPtr->Point.PivotTransform( Bone2 );
				//NewNormal += Weight2 * TransformPackedNormal( SkinVertPtr->Normal, Bone2 );				
				NewVertex.X  += Weight2*((SkinVertPtr->Point | Bone2.XAxis) + Bone2.Origin.X);																
				NewVertex.Y  += Weight2*((SkinVertPtr->Point | Bone2.YAxis) + Bone2.Origin.Y);													
				NewVertex.Z  += Weight2*((SkinVertPtr->Point | Bone2.ZAxis) + Bone2.Origin.Z);												
				NewNormal.X  += Weight2*(TempNorm | Bone2.XAxis);
				NewNormal.Y  += Weight2*(TempNorm | Bone2.YAxis);	
				NewNormal.Z  += Weight2*(TempNorm | Bone2.ZAxis);	
			}
			
			SkinVertPtr++; // Advance vertex.
									
			// Forward duplication requested?
			if( InDW & 0x80000000 )
			{
				StreamPtr[0] = DupDestination[0] = NewVertex;
				StreamPtr[1] = DupDestination[1] = NewNormal;
				DupDestination+=2;
			}
			else
			{
				StreamPtr[0] = NewVertex;
				StreamPtr[1] = NewNormal;
			}

			((FLOAT*)StreamPtr)[6]  = ((FLOAT*)InStream)[InfluenceCount+0]; // U texture coordinate.
			((FLOAT*)StreamPtr)[7]  = ((FLOAT*)InStream)[InfluenceCount+1]; // V texture coordinate.							
			*(BYTE**)&StreamPtr += sizeof(FAnimMeshVertex); // Advance Output stream pointer.
			InStream += InfluenceCount + 1 + 1;			
		}
		InDW = *InStream;
	}	

	unclock(Cycles);
	GStats.DWORDStats( GEngineStats.STATS_Mesh_SkinCycles ) += Cycles;

	//
	// Expensive but debugging-only render mode: read back normals, vertices 
	// and number-of-influences from this LOD's raw data.
	// Fresh every time (LOD level may even have changed)
	//
	if( MInst->bDisplayNormals )
	{			
		// Collect number-of-influences only if vertices needed updating.
		if( MInst->DebugInfluences.Num() != LODModel->SmoothVerts.Num() )
		{
			// Needed for explicit influence tracking..
			LODModel->Influences.Load(); 
			LODModel->Wedges.Load(); 

			MInst->DebugInfluences.Empty();
			MInst->DebugInfluences.Add( LODModel->SmoothVerts.Num() );

			// Gather influences...
			TArray<INT> TempInfs;
			TArray<INT> TempBoneIdxs;
			TempInfs.AddZeroed( LODModel->SmoothVerts.Num() );
			TempBoneIdxs.AddZeroed( LODModel->SmoothVerts.Num() );
			
            for( INT i=0; i<LODModel->Influences.Num(); i++ )
			{				
				INT VIdx = LODModel->Influences(i).VertIndex;
				TempInfs(VIdx)++;
				if( TempBoneIdxs(VIdx) == 0)
				{
					// First encountered bone index is the main index.
					TempBoneIdxs(VIdx) = LODModel->Influences(i).BoneIndex + 1;
				}				
			}
			// Copy to DebugInfluences.
			for( INT i=0; i< MInst->DebugInfluences.Num(); i++ )
			{								
					MInst->DebugInfluences(i).InfluenceCount = TempInfs(i);
					MInst->DebugInfluences(i).MainBoneIndex = TempBoneIdxs(i)-1;
			}
		}
		
		// Copy vertices and normals from the (AGP memory) vertex buffer  - slow, but for debugging purposes only.
		DWORD*		InStream = &LODModel->SkinningStream(0);		
		DWORD		InDW = *InStream;
		INT				FreshVertIndex = 0;
		INT				OutIdx = 0;
		while( ( InDW < 0xFFFFFFFF ) && ( OutIdx < LODModel->SmoothVerts.Num() ) ) // End marker...
		{			
			InStream += 3;
			// if( (InDW & (DWORD)0x70000000) >= 0x00000000 ) // 0-7: means 1-8 influences queued up in the DWORD stream 
			if( InDW <  (DWORD)0xF0000000 ) // Skip if ful upper 4 bits .
			{	
				INT InfluenceCount = ( (InDW & 0x70000000) >> (32-4)) ;								
				MInst->DebugInfluences(OutIdx).Vertex = ((FAnimMeshVertex*)Destination)[FreshVertIndex].Position;
				MInst->DebugInfluences(OutIdx).Normal = ((FAnimMeshVertex*)Destination)[FreshVertIndex].Normal;
				InStream += InfluenceCount;  // Add one extra for each influence.
				OutIdx++;   // Vertex and normal.				
			}							
			InDW = *InStream;  // Advance 'instruction' stream.		 
			FreshVertIndex++; // Advance vertex stream index.
		}				
	}

	unguardSlow;
};

// Stat drawing
void DrawStat(UCanvas* Canvas,FColor Color,INT X,const TCHAR* Format,...)
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
// Full pivot inverse (unlike special one used in skin setup)
//
FORCEINLINE FCoords UndoPivot(const FCoords& Base, const FCoords& CoordsB )
{	
	FCoords BaseInv = Base.Transpose();
	FCoords Result;
	Result.XAxis = BaseInv.XAxis.TransformVectorByTranspose(CoordsB);
	Result.YAxis = BaseInv.YAxis.TransformVectorByTranspose(CoordsB);
	Result.ZAxis = BaseInv.ZAxis.TransformVectorByTranspose(CoordsB);		
	Result.Origin = ( Base.Origin - CoordsB.Origin ).TransformVectorByTranspose(CoordsB); // Checked: correct!	
	return Result;
}

//
// GetFrame updates the skeletal bones.
//
void USkeletalMeshInstance::GetFrame
(
	AActor*		Owner,
	FLevelSceneNode* SceneNode,
	FVector*	ResultVerts,
	INT			Size,
	INT&		LODRequest,
	DWORD       TaskFlag
)   
{
	guardSlow(USkeletalMeshInstance::GetFrame);	
	DWORD Cycles=0;clock(Cycles);

	// Ensure that always at least the base channel is allocated.
	ValidateAnimChannel(0);	
  
	// Allow owner-driven animations.		
	AActor*	AnimOwner = NULL;
	if ((Owner->bAnimByOwner) && (Owner->Owner != NULL))
		AnimOwner = Owner->Owner;
	else
		AnimOwner = Owner;

	USkeletalMesh& Mesh = *(USkeletalMesh*)GetMesh();
	USkeletalMesh& OwnerMesh = ( AnimOwner->Mesh && AnimOwner->Mesh->IsA(USkeletalMesh::StaticClass()) ) ? *(USkeletalMesh*)(AnimOwner->Mesh) : Mesh;
	USkeletalMeshInstance* OwnerInstance = (USkeletalMeshInstance*) OwnerMesh.MeshGetInstance( AnimOwner );	
	UBOOL bOwnerDrivenAnim = (AnimOwner != Owner); // If true, we're animating from the Owner. 
	
	// GetFrame satisfied as long as Owner of a bAnimByOwner-mesh is up-to-date.
	FLOAT CurrentAnimFrame = OwnerInstance->Blends.Num() ? OwnerInstance->Blends(0).AnimFrame : -1.0f;
	UBOOL SamePose = ( OwnerInstance->LastGTicks == GTicks && OwnerInstance->LastAnimFrame == CurrentAnimFrame );

	// If owner driven, ensure owner's pose determination for this tick is performed first.
	if( bOwnerDrivenAnim && !SamePose )
	{
		GetFrame( AnimOwner, SceneNode,  ResultVerts, Size, LODRequest, TaskFlag );		
	}

	// Evaluate bones or root motion ?	
	UBOOL bRootTrafoOnly = ( TaskFlag == GF_RootOnly );
	// Root-only doesn't do a full update of skeletal status - update status indicators only for full compute.
	if( !bRootTrafoOnly ) 
	{
		LastAnimFrame = CurrentAnimFrame;
		LastGTicks = GTicks;
	}

	if( !CachedOrientations.Num() )
	{
		SamePose = false;
		CachedOrientations.Empty();
		CachedPositions.Empty();
		CachedOrientations.Add(Mesh.RefSkeleton.Num());				
		CachedPositions.Add(Mesh.RefSkeleton.Num());		
		// If tween-starting base channel, initialize with Reference skeleton for immediate tweening capability ?
		/*
		if( Blends.Num() && (Blends(0).AnimFrame < 0.0f) )
		{
			for( INT b=0; b< Mesh.RefSkeleton.Num(); b++)
			{
				CachedOrientations(b) = Mesh.RefSkeleton(b).BonePos.Orientation;
				CachedPositions(b) = Mesh.RefSkeleton(b).BonePos.Position;
			}
			CacheActive = true; 
		}
		*/
		for( INT Channel = 0; Channel < Blends.Num(); Channel++) 
		{
			Blends(Channel).CachedFrame = 0.0f;
		}
	}	
	
	// Check whether the right amount of spacebases have been allocated yet.
	if( SpaceBases.Num() != Mesh.RefSkeleton.Num() )
	{
		SamePose = false;
		SpaceBases.Empty();
		SpaceBases.Add( Mesh.RefSkeleton.Num() );
	}
		
	// Get stuff.	
	FScale   SkeletalScale = FScale(Mesh.Scale * Owner->DrawScale * Owner->DrawScale3D, 0.0f, SHEER_None);
	
	// Mesh->World space trafo, for bone-position retrieval functions.
	if(bIgnoreMeshOffset)
	{
		CachedMeshTrafo = GMath.UnitCoords * (Owner->Location) * Owner->Rotation * SkeletalScale;
	}
	else
	{
		CachedMeshTrafo = GMath.UnitCoords * (Owner->Location + Owner->PrePivot) * Owner->Rotation * Mesh.RotOrigin * SkeletalScale;
		CachedMeshTrafo.Origin += Mesh.Origin;
	}

	// Check whether Mesh.RefBases & Mesh.RefBasesInverse have been allocated and precomputed yet.
	if( Mesh.RefBasesInverse.Num() == 0 )
	{
		SamePose = false;
		TArray<FCoords> RefBases;
		RefBases.Add( Mesh.RefSkeleton.Num() );
		Mesh.RefBasesInverse.Add( Mesh.RefSkeleton.Num() );

		// Precompute the Mesh.RefBasesInverse.
		for( INT b=0; b<Mesh.RefSkeleton.Num(); b++)
		{
			// Render the default pose.
			FQuatToFCoordsFast( Mesh.RefSkeleton(b).BonePos.Orientation, Mesh.RefSkeleton(b).BonePos.Position, RefBases(b));
			// Construct mesh-space skeletal hierarchy.
			if( b>0 )
			{
				INT Parent = Mesh.RefSkeleton(b).ParentIndex;
				RefBases(b) = RefBases(b).ApplyPivot(RefBases(Parent));
			}
			// Precompute inverse so we can use from-refpose-skin vertices.
			Mesh.RefBasesInverse(b) = RefBases(b).PivotInverse(); 
		}		
	}

	//
	// Special-case anim by owner - reuses owner's SpaceBases when valid.
	//
	if( bOwnerDrivenAnim && OwnerInstance && OwnerInstance->SpaceBases.Num()  )
	{		
		if( ! &OwnerMesh ) //#SKEL
			appErrorf(TEXT("No OWNERMESH for mesh%s! "),Mesh.GetName() );

		if( (AnimLinkups.Num() != 1) || (USkeletalMesh*)AnimLinkups(0).Mesh != &OwnerMesh  )
		{
			// Empty AnimLinkups and associated bone matchings.
			for(INT a=0; a<AnimLinkups.Num(); a++)
			{
				AnimLinkups(a).CachedLinks.Empty();
			}
			AnimLinkups.Empty();
			AnimLinkups.AddZeroed();
			AnimLinkups(0).Mesh = &OwnerMesh;
			AnimLinkups(0).Anim = NULL;
			// Actualize linkups of this mesh ONLY to the mesh of the owner.			
			LinkMeshBonesToMesh( Mesh, OwnerMesh, &AnimLinkups(0) );
		}

		// Copy bone positions from owner or refpose, depending on name match.
		// if( AnimLinkups(0).CachedLinks.Num() >= Mesh.RefSkeleton.Num()) )

		if( AnimLinkups.Num() )
		{
			for( INT b=0; b< Mesh.RefSkeleton.Num(); b++)
			{
				INT bone = -1;			

				if( AnimLinkups(0).CachedLinks.Num() > b)
					bone = AnimLinkups(0).CachedLinks(b);

				if( bone >= 0 && ( OwnerInstance->SpaceBases.Num() > bone ) )
				{				
					SpaceBases(b) = OwnerInstance->SpaceBases(bone);		
				}
				else
				{
					// If no bone matches, revert to reference pose.
					SpaceBases(b) = GMath.UnitCoords;				
				}
			}

			// Early-out, we've used the owner's bone pose.
			return;
		}				
	}

	// Refresh linkups when necessary. 
	ActualizeAnimLinkups();

	if( !SamePose || GIsEditor )
	{			
		// If there is no dynamically assigned animation, the one associated with the mesh with #exec MESH DEFAULTANIM is activated.
		CurrentSkelAnim(0); //#SKEL

		// Orientation/position setters for arbitrary bones on the entire skeleton.		
		TArray<Twitch>	   TrafoHeap;
		TArray<Bend>       PivotHeap;
		TArray<BYTE>	   BoneFlags;
		BoneFlags.AddZeroed( Mesh.RefSkeleton.Num());	

		// Temporary single-channel skeleton for special 'global-mesh-space' blending.
		INT CheckChannelSkelSetup = -1;
		TArray <FQuat>   ChannelSkelRot;
		TArray <FVector> ChannelSkelPos;
		
		//
		// Blended- and main channels.
		//
		if( Blends.Num() && !bRootTrafoOnly )
		{
			// Partial-hierarchy marker assistance.
			TArray<INT> BoneGraph;
			BoneGraph.Add(Mesh.RefSkeleton.Num());

			// Arbitrary number of channels 
			for( INT Channel = 0; Channel < Blends.Num(); Channel++) 
			{				
				MeshAnimChannel& Blend = Blends( Channel );	

				FCoords PivotJoint = GMath.UnitCoords; // Possible global joint rotation for partial blending.
				// Channel 0 is the non-blending base layer.
				FLOAT Alpha = (Channel==0) ? 1.0f : Blend.BlendAlpha ;

				INT CachedLinksForChannel = ( (Blend.MeshAnimIndex > -1) && AnimLinkups.Num() ) ? AnimLinkups(Blend.MeshAnimIndex).CachedLinks.Num() : 0;

				if( CachedLinksForChannel && ( Alpha > 0.0f )  ) // Channel considered inactive if no animation object linked in or alpha == 0)
				{
					MeshAnimLinkup* BlendLinkup = &AnimLinkups( Blend.MeshAnimIndex ); //Any active animation will have valid bonename-based linkups.
					UMeshAnimation* SkelAnim = AnimLinkups( Blend.MeshAnimIndex ).Anim;

					UBOOL blendTweening = ( (Blend.AnimFrame < 0.0f) && CacheActive ); // Tween ?
					MotionChunk* BlendMove = SkelAnim ? SkelAnim->GetMovement( Blend.AnimSequence ): NULL;
					FMeshAnimSeq* BlendSeq = SkelAnim ? SkelAnim->GetAnimSeq( Blend.AnimSequence ): NULL;

					//debugf(TEXT(" Playing [%s]  animframe [%f] for mesh [%s] ticks %i chann %i"),*(Blend.AnimSequence),Blend.AnimFrame,Mesh.GetName(),GTicks,Channel);
					FLOAT BlendFrameTime = 0.0f;
					if( BlendSeq )
					{						
						BlendFrameTime = Min(1.f,Max( Blend.AnimFrame, 0.0f)) * BlendMove->TrackTime; 						
					}

					// See if there is anything to blend.
					if( Blend.MeshAnimIndex == -1 || (BlendMove == NULL && !blendTweening ) ) 
					{					
						// Nothing to animate.
					}
					else if ( !blendTweening ) // We have animation keys to slerp between, no tweening needed.
					{
						//debugf(TEXT(" Slerp stats: Mesh.RefSkeletonnum %i Animtracksnum %i Seqname %s"),Mesh.RefSkeleton.Num(),ThisMove->AnimTracks.Num(),*Seq->Name); 
						if( BlendMove && BlendMove->AnimTracks.Num() )
						{ 
							// We have some compressed animation keys.
							for( INT b=0; b<Mesh.RefSkeleton.Num(); b++ ) 
							{	
								// These need to be applied from a startbone up, only - even if some bones aren't present in the animation.
								if( Blend.StartBone == b )
									BoneGraph(b) = 1;
								else
									BoneGraph(b) = ( (b != 0) && (BoneGraph( Mesh.RefSkeleton(b).ParentIndex ) == 1) )? 1 : 0;

								if ( BoneGraph(b) )
								{																					
									if( BlendLinkup->CachedLinks(b) < 0 ) 
									{
										// Ignore setup for all unmatched bones.
									}
									else
									{
										// Get lerped rot/pos for this move, bone, time.
										if( !Blend.bGlobalPose )
										{
											// Default key retrieval.
											INT Idx = TrafoHeap.Add();											
											TrafoHeap(Idx).Flags = TWITCH_Normal;
											GetTrackRotPos( BlendMove, BlendLinkup->CachedLinks(b), BlendFrameTime, TrafoHeap(Idx).Rot, TrafoHeap(Idx).Pos, this, bRootTrafoOnly );
											TrafoHeap(Idx).Channel = Channel;			
											TrafoHeap(Idx).Bone = b;			
										}
										else
										{
											// Global (mesh-space) orientation override if it's the start-of-partial-hierarchy bone only.											
											// Set up the pose using only this channel, to retrieve a 'global' rotation for the pivot point.
											if( CheckChannelSkelSetup !=Channel )
											{														
												SetupChannelPose( Mesh, PivotJoint, Blend.StartBone, ChannelSkelRot, ChannelSkelPos, BlendMove, BlendLinkup, BlendFrameTime, this );
												CheckChannelSkelSetup = Channel;
											}

											// Non-rootbone partial blending.
											if( ( b>0 ) && ( Blend.StartBone == b) ) 
											{												
												// Store startbone's global (mesh-space) orientation.
												INT Idx = PivotHeap.Add();
												PivotHeap(Idx).Coords = PivotJoint;
												PivotHeap(Idx).Channel = Channel;
												PivotHeap(Idx).Bone = b;
												PivotHeap(Idx).Flags = PH_Blend;												
												PivotHeap(Idx).Alpha = Alpha;
												//debugf(TEXT("Global orientation blend - alpha: %f "),Alpha); 
											} 
											else 
											{
												// Use key from the already set-up pose for this one.
												INT Idx = TrafoHeap.Add();												
												TrafoHeap(Idx).Flags = TWITCH_Normal;
												TrafoHeap(Idx).Rot = ChannelSkelRot(b);
												TrafoHeap(Idx).Pos = ChannelSkelPos(b);
												TrafoHeap(Idx).Channel = Channel;			
												TrafoHeap(Idx).Bone = b;			
											}											
										}										
									}
								}
							}
						}
					}
					else // Tweening.
					{
						if (BlendSeq == NULL)
						{
							// No sequence - update bone markers to indicate use of previous bone state.
							// Render cached pose.
							if( CacheActive )
							{
								for( INT b=0; b<Mesh.RefSkeleton.Num(); b++)
								{		
									BoneFlags(b) = BBONE_CachedPose;
								}	
							}
						}
						else
						{
							// Calculate  overall tweening alpha.
							FLOAT Alpha = 1.0f - ( Blend.AnimFrame / Blend.CachedFrame );
							FLOAT StartFrame = BlendSeq ? (-1.0f / BlendSeq->NumFrames) : 0.0f;

							// Detect start of a tween...
							if( Blend.CachedSeq!=Blend.AnimSequence || Alpha<0.0f || Alpha>1.0f)
							{
								Blend.CachedFrame = StartFrame; 
								Alpha = 0.0f;
								Blend.CachedSeq = Blend.AnimSequence;
								Blend.TweenProgress = 0.f;
							}
							else
							{
								Blend.CachedFrame = Blend.AnimFrame; 
								Blend.TweenProgress += (1.f-Blend.TweenProgress) * Alpha;
							}

							// Sequence available, tween into it.
							for( INT b=0; b<Mesh.RefSkeleton.Num(); b++)
							{
								// These need to be applied from the startbone up, only.
								if( Blend.StartBone == b )
									BoneGraph(b) = 1;
								else
									BoneGraph(b) = ( (b != 0) && (BoneGraph( Mesh.RefSkeleton(b).ParentIndex ) == 1) )? 1 : 0;
								if ( BoneGraph(b) )
								{					

									FQuat   ThisQuat, PrevQuat;
									FVector ThisPos,  PrevPos;

									// Does this bone link up at all - if not, assumes cached position.
									if( BlendLinkup->CachedLinks(b) < 0 )
									{
										// Ignore trafo for all unknown bone names.
									}
									else
									{
										if( !Blend.bGlobalPose || (Blend.StartBone != b) || (b==0) ) // Regular tween
										{
											// We'll just tween to the start of a track.
											const AnalogTrack& Track = BlendMove->AnimTracks( BlendLinkup->CachedLinks(b) );
											ThisQuat = Track.KeyQuat(0);
											ThisPos  = Track.KeyPos(0);
											PrevPos  = CachedPositions(b);
											PrevQuat = CachedOrientations(b);

										        FQuat NewQuat;
											FastSlerpNormQuat( &PrevQuat, &ThisQuat, Alpha, &NewQuat );  

											FVector NewPos = PrevPos + (ThisPos - PrevPos)*Alpha;
											
											INT Idx = TrafoHeap.Add();						
											//debugf(TEXT("Get trafo to tweenblend ch. %i, bone: %i  cached link: %s (%i) TrafoHeapIdx %i  "),Channel, b , *(Mesh.RefSkeleton(b).Name), Blend.CachedLinks(b),Idx ); 
											TrafoHeap(Idx).Rot = NewQuat;
											TrafoHeap(Idx).Pos = NewPos;
											TrafoHeap(Idx).Channel = Channel;			
											TrafoHeap(Idx).Bone = b;
										}
										else // Special global-pivot usage.
										{	
											// if( ( b>0 ) && ( Blend.StartBone == b) && Blend.bGlobalPose )
											// Non-rootbone partial blend tween required.	
											// Note: all global-pivot code only handles rotation, not translation.
											
											// Setup the pose using only this channel, to retrieve a 'global' rotation for the pivot point.
											if( CheckChannelSkelSetup !=Channel )
											{														
												SetupChannelPose( Mesh, PivotJoint, Blend.StartBone, ChannelSkelRot, ChannelSkelPos, BlendMove, BlendLinkup, 0, this );
												CheckChannelSkelSetup = Channel;
											}

											// Store startbone's global (mesh-space) orientation.
											INT Idx = PivotHeap.Add();
											PivotHeap(Idx).Coords = PivotJoint;
											PivotHeap(Idx).Channel = Channel;
											PivotHeap(Idx).Bone = b;	
											PivotHeap(Idx).Flags = PH_Tween;										
											PivotHeap(Idx).Alpha = Blend.TweenProgress; //Alpha;
											//debugf(TEXT("Global orientation TWEEN - alpha: %f  BAF  %f Tp %f "),Alpha, Blend.AnimFrame, Blend.TweenProgress ); 
										}																				
									}												
								}							
							}							
						}			
					}
				}
				// If no animation at all for any bone: will snap into base pose by default.				
			}
			BoneGraph.Empty();
		}

		//
		//  Pull everything together; including blending and retrieving the non-updated
		//  default trafos from the cached state.
		//  The TrafoHeap entries _are_ order-sensitive.
		//
		Twitch *TrafoStart, *Trafo;
		TrafoStart = &TrafoHeap(0);
		for( INT a=0; a< TrafoHeap.Num(); a++)
		{	
			Trafo = TrafoStart++;

			INT bone = Trafo->Bone;
			INT channel = ( Trafo->Channel ); 
			if( channel == 0 ) // Simply copy if it's the base channel.
			{
				CachedOrientations(bone) = Trafo->Rot;
				CachedPositions(bone) = Trafo->Pos;
				BoneFlags(bone) = BBONE_Normal; 
			}
			else
			{
				// Blending - or replacing. Any tween action has already taken place between the cached trafos and the animations.				
				FLOAT KeyAlpha = Blends(channel).BlendAlpha;
				// Lead-in alpha ramping?
				if( Blends(channel).BlendInTime > 0.f )
				{
					if( Blends(channel).AnimFrame < 0.0f)
					{
						KeyAlpha = 0.0f;  // No logical solution for combingin lead-in starting at AnimFrame=0 with tweening (starting at negative AnimFrame) ?
					}
					else if( Blends(channel).AnimFrame < Blends(channel).BlendInTime )
					{
						KeyAlpha *= (Blends(channel).AnimFrame / Blends(channel).BlendInTime );
					}
				}

				// debugf(TEXT("Blending channel %i for bone %i with keyalpha %f blendalpha %f, BlendInTime %f af %f"), channel+1, bone, KeyAlpha, Blends(channel).BlendAlpha, Blends(channel).BlendInTime,Blends(channel).AnimFrame ); 

				// May need to be initialized with reference pose before starting any blending; 
				// unless the animation is being totally replaced..

				//#debug
				if( ((KeyAlpha < 1.0f) && (BoneFlags(bone) == BBONE_RefPose)) || bForceRefpose )
				{
					CachedOrientations(bone)= Mesh.RefSkeleton(bone).BonePos.Orientation;
					CachedPositions(bone)= Mesh.RefSkeleton(bone).BonePos.Position;
					BoneFlags(bone) = BBONE_Normal;
				}

				if( KeyAlpha != 0.0f ) // Skip inactive blend.
				{
					if( KeyAlpha >= 1.0f )
					{
						// take over entire pose.
						CachedOrientations(bone) = Trafo->Rot;
						CachedPositions(bone) = Trafo->Pos;
						BoneFlags(bone) = BBONE_Normal;
					}
					else
					{
						// Blend partial animation with the main one.
						const FQuat *Quat1 = &CachedOrientations(bone);
						const FQuat *Quat2 = &Trafo->Rot;

						FQuat ThisQuat;
						FastSlerpNormQuat( Quat1, Quat2, KeyAlpha, &ThisQuat );
						
						CachedOrientations(bone) = ThisQuat;				

						FVector Pos1 = CachedPositions(bone);
						FVector Pos2 = Trafo->Pos;
						FVector ThisPos = Pos1 + (Pos2 - Pos1)*KeyAlpha;
						CachedPositions(bone) = ThisPos;									
					}
				}
			}
		}
		TrafoHeap.Empty();	

		// Only retrieve the root.
		if( bRootTrafoOnly )
		{
			FCoords TempSpaceBase = FQuaternionCoords( CachedOrientations(0));
			TempSpaceBase.Origin = CachedPositions(0);
			
			// FCoords NewRoot = TempSpaceBase * CachedMeshTrafo; Non-rootmotion <- this was temp fix  to make GetRootLocation work (broken now!)
			FCoords NewRoot = TempSpaceBase * LockedLocalTrafo;
			
			NewRootRaw   = TempSpaceBase; 
			NewRootTrafo = NewRoot;       

			RootLocation = NewRoot.Origin;		
			RootRotation = NewRoot.OrthoRotation(); 
			// Or ??? RootRotation = NewRoot.OrthoRotation() - LockedRootTrafo.OrthoRotation();									

			bRootTrafoStale = 0;
			bRootTrafoOnly  = 0;

			unclock(Cycles);
			GStats.DWORDStats( GEngineStats.STATS_Mesh_PoseCycles ) += Cycles;
			return; // Skip the rest of the skin & skeleton setup.
		}		

		// Digest bones into the spacebases.
		for( INT bone=0; bone < Mesh.RefSkeleton.Num(); bone ++ )
		{
#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
            if( BoneFlags(bone) == BBONE_RefPose || bForceRefpose )
            {
                if(NoRefPose.Num() && NoRefPose(bone))
                {
                    CachedOrientations(bone) = FQuat(0, 0, 0, 1);
                    CachedPositions(bone) = FVector(0, 0, 0);
                }
                else
                {
                    CachedOrientations(bone) = Mesh.RefSkeleton(bone).BonePos.Orientation;
                    CachedPositions(bone) = Mesh.RefSkeleton(bone).BonePos.Position;
                }
            }
#else
			if( BoneFlags(bone) == BBONE_RefPose || bForceRefpose )
			{
				CachedOrientations(bone) = Mesh.RefSkeleton(bone).BonePos.Orientation;
				CachedPositions(bone) = Mesh.RefSkeleton(bone).BonePos.Position;
			}
#endif
						
			// Final transformation of quaternion/position to FCoords representation.
			FQuatToFCoordsFast( CachedOrientations(bone),CachedPositions(bone), SpaceBases(bone));
			// SpaceBases(bone) = FQuaternionCoords( CachedOrientations(bone));
			// SpaceBases(bone).Origin = CachedPositions(bone);
			
			// Locked root-motion implies total take-over, so clamp it to what it was at lock-time.
			if( bone==0 )
			{
				if( RootMotionMode )
				{
					FCoords& Root = SpaceBases(0);
					Root.Origin = LockedRootTrafo.Origin; 
					//#Todo - rotation locking
				}		
			}
		}

#ifdef WITH_LIPSINC

		DWORD LIPSincSetupCycles = 0;
		clock(LIPSincSetupCycles);

		if( Mesh.LIPSincController )
		{
			if( !CachedLIPSincBones.Num() )
			{
				Mesh.LIPSincController->InitializeBones( CachedLIPSincBones );
			}

			if( !CachedLIPSincValues.Num() )
			{
				CachedLIPSincValues.Empty();

				// TODO:  should be sum of all targets in normal LBP
				CachedLIPSincValues.AddZeroed( 27 );  // FIXME
			}

			if( IsPlayingLIPSincAnim() || LIPSincBlendInfo.bForceFrame )
			{
				GStats.DWORDOldStats( GEngineStats.STATS_LIPSinc_NumActiveLIPSincAnims )++;

				TLIPSincAnimation* thisAnimation = Mesh.LIPSincController->GetAnimation(m_nActiveLIPSincAnim);

				if( Mesh.LIPSincController->ProcessLIPSinc( thisAnimation, GetActor(), SpaceBases,
					CachedPositions, CachedOrientations, LIPSincBlendInfo, CachedLIPSincValues, CachedLIPSincBones ) )
				{
					LIPSincBlendInfo.Reset();
					m_nActiveLIPSincAnim = -1;
					debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: bAnimDone for %s"), this->GetName());
				}
			}

			// Update the LookAtController
			//FMatrix MeshToWorldMatrix = MeshToWorld();
			//LookAtController.Update( Mesh.LIPSincController->GetLookAtInfo(), &Mesh, SpaceBases, CachedPositions, CachedOrientations, MeshToWorldMatrix, this );
		}

		unclock(LIPSincSetupCycles);
		GStats.DWORDOldStats( GEngineStats.STATS_LIPSinc_TotalSetupCycles ) += LIPSincSetupCycles;

#endif

		CacheActive = true; 

		// Apply any active (uniform) scalers before applying the hierarchical transformation chaining.
		for(INT s=0; s<Scalers.Num(); s++ )
		{
			if( Scalers(s).Bone >= 0)
			{
				INT Bone = Scalers(s).Bone;
				FLOAT Scale = Scalers(s).ScaleUniform;
				if( SpaceBases.Num() > Bone)
				{				
					SpaceBases(Bone).XAxis *= Scale;
					SpaceBases(Bone).YAxis *= Scale;
					SpaceBases(Bone).ZAxis *= Scale;
				}
			}
		}

		// #SKEL Reorganize Scalers/Directors into more universal animation controllers ?
		// Apply any active (non-worldspace) controllers:
		{for(INT t=0; t<Directors.Num(); t++ )
		{
			if( Directors(t).Bone >= 0)
			{
				INT Bone = Directors(t).Bone;
				
				if( SpaceBases.Num() > Bone)
				{				
					// Apply local rotation; should basically add to whatever current rotation/translation is in effect ?
					if( Directors(t).TurnAlpha > 0.0f )
					{								
						FCoords NewRotCoords = GMath.UnitCoords / ( Directors(t).Turn * Directors(t).TurnAlpha);
						NewRotCoords = GMath.UnitCoords * NewRotCoords; 

						FVector OldOrigin = SpaceBases(Bone).Origin;
						SpaceBases(Bone) =  NewRotCoords * SpaceBases(Bone).Transpose(); 
						SpaceBases(Bone).Origin = OldOrigin;  // Saved original translation.
					}
					// Apply local translation.
					if( Directors(t).TransAlpha > 0.0f )
					{
						SpaceBases(Bone).Origin += Directors(t).TransAlpha*( Directors(t).Trans );
					}
				}
			}
		}}
				
		//
		// Apply 'worldspace' direction controllers _during_ the hierarchical chaining since 
		// the controllers are in worldspace but need to influence very local bone trafos.
		// 
		// Parent of root bone is the scene transformation matrix.
		// Apply pivots - run down all transformations through the hierarchy.
		//
		// Global-blended pivot start bones (bGlobalPose) need to be applied here too.
		//
		for(INT s=1; s<SpaceBases.Num(); s++ )
		{			
			INT Parent = Mesh.RefSkeleton(s).ParentIndex;			
			SpaceBases(s) = SpaceBases(s).ApplyPivot(SpaceBases(Parent)); // CPU-costly..
			
			// 'Global' (meshspace) blend-pivot action. Assumes that only
			// rotation changes for that particular joint (position not
			// blended.)
			for(INT p=0; p<PivotHeap.Num(); p++)
			{
				if( PivotHeap(p).Bone == s )
				{					
					// Generate quaternions from Coordinate matrices, to facilitate blending at this late stage.				
					MeshAnimChannel& Blend = Blends( PivotHeap(p).Channel );												
					FLOAT KeyAlpha;
					
					if( PivotHeap(p).Flags == PH_Blend ) // Blended pivot
					{
						// Alpha (must be as used above in blending exactly...)
						KeyAlpha = Blend.BlendAlpha;
						// Lead-in alpha ramping?
						if( Blend.BlendInTime > 0.f )
						{
							if( Blend.AnimFrame < 0.0f)
							{
								KeyAlpha = 0.0f;  
							}
							else if( Blend.AnimFrame < Blend.BlendInTime )
							{
								KeyAlpha *= ( Blend.AnimFrame / Blend.BlendInTime );
							}
						}									
						//debugf(TEXT("#Pivot BLEND alpha:[%f]"),KeyAlpha); 
					}
					else // 'Tweened' pivot with stored Alpha.
					{
						// Alpha (must be as used above in blending exactly...)
						FLOAT PreAlpha = Blend.BlendAlpha;
						// Lead-in alpha ramping?
						if( Blend.BlendInTime > 0.f )
						{
							if( Blend.AnimFrame < 0.0f)
							{
								PreAlpha = 0.0f;  
							}
							else if( Blend.AnimFrame < Blend.BlendInTime )
							{
								PreAlpha *= ( Blend.AnimFrame / Blend.BlendInTime );
							}
						}									
						KeyAlpha = PivotHeap(p).Alpha * PreAlpha; 
						//debugf(TEXT("#Pivot TWEEN alpha:[%f]  pre: [%f]"), KeyAlpha, PreAlpha);
					}
					
					if( KeyAlpha > 0.0f)
					{						
						FQuat BlendPivotQuat = FCoordsQuaternion( PivotHeap(p).Coords );
						FQuat OriginalQuat   = FCoordsQuaternion( SpaceBases(s) );

						// Blend PivotQuat into Original quat, and translate back to 
						// the rotation part of the SpaceBases coordinate system...					
						FQuat ThisQuat;
						FastSlerpNormQuat( &OriginalQuat, &BlendPivotQuat, KeyAlpha, &ThisQuat ); 						

						// Ensure it is normalized also ? 
						FCoords Base = FQuaternionCoords( ThisQuat );
						Base.Origin  = SpaceBases(s).Origin;
						SpaceBases(s) = Base;
						
						// Construct the resultant local in-parent-space quaternion and store in CachedOrientations,
						// so that tweening will work correctly.												
						CachedOrientations(s) = FCoordsQuaternion( UndoPivot( SpaceBases(s),SpaceBases(Mesh.RefSkeleton(s).ParentIndex) ) );
						CachedOrientations(s).Normalize();
						//debugf(TEXT("#Spine global applied and stored - using alpha: %7.5f GTicks %i Bone: %i"),KeyAlpha,(INT)GTicks,(INT)s); 
					}
				}
			} // PivotHeap.Num()
			

			// Apply worldspace bone controllers just before concatenation.
			// Check controllers (#todo - optimize search ?).
			for(INT c=0; c< WorldSpacers.Num(); c++)
			{
				if( WorldSpacers(c).Bone == s)
				{
					if( WorldSpacers(c).TurnAlpha > 0.0f)
					{					
                        FRotator Turn = WorldSpacers(c).Turn;						
						FCoords NewRotCoords = GMath.UnitCoords / ( Turn * WorldSpacers(c).TurnAlpha );
						NewRotCoords.XAxis.Normalize();
						NewRotCoords.YAxis.Normalize();
						NewRotCoords.ZAxis.Normalize();												
						
                        if( WorldSpacers(c).Flags == 0 )
						{
							// Apply the controller orientation in local mesh space, using the inverse of the local bone space transform.
    						NewRotCoords = NewRotCoords * SpaceBases(s).Transpose();
						}
                        else
						{
							// Apply the controller in global world space using the inverse of the mesh->world rotation (and normalize away any scaling.) 							
							FCoords InverseMeshTrafo = CachedMeshTrafo.Transpose();
							InverseMeshTrafo.XAxis.Normalize();
							InverseMeshTrafo.YAxis.Normalize();
							InverseMeshTrafo.ZAxis.Normalize();							
						    NewRotCoords = InverseMeshTrafo * NewRotCoords;
						}
						
						// Leave the bone origin as-is.
						NewRotCoords.Origin = SpaceBases(s).Origin;
						SpaceBases(s) = NewRotCoords;
                    }

					// Apply global translation.
					if( WorldSpacers(c).TransAlpha > 0.0f )
					{
						FVector TransVect = WorldSpacers(c).Trans;
						TransVect = TransVect.TransformVectorBy( CachedMeshTrafo.Inverse() ); 
						SpaceBases(s).Origin += TransVect * WorldSpacers(c).TransAlpha;
					}						
				}
			}		
		}		
	} 
	// End of pose determination.


	// For debugging mode: prepare the wireframe bone line drawing data.
	if( SceneNode && SceneNode->Viewport->bShowBones )  
	{	
		//debugf(TEXT("Setting up debug pivots for mesh: %s showbones: %i"), Mesh.GetName(), SceneNode ? SceneNode->Viewport->bShowBones : -1 );
		DebugPivots.Empty();
		DebugPivots.Add(SpaceBases.Num());
		DebugParents.Empty();
		DebugParents.Add(SpaceBases.Num());
		for( INT s=0; s<SpaceBases.Num(); s++ )
		{
			DebugParents(s) = Mesh.RefSkeleton(s).ParentIndex;
			DebugPivots(s) = SpaceBases(s).Origin; 
		}	
	}
					
	unclock(Cycles);
	GStats.DWORDStats( GEngineStats.STATS_Mesh_PoseCycles ) += Cycles;
			
	// STAT ANIM
	// ">Stat anim" logging  - assume only one mesh being animated (player in behindview ?) 
	// Optional extra animation statistics - print single line for every channel of any animated mesh.
	if( SceneNode && SceneNode->Viewport && SceneNode->Viewport->Actor->GetLevel()->Engine->bShowAnimStats )
	{
		if( Blends.Num() && SceneNode->Viewport->Canvas )
		{
			UCanvas* Canvas = SceneNode->Viewport->Canvas;
			FString	AnimString;			
			AnimString = FString::Printf(TEXT(" Animation - mesh:%s actor:%s  instance:%s channels:"),Mesh.GetName(),  AnimOwner->GetName(), this->GetName() );
			//SceneNode->Viewport->AnimStats.IntStat( *AnimString,TEXT(" ")) = Blends.Num();
			Canvas->CurY += 6;
			DrawStat(Canvas, FColor(255,255,0),4,TEXT("%s"),*AnimString); // Title string
			Canvas->CurY += 3;

			if( Owner->bAnimByOwner )
			{
				DrawStat(Canvas, FColor(255,255,0),4,TEXT(" AnimByOwner: %s"),AnimOwner->GetName() ); 
				Canvas->CurY += 3;
			}

			for(INT c=0; c<Blends.Num(); c++)
			{				
				if( Blends(c).AnimSequence != NAME_None )
				{
					HMeshAnim Seq = GetAnimNamed( Blends(c).AnimSequence );
					FLOAT NumFrames = Seq ? AnimGetFrameCount(Seq) : 1.0;
				
					AnimString = FString::Printf(TEXT("[%i] (%s) Seq:{%s} animframe: %4.2f frames: %i tweening %i alpha %f rate %f" ), c, AnimOwner->GetName(), *Blends(c).AnimSequence, Blends(c).AnimFrame,(INT)NumFrames,IsAnimTweening(c),Blends(c).BlendAlpha,Blends(c).AnimRate );																
					
					DrawStat(Canvas,FColor(255,255,0),4,TEXT(" Anim: [%s]"),*AnimString);
				}
			}			
		}
	}

#ifdef WITH_LIPSINC
	// STAT LIPSINC
	// ">Stat LIPSinc" logging
	if( SceneNode && SceneNode->Viewport->Canvas && SceneNode->Viewport->Actor->GetLevel()->Engine->bShowLIPSincStats )
	{
		if( Mesh.LIPSincController )
		{
			UCanvas* Canvas = SceneNode->Viewport->Canvas;
			FString LIPSincString;

			if( IsPlayingLIPSincAnim() )
			{
				FString LBM;

				if( LIPSincBlendInfo.BlendMode == ELBM_BlendIn )
				{
					LBM = FString(TEXT("ELBM_BlendIn"));
				}
				else if( LIPSincBlendInfo.BlendMode == ELBM_NormalBlend )
				{
					LBM = FString(TEXT("ELBM_NormalBlend"));
				}
				else if( LIPSincBlendInfo.BlendMode == ELBM_BlendOut )
				{
					LBM = FString(TEXT("ELBM_BlendOut"));
				}
				else
				{
					LBM = FString(TEXT("ELBM_None"));
				}

				LIPSincString = FString::Printf(TEXT("LIPSinc Animation - Mesh: %s Actor: %s Instance: %s LIPSinc Anim Name: %s Index: %d Offset: %f Duration: %d LBM: %s"),
					Mesh.GetName(), AnimOwner->GetName(), this->GetName(), *(Mesh.LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->Name()),
					m_nActiveLIPSincAnim, LIPSincBlendInfo.LastOffset, Mesh.LIPSincController->GetAnimation(m_nActiveLIPSincAnim)->EndTime(), *LBM);
			}
			else
			{
				LIPSincString = FString::Printf(TEXT("LIPSinc Animation - Mesh: %s Actor: %s Instance: %s LIPSinc Anim Name: %s Index: %d Offset: %f Duration: 0 LBM: ELBM_None"),
					Mesh.GetName(), AnimOwner->GetName(), this->GetName(), TEXT("LIPSinc_NONE"), m_nActiveLIPSincAnim, LIPSincBlendInfo.LastOffset);
			}

			Canvas->CurY += 3;

			DrawStat(Canvas,FColor(0,255,0),4,TEXT(" LIPSinc: [%s]"),*LIPSincString);
		}
	}
#endif

	unguardSlow;
}


//
// Mesh-to-world matrix - for rendering and visibility bounding boxes.
//
FMatrix USkeletalMeshInstance::MeshToWorld() 
{ 
	/* - OLD Fcoords-based transformation setup
	FCoords LocalCoords;
	FScale SkeletalScale = FScale(SkelMesh->Scale * Actor->DrawScale * Actor->DrawScale3D,0.f,SHEER_None);
	LocalCoords = GMath.UnitCoords * (Actor->Location + Actor->PrePivot) * Actor->Rotation * SkelMesh->RotOrigin * SkeletalScale;
	LocalCoords.Origin += SkelMesh->Origin;
	FMatrix	WorldMatrix = LocalCoords.Matrix();
	RI->SetTransform( TT_LocalToWorld, WorldMatrix );
	*/

	USkeletalMesh* ThisMesh = (USkeletalMesh*)GetMesh();

	FMatrix NewMatrix;

    if( GetActor()->IsA(AInventory::StaticClass()) && ((AInventory*)GetActor())->bDrawingFirstPerson )
        ThisMesh->RotOrigin = ((AInventory*)GetActor())->PlayerViewPivot;

	if(bIgnoreMeshOffset)
		NewMatrix = //FTranslationMatrix( ThisMesh->Origin  ) *
		         FScaleMatrix( GetActor()->DrawScale3D * GetActor()->DrawScale * ThisMesh->Scale ) * 
				 FRotationMatrix( GetActor()->Rotation ) *
				 FTranslationMatrix( GetActor()->Location );
	else
		NewMatrix = //FTranslationMatrix( ThisMesh->Origin  ) *
			FScaleMatrix( GetActor()->DrawScale3D * GetActor()->DrawScale * ThisMesh->Scale ) * 
			FRotationMatrix( ThisMesh->RotOrigin ) *			 
			FRotationMatrix( GetActor()->Rotation ) *
			FTranslationMatrix( GetActor()->Location + GetActor()->PrePivot );
	
	FVector XAxis( NewMatrix.M[0][0], NewMatrix.M[1][0], NewMatrix.M[2][0] );
	FVector YAxis( NewMatrix.M[0][1], NewMatrix.M[1][1], NewMatrix.M[2][1] );
	FVector ZAxis( NewMatrix.M[0][2], NewMatrix.M[1][2], NewMatrix.M[2][2] );

	if( !bForceRawOffset && !bIgnoreMeshOffset )
	{
		NewMatrix.M[3][0] += - ThisMesh->Origin | XAxis;
		NewMatrix.M[3][1] += - ThisMesh->Origin | YAxis;
		NewMatrix.M[3][2] += - ThisMesh->Origin | ZAxis;
	}

	return NewMatrix;

};

// Ramp the overlay specularity mask down over the last half second
static void InitOverlayMaterial( UMaterial* overlayMaterial, FLOAT overlayTimer )
{
	if( overlayMaterial && overlayMaterial->IsA(UShader::StaticClass()) )
	{
		UShader* overlayShader = (UShader*)overlayMaterial;
		if( overlayShader->SpecularityMask && overlayShader->SpecularityMask->IsA(UConstantColor::StaticClass()) )
		{
			((UConstantColor*)overlayShader->SpecularityMask)->Color.A = Clamp((int)(overlayTimer * 510), 0, 255);
		}
	}
}

// Make a copy of a final-blend (if present). May return none
static UFinalBlend* CopyFinalBlendFromMaterial(UMaterial* material)
{
	UFinalBlend* fb;
	DECLARE_STATIC_UOBJECT( UFinalBlend, overlayFinalBlend, { } );

	if( (fb = Cast<UFinalBlend>(material)) != NULL )
	{
		overlayFinalBlend->FrameBufferBlending =	fb->FrameBufferBlending;
		overlayFinalBlend->ZWrite = fb->ZWrite;
		overlayFinalBlend->ZTest = fb->ZTest;
		overlayFinalBlend->AlphaTest = fb->AlphaTest;
		overlayFinalBlend->TwoSided = fb->TwoSided;
		overlayFinalBlend->AlphaRef = fb->AlphaRef;

		return overlayFinalBlend;
	}
	else
		return NULL;
}

// Try and get a basic texture out of the given material. May return none.
static UTexture* GetTextureFromMaterial(UMaterial* material)
{
	UTexture* skelTexture;
	UFinalBlend* skelFinalBlend;
	UShader* skelShader;
	UCombiner* skelCombiner;

	if(!material)
	{
		return NULL;
	}
	else if( (skelTexture = Cast<UTexture>(material)) != NULL )
	{
		return skelTexture;
	}
	else if( (skelFinalBlend = Cast<UFinalBlend>(material)) != NULL )
	{
		return GetTextureFromMaterial(skelFinalBlend->Material);
	}
	else if( (skelShader = Cast<UShader>(material)) != NULL )
	{
		return GetTextureFromMaterial(skelShader->Diffuse);		
	}
	else if( (skelCombiner = Cast<UCombiner>(material)) != NULL )
	{
		return GetTextureFromMaterial(skelCombiner->Material2);
	}

	return NULL;
}

// Try and get a basic combiner out of the given material. May return none. --Dave@Psyonix
static UCombiner* GetCombinerFromMaterial(UMaterial* material)
{
	UTexture* skelTexture;
	UFinalBlend* skelFinalBlend;
	UShader* skelShader;
	UCombiner* skelCombiner;

	if(!material)
	{
		return NULL;
	}
	else if( (skelTexture = Cast<UTexture>(material)) != NULL )
	{
		return NULL;
	}
	else if( (skelFinalBlend = Cast<UFinalBlend>(material)) != NULL )
	{
		return GetCombinerFromMaterial(skelFinalBlend->Material);
	}
	else if( (skelShader = Cast<UShader>(material)) != NULL )
	{
		return GetCombinerFromMaterial(skelShader->Diffuse);		
	}
	else if( (skelCombiner = Cast<UCombiner>(material)) != NULL )
	{
		return skelCombiner;
	}

	return NULL;
}

// Take the current 'skin' material, and the desired overlay, and generate new material.
static UMaterial* ApplyOverlayMaterial(UMaterial* skelMaterial, UMaterial* overlayMaterial, UMaterial* HighDetailOverlay)
{
	// If there is no overlay - do nothing (just use skin)
	if( !overlayMaterial && !HighDetailOverlay )
		return skelMaterial;

	UMaterial* CurrentOverlayMaterial = overlayMaterial;
	if ( !CurrentOverlayMaterial )
		CurrentOverlayMaterial = HighDetailOverlay;

	UShader* overlayShader = Cast<UShader>(CurrentOverlayMaterial);
	UCombiner* overlayCombiner = Cast<UCombiner>(CurrentOverlayMaterial);

	// If overlay is not a shader or a combiner - just use it instead of skin.
	if(!overlayShader && !overlayCombiner)
		return CurrentOverlayMaterial;

	// Removed to support shader overlays using combiners as the diffuse
	//if ( overlayShader )
	//{
	//	overlayCombiner = Cast<UCombiner>(overlayShader->Diffuse);
	//	if (overlayCombiner)
	//		overlayShader = NULL;
	//}

	// Try and find a texture from characters skin material.
	UTexture* skelTexture = GetTextureFromMaterial(skelMaterial);
	if(!skelTexture)
	{
		//debugf(TEXT("ApplyOverlayMaterial: Could not obtain texture from skin(%s). Ignoring Overlay."),skelMaterial->GetName() );
		return skelMaterial;
	}

	UBOOL bForceTranslucentFinalBlend = false;

	// Plug texture into overlay material
	if(overlayShader)
	{
		// Try and find a combiner from characters skin material.
		UCombiner* skelCombiner = GetCombinerFromMaterial(skelMaterial);
		if(!skelCombiner)
			overlayShader->Diffuse = skelTexture; 		// Overlay is a shader - use skin as diffuse channel		
		else
			overlayShader->Diffuse = skelCombiner;

		if ( skelTexture->bAlphaTexture || skelTexture->bMasked )		
			bForceTranslucentFinalBlend = true;
	}
	else if(overlayCombiner)
	{
		overlayCombiner->Material2 = skelTexture;	// Overlay is a combiner - use skin as material2
		if ( CurrentOverlayMaterial == HighDetailOverlay )
			overlayCombiner->Mask = skelTexture;
		if ( skelTexture->bAlphaTexture || skelTexture->bMasked )
			bForceTranslucentFinalBlend = true;
	}
	
	UFinalBlend* overlayFinalBlend = CopyFinalBlendFromMaterial(skelMaterial);
	// See if the skin material had a final blend. If so-duplicate and add to overlay material.
	if( overlayFinalBlend )
	{
		overlayFinalBlend->Material = CurrentOverlayMaterial;
		overlayFinalBlend->FallbackMaterial = skelMaterial;
		return overlayFinalBlend;
	}
	else 
	{
		// FIXUP: a Finalblend may be necessary to maintain translucency.
		if( bForceTranslucentFinalBlend && skelTexture )
		{
			DECLARE_STATIC_UOBJECT( UFinalBlend, overlayFinalBlendObj, { } );			
			overlayFinalBlendObj->Material = CurrentOverlayMaterial;
			overlayFinalBlendObj->FallbackMaterial = skelMaterial;
			overlayFinalBlendObj->FrameBufferBlending = FB_Translucent;
			overlayFinalBlendObj->ZWrite = false;
			overlayFinalBlendObj->ZTest = true;
			overlayFinalBlendObj->AlphaTest = false;
			overlayFinalBlendObj->TwoSided = skelTexture->bTwoSided;
			overlayFinalBlendObj->AlphaRef = false;
			return overlayFinalBlendObj;
		}
		else
		{
			CurrentOverlayMaterial->FallbackMaterial = skelMaterial;
			return CurrentOverlayMaterial;
		}
	}
}



//
//	DrawSprite - special-case sprite drawing for LOD purposes.
//  TODO: - for more speed the sprite vertices could be stored (per instance or per mesh) and re-rendered quickly.
//        - not all rotation/pivot modes make sense with the RelativeRotation
//        - Experiment with alpha fading. 
//
void DrawSkelImpostor( FDynamicActor* Owner, FVector* Origin, FLevelSceneNode* SceneNode,FRenderInterface* RI, TList<FDynamicLight*>* Lights)
{
	guard(DrawImpostor);

	AActor* Actor = Owner->Actor;
	USkeletalMesh*  SkelMesh = (USkeletalMesh*)Owner->Actor->Mesh;

	// Calculate the sprite's location, right and down vectors (world)
	FVector	SpriteBase = Actor->Location + SkelMesh->ImpostorProps.RelativeLocation * Actor->DrawScale;			
	FVector SpriteX = FVector( 1.0f,0.0f,0.0f);
	FVector SpriteY = FVector( 0.0f,1.0f,0.0f);		
	
	// Rotation and non-uniform scaling.
	FCoords RotCoords = GMath.UnitCoords / SkelMesh->ImpostorProps.RelativeRotation;
	
	if( SkelMesh->ImpostorProps.ImpSpaceMode == ISM_Sprite )
	{
		// Default sprite mode		
		SpriteX =  SceneNode->CameraX.SafeNormal() * Actor->DrawScale * Actor->DrawScale3D.X * Actor->Texture->MaterialUSize();
		SpriteX = 0.5f * SkelMesh->ImpostorProps.Scale3D.X * ( SpriteX.TransformVectorBy( RotCoords ) );
		SpriteY = -SceneNode->CameraY.SafeNormal() * Actor->DrawScale * Actor->DrawScale3D.Y * Actor->Texture->MaterialUSize();
		SpriteY = 0.5f * SkelMesh->ImpostorProps.Scale3D.Y * ( SpriteY.TransformVectorBy( RotCoords ) );
	}
	else
	if( SkelMesh->ImpostorProps.ImpSpaceMode == ISM_Fixed )
	{		
		SpriteX =  FVector(0.f,1.f, 0.f) * Actor->DrawScale * Actor->DrawScale3D.X * Actor->Texture->MaterialUSize();
		SpriteX = 0.5f * SkelMesh->ImpostorProps.Scale3D.X * ( SpriteX.TransformVectorBy( RotCoords ) );
		SpriteY =  FVector(0.f,0.f,-1.f) * Actor->DrawScale * Actor->DrawScale3D.Y * Actor->Texture->MaterialUSize();		
		SpriteY = 0.5f * SkelMesh->ImpostorProps.Scale3D.Y * ( SpriteY.TransformVectorBy( RotCoords ) );
	}
	else if( SkelMesh->ImpostorProps.ImpSpaceMode == ISM_PivotVertical )
	{		
		SpriteX =  SceneNode->CameraX.SafeNormal() * Actor->DrawScale * Actor->DrawScale3D.X * Actor->Texture->MaterialUSize();
		SpriteX = 0.5f * SkelMesh->ImpostorProps.Scale3D.X * SpriteX;
		SpriteY =  FVector(0.f,0.f,-1.f) * Actor->DrawScale * Actor->DrawScale3D.Y * Actor->Texture->MaterialUSize();				
		SpriteY = 0.5f * SkelMesh->ImpostorProps.Scale3D.Y * ( SpriteY.TransformVectorBy( RotCoords ) );
	}
	else if( SkelMesh->ImpostorProps.ImpSpaceMode == ISM_PivotHorizontal )
	{		
		SpriteX =  FVector(0.f,1.f, 0.f) * Actor->DrawScale * Actor->DrawScale3D.X * Actor->Texture->MaterialUSize();
		SpriteX = 0.5f * SkelMesh->ImpostorProps.Scale3D.X * ( SpriteX.TransformVectorBy( RotCoords ) );		
		SpriteY = -SceneNode->CameraY.SafeNormal() * Actor->DrawScale * Actor->DrawScale3D.Y * Actor->Texture->MaterialUSize();		
		SpriteY = 0.5f * SkelMesh->ImpostorProps.Scale3D.Y * SpriteY;
	}

	
		
	// Vertex color - always from impostor settings.
	FPlane	ColorPlane = SkelMesh->ImpostorProps.ImpColor;
	ColorPlane.W = 1.0f;
	// Selection highlight
	if(GIsEditor)
	{
		if(SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight && Actor->bSelected)
			ColorPlane = FPlane(0.5f,0.9f,0.5f,1.0f);
		// else if( Actor->LightType != LT_None )
		// 	ColorPlane = FGetHSV(Actor->LightHue,Actor->LightSaturation,255);
	}
	FColor Color = FColor(ColorPlane);

	// Setup the (3d worldspace) sprite vertices.
	FLitSpriteVertexStream	SpriteVertices;	
	UMaterial* Material = SkelMesh->ImpostorProps.Material ? SkelMesh->ImpostorProps.Material : Actor->Texture;
	UTexture* Texture = Cast<UTexture>(Material);

	// If 'raw' texture, set blending modes explicitly:
	if( Texture )
	{
		DECLARE_STATIC_UOBJECT( UFinalBlend, FinalBlend, {} );
		FinalBlend->Material = Texture;
		FinalBlend->TwoSided = 1;

		if( Texture->bMasked || Actor->Style==STY_Masked )
		{
			FinalBlend->FrameBufferBlending = FB_Overwrite;
			FinalBlend->ZWrite				= 1;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 1;
			FinalBlend->AlphaRef			= 127;
		}
		else
		if( Texture->bAlphaTexture )
		{
			FinalBlend->FrameBufferBlending = FB_AlphaBlend;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 1;
			FinalBlend->AlphaRef			= 0;
		}
		else
		if( Actor->Style==STY_Translucent )
		{
			FinalBlend->FrameBufferBlending = FB_Translucent;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		else
		if( Actor->Style==STY_Modulated )
		{
			FinalBlend->FrameBufferBlending = FB_Modulate;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		else
		if( Actor->Style==STY_Additive ) // sjs
		{
			FinalBlend->FrameBufferBlending = FB_Brighten;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		else
		if( Actor->Style==STY_Subtractive ) // sjs
		{
			FinalBlend->FrameBufferBlending = FB_Darken;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		Material = FinalBlend;		
	}

	SpriteVertices.Vertices[0].Position = SpriteBase - SpriteX - SpriteY;
	SpriteVertices.Vertices[0].U = 0.0f;
	SpriteVertices.Vertices[0].V = 0.0f;
	SpriteVertices.Vertices[0].Diffuse = Color;

	SpriteVertices.Vertices[1].Position = SpriteBase + SpriteX - SpriteY;
	SpriteVertices.Vertices[1].U = 1.0f;
	SpriteVertices.Vertices[1].V = 0.0f;
	SpriteVertices.Vertices[1].Diffuse = Color;

	SpriteVertices.Vertices[2].Position = SpriteBase + SpriteX + SpriteY;
	SpriteVertices.Vertices[2].U = 1.0f;
	SpriteVertices.Vertices[2].V = 1.0f;
	SpriteVertices.Vertices[2].Diffuse = Color;

	SpriteVertices.Vertices[3].Position = SpriteBase - SpriteX + SpriteY;
	SpriteVertices.Vertices[3].U = 0.0f;
	SpriteVertices.Vertices[3].V = 1.0f;
	SpriteVertices.Vertices[3].Diffuse = Color;

	if( SkelMesh->ImpostorProps.ImpLightMode == ILM_PseudoShaded )
	{
		// Make each normal point outward diagonally. All in world space.
		SpriteVertices.Vertices[0].Normal = 0.5f * (SpriteVertices.Vertices[0].Position - SpriteVertices.Vertices[2].Position);
		SpriteVertices.Vertices[1].Normal = 0.5f * (SpriteVertices.Vertices[1].Position - SpriteVertices.Vertices[3].Position);
		SpriteVertices.Vertices[2].Normal = -SpriteVertices.Vertices[0].Normal;
		SpriteVertices.Vertices[3].Normal = -SpriteVertices.Vertices[1].Normal;
	}
	else
	// ILM_Uniform, or even unlit.
	{
		// Make all normals aligned, straight forward...
		SpriteVertices.Vertices[0].Normal = (SpriteX ^ SpriteY).SafeNormal();
		SpriteVertices.Vertices[1].Normal = SpriteVertices.Vertices[0].Normal;
		SpriteVertices.Vertices[2].Normal = SpriteVertices.Vertices[0].Normal;
		SpriteVertices.Vertices[3].Normal = SpriteVertices.Vertices[0].Normal;
	}
		
	// Draw the sprite.
	INT BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&SpriteVertices);
	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
	
	if( SkelMesh->ImpostorProps.ImpLightMode != ILM_Unlit )
	{
		// Enable lights.
		INT LightsLimit = Actor->MaxLights; 
		INT	NumHardwareLights = 0;
		// Set lights.
		//RI->EnableLighting(1,0,1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly, Owner->BoundingSphere);
		RI->EnableLighting(1,1,1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly, Owner->BoundingSphere);
		for(TList<FDynamicLight*>* LightList = Lights;LightList && NumHardwareLights < LightsLimit;LightList = LightList->Next)
			RI->SetLight(NumHardwareLights++,LightList->Element);
		RI->SetAmbientLight(Owner->AmbientColor);
	}
	else
	{
		// unlit
		RI->EnableLighting(0,1);  // 2nd parameter = use the static color in vertices.
	}
	
	RI->SetMaterial(Material);
	RI->SetIndexBuffer(NULL,0);
	RI->DrawPrimitive(PT_TriangleFan,BaseVertexIndex,2);

	unguard;
}

static FColor CalcCollisionPrimRenderColor(UBOOL bBlockKarma, UBOOL bBlockZeroExtent, UBOOL bBlockNonZeroExtent)
{
	if(bBlockKarma)
	{
		if(bBlockZeroExtent || bBlockNonZeroExtent)
			return FColor(255,0,255);
		else
			return FColor(255,0,0);
	}
	else
	{
		if(bBlockZeroExtent || bBlockNonZeroExtent)
			return FColor(0,0,255);
		else
			return FColor(255,255,255);
	}
}


// Assign detail factors to the discrete skeletal mesh detail level options.
inline FLOAT DetermineGlobalDetailBias( AActor* MeshActor)
{
	FLOAT GlobalDetailBias = 1.0f;
	if( MeshActor && MeshActor->GetLevel() && MeshActor->GetLevel()->GetLevelInfo() )
	{
		BYTE DetailLevel = MeshActor->GetLevel()->GetLevelInfo()->MeshLODDetailLevel;
		switch( DetailLevel)
		{
			case MDL_Low:
				GlobalDetailBias = 1.22f;
			break;
			case MDL_Medium:
				GlobalDetailBias = 1.00f;
			break;
			case MDL_High:
				GlobalDetailBias = 0.72f;
			break;
			case MDL_Ultra:
				GlobalDetailBias = 0.33f;
			break;
		}
	}	
	return GlobalDetailBias;
}


// Very Rough approximation of rigid wedge count, used for LOD-scaling only.
FORCEINLINE INT GetTotalWedgeCountEstimate( const FStaticLODModel& LODModel)
{	
	if( LODModel.RigidSections.Num() )
	{		
		INT RigidFaces = 0;
		for( INT i=0; i<LODModel.RigidSections.Num();i++)
		{
			RigidFaces += LODModel.RigidSections(i).TotalFaces;  
		}
		return ( RigidFaces / 4 ) + LODModel.SmoothStreamWedges; // Guess the wedge number from the number of faces. Also, rigid wedges do not count as heavily as smooth wedges.
	}
	else
	{
		return LODModel.SmoothStreamWedges;
	}
}

//
//	USkeletalMeshInstance::Render
//

void USkeletalMeshInstance::Render( FDynamicActor* Owner, FLevelSceneNode* SceneNode, TList<FDynamicLight*>* Lights, TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI )
{
	guard(USkeletalMeshInstance::Render);
	
	DWORD RenderCycles=0; clock(RenderCycles); // Time complete skeletal render time.
	DWORD AuxCycles=RenderCycles; 	

	// Allow owner-driven animations.
	AActor*	AnimOwner = NULL;
	if( (Owner->Actor) && (Owner->Actor->bAnimByOwner) && (Owner->Actor->Owner != NULL) )
		AnimOwner = Owner->Actor->Owner;
	else
		AnimOwner = Owner->Actor;

	AActor*	Actor = Owner->Actor;
	USkeletalMesh* SkelMesh = (USkeletalMesh*)GetMesh();		
	
	// Verify render data availability.
	if( ! SkelMesh->LODModels.Num() ) 
	{
		debugf(TEXT("No renderable data available for mesh [%s]"),SkelMesh->GetName());
		return;
	}

	// Evaluate root before LOD distance determination unless we have a potential 
	// impostor (for which we don't want to waste any bone evaluation time. )
	UBOOL bRootSensitiveLOD = SkelMesh->bImpostorPresent ? true: false;
	
	// Currently unused for PC.
	SkelMesh->RenderPreProcess(); 	

	// Requested debug drawing modes.
	bDisplayNormals = ( SceneNode && SceneNode->Viewport->bShowNormals);
		
	//
	// GETFRAME - Get all bone trafos & global scaler trafo.
	// This initializes/updates the SkelMesh->SpaceBases and 
	// the 3d FVectors in the vertex stream.
	//
	INT DummyVerts = 0;
	if( bRootSensitiveLOD )
	{
		GetFrame( Actor, SceneNode ,NULL, 0, DummyVerts, GF_FullSkin );
	}	

	const FMatrix& MeshToWorldMatrix = MeshToWorld();
	
	FVector MeshLODOrigin;
	// If root bone available and we're not using 'rootoffset-sensitive-lod', 
	// use its world location to determine LOD rather than actor center.
	if( bRootSensitiveLOD && SpaceBases.Num())
	{
		MeshLODOrigin = MeshToWorldMatrix.TransformFVector( SpaceBases(0).Origin ); // Root bone position in world.
	}
	else
	{
		MeshLODOrigin = Actor->Location; // Actor position in world.
	}

	// Determine desired VertexSubset for LOD
	FSceneNode*	LodSceneNode = SceneNode->GetLodSceneNode();
	FLOAT FovBias = (LodSceneNode->Viewport && LodSceneNode->Viewport->Actor) ? LodSceneNode->Viewport->Actor->FOVBias : 1.f;  // calculated in RenderLevel
	// Complexity bias: effectively, stronger LOD for more complex meshes. Assume 0th lod is representative.
	FLOAT CpxBias =  0.25f + 0.75f * GetTotalWedgeCountEstimate( SkelMesh->LODModels(0) ) / 250.f; // About even for 250-vertex meshes.
	FLOAT ResolutionX = Max( 820, LodSceneNode->RenderTarget->GetWidth() ); // Prevent too strong LOD at lower/around 800x600 resolutons.
	FLOAT ResolutionBias = 0.3f + 0.7f* ResolutionX/640.f; // Moderated resolution influence.
	// Z coordinate :in units. 60 units is about the player's height. 
	FLOAT Z = (LodSceneNode->Project( MeshLODOrigin ) ).W - SkelMesh->LODZDisplace;
	FLOAT GlobalLODFactor =    GIsEditor ? 1.0f : ( Actor->GetLevel() ? Actor->GetLevel()->GetLevelInfo()->AnimMeshGlobalLOD : 1.0f )  * DetermineGlobalDetailBias( Actor ) ;	
	FLOAT DetailDiv = SkelMesh->LODStrength * FovBias * Max( 1.f, Z ) * CpxBias * GlobalLODFactor;
	FLOAT MeshVertLODFactor  = 420.0f * ResolutionBias * Actor->DrawScale3D.GetMax() * Actor->DrawScale * Actor->LODBias * SkelMesh->MeshScaleMax / DetailDiv;   
	// 530.0f seems reasonable LOD factor ( early Warfare portal-based build )	

	LastLodFactor = MeshVertLODFactor;

	UBOOL bDrawImpostor = false;

	// Old  smooth-lod vertex count determination.
	// FLOAT TargetSubset    = Max( (FLOAT)SkelMesh->LODMinVerts, (FLOAT)SkelMesh->Points.Num() * MeshVertLODFactor  );
	// INT VertexSubset      = Min( appRound(TargetSubset), SkelMesh->Points.Num() );

	//
	// Choose Static LOD level depending on MeshVertLODFactor, or use forced LOD level.
	//
	INT NewLODLevel = 0;
	if( (ForcedLodModel > 0 ) && (ForcedLodModel <= SkelMesh->LODModels.Num()) )
	{
		CurrentLODLevel = ForcedLodModel-1;
	}
	else if( SkelMesh->LODModels.Num()>1)
	{
	
		while( NewLODLevel < (SkelMesh->LODModels.Num()-1) &&  (SkelMesh->LODModels(NewLODLevel).DisplayFactor >= MeshVertLODFactor  ) )
		{
			NewLODLevel++;
		}
	
		// Hysteresis works by allowing the more complex (lower index) LOD model to persist when it needs to pop out, in complex->simple direction only.
		INT OldLODLevel = CurrentLODLevel;
		if( ( OldLODLevel < NewLODLevel ) )
		{
			FLOAT Hysteresis = SkelMesh->LODModels(OldLODLevel).LODHysteresis;			
			FLOAT OldFactor = SkelMesh->LODModels(OldLODLevel).DisplayFactor;
			// If  MeshVertLODFactor still within allowed hysteresis range, don't change the LOD level.
			if(  OldFactor <= (MeshVertLODFactor + Hysteresis) ) 				
			{	
				NewLODLevel = OldLODLevel;				
			}
		}
		CurrentLODLevel = NewLODLevel;

		if( OldLODLevel != NewLODLevel ) // Fixes corruption when meshes are rendered multiple times in a single frame  (from Bob Berry.)
			SkinStream.Revision++;
	}
	
	// Check whether we're pushing beyond last LOD and a sprite impostor is allowed.
	if( (ForcedLodModel == 0) && 
		 SkelMesh->bImpostorPresent && 
		(NewLODLevel == (SkelMesh->LODModels.Num()-1)) &&  
		(SkelMesh->LODModels(NewLODLevel).DisplayFactor >= MeshVertLODFactor ) )
	{
		bDrawImpostor = true;
	}		

	// Sort out rendering flags.
	UBOOL  bDrawWireframe = SceneNode->Viewport->IsWire() || bForceWireframe;	
	DWORD  ExtraFlags = 0;
	if( bDrawWireframe )
		ExtraFlags |= (PF_Wireframe | PF_FlatShaded);
	if( SceneNode->Viewport->Actor->RendMap == REN_LightingOnly )
		ExtraFlags |= PF_FlatShaded;

	if( bDrawImpostor ) 
	{
		// Draw impostor LOD instead.		
		DrawSkelImpostor( Owner, &MeshLODOrigin, SceneNode, RI, Lights );
		unclock(RenderCycles); 
		GStats.DWORDStats( GEngineStats.STATS_Mesh_SkelCycles ) += RenderCycles;
		return;
	}

	// No root-sensitive LOD ( i.e. potential impostor drawing ) - GetFrame after  LOD determination...
	if( ! bRootSensitiveLOD )
	{
		GetFrame( Actor, SceneNode ,NULL, 0, DummyVerts, GF_FullSkin );
	}
		
	//
    // Start actual drawing - send lights, buffers, and draw primitives.
	//	
			
	UBOOL DrawBones = ( SceneNode && SceneNode->Viewport->bShowBones &&  DebugPivots.Num() );
	UBOOL DrawSkin  = ( SceneNode && ! SceneNode->Viewport->bHideSkin );
		
	RI->SetTransform(TT_LocalToWorld,MeshToWorldMatrix);
	
	unclock(AuxCycles);
	GStats.DWORDStats( GEngineStats.STATS_Mesh_LODCycles ) += AuxCycles; // All preparations, excl. buffer-setting.

	INT	NumHardwareLights = 0;
	FSphere	LightSphere( FVector(0,0,0),0.f); 

		
	// Set light if there's anything to render.
	if( DrawSkin && ( ( SkelMesh->LODModels(CurrentLODLevel).SmoothSections.Num() > 0 ) || ( SkelMesh->LODModels(CurrentLODLevel).RigidSections.Num() > 0 ) ) ) 
	{
		// Lighting.		
		if( bDrawWireframe )
		{			
			RI->EnableLighting(1,0,0,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly,Owner->BoundingSphere);
			// Determine the wireframe color.
			UEngine*	Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
			FColor		WireColor = Engine->C_AnimMesh ;
			if( Actor->bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
				RI->SetAmbientLight(FColor(120,255,75,255)); // higlight wire with a yellowish green.
			else
				RI->SetAmbientLight(FColor(WireColor.Plane())); 
		}	
		else if( Actor->bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
		{			
			RI->EnableLighting(1,0,0,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly,Owner->BoundingSphere);
			// Highlight the skeletal mesh with green marker color.			
			RI->SetAmbientLight(FColor(128,255,128,255));  						
		}
		else if( SceneNode->Viewport->Actor && ( SceneNode->Viewport->Actor->RendMap == REN_ScreenActor ) ) 
		{
			// Skip lighting for ScreenActors.
		}
		else if( Actor->GetAmbientLightingActor()->bUnlit || !SceneNode->Viewport->IsLit() )
		{
			RI->EnableLighting(0,0);
		}
		else
		{
			INT LightsLimit = Actor->MaxLights; 

			// Set lights.
			FSphere	LightingSphere(Owner->BoundingSphere + (MeshToWorldMatrix.TransformFVector( SpaceBases(0).Origin ) - Owner->Actor->Location),Owner->BoundingSphere.W);
			LightSphere = LightingSphere;
			RI->EnableLighting(1,0,1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly,LightingSphere);
			for(TList<FDynamicLight*>* LightList = Lights;LightList && NumHardwareLights < LightsLimit;LightList = LightList->Next)
			{				
				RI->SetLight(NumHardwareLights++,LightList->Element);			
			}
			RI->SetAmbientLight(Owner->AmbientColor);		
		}
	}

			
	// Render smooth (software-skinned) parts.	
	if( DrawSkin && ( SkelMesh->LODModels(CurrentLODLevel).SmoothSections.Num() > 0 )  )
	{	
		DWORD IdxCycles=0;clock(IdxCycles);			
		//  Calling the SetDynamicStream causes ComputeSkinVerts to generate the vertices directly into stream buffer memory.
		SkinStream.bStreamCallback = true; // Indicate software-skinning callback required.
		SkinStream.MeshInstance = this; // Make aware of current mesh/Instance				

		// Get a vertex pool to render this skeletal mesh from.

#if 1
		FVertexPool*	VertexPool = GetVertexPool(&SkinStream);

		if(VertexPool->GetClient() != &SkinStream)
			VertexPool->SetClient(&SkinStream);

		VertexPool->LifeTimeFrames = 1000;

		FVertexStream*	VertexStreams[] = { VertexPool };
		
		RI->SetVertexStreams( VS_FixedFunction, VertexStreams, 1 ); 
		if( SkelMesh->SkinTesselationFactor > 1.0f )
			RI->SetNPatchTesselation( SkelMesh->SkinTesselationFactor);
#else
		INT	BaseVertexIndex = RI->SetDynamicStream( VS_FixedFunction, &SkinStream ); 
#endif
		unclock(IdxCycles);
		GStats.DWORDStats( GEngineStats.STATS_Mesh_RigidCycles ) += IdxCycles;
		
		DWORD ResultCycles=0;clock(ResultCycles);					
		// Force valid revision index for static index buffer ( older versions left Revision at 0 which crashes. )
		if( SkelMesh->LODModels(CurrentLODLevel).SmoothIndexBuffer.Revision <= 0)
			SkelMesh->LODModels(CurrentLODLevel).SmoothIndexBuffer.Revision = 1;

		INT BaseIdxIndex = 0; 				
		RI->SetIndexBuffer( &SkelMesh->LODModels(CurrentLODLevel).SmoothIndexBuffer, 0 ); 		
		unclock(ResultCycles);
		GStats.DWORDStats( GEngineStats.STATS_Mesh_ResultCycles ) += ResultCycles; // Indexbuffer upload time.
		
		DWORD DrawCycles=0;clock(DrawCycles); //Time draw cycles including light setting.
		
		// Do any special set-up on the overlay material
		InitOverlayMaterial(Actor->OverlayMaterial, Actor->ClientOverlayCounter);	

		//
		// Smooth sections: draw the active level of detail.
		//
		for( INT SectionIndex = 0; SectionIndex < SkelMesh->LODModels(CurrentLODLevel).SmoothSections.Num(); SectionIndex++ )
		{			
			FSkelMeshSection& Section = SkelMesh->LODModels(CurrentLODLevel).SmoothSections(SectionIndex);

			if( Section.TotalFaces )  
			{						
				UMaterial*	Material = GetMaterial( SkelMesh->MeshMaterials(Section.MaterialIndex).MaterialIndex, Actor );
				if( bDrawWireframe ) 
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
					RI->SetMaterial( ApplyOverlayMaterial(Material, Actor->OverlayMaterial, (Actor->Level->DetailMode == DM_Low) ? NULL : Actor->HighDetailOverlay) );			
				}
			
			
				/*
				// Fun 'stripper'....gradually erase polygons to verify the drawing sequence. Use DrawFaceNum instead of Section.TotalFaces to make it work.
				static INT StripSection = 0;				
				static INT StripIndex = 0;				
				StripIndex--; 
				if( StripIndex<0 )
				{					
					StripSection--;
					if( StripSection < 0 ) StripSection = SkelMesh->LODModels(CurrentLODLevel).SmoothSections.Num()-1;
					StripIndex = SkelMesh->LODModels(CurrentLODLevel).SmoothSections(StripSection).TotalFaces * 7;					
				}
				INT DrawFaceNum = (StripSection < SectionIndex) ? 0 : (StripSection == SectionIndex) ? Min((INT)Section.TotalFaces, StripIndex/7 ) : Section.TotalFaces ;
				DrawFaceNum =  Max(Min((INT)DrawFaceNum,(INT)Section.TotalFaces),1);				
				*/												
														
				// Draw the section.
				RI->DrawPrimitive
				( 
					PT_TriangleList,                              // 
					BaseIdxIndex,						          //  First index of this section in the index buffer.
					Section.TotalFaces,                           //  Total faces to be drawn. 
					Section.MinIndex,							  //  Lowest used index into the vertex buffer for this section.
					Section.MaxIndex                              //  Highest used index into the vertex buffer for this section.
				);
				
				// Draw projectors for the section.
				AActor* ProjectorBase = Owner->Actor->GetProjectorBase();
				if( (ProjectorBase->Projectors.Num() || Projectors) && !SceneNode->Viewport->IsWire() && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Projectors) )
				{
					RI->PushState();
					RI->SetZBias(1);
					RI->EnableLighting(0,0);
					for( INT i=0;i<ProjectorBase->Projectors.Num();i++ )
					{
						FProjectorRenderInfo* P = ProjectorBase->Projectors(i);
						if( !P->Render( Owner->Actor->Level->TimeSeconds ) )
						{
							ProjectorBase->Projectors.Remove(i--);
							continue;
						}
						if( (ExtraFlags&PF_Unlit) && !(P->ProjectorFlags&PRF_ProjectOnUnlit) )
							continue;

						// Setup blending.

						RI->SetMaterial(P->GetMaterial(SceneNode,Material));

						RI->DrawPrimitive
						( 
							PT_TriangleList,
							BaseIdxIndex,						          //  First index of this section in the index buffer.
							Section.TotalFaces,                           //  Total faces to be drawn. 							
							Section.MinIndex,							  //  Lowest used index into the vertex buffer for this section.
							Section.MaxIndex                              //  Highest used index into the vertex buffer for this section.							
						);
					}
					for(TList<FProjectorRenderInfo*>* ProjectorList = Projectors;ProjectorList;ProjectorList = ProjectorList->Next)
					{
						FProjectorRenderInfo*	Projector = ProjectorList->Element;

						if(!Projector->Render( Owner->Actor->Level->TimeSeconds ) || ((ExtraFlags & PF_Unlit) && !(Projector->ProjectorFlags & PRF_ProjectOnUnlit)))
							continue;

						if( Projector->Projector->bNoProjectOnOwner && Projector->Projector->Owner && (Projector->Projector->Owner == Actor) )
							continue;

						// Render the projector.

						RI->SetMaterial(Projector->GetMaterial(SceneNode,Material));

						RI->DrawPrimitive(
							PT_TriangleList,
							BaseIdxIndex,
							Section.TotalFaces,
							Section.MinIndex,
							Section.MaxIndex
							);
					}
					RI->PopState();
				}

				BaseIdxIndex += Section.TotalFaces * 3;		
			}
		}

		unclock(DrawCycles);
		GStats.DWORDStats( GEngineStats.STATS_Mesh_DrawCycles ) += DrawCycles;
	}
	
	//
	// Draw rigid sections; independent ( vertex stream & index buffer wise ) of smooth sections.	
	//

	if( DrawSkin && SkelMesh->LODModels(CurrentLODLevel).RigidSections.Num() )
	{				
		// Distinguish raw from smooth in wireframe colors..
		if( bDrawWireframe )
		{
			// Determine the wireframe color.
			UEngine*	Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
			FColor		WireColor = Engine->C_BrushWire;
			if( Actor->bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
				RI->SetAmbientLight(FColor(120,255,75,255)); // higlight wire with a yellowish green.
			else
				RI->SetAmbientLight(FColor(WireColor.Plane())); 			
		}
				
		RI->SetIndexBuffer( &(SkelMesh->LODModels(CurrentLODLevel).RigidIndexBuffer), 0 ); // NON-dynamic index buffer.
		FVertexStream*	VertexStreams[1] = { &SkelMesh->LODModels(CurrentLODLevel).RigidVertexStream };
		RI->SetVertexStreams( VS_FixedFunction, VertexStreams, 1 );		
				
		TArray<FMatrix> FinalMatrices;		
		FinalMatrices.Add( SpaceBases.Num() );

		for( INT s=0; s< SpaceBases.Num(); s++ ) 
		{
			// Perform the transform recombine that effectively accomplishes: NewTrafo = MInst->SpaceBases(s).ApplyPivot(Mesh->RefBases(s).PivotInverse());			
			FCoords& SpaceBaseRef   = SpaceBases(s);
			FCoords& SpaceCopy      = SkelMesh->RefBasesInverse(s); 
			FMatrix& SpaceFinal     = FinalMatrices(s);

			FVector XAxis = SpaceCopy.XAxis * SpaceBaseRef.XAxis.X + SpaceCopy.YAxis * SpaceBaseRef.XAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.XAxis.Z;
			FVector YAxis = SpaceCopy.XAxis * SpaceBaseRef.YAxis.X + SpaceCopy.YAxis * SpaceBaseRef.YAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.YAxis.Z;
			FVector ZAxis = SpaceCopy.XAxis * SpaceBaseRef.ZAxis.X + SpaceCopy.YAxis * SpaceBaseRef.ZAxis.Y + SpaceCopy.ZAxis * SpaceBaseRef.ZAxis.Z;			
			
			SpaceFinal.M[3][0] = SpaceBaseRef.Origin.X + ( SpaceBaseRef.XAxis | SpaceCopy.Origin );
			SpaceFinal.M[3][1] = SpaceBaseRef.Origin.Y + ( SpaceBaseRef.YAxis | SpaceCopy.Origin );
			SpaceFinal.M[3][2] = SpaceBaseRef.Origin.Z + ( SpaceBaseRef.ZAxis | SpaceCopy.Origin );
			SpaceFinal.M[3][3] = 1.0f;	

			SpaceFinal.M[0][0] = XAxis.X;
			SpaceFinal.M[0][1] = YAxis.X;
			SpaceFinal.M[0][2] = ZAxis.X;
			SpaceFinal.M[0][3] = 0.0f;
			SpaceFinal.M[1][0] = XAxis.Y;
			SpaceFinal.M[1][1] = YAxis.Y;
			SpaceFinal.M[1][2] = ZAxis.Y;
			SpaceFinal.M[1][3] = 0.0f;
			SpaceFinal.M[2][0] = XAxis.Z;
			SpaceFinal.M[2][1] = YAxis.Z;
			SpaceFinal.M[2][2] = ZAxis.Z;
			SpaceFinal.M[2][3] = 0.0f;
		}

		UMaterial*	Material = NULL;
		// Do any special set-up on the overlay material
		InitOverlayMaterial(Actor->OverlayMaterial, Actor->ClientOverlayCounter);	

		INT LastMaterialIndexSet = -1;
		INT LastTrafoJointSet = -1;
		for( INT SectionIndex = 0; SectionIndex < SkelMesh->LODModels(CurrentLODLevel).RigidSections.Num(); SectionIndex++ )
		{								
			FSkelMeshSection& Section = SkelMesh->LODModels(CurrentLODLevel).RigidSections(SectionIndex);						
			INT JointIndex = Section.MaxInfluences; // Re-used as bone index.
			
			// Joint shared between sections - possible when materials differ.
			if( LastTrafoJointSet != JointIndex && SpaceBases.Num() > JointIndex )
			{
				LastTrafoJointSet = JointIndex;
				const FMatrix& SectionToWorldMatrix = FinalMatrices( JointIndex ) * MeshToWorldMatrix; 
				RI->SetTransform( TT_LocalToWorld, SectionToWorldMatrix );				
			}

			if( Section.TotalFaces )
			{						
				// Materials shared between sections - possible when joints differ.
				if( Section.MaterialIndex != LastMaterialIndexSet ) 
				{
					LastMaterialIndexSet = Section.MaterialIndex;
					Material = GetMaterial( SkelMesh->MeshMaterials(Section.MaterialIndex).MaterialIndex, Actor );
					if( bDrawWireframe ) 
					{
						DECLARE_STATIC_UOBJECT
						(
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
						RI->SetMaterial( ApplyOverlayMaterial(Material, Actor->OverlayMaterial, (Actor->Level->DetailMode == DM_Low) ? NULL : Actor->HighDetailOverlay) );			
					}					
				}

				// Draw the section.
				RI->DrawPrimitive
					( 
					PT_TriangleList,                              // 
					Section.FirstIndex,					          //  First index of this section in the index buffer.
					Section.TotalFaces,                           //  Total faces to be drawn.
					Section.MinIndex,							  //  Lowest used index into the vertex buffer for this section.
					Section.MaxIndex                              //  Highest used index into the vertex buffer for this section.
					);
				
				// Render the projectors hitting the section.
				AActor* ProjectorBase = Owner->Actor->GetProjectorBase();
				if( (ProjectorBase->Projectors.Num() || Projectors) && !SceneNode->Viewport->IsWire() && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Projectors) )
				{
					//#SKEL!-> Move shadow drawing into a per-material re-render instead of per rigidsection- saves material state changes..
					LastMaterialIndexSet = -1; 

					RI->PushState();
					RI->SetZBias(1);
					RI->EnableLighting(0,0);
					for( INT i=0;i<ProjectorBase->Projectors.Num();i++ )
					{
						FProjectorRenderInfo* P = ProjectorBase->Projectors(i);
						if( !P->Render( Owner->Actor->Level->TimeSeconds ) )
						{
							ProjectorBase->Projectors.Remove(i--);
							continue;
						}
						if( (ExtraFlags&PF_Unlit) && !(P->ProjectorFlags&PRF_ProjectOnUnlit) )
							continue;

						// Setup blending.
						RI->SetMaterial(P->GetMaterial(SceneNode,Material));

						RI->DrawPrimitive
						( 
							PT_TriangleList,
							Section.FirstIndex,
							Section.TotalFaces,
							Section.MinIndex,
							Section.MaxIndex
						);
					}

					// Render the dynamic projectors hitting the skeletal mesh section.
					for(TList<FProjectorRenderInfo*>* ProjectorList = Projectors;ProjectorList;ProjectorList = ProjectorList->Next)
					{
						FProjectorRenderInfo*	Projector = ProjectorList->Element;

						if(!Projector->Render( Owner->Actor->Level->TimeSeconds ) || ((ExtraFlags & PF_Unlit) && !(Projector->ProjectorFlags & PRF_ProjectOnUnlit)))
							continue;

						if( Projector->Projector->bNoProjectOnOwner && Projector->Projector->Owner && (Projector->Projector->Owner == Actor) )
							continue;

						// Render the projector.
						RI->SetMaterial(Projector->GetMaterial(SceneNode,Material));

						RI->DrawPrimitive
						(
							PT_TriangleList,
							Section.FirstIndex,
							Section.TotalFaces,
							Section.MinIndex,
							Section.MaxIndex
						);
					}
					RI->PopState();				
				}
				// end of section.
			}		
		}		
	}

	unclock(RenderCycles); 
	GStats.DWORDStats( GEngineStats.STATS_Mesh_SkelCycles ) += RenderCycles;
	
	//
    // Debug bone/normal/influence drawing code ( all unlit )
	//

	// Draw skeleton (REND BONE console command) for debugging purposes.
	if( DrawBones )
	{
		RI->SetTransform(TT_LocalToWorld,MeshToWorldMatrix); 

		FLineBatcher LineBatcher(RI);

		FVector* Pivots  = &DebugPivots(0);
		INT* ParentIndex = &DebugParents(0);

		// Draw bones - use same trafo already set for the mesh.
		FColor BoneColor = FColor( Actor->bSelected ? FPlane( .3f,1.2f,.3f,0.f ) : FPlane( 2.f,2.f,2.f, 0.f ) );
		for( INT b=0; b< DebugPivots.Num(); b++)
		{
			// Pivots:
			// From each child to its parent.
			if( ParentIndex[b] != b )
			{
				FVector B1 = Pivots[b]; //.TransformPointBy(LocalCoords);
				FVector B2 = Pivots[ParentIndex[b]];//.TransformPointBy(LocalCoords);				
				LineBatcher.DrawLine(B1,B2,BoneColor);
			}			
		}

		// Draw the 'umbilical' line from the mesh-space-origin to the root-bone.
		// TODO: mark the origin with a little 'actor space origin' axes system.
		if( DebugPivots.Num() )
		{			
			FVector B1 =  FVector(0.0f,0.0f,0.0f);
			FVector B2 =  Pivots[0];
			LineBatcher.DrawLine( B1,B2, FColor( 220,0,245,0 ) );
		}

		LineBatcher.Flush();
		RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);

		FColor RedColor   = FColor( FPlane(1.0f,0.0f,0.0f,0.f) );
		FColor GreenColor = FColor( FPlane(0.0f,1.0f,0.0f,0.f) );
		FColor BlueColor  = FColor( FPlane(0.0f,0.0f,1.0f,0.f) );

		// Draw coordinate axi
		for( INT b=0; b< DebugPivots.Num(); b++)
		{		
			if( ( ParentIndex[b] != b ) && SpaceBases.Num()>= DebugPivots.Num() )
			{
				FVector B1 = Pivots[b]; 

				// Now for each node, draw a tiny XYZ axis system.
				const FMatrix& JointMatrix = SpaceBases(b).Matrix() * MeshToWorldMatrix;
				FVector B2;
				B1 = MeshToWorldMatrix.TransformFVector( B1 );

				// X-axis - red
				FVector Axis = FVector(1.0f,0.0f,0.0f);
				Axis = JointMatrix.TransformNormal( Axis ); // Axis.TransformVectorBy( JointTrafo );   
				Axis.Normalize();									
				B2 = B1 + Axis * 7.0f; 
				LineBatcher.DrawLine(B1,B2,RedColor);

				// Y axis - green				
				Axis = FVector(0.0f,1.0f,0.0f);
				Axis = JointMatrix.TransformNormal( Axis ); // Axis.TransformVectorBy( JointTrafo ); 				
				Axis.Normalize();
				B2 = B1 + Axis * 7.0f;
				LineBatcher.DrawLine(B1,B2,GreenColor);		

				// Z axis - blue				
				Axis = FVector(0.0f,0.0f,1.0f);
				Axis = JointMatrix.TransformNormal( Axis ); // Axis.TransformVectorBy( JointTrafo ); 				
				Axis.Normalize();
				B2 = B1 + Axis * 7.0f;
				LineBatcher.DrawLine(B1,B2,BlueColor);				
			}
		}
		LineBatcher.Flush();
		RI->SetTransform(TT_LocalToWorld,MeshToWorldMatrix); // Restore.

	}

	// Debug box drawing.
	if( SceneNode && SceneNode->Viewport->bShowBounds )
	{		
		FLineBatcher LineBatcher(RI);
		FBox BoundBox = Owner->BoundingBox; // Bounding box in world space.
		RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
		LineBatcher.DrawBox( BoundBox, FColor(72,90,255) );
		LineBatcher.DrawCircle( Owner->BoundingSphere, FVector(1,0,0), FVector(0,1,0), FColor(72,90,255), Owner->BoundingSphere.W, 16 );
		LineBatcher.DrawCircle( Owner->BoundingSphere, FVector(1,0,0), FVector(0,0,1), FColor(72,90,255), Owner->BoundingSphere.W, 16 );
		LineBatcher.DrawCircle( Owner->BoundingSphere, FVector(0,1,0), FVector(0,0,1), FColor(72,90,255), Owner->BoundingSphere.W, 16 );
		LineBatcher.Flush();
	}
	
	FVector UseScale = SkelMesh->Scale * Actor->DrawScale3D * Actor->DrawScale;

	// Collision geometry drawing. Only supported if draw scale is uniform.
	if( UseScale.IsUniform() &&
		SceneNode && 
		SceneNode->Viewport && 
		SceneNode->Viewport->Actor && 
		SceneNode->Viewport->Actor->ShowFlags & SHOW_Collision )
	{
		FLineBatcher LineBatcher(RI);
		RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);

		for( INT i=0; i<SkelMesh->BoneCollisionSpheres.Num(); i++ )
		{
			FSkelBoneSphere* s = &SkelMesh->BoneCollisionSpheres(i);

			// Find the bone this collision sphere is for.
			INT BoneIx = MatchRefBone(*(s->BoneName));
			if(BoneIx == INDEX_NONE)
				continue;

			FMatrix BoneMatrix = SpaceBases(BoneIx).Matrix() * MeshToWorldMatrix; // Bone -> World TM
			FMatrix BoneMatrix2 = SpaceBases(BoneIx).Matrix().Inverse() * MeshToWorldMatrix;

			BoneMatrix.M[3][0] = BoneMatrix2.M[3][0];
			BoneMatrix.M[3][1] = BoneMatrix2.M[3][1];
			BoneMatrix.M[3][2] = BoneMatrix2.M[3][2];

			BoneMatrix.RemoveScaling();

			FVector WorldCollOrigin = BoneMatrix.TransformFVector( s->Offset * UseScale ); // Transform collision prim into world space
			
			FColor CollisionColor = CalcCollisionPrimRenderColor(s->bBlockKarma, s->bBlockZeroExtent, s->bBlockNonZeroExtent);

			// Draw 2 circles to indicate sphere
			LineBatcher.DrawCircle( WorldCollOrigin, FVector(1,0,0), FVector(0,1,0), CollisionColor, s->Radius * UseScale.X, 16 );
			LineBatcher.DrawCircle( WorldCollOrigin, FVector(1,0,0), FVector(0,0,1), CollisionColor, s->Radius * UseScale.X, 16 );
			LineBatcher.DrawCircle( WorldCollOrigin, FVector(0,1,0), FVector(0,0,1), CollisionColor, s->Radius * UseScale.X, 16 );
			LineBatcher.Flush();
		}

		for( INT i=0; i<SkelMesh->BoneCollisionBoxes.Num(); i++ )
		{
			FSkelBoneBox* b = &SkelMesh->BoneCollisionBoxes(i);

			// Find the bone this collision sphere is for.
			INT BoneIx = MatchRefBone(*(b->BoneName));
			if(BoneIx == INDEX_NONE)
				continue;

			FMatrix BoneMatrix = SpaceBases(BoneIx).Matrix() * MeshToWorldMatrix; // Bone -> World TM
			FMatrix BoneMatrix2 = SpaceBases(BoneIx).Matrix().Inverse() * MeshToWorldMatrix;

			BoneMatrix.M[3][0] = BoneMatrix2.M[3][0];
			BoneMatrix.M[3][1] = BoneMatrix2.M[3][1];
			BoneMatrix.M[3][2] = BoneMatrix2.M[3][2];

			BoneMatrix.RemoveScaling();

			// Draw box (transform from bone space to world space)
			FVector	B[2],P,Q;
			int j,k;

			// Top left/bottom right corner.
			B[0] = (b->Offset - b->Radii) * UseScale;
			B[1] = (b->Offset + b->Radii) * UseScale;

			FColor CollisionColor = CalcCollisionPrimRenderColor(b->bBlockKarma, b->bBlockZeroExtent, b->bBlockNonZeroExtent);

			for( j=0; j<2; j++ ) 
			{
				for( k=0; k<2; k++ )
				{
					P.X=B[j].X; Q.X=B[j].X;
					P.Y=B[k].Y; Q.Y=B[k].Y;
					P.Z=B[0].Z; Q.Z=B[1].Z;
					LineBatcher.DrawLine( BoneMatrix.TransformFVector(P), BoneMatrix.TransformFVector(Q), CollisionColor );

					P.Y=B[j].Y; Q.Y=B[j].Y;
					P.Z=B[k].Z; Q.Z=B[k].Z;
					P.X=B[0].X; Q.X=B[1].X;
					LineBatcher.DrawLine( BoneMatrix.TransformFVector(P), BoneMatrix.TransformFVector(Q), CollisionColor );

					P.Z=B[j].Z; Q.Z=B[j].Z;
					P.X=B[k].X; Q.X=B[k].X;
					P.Y=B[0].Y; Q.Y=B[1].Y;
					LineBatcher.DrawLine( BoneMatrix.TransformFVector(P), BoneMatrix.TransformFVector(Q), CollisionColor );
				}
			}

			LineBatcher.Flush();
		}

		UStaticMesh* StaticMesh = SkelMesh->CollisionStaticMesh;
		if (StaticMesh)
		{
			// Set the local to world transform.

			RI->SetTransform(TT_LocalToWorld,Owner->LocalToWorld);

			// Determine the wireframe color.

			UEngine*	Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
			FColor		WireColor = Actor->IsA(AMover::StaticClass()) ? Engine->C_Mover : Engine->C_StaticMesh;

			RI->EnableLighting(1,0,0,NULL,0,Owner->BoundingSphere);

			if(Actor->bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
				RI->SetAmbientLight(WireColor);
			else
				RI->SetAmbientLight(FColor(WireColor.Plane() * 0.5f));

			static FSolidColorTexture	WhiteTexture(FColor(255,255,255));

			DECLARE_STATIC_UOBJECT( UProxyBitmapMaterial, HACKGAH, { HACKGAH->SetTextureInterface(&WhiteTexture); } );
			DECLARE_STATIC_UOBJECT( UFinalBlend, LineMaterial, { LineMaterial->Material = HACKGAH; } );

			RI->SetMaterial(LineMaterial);

			// Set the vertex stream and index buffer.

			FVertexStream*	VertexStreams[1] = { &StaticMesh->VertexStream };

			RI->SetVertexStreams(VS_FixedFunction,VertexStreams,1);
			RI->SetIndexBuffer(&StaticMesh->WireframeIndexBuffer,0);

			// Draw the wireframe static mesh.

			RI->DrawPrimitive(
				PT_LineList,
				0,
				StaticMesh->WireframeIndexBuffer.Indices.Num() / 2,
				0,
				StaticMesh->VertexStream.Vertices.Num() - 1
				);
		}
	}

#ifdef WITH_KARMA
	if(SkelMesh->KPhysicsProps)
	{
		// For in-game, draw the actual model
		McdModelID model = Actor->getKModel();
		if( model )
		{
			KModelDraw(model, KGData->DebugDrawOpt, KLineDraw);
		}
		// This is mostly for editor where we dont instance the model
		else if( SceneNode && SceneNode->Viewport && SceneNode->Viewport->Actor )
		{
			RI->SetTransform( TT_LocalToWorld, Actor->LocalToWorld() );
			SkelMesh->KPhysicsProps->Draw(RI, SceneNode->Viewport->Actor->ShowFlags);
		}
	}
#endif

	// Normals/influence-colors drawing.
	if( bDisplayNormals && DebugInfluences.Num() && !SceneNode->Viewport->IsOrtho())
	{		
		// Skin may not have been computed yet:
		if( !DrawSkin ) 
		{
			// Ensure fresh "DebugInfluences" are computed.
			if( SkelMesh->LODModels.Num() && ( SkelMesh->LODModels(CurrentLODLevel).SmoothStreamWedges > 0 ) )
			{
				TArray <FAnimMeshVertex> NewVerts;
				INT VertCount = SkelMesh->LODModels(CurrentLODLevel).SmoothStreamWedges;
				NewVerts.AddZeroed( VertCount );																
				ComputeSkinVerts( SkelMesh, this, &NewVerts(0), sizeof(FAnimMeshVertex), VertCount );
				NewVerts.Empty();
			}
		}

		FLineBatcher LineBatcher(RI);
		//RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
		RI->SetTransform(TT_LocalToWorld, MeshToWorldMatrix);
		for(INT i=0; i<DebugInfluences.Num(); i++)
		{
			// Original 3d index: gleam from SkelMesh->Wedges.				
			INT VertInfluences = DebugInfluences(i).InfluenceCount;

			FColor Color;
			if( VertInfluences==1 )
				Color = FPlane( 0.f,0.8f,0.f,0.f );  // Green for single influence.
			else if( VertInfluences==2 )
				Color = FPlane( 0.8f,0.2f,0.f,0.f ); // Red for two.
			else if( VertInfluences==3 )
				Color = FPlane( 0.8f,0.f,1.f,0.f );  // Pink for three.
			else if( VertInfluences==4 )				
				Color = FPlane(0.15f,0.75f,1.f,0.f); // Light blue.
			else if( VertInfluences>=5 )
				Color = FPlane(0.9f,0.9f,0.9f,0.f);  // White for 5 +...  using this many links is often unnecessary.
							
			// Draw small influence-colored normal linesegments.
			// #TODO - scale the normals to a reasonable screensize regardless of mesh distance ?						
			FVector &Point = DebugInfluences(i).Vertex;
			FVector Norm =  DebugInfluences(i).Normal; 
			// Normalize and scale down a bit. // Norm.Normalize();
			FLOAT SquareSum = Norm.X*Norm.X+Norm.Y*Norm.Y+Norm.Z*Norm.Z;
			if( SquareSum >= SMALL_NUMBER )
			{
				FLOAT Scale = 0.35f/appSqrt(SquareSum);
				Norm.X *= Scale; 
				Norm.Y *= Scale; 
				Norm.Z *= Scale;
			}
			LineBatcher.DrawLine(Point,Point+Norm,Color);
			// Draw little squares in wireframe mode.
			// if( bDrawWireframe )LineBatcher.DrawPoint( SceneNode, Point, Color );											
		}						
		LineBatcher.Flush();
	} 		

	// Actor's light debugging
	// #SKEL - this bone # criterium helps focus on player lighting - still a hack, though.	
	if( SceneNode->Viewport->Actor->GetLevel()->Engine->bShowLightStats && (NumHardwareLights>0) && (SkelMesh->RefSkeleton.Num() > 30) &&  SceneNode && SceneNode->Viewport ) 
	{
		if( Blends.Num() && SceneNode->Viewport->Canvas )
		{
			UCanvas* Canvas = SceneNode->Viewport->Canvas;
			FString	LightString;			
			LightString = FString::Printf(TEXT(" Lighting - mesh:%s actor:%s sphereradius %f "), SkelMesh->GetName(), AnimOwner->GetName(), LightSphere.W ); //#SKEL
			
			Canvas->CurY += 16; //skip line

			DrawStat(Canvas, FColor(255,255,0),4,TEXT("%s"),*LightString); //Title string

			Canvas->CurY += 3;
			INT Number = 0;
			INT LightsLimit = Actor->MaxLights; 
			INT	NumHardwareLights = 0;

			for(TList<FDynamicLight*>* LightList = Lights;LightList && NumHardwareLights < LightsLimit;LightList = LightList->Next)
			{				
				if( LightList->Element && LightList->Element->Actor )
				{
					FLOAT OurDistance =  FDist( LightList->Element->Position, (FVector)LightSphere ); // Actor->Location );
					
					LightString = FString::Printf(TEXT("[%i]{%2i}(%s) Rad %4.4f Col %3.2f %3.2f %3.2f Alph %4.4f Dst %4.5f Ky %i br %3.2f"), 
						        Number, 
								LightList->Element->Actor->LightEffect,
								LightList->Element->Actor->GetName(), 
								LightList->Element->Radius,
								LightList->Element->Color.X,
								LightList->Element->Color.Y,
								LightList->Element->Color.Z,
								LightList->Element->Alpha,
								OurDistance,								
								LightList->Element->SortKey,
								LightList->Element->Actor->LightBrightness
								);

					DrawStat(Canvas,FColor(255,255,0),4,TEXT("%s"),*LightString);
				}
				Number++;
			}			
		}
	}

	unguard; 
}

// 
// Copy animation from one channel to another
//
void USkeletalMeshInstance::CopyAnimation(INT SrcChannel, INT DestChannel)
{
	guard(USkeletalMeshInstance::CopyAnimation);	

	if( (SrcChannel < 0) || (SrcChannel >= Blends.Num()) || (DestChannel < 0) )
		return;
	if( !ValidateAnimChannel( DestChannel ) )
		return;

	MeshAnimChannel *SrcBlend  = &Blends(SrcChannel);
	MeshAnimChannel *DestBlend = &Blends(DestChannel);
		
	//DestBlend = SrcBlend;
	
	DestBlend->AnimSequence  = SrcBlend->AnimSequence;
	DestBlend->MeshAnimIndex = SrcBlend->MeshAnimIndex;
	
	DestBlend->AnimRate = SrcBlend->AnimRate;
	DestBlend->OrigRate = SrcBlend->OrigRate;
	DestBlend->AnimFrame = SrcBlend->AnimFrame;
	DestBlend->BlendAlpha = SrcBlend->BlendAlpha;
	DestBlend->AnimLast = SrcBlend->AnimLast;
	DestBlend->bAnimNotify = SrcBlend->bAnimNotify;
	DestBlend->bAnimFinished = SrcBlend->bAnimFinished;
	DestBlend->bAnimLoop = SrcBlend->bAnimLoop;
	DestBlend->TweenRate = SrcBlend->TweenRate;
	DestBlend->OldAnimRate = SrcBlend->OldAnimRate;	

	DestBlend->bTimedBlend = SrcBlend->bTimedBlend;
	DestBlend->BlendTargetAlpha = SrcBlend->BlendTargetAlpha;
	DestBlend->BlendTargetInterval = SrcBlend->BlendTargetInterval;
		
	unguard;
}


//
// SetAnimRate - multiplies the animator-defined rate.
//
void USkeletalMeshInstance::SetAnimRate(INT Channel, FLOAT NewRate)
{
	guard(USkeletalMeshInstance::SetAnimRate);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return;
	MeshAnimChannel *Blend = &Blends(Channel);
	
	//debugf(TEXT(">>SetAnimRATE: Channel %i  Rate %f  Mesh: %s  Origrate: %f OldAnimRate %f"),Channel,NewRate,GetMesh()->GetName(),Blend->OrigRate,Blend->OldAnimRate ); 

	Blend->AnimRate = NewRate * Blend->OrigRate; 
	unguard;
}

//
// ForceAnimRate - overrides whatever the animation's rate was.
//
void USkeletalMeshInstance::ForceAnimRate(INT Channel, FLOAT NewRate)
{
	guard(USkeletalMeshInstance::ForceAnimRate);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return;
	MeshAnimChannel *Blend = &Blends(Channel);
	
	Blend->AnimRate = NewRate; 
	unguard;
}

void USkeletalMeshInstance::SetBlendAlpha(INT Channel, FLOAT NewAlpha)
{
	guard(USkeletalMeshInstance::SetBlendAlpha);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return;
	MeshAnimChannel *Blend = &Blends(Channel);

	Blend->BlendAlpha = Clamp(NewAlpha,0.f,1.f);
	unguard;
}

FLOAT USkeletalMeshInstance::GetBlendAlpha(INT Channel)
{
	guard(USkeletalMeshInstance::GetBlendAlpha);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return 0.f;
	MeshAnimChannel *Blend = &Blends(Channel);
	return Blend->BlendAlpha;
	unguard;
}

void USkeletalMeshInstance::SetAnimFrame(INT Channel, FLOAT NewFrame, INT UnitFlag )
{
	guard(USkeletalMeshInstance::SetAnimFrame);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return;
	MeshAnimChannel *Blend = &Blends(Channel);

	if( UnitFlag == 0)
	{
		// NewFrame in 0-1 range
		Blend->AnimFrame = NewFrame;
	}
	else
	{
		// NewFrame in 0-(TotalFrames-1) range, where the interfalc (TotalFrames-2)  up to (TotalFrames-1) cycles the pose from the last frame to the first..
		FLOAT NumFrames = AnimGetFrameCount( GetAnimNamed( Blend->AnimSequence ) );		
		Blend->AnimFrame = (NumFrames >= 1.0f) ? NewFrame/(NumFrames-1.0f) : NewFrame ;
	}
	unguard;
}

FLOAT USkeletalMeshInstance::GetAnimFrame(INT Channel)
{
	guard(USkeletalMeshInstance::GetAnimFrame);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return 0.f;
	MeshAnimChannel *Blend = &Blends(Channel);
	return Blend->AnimFrame;
	unguard;
}

//
// Change the blendalpha in the specified channel by StepSize towards the GoalAlpha
//
void USkeletalMeshInstance::UpdateBlendAlpha(INT Channel, FLOAT GoalAlpha, FLOAT StepSize)
{
	guard(USkeletalMeshInstance::UpdateBlendAlpha);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return;
	MeshAnimChannel *Blend = &Blends(Channel);

	if ( Abs(Blend->BlendAlpha - GoalAlpha) <= StepSize )
	{
		Blend->BlendAlpha = GoalAlpha;
		return;
	}
	if ( GoalAlpha > Blend->BlendAlpha )
		Blend->BlendAlpha += StepSize;
	else
		Blend->BlendAlpha -= StepSize;
	unguard;
}

void USkeletalMeshInstance::SetAnimSequence(INT Channel, FName NewSequence)
{
	guard(USkeletalMeshInstance::SetAnimSequence);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return;
	MeshAnimChannel *Blend = &Blends(Channel);

	Blend->AnimSequence = NewSequence;	
	HMeshAnim InAnim = GetAnimNamed( NewSequence );
	// Important to update OrigRate with _any_ change of aequence being played.
	if( InAnim )
		Blend->OrigRate = AnimGetRate( InAnim) / AnimGetFrameCount( InAnim );
	else 
		Blend->OrigRate = 0.0f; 

	unguard;
}

FName USkeletalMeshInstance::GetAnimSequence(INT Channel)
{
	guard(USkeletalMeshInstance::GetAnimSequence);

	if( (Channel < 0) || (Channel >= Blends.Num()) )
		return NAME_None;
	MeshAnimChannel *Blend = &Blends(Channel);
	return Blend->AnimSequence;
	unguard;
}


void USkeletalMeshInstance::BlendToAlpha(INT Channel, FLOAT TargetAlpha, FLOAT TimeInterval)
{
	guard(USkeletalMeshInstance::BlendToAlpha)
	if( ((Channel < 0) || (Channel >= Blends.Num())) && (TimeInterval >= 0.f) )
		return;

	MeshAnimChannel *Blend = &Blends(Channel);
	Blend->BlendTargetAlpha = TargetAlpha;
	Blend->BlendTargetInterval = TimeInterval;
	Blend->bTimedBlend = true;
	
	unguard;
}

UBOOL USkeletalMesh::SetAttachmentLocation(AActor * Actor, AActor *AttachedActor)
{
	guard(USkeletalMesh::SetAttachmentLocation)

	// AttachmentBone has FName of the (alias of) the bone of the base parent, to which - if matched - it will be attached to.
	UBOOL FoundAttachSpot = false;
	FCoords AttachCoords;
	FCoords AttachTagCoords;

	//debugf(TEXT("Attachment bone plug: %s"),*(AttachedActor->AttachmentBone));
	INT TagIndex = TagAliases.FindItemIndex(AttachedActor->AttachmentBone);	
											
	// Try to get an attachment FCoords and adjustment FCoords if available.
	if( TagIndex != INDEX_NONE )
	{												
		FName RealBone = TagNames(TagIndex);
		INT RealIndex = -1;
		for( int n=0; n< RefSkeleton.Num(); n++)
		{
			if( RealBone == RefSkeleton(n).Name )
			{
				RealIndex = n;
				break;
			}
		}

		if( RealIndex > -1 )
		{												
			MeshGetInstance(Actor); 
			USkeletalMeshInstance *MInst= ((USkeletalMeshInstance*)Actor->MeshInstance);							
			if( MInst->SpaceBases.Num() > RealIndex ) 
			{
				// The per-tag adjustment. 
				AttachCoords = MInst->SpaceBases(RealIndex);
				AttachTagCoords = TagCoords(TagIndex);
				FoundAttachSpot = true;
			}
		}						
	}
	else // Attach to a 'raw' bone if present.
	{
		INT RealIndex = -1;
		for( int n=0; n< RefSkeleton.Num(); n++)
		{
			if( AttachedActor->AttachmentBone == RefSkeleton(n).Name )
			{
				RealIndex = n;
				break;
			}
		}
		if( RealIndex > -1 )
		{
			MeshGetInstance(Actor); 
			USkeletalMeshInstance *MInst= ((USkeletalMeshInstance*)Actor->MeshInstance);

			if( MInst->SpaceBases.Num() > RealIndex ) 
			{
				// The per-tag adjustment. 
				AttachCoords = MInst->SpaceBases(RealIndex); 
				AttachTagCoords = GMath.UnitCoords;
				FoundAttachSpot = true;
			}							
		}												
	}

	if( FoundAttachSpot )
	{				
		// 'Bone link' responsible for updating location/rotation of the attachment just before rendering it. 
		USkeletalMeshInstance *MInst= ((USkeletalMeshInstance*)Actor->MeshInstance);
		
		// Apply the per-tag adjustment. 
		FCoords LinkupCoords = AttachTagCoords.ApplyPivot( AttachCoords );

		// Add Actor's ->RelativeRotation and ->RelativeLocation too..
		FVector  RelLoc = AttachedActor->RelativeLocation.TransformVectorBy( LinkupCoords);
		//FRotator RelRot = AttachedActor->RelativeRotation;
		FCoords  RelRotCoords = GMath.UnitCoords / AttachedActor->RelativeRotation;

		// Handle location and rotation separately 
		if( AttachedActor->bCollideActors && AttachedActor->GetLevel()->Hash )  
			AttachedActor->GetLevel()->Hash->RemoveActor( AttachedActor );

		LinkupCoords.Origin += RelLoc; 
		FVector WorldBoneLocation = ( LinkupCoords * MInst->CachedMeshTrafo ).Origin;
		AttachedActor->Location = WorldBoneLocation;

		LinkupCoords *= RelRotCoords;
		FCoords WorldBoneCoords = LinkupCoords.Transpose()*MInst->CachedMeshTrafo;
		AttachedActor->Rotation = WorldBoneCoords.OrthoRotation();								

		if( AttachedActor->bCollideActors && AttachedActor->GetLevel()->Hash )
			AttachedActor->GetLevel()->Hash->AddActor( AttachedActor );
		return true;
	}
						
	debugf(TEXT("%s Attachment not found for bone %s to Attach %s "),GetName(), *(AttachedActor->AttachmentBone), AttachedActor->GetName() );
	return false;
	unguard;
}


/*-----------------------------------------------------------------------------
	AnimNotify subclasses
-----------------------------------------------------------------------------*/
//
// UAnimNotify
//
void UAnimNotify::PostEditChange()
{
	guard(UAnimNotify::PostEditChange);
	Revision++;
	unguard;
}
IMPLEMENT_CLASS(UAnimNotify);

//
// UAnimNotify_Effect
//
void UAnimNotify_Effect::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_Effect::Notify);

	if( EffectClass )
	{
		if( GIsEditor )
			debugf( TEXT("UAnimNotify_Effect: spawning %s"), EffectClass->GetName() );

		FVector Location = Owner->Location;
		FRotator Rotation = Owner->Rotation;

		USkeletalMeshInstance* SkelInstance = Cast<USkeletalMeshInstance>(Instance);
		if( Bone!=NAME_None )
		{
			// Spawn at bone location but don't attach.
			if( SkelInstance && !Attach )
			{
				FCoords BoneCoords = SkelInstance->GetBoneCoords( SkelInstance->MatchRefBone(Bone) );
				FCoords InverseBoneCoords = BoneCoords.Inverse();

				Location = BoneCoords.Origin + OffsetLocation.TransformVectorBy( InverseBoneCoords );				
				Rotation = ((GMath.UnitCoords/OffsetRotation) * InverseBoneCoords).OrthoRotation();
			}
		}
		else
		{
			// Spawn relative to actor location/rotation.
			FCoords OwnerRotationCoords = GMath.UnitCoords * Owner->Rotation;
			Location = Location + OffsetLocation.TransformVectorBy(OwnerRotationCoords);
			Rotation = ((GMath.UnitCoords/OffsetRotation) * OwnerRotationCoords).OrthoRotation();
		}

		AActor* EffectActor = Owner->GetLevel()->SpawnActor( EffectClass, NAME_None, Location, Rotation, NULL, GIsEditor, 0, Owner );
		if( !EffectActor )
			return;

		if( Tag != NAME_None )
			EffectActor->Tag = Tag;
		EffectActor->DrawScale = DrawScale;
		EffectActor->DrawScale3D = DrawScale3D;
		
		// Attach to bone
		if( Attach && SkelInstance && Bone!=NAME_None )
		{
			Owner->AttachToBone( EffectActor, Bone );
			EffectActor->RelativeLocation = OffsetLocation;
			EffectActor->RelativeRotation = OffsetRotation;
		}

		if( GIsEditor )
			LastSpawnedEffect = EffectActor;
	}

	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_Effect);

//
// UAnimNotify_DestroyEffect
//
void UAnimNotify_DestroyEffect::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_DestroyEffect::Notify);
	if( DestroyTag != NAME_None )
	{
		for( INT ActorNum=Owner->GetLevel()->Actors.Num()-1; ActorNum>=0; --ActorNum )
		{
			AActor* Actor = Owner->GetLevel()->Actors(ActorNum);
			if( Actor && 
				Actor->Owner == Owner && 
				Actor->Tag == DestroyTag )
			{
				AEmitter* Emitter;
				if( bExpireParticles && (Emitter=Cast<AEmitter>(Actor)) != NULL )
					Emitter->Kill();
				else
					Owner->GetLevel()->DestroyActor( Actor );
			}
		}
	}
	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_DestroyEffect);

//
// UAnimNotify_Sound
//
void UAnimNotify_Sound::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_Sound::Notify);

	if( GIsEditor && Sound )
		debugf( TEXT("UAnimNotify_Sound: playing %s"), Sound->GetName() );

	if( Sound && Owner && Owner->GetLevel()->Engine->Audio )
		Owner->GetLevel()->Engine->Audio->PlaySound( Owner, SLOT_None, Sound, Owner->GetRootLocation(), Volume, Radius ? Radius : GAudioDefaultRadius, 1.f, SF_RootMotion, 0.f );

	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_Sound);

//
// UAnimNotify_LIPSinc
//
void UAnimNotify_LIPSinc::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_LIPSinc::Notify);

#ifdef WITH_LIPSINC

	USkeletalMeshInstance* SkelInstance = Cast<USkeletalMeshInstance>(Instance);

	if( GIsEditor && SkelInstance )
		debugf( TEXT("UAnimNotify_LIPSinc: playing %s"), *(LIPSincAnimName));

	if( SkelInstance )
		SkelInstance->PlayLIPSincAnim( LIPSincAnimName, Volume, Radius ? Radius : GAudioDefaultRadius, Pitch );

#endif

	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_LIPSinc);

//
// UAnimNotify_Script
//
void UAnimNotify_Script::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_Script::Notify);

	if( NotifyName != NAME_None )
	{
		if( GIsEditor )
		{
			//debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Script %s"), *NotifyName );
		}
		else
		{
			UFunction* Function = Owner->FindFunction( NotifyName );
			if( Function )
				Owner->ProcessEvent( Function, NULL );								
		}
	}

	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_Script);

//
// UAnimNotify_Scripted
//
void UAnimNotify_Scripted::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_Scripted::Notify);

	if( GIsEditor )
	{
		//debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Scripted %s"), GetName() );
	}
	else
		eventNotify( Owner );

	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_Scripted);

//
// AnimNotify_MatSubAction
//
void UAnimNotify_MatSubAction::Notify( UMeshInstance *Instance, AActor *Owner )
{
	guard(UAnimNotify_MatSubAction::Notify);
	
	if( GIsEditor )
	{
		//debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_MatSubAction %s"), GetName() );
	}
	else
	if( SubAction )
	{
		ASceneManager* SceneManager = NULL;
		for( INT i=0;i<Owner->GetLevel()->Actors.Num();i++ )
		{
			AActor* Actor = Owner->GetLevel()->Actors(i);
			if( Actor && !Actor->bDeleteMe )
			{
				ASceneManager* SM = Cast<ASceneManager>(Actor);
				if( SM && SM->bIsRunning )
				{
					SceneManager = SM;
					break;
				}
			}
		}

		if( SceneManager )
		{
			SceneManager->SubActions.AddItem( SubAction );
			SubAction->PctStarting = SceneManager->CurrentTime / SceneManager->TotalSceneTime; 
			SubAction->PctEnding = (SceneManager->CurrentTime + SubAction->Duration) / SceneManager->TotalSceneTime; 
			SubAction->PctDuration = SubAction->PctEnding - SubAction->PctStarting;
			SubAction->Status = SASTATUS_Running;
			//debugf( NAME_Log, TEXT("AnimNotify_MatSubAction %s: attached subaction %s to scene %s"), GetName(), SubAction->GetName(), SceneManager->GetName() );
		}
		else
			debugf( NAME_Log, TEXT("AnimNotify_MatSubAction %s: unable to find current SceneManager"), GetName() );
	}

	unguard;
}
IMPLEMENT_CLASS(UAnimNotify_MatSubAction);


/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

