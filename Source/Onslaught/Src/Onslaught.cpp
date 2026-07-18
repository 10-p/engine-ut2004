/*=============================================================================
	Onslaught.cpp: Native Onslaught specific code
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Dave Hagewood @ Psyonix
=============================================================================*/

#include "OnslaughtPrivate.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Package.
IMPLEMENT_PACKAGE(Onslaught);

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) ONSLAUGHT_API FName ONSLAUGHT_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "OnslaughtClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Import natives
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "OnslaughtClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

void RegisterNamesOnslaught(void)
{
    #define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) extern ONSLAUGHT_API FName ONSLAUGHT_##name; ONSLAUGHT_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "OnslaughtClasses.h"
	#undef DECLARE_NAME
	#undef NAMES_ONLY
}


#if !__STATIC_LINK
struct FOnslaughtInitManager
{
	FOnslaughtInitManager()
	{
		RegisterNamesOnslaught();
	}
} FOnslaughtInitManager;
#endif


/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/

#include "UnNet.h"

#if !FORCE_EXTREME_PACKING   // with this #define, BITFIELD is BYTE. --ryan.
static inline UBOOL NEQ(BITFIELD A,BITFIELD B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
#endif

static inline UBOOL NEQ(BYTE A,BYTE B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(INT A,INT B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(FLOAT& A,FLOAT& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FVector& A,FVector& B,UPackageMap* Map,UActorChannel* Channel) {return ((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] || ((INT*)&A)[2]!=((INT*)&B)[2];}
static inline UBOOL NEQ(FRotator& A,FRotator& B,UPackageMap* Map,UActorChannel* Channel) {return A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;}
static inline UBOOL NEQ(UObject* A,UObject* B,UPackageMap* Map,UActorChannel* Channel) {if( Map->CanSerializeObject(A) )return A!=B; Channel->bActorMustStayDirty = true; 
//debugf(TEXT("%s Must stay dirty because of %s"),Channel->Actor->GetName(),A->GetName());
return (B!=NULL);}
static inline UBOOL NEQ(FName& A,FName B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FColor& A,FColor& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FPlane& A,FPlane& B,UPackageMap* Map,UActorChannel* Channel) {return
((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] ||
((INT*)&A)[2]!=((INT*)&B)[2] || ((INT*)&A)[3]!=((INT*)&B)[3];}
static inline UBOOL NEQ(const FString& A,const FString& B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}

static inline UBOOL NEQ(const FAnimRep& A, const FAnimRep& B,UPackageMap* Map,UActorChannel* Channel)
{
	if ( (A.AnimSequence != B.AnimSequence)
		|| (A.AnimRate != B.AnimRate)
		|| (A.bAnimLoop != B.bAnimLoop) )
	{
		return 1;
	}

	return 0;
}

static inline UBOOL NEQ(const FHitFXData& A, const FHitFXData& B,UPackageMap* Map,UActorChannel* Channel)
{
	if ( A.Bone != B.Bone )
		return 1;

	return 0;
}

static inline UBOOL NEQ(const FCompressedPosition& A, const FCompressedPosition& B,UPackageMap* Map,UActorChannel* Channel)
{
		return 1; // only try to replicate in compressed form if already know location has changed
}


static inline UBOOL NEQ(const FMoverPosition& A, const FMoverPosition& B,UPackageMap* Map,UActorChannel* Channel)
{
		return (A.PositionNumber != B.PositionNumber); 
}

//Onslaught specific
static inline UBOOL NEQ(const FSCarState& A,const FSCarState& B,UPackageMap* Map,UActorChannel* Channel)
{
	return 1;
}

static inline UBOOL NEQ(const FTreadCraftState& A,const FTreadCraftState& B,UPackageMap* Map,UActorChannel* Channel)
{
	return 1;
}

static inline UBOOL NEQ(const FPlaneStateStruct& A,const FPlaneStateStruct& B,UPackageMap* Map,UActorChannel* Channel)
{
	return 1;
}

static inline UBOOL NEQ(const FHoverCraftState& A,const FHoverCraftState& B,UPackageMap* Map,UActorChannel* Channel)
{
	return 1;
}

static inline UBOOL NEQ(const FCopterState& A,const FCopterState& B,UPackageMap* Map,UActorChannel* Channel)
{
	return 1;
}

#define DOREP(c,v) \
	if( NEQ(v,((A##c*)Recent)->v,Map,Channel) ) \
	{ \
		static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
		*Ptr++ = sp##v->RepIndex; \
	}

#define DOREPARRAY(c,v) \
	{static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
	for( INT i=0; i<ARRAY_COUNT(v); i++ ) \
		if( NEQ(v[i],((A##c*)Recent)->v[i],Map,Channel) ) \
				*Ptr++ = sp##v->RepIndex+i;}


INT* AONSWeapon::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSWeapon::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if (StaticClass()->ClassFlags & CLASS_NativeReplication)
	{
		if (bNetDirty && Role == ROLE_Authority)
		{
			if (!bNetOwner)
			{
				DOREP(ONSWeapon,CurrentHitLocation);
				DOREP(ONSWeapon,FlashCount);
			}

			DOREP(ONSWeapon,HitCount);
			DOREP(ONSWeapon,LastHitLocation);
			DOREP(ONSWeapon,bActive);
			DOREP(ONSWeapon,bForceCenterAim);
			DOREP(ONSWeapon,bCallInstigatorPostRender);
			DOREP(ONSWeapon,Team);
		}
	}

	return Ptr;
	unguard;
}

INT* AONSWeaponPawn::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSWeaponPawn::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) && Role == ROLE_Authority)
	{
		DOREP(ONSWeaponPawn,VehicleBase);
		DOREP(ONSWeaponPawn,Gun);
	}

	return Ptr;
	unguard;
}

INT* AONSVehicle::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSVehicle::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) && bNetDirty && Role == ROLE_Authority)
	{
		if (bNetOwner)
			DOREP(ONSVehicle,ActiveWeapon);

		DOREP(ONSVehicle,ExplosionCount);
		DOREP(ONSVehicle,bDestroyAppearance);
		DOREP(ONSVehicle,bDisintegrateVehicle);
	}

	return Ptr;
	unguard;
}

INT* AONSWheeledCraft::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSWheeledCraft::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) && Role == ROLE_Authority)
	{
		if ( bIsAwake || bDemoRecording || (bHasBeenAwake && ((CarState.ChassisPosition != ((AONSWheeledCraft*)Recent)->CarState.ChassisPosition)
											|| (CarState.ChassisLinVel != ((AONSWheeledCraft*)Recent)->CarState.ChassisLinVel))) )
		{
			DOREP(ONSWheeledCraft,CarState);
		}
		if ( bNetInitial )
		{
			DOREP(ONSWheeledCraft,bAllowAirControl);
			DOREP(ONSWheeledCraft,bAllowChargingJump);
			if (bAllowChargingJump)
			{
				DOREP(ONSWheeledCraft,JumpChargeTime);
				DOREP(ONSWheeledCraft,MaxJumpForce);
				DOREP(ONSWheeledCraft,MaxJumpSpin);
			}

			if (bDoStuntInfo)
			{
				DOREP(ONSWheeledCraft,DaredevilThreshInAirDistance);
				DOREP(ONSWheeledCraft,DaredevilThreshInAirTime);
				DOREP(ONSWheeledCraft,DaredevilThreshInAirSpin);
				DOREP(ONSWheeledCraft,DaredevilThreshInAirPitch);
				DOREP(ONSWheeledCraft,DaredevilThreshInAirRoll);
			}
		}
	}
	return Ptr;
	unguard;
}

INT* AONSTreadCraft::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSTreadCraft::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) 
		&& (bIsAwake || bDemoRecording || (bHasBeenAwake && ((TreadState.ChassisPosition != ((AONSTreadCraft*)Recent)->TreadState.ChassisPosition)
				|| (TreadState.ChassisLinVel != ((AONSTreadCraft*)Recent)->TreadState.ChassisLinVel)))) 
		&& Role == ROLE_Authority)
		DOREP(ONSTreadCraft,TreadState);

	return Ptr;
	unguard;
}

INT* AONSPlaneCraft::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSPlaneCraft::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) 
		&& (bIsAwake || bHasBeenAwake || bDemoRecording ) 
		&& Role == ROLE_Authority)
		DOREP(ONSPlaneCraft,PlaneState);

	return Ptr;
	unguard;
}

