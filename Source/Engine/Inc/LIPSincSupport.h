/*=============================================================================
	LIPSincSupport.h : LIPSinc Support for Unreal
	
	  Revision history:
		* Created by Jamie Redmond

    Work-in-progress TODO's:
		
=============================================================================*/

#if !defined(_TANIMATIONINFO_)
#define _TANIMATIONINFO_

#include <float.h>  // DBL_EPSILON

// -----------------------------------------------------------------------------
// Constants

static const DOUBLE kLIPSinc2Epsilon = 2.0f * DBL_EPSILON;
static const DOUBLE kLIPSincOneThird = 1.0f / 3.0f;
static const INT kNumLIPSincPhonemes = 43;

// -----------------------------------------------------------------------------
// Enumerations

enum ELIPSincBlendMode
{	
	ELBM_None		 = 0,		// None
	ELBM_BlendIn     = 1,		// Blending into a LIPSinc Animation
	ELBM_NormalBlend = 2,		// Normal LIPSinc Animation Blending
	ELBM_BlendOut    = 3,		// Blending out of a LIPSinc Animation
};

// -----------------------------------------------------------------------------
// Globals

void ENGINE_API GLIPSincInitGame       ( void );
void ENGINE_API GLIPSincShutdownGame   ( void );
void ENGINE_API GLIPSincInitLevel      ( ULevel* level );
void ENGINE_API GLIPSincShutdownLevel  ( void );
void ENGINE_API GLIPSincInitEditor     ( void );
void ENGINE_API GLIPSincShutdownEditor ( void );

void ENGINE_API _Blend( const FVector &ToTrans,
						FQuat		  &ToRot,
						const FVector &FromTrans,
						const FQuat   &FromRot,
						const FQuat   &FromRotInv,
						FVector       &OutTrans,
						FQuat         &OutRot,
						FLOAT         alpha
					  );

class TLIPSincDB; 
extern ENGINE_API TLIPSincDB GLIPSincDB;

// -----------------------------------------------------------------------------
// TLIPSincBase
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincBase
{

public:
	
	INT m_nVersion;
};

// -----------------------------------------------------------------------------
// TLIPSincAnimationKey
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincAnimationKey : public TLIPSincBase
{

public:

	static const INT kTLIPSincAnimationKeyVersion = 2;
		
	// -------------------------------------------------------------
	// Default Constructor

	TLIPSincAnimationKey( )
	: m_nTime     (    0 )
	, m_fValue    ( 0.0f )
	{
		guard(TLIPSincAnimationKey::TLIPSincAnimationKey);
		m_nVersion = kTLIPSincAnimationKeyVersion;
		unguard;
	}

	// -------------------------------------------------------------
	// Constructor

	TLIPSincAnimationKey( 
		INT    time, 
		DOUBLE value )
	: m_nTime     ( time     )
	, m_fValue    ( value    )
	{
		guard(TLIPSincAnimationKey::TLIPSincAnimationKey);
		m_nVersion = kTLIPSincAnimationKeyVersion;
		unguard;
	}
	
	// -------------------------------------------------------------
	// Copy Constructor
		
	TLIPSincAnimationKey(
		const TLIPSincAnimationKey &other )
	: m_nTime     ( other.m_nTime     )
	, m_fValue    ( other.m_fValue    )
	{
		guard(TLIPSincAnimationKey::TLIPSincAnimationKey);
		m_nVersion = other.m_nVersion;
		unguard;
	}

	// -------------------------------------------------------------
	// Destructor

	~TLIPSincAnimationKey( )  // Nothing
	{
		guard(TLIPSincAnimationKey::~TLIPSincAnimationKey);
		unguard;
	}

	// -------------------------------------------------------------
	// Assignment Operator

	TLIPSincAnimationKey& operator=( 
		const TLIPSincAnimationKey &other )
	{
		guard(TLIPSincAnimationKey::operator=);
		
		if ( this != &other )
		{
			m_nTime     = other.m_nTime;
			m_fValue    = other.m_fValue;
			m_nVersion  = other.m_nVersion;
		}

		return *this;

		unguard;
	}
		
	// -------------------------------------------------------------
	// Comparison Operator

	UBOOL operator==(
		const TLIPSincAnimationKey &other )
		const
	{
		guard(TLIPSincAnimationKey::operator==);

		if ( ( m_nTime     != other.m_nTime     ) ||
			 ( m_fValue    != other.m_fValue    )
		   )
		{
			return 0;
		}
		else
		{
			return 1;
		}

		unguard;
	}

	// -------------------------------------------------------------
	// Negative Comparison Operator

	UBOOL operator!=(
		const TLIPSincAnimationKey &other )
		const
	{
		guard(TLIPSincAnimationKey::operator!=);
		return ( !(operator==( other )) );
		unguard;
	}

	// -------------------------------------------------------------
	// Set Functions

	void SetTime     ( INT    time     ) { m_nTime     = time;     }
	void SetValue    ( DOUBLE value    ) { m_fValue    = value;    }
	
	// -------------------------------------------------------------
	// Get Functions
	
	INT    Time      ( void ) { return m_nTime;     }
	DOUBLE Value     ( void ) { return m_fValue;    }
	
	friend FArchive& operator<<(FArchive& Ar,TLIPSincAnimationKey& A)
	{	
		Ar << A.m_nVersion;
		return Ar << A.m_nTime << A.m_fValue;
	}	

protected:

	INT    m_nTime;			// Time of this key, in milliseconds
	DOUBLE m_fValue;		// Value of this key
};

