/*=============================================================================
	UnMesh.cpp: Unreal mesh / meshinstance support.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

  	Revision history:
		* Created by Erik de Neve using Chris Hargrove's mesh-instance concept.

=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"

IMPLEMENT_CLASS(UMeshInstance);

//
// MeshGetInstance: guaranteed to return a valid pointer in all cases.
//


void UMesh::Serialize( FArchive& Ar )
{
	guard(UMesh::Serialize);

	// Serialize parent's variables
	Super::Serialize(Ar);

	// Essential for garbage collection.
	if( !Ar.IsPersistent() ) 
	{
		Ar << DefMeshInstance;
	}
	
	unguardobj;
}


UMeshInstance* UMesh::MeshGetInstance( const AActor* InActor)
{
    UMeshInstance** pInst; 

    if (!InActor)
    {
        // Validate the default instance.
        if( DefMeshInstance )
		{
            return DefMeshInstance;
		}
        // Default instance doesn't exist, so it needs to be created.
		// debugf(TEXT("MeshGetInstance: setting default instance.") ); 
        pInst = &DefMeshInstance; 
    }
    else
    {
        // Check actor validity.
        if ( InActor->bDeleteMe )
		{
			//debugf(TEXT("MeshGetInstance: invalid actor, not constructing meshinstance."));			
			if( DefMeshInstance )
			{
				return DefMeshInstance; 
			}
			//debugf(TEXT("MeshGetInstance: WARNING: setting default instance for invalid or expired actor.") );
			pInst = &DefMeshInstance;
		}
		else
		{
			// Verify validity of existing MeshInstance.
			if (InActor->MeshInstance )
			{
				// Detect whether actor and mesh are still the same.
				if ((InActor->MeshInstance->GetActor()==InActor) && (InActor->MeshInstance->GetMesh()==this))
					return(InActor->MeshInstance);

				// debugf(TEXT("MeshGetInstance: discarding old, creating new one.") ); 
				// Need a new instance.
				if( !(InActor->MeshInstance->GetStatus() & MINST_InUse ) )
				{
					delete InActor->MeshInstance;
				}
				else // If current instance is on the callstack, queue it for later deletion.
				{
					InActor->MeshInstance->SetStatus( MINST_DeleteMe );
				}
			}
			// Create new instance 
			pInst = (class UMeshInstance **) &InActor->MeshInstance;
		}
    }

    // Get the instance class used for construction.
    UClass* InstClass = MeshGetInstanceClass();
    if (!InstClass)
        InstClass = UMeshInstance::StaticClass();

    // Construct the object.
    *pInst = (UMeshInstance*)StaticConstructObject(
        InstClass,
        GetOuter(),
        NAME_None,
        RF_Transient|RF_Public|RF_Standalone,
        InstClass->GetDefaultObject());

    (*pInst)->SetMesh(this);      // Instance's mesh always points back to this mesh.
    (*pInst)->SetActor( (class AActor*) InActor);  // Can be NULL.

    return(*pInst);
}

IMPLEMENT_CLASS(UMesh);


//
// FMeshAnimNotify support
//
FArchive &operator<<( FArchive& Ar, FMeshAnimNotify& N )
{
	if( Ar.Ver() < 112 )
	{
		N.NotifyObject = NULL;
		return Ar << N.Time << N.Function;
	}
	else
		return Ar << N.Time << N.Function << N.NotifyObject;
}

//
// FMeshAnimSeq support
//
void FMeshAnimSeq::UpdateOldNotifies(UObject* Outer)
{
	guard(FMeshAnimSeq::UpdateOldNotifies);
	// Update old notify function names to new UAnimNotify objects.
	for( INT i=0;i<Notifys.Num();i++ )
	{
		if( Notifys(i).Function != NAME_None )
		{
			UAnimNotify_Script* ScriptNotify = ConstructObject<UAnimNotify_Script>( UAnimNotify_Script::StaticClass(),Outer,NAME_None,RF_Public);
			ScriptNotify->NotifyName = Notifys(i).Function;
			
			Notifys(i).NotifyObject = ScriptNotify;
			Notifys(i).Function = NAME_None;
		}
	}
	unguard;
}

//
// FMeshAnimSeq serializer;
//
FArchive &operator<<( FArchive& Ar, FMeshAnimSeq& A )
{
	if( Ar.Ver() >= 115 )
		Ar << A.Bookmark;
	else
		A.Bookmark = 0.f;
	return Ar << A.Name << A.Groups << A.StartFrame << A.NumFrames << A.Notifys << A.Rate;
}