INT* AONSHoverCraft::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSHoverCraft::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) 
		&& (bIsAwake || bDemoRecording || (bHasBeenAwake && ((HoverState.ChassisPosition != ((AONSHoverCraft*)Recent)->HoverState.ChassisPosition)
				|| (HoverState.ChassisLinVel != ((AONSHoverCraft*)Recent)->HoverState.ChassisLinVel)))) 
		&& Role == ROLE_Authority)
		DOREP(ONSHoverCraft,HoverState);

	return Ptr;
	unguard;
}

INT* AONSChopperCraft::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSChopperCraft::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) 
		&& (bIsAwake || bDemoRecording || (bHasBeenAwake && ((CopState.ChassisPosition != ((AONSChopperCraft*)Recent)->CopState.ChassisPosition)
				|| (CopState.ChassisLinVel != ((AONSChopperCraft*)Recent)->CopState.ChassisLinVel)))) 
		&& Role == ROLE_Authority)
		DOREP(ONSChopperCraft,CopState);

	return Ptr;
	unguard;
}

INT* AONSRVWebProjectile::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSRVWebProjectile::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) && bNetInitial && Role == ROLE_Authority)
	{
		DOREP(ONSRVWebProjectile,Leader);
		DOREP(ONSRVWebProjectile,ProjNumber);
	}

	return Ptr;
	unguard;
}

INT* AONSRVWebProjectileLeader::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	guard(AONSRVWebProjectileLeader::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	if ((StaticClass()->ClassFlags & CLASS_NativeReplication) && bNetInitial && Role == ROLE_Authority)
		DOREP(ONSRVWebProjectileLeader,ProjTeam);

	return Ptr;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/