// -----------------------------------------------------------------------------
// TLIPSincAnimationTrack
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincAnimationTrack : public TLIPSincBase
{

public:

	static const INT kTLIPSincAnimationTrackVersion = 2;

	TLIPSincAnimationTrack( );
	~TLIPSincAnimationTrack( ); 

	FString GetName( void			) { return m_sName;   }
	void    SetName( FString InName ) { m_sName = InName; }

	INT NumKeys ( void ) { return m_akKeys.Num(); }
	
	void AddKey ( TLIPSincAnimationKey &key ); 
	TLIPSincAnimationKey* GetKey ( INT index );

    void DeleteKey ( INT index );
	
	DOUBLE ValueAtTime ( DOUBLE time );

	friend FArchive& operator<<(FArchive& Ar,TLIPSincAnimationTrack& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_sName << A.m_akKeys;
	}
	

protected:

	FString						 m_sName;	// The name of this track.

	TArray<TLIPSincAnimationKey> m_akKeys;  // The track keys sorted by time
};

// -----------------------------------------------------------------------------
// TLIPSincAnimation
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincAnimation : public TLIPSincBase
{

public:

	static const INT kTLIPSincAnimationVersion = 2;
		
	TLIPSincAnimation( );
	~TLIPSincAnimation( );

	void SetName ( FString name )
	{
		m_szName = name;
		m_Sound  = NULL;
	}

	FString   Name ( void ) { return m_szName; }

	void SetFullPkgName ( FString name )
	{
		m_szFullPkgName = name;
		m_Sound         = NULL;
	}

	FString	FullPkgName ( void ) { return m_szFullPkgName; }

	INT NumTracks ( void ) { return m_atTracks.Num(); }

	void Digest( void );

	void AddTrack ( TLIPSincAnimationTrack &track );

	void AddExpressionTrack ( TLIPSincAnimationTrack &track );
	TLIPSincAnimationTrack* GetExpressionTrack( FString ExpressionName );

	TLIPSincAnimationTrack* GetTrack ( INT track );

	INT EndTime( void ) { return m_nEndTime; }

	INT MemFootprint( void );

	INT DiskFootprint( void );
	
	// return true if anim finished, false otherwise
	UBOOL ValuesAtTime ( DOUBLE time, TArray<DOUBLE> &LIPSincValues );

	// returns the value of the speceified expression track at the specified time.
	// if the track doesn't exist, return 0.0.
	DOUBLE ExpressionValueAtTime( DOUBLE time, FString ExpressionName );
	
	void SetInterruptible( UBOOL Interruptible ) { m_bInterruptible = Interruptible; }
	UBOOL IsInterruptible( void ) { return m_bInterruptible; }

	FLOAT GetBlendInTime( void ) { return m_fBlendInTime; }
	void  SetBlendInTime( FLOAT BlendInTime ) { m_fBlendInTime = BlendInTime; }

	FLOAT GetBlendOutTime( void ) { return m_fBlendOutTime; }
	void  SetBlendOutTime( FLOAT BlendOutTime) { m_fBlendOutTime = BlendOutTime; }

	USound* GetUSound( void );

	UBOOL Play   ( AActor* theActor, FLOAT Volume, FLOAT Radius, FLOAT Pitch );
	UBOOL Stop   ( AActor* theActor );
	UBOOL Pause  ( AActor* theActor );
	UBOOL Resume ( AActor* theActor );

	friend FArchive& operator<<(FArchive& Ar,TLIPSincAnimation& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_szName << A.m_szFullPkgName << A.m_bInterruptible << A.m_fBlendInTime << A.m_fBlendOutTime << A.m_atTracks << A.m_atExpressionTracks;
	}

protected:

	FString m_szName;
	
	FString m_szFullPkgName;

	INT m_nEndTime;

	USound* m_Sound;

	UBOOL m_bInterruptible;

	FLOAT m_fBlendInTime;
	FLOAT m_fBlendOutTime;

	TArray<TLIPSincAnimationTrack> m_atTracks;

	TArray<TLIPSincAnimationTrack> m_atExpressionTracks;
};

