/*=============================================================================
	UnAnim.h: 

	Revision history:
		* Created by Erik de Neve
		* Moved FMeshAnimNotify/FMeshAnimSeq from UnMesh.h to UnAnim.h
=============================================================================*/

#if FORCE_EXTREME_PACKING
#include "UnForcePacking_begin.h"
#elif SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif


/*-----------------------------------------------------------------------------
	FMeshAnimNotify.
-----------------------------------------------------------------------------*/

// An actor notification event associated with an animation sequence.
struct FMeshAnimNotify
{
	FLOAT	Time;						// Time to occur, 0.0-1.0.
	FName	Function;					//!!OLDVER Name of the actor function to call.
	class UAnimNotify* NotifyObject;	// Notify object.
	friend FArchive &operator<<( FArchive& Ar, FMeshAnimNotify& N );

	FMeshAnimNotify()
		: Time(0.0), Function(NAME_None), NotifyObject(NULL) {}
};

/*-----------------------------------------------------------------------------
	FMeshAnimSeq.
-----------------------------------------------------------------------------*/

// Information about one animation sequence associated with a mesh,
// a group of contiguous frames.
struct FMeshAnimSeq
{
	FName					Name;		// Sequence's name.
	TArray<FName>			Groups;		// Groups.
	INT						StartFrame;	// Starting frame number.
	INT						NumFrames;	// Number of frames in sequence.
	FLOAT					Rate;		// Playback rate in frames per second.
	TArray<FMeshAnimNotify> Notifys;	// Notifications.
	FLOAT					Bookmark;	// Last position in the animation browser.

	void AddGroup(FName NewGroup)
	{
		if( NewGroup != NAME_None )
			Groups.AddUniqueItem(NewGroup);
	}

	friend FArchive &operator<<( FArchive& Ar, FMeshAnimSeq& A );

	FMeshAnimSeq()
		: Name(NAME_None), Groups(), StartFrame(0), NumFrames(0), Rate(30.0f), Notifys(), Bookmark(0.0f) {}

	FMeshAnimSeq& operator=( const FMeshAnimSeq& Other )
	{
		guard(FMeshAnimSeq::operator =);
		this->Name = Other.Name;
		this->Groups = Other.Groups;
		this->StartFrame = Other.StartFrame;
		this->NumFrames = Other.NumFrames;
		this->Rate = Other.Rate;
		this->Notifys = Other.Notifys;		
		this->Bookmark = Other.Bookmark;
		return *this;
		unguard;
	}

	void Erase()
	{
		guard(FMeshAnimSeq::Erase);
		Groups.Empty();
		Notifys.Empty();		
		StartFrame = 0;
		NumFrames = 0;
		Name = NAME_None;
		unguard;
	}	

	void UpdateOldNotifies(UObject *Outer);
};


/*-----------------------------------------------------------------------------
FAnimRep
struct used for replicating animation information to net clients
*/
struct FAnimRep
{
	FName AnimSequence;
	UBOOL bAnimLoop:1 GCC_PACK(4);
	BYTE AnimRate GCC_PACK(4);
	BYTE AnimFrame;
	BYTE TweenRate;
};


#if FORCE_EXTREME_PACKING
#include "UnForcePacking_end.h"
#elif SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