// -----------------------------------------------------------------------------
// TLIPSincBone
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincBone : public TLIPSincBase
{

public:

	static const INT kTLIPSincBoneVersion = 2;
	
	TLIPSincBone( )
	{
		guard(TLIPSincBone::TLIPSincBone);
		m_nVersion = kTLIPSincBoneVersion;
		unguard;
	}

	// -------------------------------------------------------------
	// Copy Constructor
		
	TLIPSincBone(
		const TLIPSincBone &other )
	: m_fvPosition   ( other.m_fvPosition )
	, m_fqOrientation( other.m_fqOrientation )
	{
		guard(TLIPSincBone::TLIPSincBone);
		m_nVersion = other.m_nVersion;
		unguard;
	}

	~TLIPSincBone( )
	{
		guard(TLIPSincBone::~TLIPSincBone);
		unguard;
	}

	void SetPosition ( FVector pos ) 
	{
		m_fvPosition = pos;
	}

	void SetOrientation ( FQuat orientation )
	{
		m_fqOrientation = orientation;
	}

	FVector Position    ( void ) { return m_fvPosition;    }
	FQuat   Orientation ( void ) { return m_fqOrientation; }

	friend FArchive& operator<<(FArchive& Ar,TLIPSincBone& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_fvPosition << A.m_fqOrientation;
	}

protected:
	
	FVector m_fvPosition;
	FQuat   m_fqOrientation;

};

// -----------------------------------------------------------------------------
// TLIPSincIndexedBone
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincIndexedBone : public TLIPSincBone
{

public:

	void SetUnrealIndex( INT InUnrealIndex )
	{
		m_nUnrealIndex = InUnrealIndex;
	}

	INT GetUnrealIndex( void )
	{
		return m_nUnrealIndex;
	}

	friend FArchive& operator<<(FArchive& Ar,TLIPSincIndexedBone& A)
	{
		Ar << A.m_nVersion;
		return Ar << A.m_fvPosition << A.m_fqOrientation << A.m_nUnrealIndex;
	}

protected:

	INT m_nUnrealIndex;

};

// -----------------------------------------------------------------------------
// TLIPSincBonePose
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincBonePose : public TLIPSincBase
{

public:

	static const INT kTLIPSincBonePoseVersion = 2;
		
	TLIPSincBonePose( );
	TLIPSincBonePose( const TLIPSincBonePose &other );
	~TLIPSincBonePose( );
	
	void AddBone ( TLIPSincBone &bone );
	
	TLIPSincBone* GetBone ( INT index );
	
	friend FArchive& operator<<(FArchive& Ar,TLIPSincBonePose& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_lbBones;
	}
	
protected:

	TArray<TLIPSincBone> m_lbBones;
};

class USkeletalMesh;
class USkeletalMeshInstance;

// -----------------------------------------------------------------------------
// TLIPSincBonePoseInfo
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincBonePoseInfo : public TLIPSincBase
{

public:

	static const INT kTLIPSincBonePoseInfoVersion = 2;
		
	TLIPSincBonePoseInfo( ); 
	~TLIPSincBonePoseInfo( );
	
	void AddPose ( TLIPSincBonePose &pose );
	
	void AddBone ( FString name );

	FString GetBoneName ( INT index );

	INT GetUnrealBoneIndex( INT index );

	TLIPSincBone* FindUnrealBone( INT index );
	INT			  FindUnrealBoneIndex( INT index );

	UBOOL BuildBoneMap( USkeletalMesh* Mesh, TArray<FString> *DanglingBones );

	void Clear( void );
	
	INT NumBonePoses    ( void ) { return m_lbpBonePoses.Num(); }
	INT NumBonesPerPose ( void ) { return m_sBoneNames.Num();   }

	INT MemFootprint( void );

	TLIPSincBonePose* GetPose ( INT index );

	TArray<TLIPSincBone>& GetBoneTransforms( void ) { return m_BoneTransforms; }

	void Blend ( TArray<DOUBLE> &LIPSincValues );

	friend FArchive& operator<<(FArchive& Ar,TLIPSincBonePoseInfo& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_sBoneNames << A.m_lbpBonePoses;
	}
		
protected:

	TArray<FString> m_sBoneNames;
	TArray<INT>     m_nBoneMap;

	TArray<TLIPSincBonePose> m_lbpBonePoses;

	TArray<TLIPSincBone> m_BoneTransforms;

};

// -----------------------------------------------------------------------------
// TLIPSincExpressionInfo
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincExpressionInfo : public TLIPSincBonePoseInfo
{

public:

	static const INT kTLIPSincExpressionInfoVersion = 2;
	
	TLIPSincExpressionInfo( ); 
	~TLIPSincExpressionInfo( );

	INT MemFootprint( void );
	
	FString GetName( void			) { return m_sName;	  }
	void    SetName( FString InName ) { m_sName = InName; }

	void Blend ( TLIPSincBonePose* pDefBonePose, TArray<DOUBLE> &LIPSincValues, UBOOL bBlendToNeutral );

	friend FArchive& operator<<(FArchive& Ar,TLIPSincExpressionInfo& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_sBoneNames << A.m_lbpBonePoses << A.m_sName;
	}

protected:

	FString		m_sName;

public:

	DOUBLE      m_fWeight;
};

// -----------------------------------------------------------------------------
// TLIPSincBlendInfo
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincBlendInfo : public TLIPSincBase
{

public:

	TLIPSincBlendInfo( )
	{
		guard(TLIPSincBlendInfo::TLIPSincBlendInfo);

		BlendMode					= ELBM_BlendIn;

		Volume						= 0.0;
		Radius						= 0.0;
		Pitch						= 0.0;

		LastOffset					= 0.0;

		TargetLIPSincBoneTrafos.Empty();
		
		BlendInStartTime			= 0.0;
		BlendOutStartTime			= 0.0;
		PlayStartTime				= 0.0;

		bNoSound					= 0;

		bHasStartedPlaying			= 0;
		bWasAlreadyPlaying			= 0;

		bHasStartedBlendIn			= 0;
		bHasStartedBlendOut			= 0;

		bForceFrame					= 0;
		
		unguard;
	}

	~TLIPSincBlendInfo( )
	{
		guard(TLIPSincBlendInfo::~TLIPSincBlendInfo);
		unguard;
	}

	void Reset( void )
	{
		guard(TLIPSincBlendInfo::Reset);

		BlendMode					= ELBM_BlendIn;

		Volume						= 0.0;
		Radius						= 0.0;
		Pitch						= 0.0;

		LastOffset					= 0.0;

		TargetLIPSincBoneTrafos.Empty();
		
		BlendInStartTime			= 0.0;
		BlendOutStartTime			= 0.0;
		PlayStartTime				= 0.0;

		bNoSound					= 0;

		bHasStartedPlaying			= 0;
		bWasAlreadyPlaying			= 0;

		bHasStartedBlendIn			= 0;
		bHasStartedBlendOut			= 0;

		bForceFrame					= 0;

		unguard;
	}

	friend FArchive& operator<<(FArchive& Ar,TLIPSincBlendInfo& A)
	{	
		return	Ar << A.TargetLIPSincBoneTrafos;
	}

	ELIPSincBlendMode			BlendMode;

	TArray<TLIPSincIndexedBone>	TargetLIPSincBoneTrafos;

	FLOAT						Volume;
	FLOAT						Radius;
	FLOAT						Pitch;
	
	DOUBLE						LastOffset;

	DOUBLE						BlendInStartTime;
	DOUBLE						BlendOutStartTime;
	DOUBLE						PlayStartTime;

	UBOOL						bNoSound;

	UBOOL						bHasStartedPlaying;
	UBOOL						bWasAlreadyPlaying;

	UBOOL						bHasStartedBlendIn;
	UBOOL						bHasStartedBlendOut;

	UBOOL						bForceFrame;
};

// ==================
// EXPERIMENTAL STUFF
// ==================

// NOTE: This look at controller stuff is unfinished and untested.  It is left
//       here because it is a nice container for head and eye bones (and it is
//       being used for that purpose in parts of the code).
//       The look at controller only works correctly when the mesh is in the
//       reference pose (not when an animation is playing).  This will be
//       finished with a realistic head and eye dynamics system in a future 
//       version, but the stubs of this system are left here for you to play 
//       around with.

// -----------------------------------------------------------------------------
// TLIPSincLookAtInfo
// -----------------------------------------------------------------------------

// Support for LookAt Controllers
class ENGINE_API TLIPSincLookAtInfo : public TLIPSincBase
{

public:

	static const INT kTLIPSincLookAtInfoVersion = 2;
	
	TLIPSincLookAtInfo( );
	~TLIPSincLookAtInfo( );

	FString GetHeadBoneName    ( void ) { return m_sHeadBone;     }
	FString GetLeftEyeBoneName ( void ) { return m_sLeftEyeBone;  }
	FString GetRightEyeBoneName( void ) { return m_sRightEyeBone; }

	void SetHeadBoneName    ( FString InHeadBoneName )     { m_sHeadBone     = InHeadBoneName;     }
	void SetLeftEyeBoneName ( FString InLeftEyeBoneName )  { m_sLeftEyeBone  = InLeftEyeBoneName;  }
	void SetRightEyeBoneName( FString InRightEyeBoneName ) { m_sRightEyeBone = InRightEyeBoneName; }

	INT GetUnrealHeadBoneIndex    ( void ) { return m_nHeadBoneIndex;     }
	INT GetUnrealLeftEyeBoneIndex ( void ) { return m_nLeftEyeBoneIndex;  }
	INT GetUnrealRightEyeBoneIndex( void ) { return m_nRightEyeBoneIndex; }

	UBOOL BuildBoneMap( USkeletalMesh* Mesh );

	friend FArchive& operator<<(FArchive& Ar,TLIPSincLookAtInfo& A)
	{
		Ar << A.m_nVersion;
		return Ar << A.m_sHeadBone << A.m_sLeftEyeBone << A.m_sRightEyeBone;
	}

protected:

	FString			m_sHeadBone;
	FString			m_sLeftEyeBone;
	FString			m_sRightEyeBone;

	INT				m_nHeadBoneIndex;
	INT				m_nLeftEyeBoneIndex;
	INT				m_nRightEyeBoneIndex;
};

// -----------------------------------------------------------------------------
// TLIPSincLookAtController
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincLookAtController : public TLIPSincBase
{

public:

	TLIPSincLookAtController( );
	~TLIPSincLookAtController( );

	TLIPSincBone& GetHeadBoneTransform    ( void ) { return m_HeadBoneTransform;     }
	TLIPSincBone& GetLeftEyeBoneTransform ( void ) { return m_LeftEyeBoneTransform;  }
	TLIPSincBone& GetRightEyeBoneTransform( void ) { return m_RightEyeBoneTransform; }

	void StartLookAt( AActor* pInLookAtObject, FVector InLookAtOffsetVector );
	void StopLookAt( void );

	UBOOL IsLookingAtSomething( void ) { return m_bCurrentlyLookingAt; }
	
	void Update( TLIPSincLookAtInfo	   *pLookAtInfo, 
				 USkeletalMesh		   *pMesh, 
		         TArray<FCoords>	   &UnrealSpaceBases, 
				 TArray<FVector>	   &CachedUnrealPositions, 
				 TArray<FQuat>		   &CachedUnrealOrientations, 
				 FMatrix			   &MeshToWorldMatrix,
				 USkeletalMeshInstance *pMeshInst
			    );

protected:

	TLIPSincBone	m_HeadBoneTransform;
	TLIPSincBone	m_LeftEyeBoneTransform;
	TLIPSincBone	m_RightEyeBoneTransform;

	AActor*			m_pLookAtObject;
	FVector			m_LookAtOffsetVector;
	UBOOL			m_bCurrentlyLookingAt;

};

// ======================
// END EXPERIMENTAL STUFF
// ======================

// -----------------------------------------------------------------------------
// TLIPSincController
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincController : public TLIPSincBase
{

public:

	static const INT kTLIPSincControllerVersion = 2;
	
	TLIPSincController( );	
	~TLIPSincController( );

	TLIPSincBonePoseInfo* BonePoseInfo ( void ); 
	
	INT NumAnimations ( void ) { return m_aAnimations.Num(); }

	INT NumExpressions ( void ) { return m_Expressions.Num(); }

	void AddAnimation ( TLIPSincAnimation &anim );

	void AddExpression ( TLIPSincExpressionInfo &expression );

	TLIPSincExpressionInfo* GetExpression( FString ExpressionName );
	TLIPSincExpressionInfo* GetExpression( INT index );

	void DeleteExpression( FString ExpressionName );
	
	TLIPSincAnimation* GetAnimation ( INT index ); 

	INT FindAnimIndex ( FString animName );

	void DeleteAnimation ( INT index );

	DOUBLE TotalAnimTime ( void );

	INT MemFootprint ( void );

	INT DiskFootprint ( void );

	UBOOL GetDirty( void ) { return m_bDirty; }

	void SetDirty( void ) { m_bDirty = 1; }
	
	void SetName ( FString name )
	{
		guard(TLIPSincController::SetName);
		m_szName = name;
		unguard;
	}

	FString Name ( void ) { return m_szName; }

	void BlendIn				(	TArray<FVector>				&CachedUnrealPositions, 
									TArray<FQuat>				&CachedUnrealOrientations, 
									TLIPSincAnimation			*ActiveAnim, 
									TLIPSincBlendInfo			&BlendInfo, 
									TArray<TLIPSincIndexedBone>	&LIPSincBones, 
									TArray<DOUBLE>				&LIPSincValues, 
									FLOAT						alpha 
								);

	void BlendOut				(	TArray<FVector>				&CachedUnrealPositions, 
									TArray<FQuat>				&CachedUnrealOrientations,
									TArray<TLIPSincIndexedBone>	&LIPSincBones, 
									FLOAT						alpha 
								);

	UBOOL ProcessLIPSinc		(	TLIPSincAnimation			*ActiveAnim, 
									AActor						*OurActor, 
									TArray<FCoords>				&UnrealSpaceBases, 
									TArray<FVector>				&CachedUnrealPositions, 
									TArray<FQuat>				&CachedUnrealOrientations, 
									TLIPSincBlendInfo			&BlendInfo, 
									TArray<DOUBLE>				&LIPSincValues, 
									TArray<TLIPSincIndexedBone>	&LIPSincBones 
								);

	void UpdateUnrealBoneCache	(	TArray<FCoords>				&UnrealSpaceBases, 
									TArray<FVector>				&CachedUnrealPositions, 
									TArray<FQuat>				&CachedUnrealOrientations, 
									TArray<TLIPSincIndexedBone>	&LIPSincBones 
								);

	void FinalBlend				(	TArray<TLIPSincIndexedBone>	&LIPSincBones	);

	TLIPSincLookAtInfo* GetLookAtInfo( void ) { return &m_LookAtInfo; }

	INT InitializeBones( TArray<TLIPSincIndexedBone> &LIPSincBones );
	
	void SaveToDisk ( FString filename );
	
	void LoadFromDisk ( FString filename );

	friend FArchive& operator<<(FArchive& Ar,TLIPSincController& A)
	{	
		Ar << A.m_nVersion;
		return	Ar << A.m_lbpiBonePoseInfo << A.m_Expressions << A.m_aAnimations << A.m_LookAtInfo;
	}

protected:

	FString m_szName;

	UBOOL m_bDirty;
	
	TLIPSincBonePoseInfo m_lbpiBonePoseInfo;

	TArray<TLIPSincExpressionInfo> m_Expressions;

	TArray<TLIPSincAnimation> m_aAnimations;

	TLIPSincLookAtInfo m_LookAtInfo;

};

// -----------------------------------------------------------------------------
// TLIPSincDB
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincDB : public TLIPSincBase
{

public:

	TLIPSincDB( );
	~TLIPSincDB( );

	TLIPSincController* FindControllerByName( FString ControllerName );
	
	INT GetControllerIndex( FString ControllerName );
	
	TLIPSincController* GetController( INT index );

	void AddController( TLIPSincController &controller );
	void DeleteController( INT index );

	INT NumLoadedControllers( void ) { return m_LoadedControllers.Num(); }

	void LoadLIPSincDBFromDisk( TArray<FString> &Filenames );
		
protected:
	
	TArray<TLIPSincController> m_LoadedControllers;
};

// -----------------------------------------------------------------------------
// TLIPSincPhonemeMap
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincPhonemeMap : public TLIPSincBase
{

public:

	class ENGINE_API TLIPSincPhonemeWeights : public TLIPSincBase
	{
	
	public:

		TLIPSincPhonemeWeights( ) { }
		~TLIPSincPhonemeWeights( ) { m_fWeights.Empty(); }

		INT NumWeights( void ) { return m_fWeights.Num(); }

		friend FArchive& operator<<(FArchive& Ar,TLIPSincPhonemeWeights& A)
		{
			return Ar << A.m_fWeights;
		}
	
		TArray<FLOAT> m_fWeights;
	};

	TLIPSincPhonemeMap( );
	~TLIPSincPhonemeMap( );

	INT NumSpeechTargets ( void )  { return m_sSpeechTargetNames.Num(); }
	INT NumPhonemeEntries( void )  { return m_sPhonemeNames.Num();      }

	void AddSpeechTargetName( FString SpeechTargetName );
	void AddPhonemeMappingEntry( FString PhonemeName, TLIPSincPhonemeWeights &theWeights );
	
	void SaveToDisk( FString filename, FString MeshName );
	
	friend FArchive& operator<<(FArchive& Ar,TLIPSincPhonemeMap& A)
	{	
		return	Ar << A.m_sSpeechTargetNames << A.m_sPhonemeNames << A.m_SpeechTargetWeights;
	}

protected:
	
	// List of target names
	TArray<FString> m_sSpeechTargetNames;
	
	// List of phoneme names
	TArray<FString> m_sPhonemeNames;
	
	// List of target weights for phonemes.  This list is in the same order as the
	// phoneme name lists, so if the first entry in m_sPhonemeNames is "Iy", the first entry
	// in m_fTargetWeights will contain the target weights for the "Iy" phoneme.
	TArray<TLIPSincPhonemeWeights> m_SpeechTargetWeights;
};

// -----------------------------------------------------------------------------
// TLIPSincLBPFileParser
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincLBPFileParser : public TLIPSincBase
{

public:

	TLIPSincLBPFileParser( )
	{
		guard(TLIPSincLBPFileParser::TLIPSincLBPFileParser);
		unguard;
	}

	~TLIPSincLBPFileParser( )
	{
		guard(TLIPSincLBPFileParser::~TLIPSincLBPFileParser);
		unguard;
	}

	UBOOL Parse( FString              Filename, 
				 TLIPSincBonePoseInfo *pbi, 
				 TLIPSincPhonemeMap   *pm,
				 TLIPSincLookAtInfo   *plai,
				 USkeletalMesh        *Mesh, 
				 TArray<FString>      *DanglingBones = NULL 
			   );

	static const INT kLBPVersion;
};

// -----------------------------------------------------------------------------
// TLIPSincLTFFileParser
// -----------------------------------------------------------------------------

class ENGINE_API TLIPSincLTFFileParser : public TLIPSincBase
{

public:

	TLIPSincLTFFileParser( )
	{
		guard(TLIPSincLTFFileParser::TLIPSincLTFFileParser);
		unguard;
	}

	~TLIPSincLTFFileParser( )
	{
		guard(TLIPSincLTFFileParser::~TLIPSincLTFFileParser);
		unguard;
	}

	UBOOL Parse( FString			Filename,
				 TLIPSincAnimation  *pa,
				 USkeletalMesh		*Mesh
			   );

	static const FLOAT kLTFVersion;
};

class USkeletalMeshInstance;

// -----------------------------------------------------------------------------
// TLIPSincTalkerList
// -----------------------------------------------------------------------------

// A simple linked-list of 'Talkers'.
class ENGINE_API TLIPSincTalkerList
{

public:

	TLIPSincTalkerList( )
	{
		guard(TLIPSincTalkerList::TLIPSincTalkerList);
		pHead = 0;
		unguard;
	}

	~TLIPSincTalkerList( )
	{
		guard(TLIPSincTalkerList::~TLIPSincTalkerList);
		Clear();
		unguard;
	}

	// Clear the whole list.
	void Clear      ( void );

	// Add a new talker to the list.
	void Add        ( USkeletalMeshInstance* pSkeletalMeshInstance );

	// Remove a talker from the list.
	void Remove     ( USkeletalMeshInstance* pSkeletalMeshInstance );

	// Iterate through all talkers, forcing an update on their mesh whether it is visible or not.
	void ForceUpdate( void );

	// Debugging Only.
	//void PrintList( void );

protected:

	// A single 'Talker'.
	class ENGINE_API TLIPSincTalker
	{

	public:

		TLIPSincTalker( )
		{
			guard(TLIPSincTalker::TLIPSincTalker);
			pSkeletalMeshInstance = 0;
			pNext                 = 0;
			unguard;
		}

		TLIPSincTalker( USkeletalMeshInstance* pInSkeletalMeshInstance, TLIPSincTalker* pInNext )
		{
			guard(TLIPSincTalker::TLIPSincTalker);
			pSkeletalMeshInstance = pInSkeletalMeshInstance;
			pNext                 = pInNext;
			unguard;
		}

		~TLIPSincTalker( )
		{
			guard(TLIPSincTalker::~TLIPSincTalker);
			unguard;
		}

		USkeletalMeshInstance* pSkeletalMeshInstance;
		TLIPSincTalker*        pNext;
	};

	TLIPSincTalker* pHead;
};

#endif