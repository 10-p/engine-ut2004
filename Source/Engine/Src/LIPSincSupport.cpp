/*=============================================================================
	LIPSincSupport.cpp : LIPSinc Support for Unreal
	
	  Revision history:
		* Created by Jamie Redmond

    Work-in-progress TODO's:
		
=============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_LIPSINC

// -----------------------------------------------------------------------------
// Globals

ENGINE_API TLIPSincDB         GLIPSincDB;
ENGINE_API TLIPSincTalkerList GLIPSincTalkerList;

void ENGINE_API GLIPSincInitGame( void )
{
	guard(LIPSincInitGame);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LIPSincInitGame"));

	// Iterate over *.lad files in LIPSinc directory
#ifndef _XBOX
	TArray<FString> Found = GFileManager->FindFiles( TEXT(".\\LIPSincData\\Controllers\\*.lad"), 1, 0 );
#else
	TArray<FString> Found = GFileManager->FindFiles( TEXT("..\\LIPSincData\\Controllers\\*.lad"), 1, 0 );
#endif

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Found files..."));

	for( INT i=0; i<Found.Num(); ++i )
	{
		FString Temp = Found(i);
#ifndef _XBOX	
		Found(i) = FString( TEXT(".\\LIPSincData\\Controllers\\") );
#else
		Found(i) = FString( TEXT("..\\LIPSincData\\Controllers\\") );
#endif
		Found(i) += Temp;
	}

	GLIPSincDB.LoadLIPSincDBFromDisk( Found );

	unguard;
}

void ENGINE_API GLIPSincShutdownGame( void )
{
	guard(LIPSincShutdownGame);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LIPSincShutdownGame"));

	unguard;
}

void ENGINE_API GLIPSincInitLevel( ULevel* level )
{
	guard(LIPSincInitLevel);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LIPSincInitLevel"));
	
	unguard;
}

void ENGINE_API GLIPSincShutdownLevel( void )
{
	guard(LIPSincShutdownLevel);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LIPSincShutdownLevel"));

	GLIPSincTalkerList.Clear();

	unguard;
}

void ENGINE_API GLIPSincInitEditor( void )
{
	guard(LIPSincInitEditor);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LIPSincInitEditor"));

	GLIPSincInitGame();

	unguard;
}

void ENGINE_API GLIPSincShutdownEditor( void )
{
	guard(LIPSincShutdownEditor);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LIPSincShutdownEditor"));

	unguard;
}

void ENGINE_API _Blend
(
 const FVector &ToTrans,
 FQuat		   &ToRot,
 const FVector &FromTrans,
 const FQuat   &FromRot,
 const FQuat   &FromRotInv,
 FVector       &OutTrans,
 FQuat		   &OutRot,
 FLOAT		   alpha
)
{
	guard(TLIPSincController::_Blend);

	FVector tDelta;
	FQuat rDelta;

	tDelta =  ((ToTrans - FromTrans) * alpha);

	OutTrans += tDelta;

	AlignFQuatWith( ToRot, FromRot );
	ToRot.Normalize();

	FastSlerpNormQuat(&FromRot, &ToRot, alpha, &rDelta);

	rDelta = FromRotInv * rDelta;

	OutRot =  OutRot * rDelta;

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincAnimationTrack
// -----------------------------------------------------------------------------

TLIPSincAnimationTrack::TLIPSincAnimationTrack( )
{
	guard(TLIPSincAnimationTrack::TLIPSincAnimationTrack);
	m_nVersion = kTLIPSincAnimationTrackVersion;
	unguard;
}

TLIPSincAnimationTrack::~TLIPSincAnimationTrack( ) // Nothing
{
	guard(TLIPSincAnimationTrack::~TLIPSincAnimationTrack);
	m_akKeys.Empty();
	unguard;
}

void TLIPSincAnimationTrack::AddKey ( TLIPSincAnimationKey &key )
{
	guard(TLIPSincAnimationTrack::AddKey);
	TLIPSincAnimationKey* newKey = new(m_akKeys)TLIPSincAnimationKey();
	*newKey = key;
	unguard;
}

TLIPSincAnimationKey* TLIPSincAnimationTrack::GetKey ( INT index )
{
	guard(TLIPSincAnimationTrack::GetKey);
	if ( index >= 0 && index < m_akKeys.Num() )
	{
		return &m_akKeys(index);
	}
	
	return NULL;
	unguard;
}

void TLIPSincAnimationTrack::DeleteKey ( INT index )
{
	guard(TLIPSincAnimationTrack::DeleteKey);
	m_akKeys.Remove( index );
	unguard;
}

DOUBLE TLIPSincAnimationTrack::ValueAtTime ( DOUBLE time )
{
	guard(TLIPSincAnimationTrack::ValueAtTime);
	
	DOUBLE value = 0.0f;
	
	if ( m_akKeys.Num() > 0 )
	{
		// If the requested time is out-of-range for this track, simply clamp to
		// the end point values
		
		if ( time < (DOUBLE)(m_akKeys(0).Time()) )
		{
			value = m_akKeys(0).Value();
		}
		else if ( time > (DOUBLE)(m_akKeys(( m_akKeys.Num() - 1 )).Time()) )
		{
			value = m_akKeys(m_akKeys.Num() - 1).Value();
		}
		
		// If the requested time is in range, compute the value based on Bezier curves
		else
		{
			if ( m_akKeys.Num() == 1 )
			{
				value = m_akKeys(0).Value();
			}
			else
			{
				// indexForThisTime will become the index of the key prior to the specified time,
				//    or the index of the key with exactly the specified time.
				// If however, the time is the endTime of the keyList, then indexForThisTime
				//    has to be found in a special way.
				
				INT indexForThisTime = -1;
				
				// Requested time is not the end time of the track
				INT time_i = m_akKeys(0).Time();
				INT time_ip1 = 0;
				for (INT index = 0; index < m_akKeys.Num() - 1; index++)
				{
					time_ip1 = m_akKeys(( index + 1 )).Time();
					
					if ( ((DOUBLE)(time_i) <= time) && ((DOUBLE)(time_ip1) > time) )
					{
						indexForThisTime = index;
						break;
					}
					
					time_i = time_ip1;
				}
				
				// Special case: spefified time is the endTime of the keyList.
				//   The interval assigned to this key is the same one as the one assigned to
				//   the previous key. The only difference is that for this special case, the
				//   parametric time will be equal to one, and for the time of the previous key
				//   it is zero.
				if (indexForThisTime < 0)
				{
					check( time == (DOUBLE)(m_akKeys(( m_akKeys.Num() - 1 )).Time()) );
					indexForThisTime = m_akKeys.Num() - 2;
				}
				
				// Set common references
				TLIPSincAnimationKey *thisKey = &m_akKeys(indexForThisTime);
				TLIPSincAnimationKey *nextKey = &m_akKeys(( indexForThisTime + 1 ));
				
				// Get the time of the ends of the interval and compute parametric time
				INT time1 = thisKey->Time();
				INT time2 = nextKey->Time();
				check(time2 > time1);
				
				DOUBLE parametricTime = (time - (DOUBLE)(time1)) / ((DOUBLE)(time2) - (DOUBLE)(time1));
				DOUBLE tt = parametricTime * parametricTime;
				DOUBLE ttt = tt * parametricTime;
				
				// Compute control points.
				DOUBLE p1 = thisKey->Value();
				DOUBLE p2 = thisKey->Value();
				DOUBLE p3 = nextKey->Value();
				DOUBLE p4 = nextKey->Value();
				
				// Get the value.
				DOUBLE p1_3 = p1 * 3.0;
				DOUBLE p2_3 = p2 * 3.0;
				DOUBLE p3_3 = p3 * 3.0;
				DOUBLE cubicTerm = (-p1 + p2_3 - p3_3 + p4);
				DOUBLE quadraticTerm = (p1_3 - 6.0 * p2 + p3_3);
				DOUBLE linearTerm = (-p1_3 + p2_3);
				
				value = ttt * cubicTerm + tt * quadraticTerm + parametricTime * linearTerm + p1;
			}
			
			// If the value is very close to zero, set it to be equal to zero.
			if (Abs(value) < kLIPSinc2Epsilon)
			{
				value = 0.0;
			}
		}
	}
	
	return value;
	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincAnimation
// -----------------------------------------------------------------------------

TLIPSincAnimation::TLIPSincAnimation( )
: m_nEndTime      ( 0 )
, m_Sound         ( NULL )
, m_bInterruptible( 1 )
, m_fBlendInTime( 160.0 )
, m_fBlendOutTime( 220.0 )
{
	guard(TLIPSincAnimation::TLIPSincAnimation);
	m_szFullPkgName = FString(TEXT("LIPSinc_NoSound"));
	m_nVersion = kTLIPSincAnimationVersion;
	unguard;
}

TLIPSincAnimation::~TLIPSincAnimation( )  // Nothing
{
	guard(TLIPSincAnimation::~TLIPSincAnimation);
	m_atTracks.Empty();
	m_atExpressionTracks.Empty();
	m_Sound = NULL;
	unguard;
}

USound* TLIPSincAnimation::GetUSound( void )
{
	guard(TLIPSincAnimation::GetUSound);
	
	// LoadObject is smart enough to not load it again if it is already loaded.
	m_Sound = LoadObject<USound>( NULL, *(m_szFullPkgName), NULL, LOAD_NoWarn, NULL );

	return m_Sound;

	unguard;
}

UBOOL TLIPSincAnimation::Play( AActor* theActor, FLOAT Volume, FLOAT Radius, FLOAT Pitch )
{
	guard(TLIPSincAnimation::Play);
	
	if( !GetUSound() )
	{
		return 0;
	}

	if( theActor->GetLevel()->Engine->Audio )
	{
		theActor->GetLevel()->Engine->Audio->PlaySound( theActor, (theActor->GetIndex()*16 + SLOT_Talk*2 + 0),
			m_Sound, theActor->Location, Volume, Radius, Pitch, 0.f, 0.f );

		return 1;		
	}

	return 0;

	unguard;
}

UBOOL TLIPSincAnimation::Stop( AActor* theActor )
{
	guard(TLIPSincAnimation::Stop);

	if( theActor->GetLevel()->Engine->Audio )
	{
		return (theActor->GetLevel()->Engine->Audio->StopSound( theActor, m_Sound ));
	}
	else
	{
		return 0;
	}

	unguard;
}

UBOOL TLIPSincAnimation::Pause( AActor* theActor )
{
	guard(TLIPSincAnimation::Pause);

	if( theActor->GetLevel()->Engine->Audio )
	{
		return (theActor->GetLevel()->Engine->Audio->PauseSound( theActor, m_Sound ));
	}
	else
	{
		return 0;
	}

	unguard;
}

UBOOL TLIPSincAnimation::Resume( AActor* theActor )
{
	guard(TLIPSincAnimation::Resume);

	if( theActor->GetLevel()->Engine->Audio )
	{
		return (theActor->GetLevel()->Engine->Audio->ResumeSound( theActor, m_Sound ));
	}
	else
	{
		return 0;
	}

	unguard;
}

void TLIPSincAnimation::Digest( void )
{
	guard(TLIPSincAnimation::Digest);
	
	INT cur_time = -1;
	INT i = 0;

	// make a pass through and get rid of negative time keys
	for ( i = 0; i < m_atTracks.Num(); ++i )
	{
		for ( INT k = 0; k < m_atTracks(i).NumKeys(); ++ k )
		{
			if ( m_atTracks(i).GetKey(k)->Time() < 0 )
			{
				m_atTracks(i).DeleteKey(k);				
			}
		}
	}

	// make a pass through the expression tracks and get rid of negative time keys
	for ( i = 0; i < m_atExpressionTracks.Num(); ++i )
	{
		for ( INT k = 0; k < m_atExpressionTracks(i).NumKeys(); ++ k )
		{
			if ( m_atExpressionTracks(i).GetKey(k)->Time() < 0 )
			{
				m_atExpressionTracks(i).DeleteKey(k);				
			}
		}
	}

	// look for end time
	for ( i = 0; i < m_atTracks.Num(); ++i )
	{
		if(	m_atTracks(i).GetKey(m_atTracks(i).NumKeys()-1) )
		{
			cur_time = m_atTracks(i).GetKey(m_atTracks(i).NumKeys()-1)->Time();
		}
		else
		{
			cur_time = -1;
		}

		if( cur_time > m_nEndTime )
		{
			m_nEndTime = m_atTracks(i).GetKey(m_atTracks(i).NumKeys()-1)->Time();
		}
	}

	for ( i = 0; i < m_atExpressionTracks.Num(); ++i )
	{
		if( m_atExpressionTracks(i).GetKey(m_atExpressionTracks(i).NumKeys()-1) )
		{
			cur_time = m_atExpressionTracks(i).GetKey(m_atExpressionTracks(i).NumKeys()-1)->Time();
		}
		else
		{
			cur_time = -1;
		}

		if( cur_time > m_nEndTime )
		{
			m_nEndTime = m_atExpressionTracks(i).GetKey(m_atExpressionTracks(i).NumKeys()-1)->Time();
		}
	}
	
	unguard;
}

INT TLIPSincAnimation::MemFootprint ( void )
{
	guard(TLIPSincAnimation::MemFootprint);

	INT MemTotal = 0;
	
	for ( INT j=0; j<NumTracks(); ++j )
	{
		MemTotal += GetTrack(j)->NumKeys() * sizeof(TLIPSincAnimationKey);
		MemTotal += GetTrack(j)->GetName().Len() * sizeof(TCHAR);
	}

	for ( INT i=0; i<m_atExpressionTracks.Num(); ++i )
	{
		MemTotal += m_atExpressionTracks(i).NumKeys() * sizeof(TLIPSincAnimationKey);
		MemTotal += m_atExpressionTracks(i).GetName().Len() * sizeof(TCHAR);
	}
	
	MemTotal += Name().Len() * sizeof(TCHAR);
	
	MemTotal += FullPkgName().Len() * sizeof(TCHAR);
	
	MemTotal += sizeof(INT);

	MemTotal += sizeof(USound*);

	MemTotal += sizeof(UBOOL);

	MemTotal += sizeof(FLOAT);
	MemTotal += sizeof(FLOAT);

	return MemTotal;

	unguard;
}

INT TLIPSincAnimation::DiskFootprint ( void )
{
	guard(TLIPSincAnimation::DiskFootprint);

	INT DiskTotal = 0;

	for ( INT j=0; j<NumTracks(); ++j )
	{
		DiskTotal += GetTrack(j)->NumKeys() * sizeof(TLIPSincAnimationKey);
		DiskTotal += GetTrack(j)->GetName().Len() * sizeof(TCHAR);
	}

	for ( INT i=0; i<m_atExpressionTracks.Num(); ++i )
	{
		DiskTotal += m_atExpressionTracks(i).NumKeys() * sizeof(TLIPSincAnimationKey);
		DiskTotal += m_atExpressionTracks(i).GetName().Len() * sizeof(TCHAR);
	}
	
	DiskTotal += Name().Len() * sizeof(TCHAR);
	
	DiskTotal += FullPkgName().Len() * sizeof(TCHAR);
	
	DiskTotal += sizeof(INT);

	DiskTotal += sizeof(UBOOL);

	DiskTotal += sizeof(FLOAT);
	DiskTotal += sizeof(FLOAT);

	return DiskTotal;

	unguard;
}

void TLIPSincAnimation::AddTrack ( TLIPSincAnimationTrack &track )
{
	guard(TLIPSincAnimation::AddTrack);
	TLIPSincAnimationTrack* newTrack = new(m_atTracks)TLIPSincAnimationTrack();
	*newTrack = track;
	unguard;
}

TLIPSincAnimationTrack* TLIPSincAnimation::GetTrack( INT track )
{
	guard(TLIPSincAnimation::GetTrack);
	
	if( track >= 0 && track < m_atTracks.Num() )
	{
		return &m_atTracks(track);
	}
	else
	{
		return NULL;
	}
	
	unguard;
}

void TLIPSincAnimation::AddExpressionTrack ( TLIPSincAnimationTrack &track )
{
	guard(TLIPSincAnimation::AddExpressionTrack);
	TLIPSincAnimationTrack* newTrack = new(m_atExpressionTracks)TLIPSincAnimationTrack();
	*newTrack = track;
	unguard;
}

TLIPSincAnimationTrack* TLIPSincAnimation::GetExpressionTrack( FString ExpressionName )
{
	guard(TLIPSincAnimation::GetExpressionTrack);

	for( INT track = 0; track < m_atExpressionTracks.Num(); ++track )
	{
		if( m_atExpressionTracks(track).GetName() == ExpressionName )
		{
			return &m_atExpressionTracks(track);
		}
	}

	return NULL;

	unguard;
}

// return true if anim finished, false otherwise
UBOOL TLIPSincAnimation::ValuesAtTime ( DOUBLE time, TArray<DOUBLE> &LIPSincValues )
{
	guard(TLIPSincAnimation::ValuesAtTime);

	DWORD Cycles = 0;
	clock(Cycles);	

	if( time > (DOUBLE)m_nEndTime || time < 0 )
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ValuesAtTime, m_nEndTime = %i, time = %f"), m_nEndTime, time);
		return 1;
	}
	
	for ( INT i = 0; i < m_atTracks.Num(); ++i )
	{
		// Skip the neutral pose values
		LIPSincValues(i+1) = m_atTracks(i).ValueAtTime( time );
	}

	unclock(Cycles);
	GStats.DWORDStats( GEngineStats.STATS_LIPSinc_TotalCurveEvalCycles ) += Cycles;

	return 0;
	
	unguard;
}

// returns the value of the speceified expression track at the specified time.
// if the track doesn't exist, return 0.0.
DOUBLE TLIPSincAnimation::ExpressionValueAtTime( DOUBLE time, FString ExpressionName )
{
	guard(TLIPSincAnimation::ExpressionValueAtTime);

	TLIPSincAnimationTrack* eTrack = GetExpressionTrack( ExpressionName );
	
	if( eTrack )
	{
		return eTrack->ValueAtTime( time );
	}
	else
	{
		return 0.0;
	}

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincBonePose
// -----------------------------------------------------------------------------

TLIPSincBonePose::TLIPSincBonePose( )
{
	guard(TLIPSincBonePose::TLIPSincBonePose);
	m_nVersion = kTLIPSincBonePoseVersion;
	unguard;
}

TLIPSincBonePose::TLIPSincBonePose(
				 const TLIPSincBonePose &other )
: m_lbBones   ( other.m_lbBones )
{
	guard(TLIPSincBonePose::TLIPSincBonePose);
	m_nVersion = other.m_nVersion;
	unguard;
}

TLIPSincBonePose::~TLIPSincBonePose( )
{
	guard(TLIPSincBonePose::~TLIPSincBone);
	m_lbBones.Empty();
	unguard;
}

void TLIPSincBonePose::AddBone ( TLIPSincBone &bone )
{
	guard(TLIPSincBonePose::AddBone);
	TLIPSincBone* newBone = new(m_lbBones)TLIPSincBone();
	*newBone = bone;
	unguard;
}

TLIPSincBone* TLIPSincBonePose::GetBone ( INT index )
{
	guard(TLIPSincBonePose::GetBone);
	return &m_lbBones(index);
	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincBonePoseInfo
// -----------------------------------------------------------------------------

TLIPSincBonePoseInfo::TLIPSincBonePoseInfo( )
{
	guard(TLIPSincBonePoseInfo::TLIPSincBonePoseInfo);
	m_nVersion = kTLIPSincBonePoseInfoVersion;
	unguard;
}

TLIPSincBonePoseInfo::~TLIPSincBonePoseInfo( )
{
	guard(TLIPSincBonePoseInfo::~TLIPSincBonePoseInfo);
	m_lbpBonePoses.Empty();
	m_sBoneNames.Empty();
	m_nBoneMap.Empty();
	m_BoneTransforms.Empty();
	unguard;
}

void TLIPSincBonePoseInfo::AddPose ( TLIPSincBonePose &pose )
{
	guard(TLIPSincBonePoseInfo::AddPose);
	TLIPSincBonePose* newPose = new(m_lbpBonePoses)TLIPSincBonePose();
	*newPose = pose;
	unguard;
}

void TLIPSincBonePoseInfo::Clear( void )
{
	guard(TLIPSincBonePoseInfo::Clear);
	m_lbpBonePoses.Empty();
	m_sBoneNames.Empty();
	m_nBoneMap.Empty();
	m_BoneTransforms.Empty();
	unguard;
}

void TLIPSincBonePoseInfo::AddBone ( FString name )
{
	guard(TLIPSincBonePoseInfo::AddBone);
	FString* boneName = new(m_sBoneNames)FString();
	*boneName = name;
	unguard;
}

FString TLIPSincBonePoseInfo::GetBoneName ( INT index )
{
	guard(TLIPSincBonePoseInfo::GetBoneName);
	if( index >= 0 && index < NumBonesPerPose() )
	{
		return m_sBoneNames(index);
	}
	else
	{
		return FString(TEXT(""));
	}
	unguard;
}

INT TLIPSincBonePoseInfo::GetUnrealBoneIndex ( INT index )
{
	guard(TLIPSincBonePoseInfo::GetUnrealBoneIndex);
	if( index >= 0 && index < NumBonesPerPose() )
	{
		return m_nBoneMap(index);
	}
	else
	{
		return -1;
	}
	unguard;
}

TLIPSincBone* TLIPSincBonePoseInfo::FindUnrealBone( INT index )
{
	guard(TLIPSincBonePoseInfo::FindUnrealBone);

	for( INT i = 0; i < m_nBoneMap.Num(); ++i )
	{
		if( m_nBoneMap(i) == index )
		{
			return &m_BoneTransforms(i);
		}
	}

	return NULL;

	unguard;
}

INT TLIPSincBonePoseInfo::FindUnrealBoneIndex( INT index )
{
	guard(TLIPSincBonePoseInfo::FindUnrealBone);

	for( INT i = 0; i < m_nBoneMap.Num(); ++i )
	{
		if( m_nBoneMap(i) == index )
		{
			return i;
		}
	}

	return -1;

	unguard;
}

UBOOL TLIPSincBonePoseInfo::BuildBoneMap( USkeletalMesh* Mesh, TArray<FString> *DanglingBones )
{
	guard(USkeletalMesh::BuildBoneMap);

	m_nBoneMap.Empty();

	m_nBoneMap.Add(NumBonesPerPose());

	m_BoneTransforms.Empty();

	m_BoneTransforms.AddZeroed( NumBonesPerPose() );

	INT numValidBones = 0;
	UBOOL foundBone   = 0;

	for ( INT lbi = 0; lbi < NumBonesPerPose(); ++lbi )
	{
		FString thisBone = GetBoneName(lbi);

		//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: thisBone %s"), *(thisBone));

		foundBone = 0;
		
		for( INT bone=0; bone < Mesh->RefSkeleton.Num(); bone ++ )
		{
			if ( !appStricmp(*(Mesh->RefSkeleton(bone).Name), *(thisBone)) )
			{
				foundBone = 1;

				//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: BuildBoneMap -> found %s, updating map [%i -> %i]!"), *(Mesh->RefSkeleton(bone).Name), lbi, bone);
				
				m_nBoneMap(lbi) = bone;			
				numValidBones++;
			}
		}

		if( !foundBone && DanglingBones )
		{
			FString* boneName = new(*DanglingBones)FString();
			*boneName = thisBone;
		}
	}

	if( numValidBones == NumBonesPerPose() )
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: is valid controller"));
		return 1;
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: NOT VALID CONTROLLER!"));
		return 0;
	}

	unguard;
}

TLIPSincBonePose* TLIPSincBonePoseInfo::GetPose ( INT index )
{
	guard(TLIPSincBonePoseInfo::GetPose);
	return &m_lbpBonePoses(index);
	unguard;
}

INT TLIPSincBonePoseInfo::MemFootprint ( void )
{
	guard(TLIPSincBonePoseInfo::MemFootprint);

	INT MemTotal = 0;

	for ( INT k=0; k<NumBonesPerPose(); ++k )
	{
		MemTotal += GetBoneName(k).Len() * sizeof(TCHAR);
	}
	
	MemTotal += NumBonesPerPose() * sizeof(INT);

	MemTotal += NumBonePoses() * NumBonesPerPose() * sizeof(TLIPSincBone);
	
	MemTotal += NumBonesPerPose() * sizeof(TLIPSincBone);

	return MemTotal;

	unguard;
}

void TLIPSincBonePoseInfo::Blend ( TArray<DOUBLE> &LIPSincValues )
{
	guard(TLIPSincBonePoseInfo::Blend);

	TLIPSincBonePose *pDefBonePose = &m_lbpBonePoses(0);
		
	if( pDefBonePose )
	{
		for (INT i = 0; i < NumBonesPerPose(); ++i)
		{
			// pose index 0 is the default pose.
			TLIPSincBone* pDefaultTransform = pDefBonePose->GetBone(i);
			FVector tDefault(0,0,0);
			FQuat rDefault(0,0,0,0);
			
			if( pDefaultTransform )
			{
				tDefault = pDefaultTransform->Position();
				rDefault = pDefaultTransform->Orientation();
			}
			else
			{
				debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pDefaultTransform"));
			}
			
			FVector tSum = tDefault;
			FQuat rSum   = rDefault;
			
			FQuat rDefaultInv = -rDefault;
			
			for(INT iPose=0; iPose<NumBonePoses(); iPose++)
			{
				FLOAT fWeight = LIPSincValues.Num() ? LIPSincValues(iPose) : 0.0f;
				
				TLIPSincBone* pPoseTransform = m_lbpBonePoses(iPose).GetBone(i);
				
				FVector tPose(0,0,0);
				FQuat rPose(0,0,0,0);

				if( pPoseTransform )
				{
					tPose = pPoseTransform->Position();
					rPose = pPoseTransform->Orientation();
				}
				else
				{
					debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pPoseTransform"));
				}
				
				_Blend( tPose, rPose, tDefault, rDefault, rDefaultInv, tSum, rSum, fWeight );
			}
			
			m_BoneTransforms(i).SetPosition( tSum );
			m_BoneTransforms(i).SetOrientation( rSum );
		}
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pDefBonePose"));
	}

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincExpressionInfo
// -----------------------------------------------------------------------------

TLIPSincExpressionInfo::TLIPSincExpressionInfo( )
: m_fWeight( 0.f )
{
	guard(TLIPSincExpressionInfo::TLIPSincExpressionInfo);
	m_nVersion = kTLIPSincExpressionInfoVersion;
	m_sName.Empty();
	unguard;
}

TLIPSincExpressionInfo::~TLIPSincExpressionInfo( )
{
	guard(TLIPSincExpressionInfo::~TLIPSincExpressionInfo);
	m_sName.Empty();
	unguard;
}

INT TLIPSincExpressionInfo::MemFootprint ( void )
{
	guard(TLIPSincExpressionInfo::MemFootprint);

	INT MemTotal = 0;

	for ( INT k=0; k<NumBonesPerPose(); ++k )
	{
		MemTotal += GetBoneName(k).Len() * sizeof(TCHAR);
	}

	MemTotal += NumBonesPerPose() * sizeof(INT);

	MemTotal += NumBonePoses() * NumBonesPerPose() * sizeof(TLIPSincBone);

	MemTotal += NumBonesPerPose() * sizeof(TLIPSincBone);

	MemTotal += m_sName.Len() * sizeof(TCHAR);

	MemTotal += sizeof(DOUBLE);

	return MemTotal;

	unguard;
}

void TLIPSincExpressionInfo::Blend
(
	TLIPSincBonePose* pDefBonePose,
	TArray<DOUBLE>&   LIPSincValues,
	UBOOL             bBlendToNeutral
)
{
	guard(TLIPSincExpressionInfo::Blend);

	if( pDefBonePose )
	{
		for (INT i = 0; i < NumBonesPerPose(); ++i)
		{
			TLIPSincBone* pDefaultTransform = pDefBonePose->GetBone(i);
			FVector tDefault(0,0,0);
			FQuat rDefault(0,0,0,0);
			
			if( pDefaultTransform )
			{
				tDefault = pDefaultTransform->Position();
				rDefault = pDefaultTransform->Orientation();
			}
			else
			{
				debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pDefaultTransform"));
			}
			
			FVector tSum = tDefault;
			FQuat rSum   = rDefault;
			
			FQuat rDefaultInv = -rDefault;
			
			if( bBlendToNeutral )
			{
				// Blend to the neutral pose (pose 0).
				// Use a blend alpha of 1.0, since the FinalBlend function
				// will take care of actually alpha blending into this bone set.
                TLIPSincBone* pPoseTransform = m_lbpBonePoses(0).GetBone(i);

				// There was no cumulative blend, so set tSum and rSum to the full
				// pose values.
				tSum = pPoseTransform->Position();
				rSum = pPoseTransform->Orientation();
			}
			else
			{
				// Blend all the poses in this set.
				for(INT iPose=0; iPose<NumBonePoses(); iPose++)
				{
					FLOAT fWeight = LIPSincValues.Num() ? LIPSincValues(iPose) : 0.0f;

					TLIPSincBone* pPoseTransform = m_lbpBonePoses(iPose).GetBone(i);

					FVector tPose(0,0,0);
					FQuat rPose(0,0,0,0);

					if( pPoseTransform )
					{
						tPose = pPoseTransform->Position();
						rPose = pPoseTransform->Orientation();
					}
					else
					{
						debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pPoseTransform"));
					}

					_Blend( tPose, rPose, tDefault, rDefault, rDefaultInv, tSum, rSum, fWeight );
				}
			}
			
			m_BoneTransforms(i).SetPosition( tSum );
			m_BoneTransforms(i).SetOrientation( rSum );
		}
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pDefBonePose"));
	}

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincController
// -----------------------------------------------------------------------------

TLIPSincController::TLIPSincController( )
: m_bDirty( 0 )
{
	guard(TLIPSincController::TLIPSincController);
	m_nVersion = kTLIPSincControllerVersion;
	unguard;
}

TLIPSincController::~TLIPSincController( )
{
	guard(TLIPSincController::~TLIPSincController);
	m_aAnimations.Empty();
	m_Expressions.Empty();
	unguard;
}

TLIPSincBonePoseInfo* TLIPSincController::BonePoseInfo ( void )
{
	guard(TLIPSincController::BonePoseInfo);
	return &m_lbpiBonePoseInfo;
	unguard;
}

void TLIPSincController::AddAnimation ( TLIPSincAnimation &anim )
{
	guard(TLIPSincController::AddAnimation);
	TLIPSincAnimation* newAnim = new(m_aAnimations)TLIPSincAnimation();
	*newAnim = anim;
	m_aAnimations(m_aAnimations.Num()-1).Digest();
	m_bDirty = 1;
	unguard;
}

void TLIPSincController::AddExpression ( TLIPSincExpressionInfo &expression )
{
	guard(TLIPSincController::AddExpression);
	TLIPSincExpressionInfo* newExpression = new(m_Expressions)TLIPSincExpressionInfo();
	*newExpression = expression;
	unguard;
}

TLIPSincExpressionInfo* TLIPSincController::GetExpression( FString ExpressionName )
{
	guard(TLIPSincController::GetExpression);
	
	for( INT e = 0; e < m_Expressions.Num(); ++e )
	{
		if( m_Expressions(e).GetName() == ExpressionName )
		{
			return &m_Expressions(e);
		}
	}

	return NULL;

	unguard;
}

void TLIPSincController::DeleteExpression( FString ExpressionName )
{
	guard(TLIPSincController::DeleteExpression);

	for( INT e = 0; e < m_Expressions.Num(); ++e )
	{
		if( m_Expressions(e).GetName() == ExpressionName )
		{
			m_Expressions.Remove(e);
			m_bDirty = 1;
			return;
		}
	}

	unguard;
}

TLIPSincExpressionInfo* TLIPSincController::GetExpression( INT index )
{
	guard(TLIPSincController::GetExpression);
	if ( m_Expressions.Num() > 0 && index < m_Expressions.Num() && index >= 0 )
	{
		return &m_Expressions(index);
	}
	else
	{
		return NULL;
	}
	unguard;
}

TLIPSincAnimation* TLIPSincController::GetAnimation ( INT index )
{
	guard(TLIPSincController::GetAnimation);
	if ( m_aAnimations.Num() > 0 && index < m_aAnimations.Num() && index >= 0 )
	{
		return &m_aAnimations(index);
	}
	else
	{
		return NULL;
	}
	unguard;
}

DOUBLE TLIPSincController::TotalAnimTime ( void )
{
	guard(TLIPSincController::TotalAnimTime);

	DOUBLE total = 0.0;

	for ( INT i=0; i<m_aAnimations.Num(); ++i )
	{
		total += m_aAnimations(i).EndTime();
	}

	return total;

	unguard;
}

INT TLIPSincController::MemFootprint ( void )
{
	guard(TLIPSincController::MemFootprint);

	INT MemTotal = 0;

	MemTotal += m_szName.Len() * sizeof(TCHAR);

	MemTotal += sizeof(UBOOL);

	MemTotal += m_lbpiBonePoseInfo.MemFootprint();

	for( INT j=0; j<NumExpressions(); ++j )
	{
		MemTotal += m_Expressions(j).MemFootprint();
	}
	
	for ( INT i=0; i<NumAnimations(); ++i )
	{
		MemTotal += m_aAnimations(i).MemFootprint();
	}

	MemTotal += m_LookAtInfo.GetHeadBoneName().Len() * sizeof(TCHAR);
	MemTotal += m_LookAtInfo.GetLeftEyeBoneName().Len() * sizeof(TCHAR);
	MemTotal += m_LookAtInfo.GetRightEyeBoneName().Len() * sizeof(TCHAR);

	MemTotal += 3 * sizeof(INT);

	return MemTotal;

	unguard;
}

INT TLIPSincController::DiskFootprint ( void )
{
	guard(TLIPSincController::DiskFootprint);

	INT DiskTotal = 0;

	DiskTotal += m_szName.Len() * sizeof(TCHAR);

	DiskTotal += sizeof(UBOOL);

	DiskTotal += m_lbpiBonePoseInfo.MemFootprint();

	for( INT j=0; j<NumExpressions(); ++j )
	{
		DiskTotal += m_Expressions(j).MemFootprint();
	}
	
	for ( INT i=0; i<NumAnimations(); ++i )
	{
		DiskTotal += m_aAnimations(i).MemFootprint();
	}

	DiskTotal += m_LookAtInfo.GetHeadBoneName().Len() * sizeof(TCHAR);
	DiskTotal += m_LookAtInfo.GetLeftEyeBoneName().Len() * sizeof(TCHAR);
	DiskTotal += m_LookAtInfo.GetRightEyeBoneName().Len() * sizeof(TCHAR);

	DiskTotal += 3 * sizeof(INT);

	return DiskTotal;

	unguard;
}

INT TLIPSincController::FindAnimIndex ( FString animName )
{
	for ( INT i=0; i<m_aAnimations.Num(); ++i )
	{
		if( m_aAnimations(i).Name() == animName )
		{
			return i;
		}
	}
	
	return -1;
}

void TLIPSincController::DeleteAnimation ( INT index )
{
	guard(TLIPSincController::DeleteAnimation);
	if ( m_aAnimations.Num() > 0 && index < m_aAnimations.Num() && index >= 0 )
	{
		m_aAnimations.Remove(index);
		m_bDirty = 1;
	}
	unguard;
}

void TLIPSincController::BlendIn
(
	TArray<FVector>				&CachedUnrealPositions,
	TArray<FQuat>				&CachedUnrealOrientations,
	TLIPSincAnimation			*ActiveAnim,
	TLIPSincBlendInfo			&BlendInfo,
	TArray<TLIPSincIndexedBone> &LIPSincBones,
	TArray<DOUBLE>				&LIPSincValues,
	FLOAT						alpha
)
{
	guard(TLIPSincController::BlendIn);

	INT NumBones = LIPSincBones.Num();

	if( alpha == 0.0 )
	{
		BlendInfo.TargetLIPSincBoneTrafos.Empty();
		BlendInfo.TargetLIPSincBoneTrafos.AddZeroed( LIPSincBones.Num() );

		for( INT i = 0; i < NumBones; ++i )
		{
			BlendInfo.TargetLIPSincBoneTrafos(i).SetUnrealIndex( LIPSincBones(i).GetUnrealIndex() );
		}

		ActiveAnim->ValuesAtTime( 0.0, LIPSincValues );

		// Blend the base set.  LIPSincBones are the final bone transforms for this frame.
		m_lbpiBonePoseInfo.Blend( LIPSincValues );

		INT NumExpressions = m_Expressions.Num();

		for( INT e = 0; e < NumExpressions; ++e )
		{
			DOUBLE eValue = ActiveAnim->ExpressionValueAtTime( 0.0, m_Expressions(e).GetName() );

			m_Expressions(e).m_fWeight = eValue;

			if( eValue != 0.0 )
			{
				UBOOL bBlendToNeutral = 0;

				if( m_Expressions(e).NumBonePoses() == 1 )
				{
					bBlendToNeutral = 1;
				}

				m_Expressions(e).Blend(
					(bBlendToNeutral) ? BonePoseInfo()->GetPose(0) : m_Expressions(e).GetPose(0),
					LIPSincValues,
					bBlendToNeutral );
			}
		}

		// Blend the expressions in.
		FinalBlend( BlendInfo.TargetLIPSincBoneTrafos );
	}

	for (INT i = 0; i < NumBones; ++i)
	{
		TLIPSincBone* pTargetTransform = &(BlendInfo.TargetLIPSincBoneTrafos(i));
		FVector tTarget(0,0,0);
		FQuat   rTarget(0,0,0,0);
		
		if( pTargetTransform )
		{
			tTarget = pTargetTransform->Position();
			rTarget = pTargetTransform->Orientation();

			FVector tCurrent(0,0,0);
			FQuat   rCurrent(0,0,0,0);

			if( BlendInfo.bWasAlreadyPlaying )
			{
				tCurrent = LIPSincBones(i).Position();
				rCurrent = LIPSincBones(i).Orientation();
			}
			else
			{
				tCurrent = CachedUnrealPositions   ( LIPSincBones(i).GetUnrealIndex() );
				rCurrent = CachedUnrealOrientations( LIPSincBones(i).GetUnrealIndex() );
			}

			AlignFQuatWith( rTarget, rCurrent );
			rTarget.Normalize();

			FVector tDelta = tCurrent + ((tTarget - tCurrent) * alpha);
			FQuat   rDelta;

			FastSlerpNormQuat(&rCurrent, &rTarget, alpha, &rDelta);

			LIPSincBones(i).SetPosition( tDelta );
			LIPSincBones(i).SetOrientation( rDelta );
		}
		else
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pTargetTransform"));
		}
	}

	unguard;
}

void TLIPSincController::BlendOut
(
	TArray<FVector>				&CachedUnrealPositions,
	TArray<FQuat>				&CachedUnrealOrientations,
	TArray<TLIPSincIndexedBone> &LIPSincBones,
	FLOAT						alpha
)
{
	guard(TLIPSincController::BlendOut);

	INT NumBones = LIPSincBones.Num();
	
	for (INT i = 0; i < NumBones; ++i)
	{
		TLIPSincBone* pTargetTransform = &(LIPSincBones(i));
		FVector tCurrent(0,0,0);
		FQuat rCurrent(0,0,0,0);
		
		if( pTargetTransform )
		{
			tCurrent = pTargetTransform->Position();
			rCurrent = pTargetTransform->Orientation();

			FVector tNextFrame = CachedUnrealPositions   ( LIPSincBones(i).GetUnrealIndex() );
			FQuat   rNextFrame = CachedUnrealOrientations( LIPSincBones(i).GetUnrealIndex() );

			AlignFQuatWith( rCurrent, rNextFrame );
			rCurrent.Normalize();

			FVector tDelta = tCurrent + ((tNextFrame - tCurrent) * alpha);
			FQuat   rDelta;

			FastSlerpNormQuat(&rCurrent, &rNextFrame, alpha, &rDelta);

			LIPSincBones(i).SetPosition( tDelta );
			LIPSincBones(i).SetOrientation( rDelta );
		}
		else
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pTargetTransform"));
		}
	}
	
	unguard;
}

UBOOL TLIPSincController::ProcessLIPSinc
(
	TLIPSincAnimation			*ActiveAnim,
	AActor						*OurActor,
	TArray<FCoords>				&UnrealSpaceBases,
	TArray<FVector>				&CachedUnrealPositions,
	TArray<FQuat>				&CachedUnrealOrientations,
	TLIPSincBlendInfo			&BlendInfo,
	TArray<DOUBLE>				&LIPSincValues,
	TArray<TLIPSincIndexedBone> &LIPSincBones
)
{
	guard(TLIPSincController::ProcessLIPSinc);

	DOUBLE BlendCycles = 0;
	clock(BlendCycles);	

	UBOOL bAnimDone = 0;

	if( ActiveAnim && OurActor && OurActor->GetLevel()->Engine->Audio )
	{
		DOUBLE BlendInTime  = ActiveAnim->GetBlendInTime();
		DOUBLE BlendOutTime = ActiveAnim->GetBlendOutTime();

		DOUBLE CurrentTime = appSeconds() * 1000.0;

		if( BlendInfo.BlendMode == ELBM_BlendIn )
		{
			DOUBLE BlendInCycles = 0;
			clock(BlendInCycles);

			if( !BlendInfo.bHasStartedBlendIn )
			{
				BlendInfo.BlendInStartTime   = CurrentTime;
				BlendInfo.bHasStartedBlendIn = 1;
			}

			DOUBLE DeltaBlendInTime = (CurrentTime - BlendInfo.BlendInStartTime);
			
			if( DeltaBlendInTime <= BlendInTime )
			{
				BlendIn( CachedUnrealPositions, CachedUnrealOrientations, ActiveAnim, BlendInfo, LIPSincBones, LIPSincValues, (DeltaBlendInTime / BlendInTime) );
			}
			else
			{
				BlendInfo.BlendMode = ELBM_NormalBlend;
			}

			unclock(BlendInCycles);
			GStats.DWORDStats( GEngineStats.STATS_LIPSinc_TotalBlendInCycles ) += BlendInCycles;
		}
		else if( BlendInfo.BlendMode == ELBM_NormalBlend )
		{
			DOUBLE NormalBlendCycles = 0;
			clock(NormalBlendCycles);

			if( !BlendInfo.bHasStartedPlaying )
			{
				if( !BlendInfo.bForceFrame )
				{
					BlendInfo.bHasStartedPlaying = 1;
					
					if( BlendInfo.bNoSound )
					{
						BlendInfo.PlayStartTime = CurrentTime;
					}
					else
					{
						ActiveAnim->Play( OurActor, BlendInfo.Volume, BlendInfo.Radius, BlendInfo.Pitch );
					}
				}
			}

			if( !BlendInfo.bForceFrame )
			{
				if( BlendInfo.bNoSound )
				{
					BlendInfo.LastOffset = (CurrentTime) - BlendInfo.PlayStartTime;
				}
				else
				{
					if( OurActor->GetLevel()->Engine->Audio->IsQueued( ActiveAnim->GetUSound() ) )
					{
						BlendInfo.LastOffset = 0;
					}
					else
					{
						BlendInfo.LastOffset = OurActor->GetLevel()->Engine->Audio->OffsetInSound( OurActor, ActiveAnim->GetUSound() );
					}					
				}
			}
			
			bAnimDone = ActiveAnim->ValuesAtTime( BlendInfo.LastOffset, LIPSincValues );
			
			if( bAnimDone || BlendInfo.LastOffset < 0.f )
			{
				bAnimDone = 0;
				BlendInfo.LastOffset = 0.0;

				if( BlendInfo.bForceFrame )
				{
					BlendInfo.BlendMode = ELBM_None;
					bAnimDone = 1;
				}
				else
				{
					BlendInfo.BlendMode  = ELBM_BlendOut;
				}
			}
			else
			{
				// Blend the base set.  LIPSincBones are the final bone transforms for this frame.
				m_lbpiBonePoseInfo.Blend( LIPSincValues );

				for( INT e = 0; e < m_Expressions.Num(); ++e )
				{
					DOUBLE eValue = ActiveAnim->ExpressionValueAtTime( BlendInfo.LastOffset, m_Expressions(e).GetName() );

					m_Expressions(e).m_fWeight = eValue;

					if( eValue != 0.0 )
					{
						UBOOL bBlendToNeutral = 0;

						if( m_Expressions(e).NumBonePoses() == 1 )
						{
							bBlendToNeutral = 1;
						}

						m_Expressions(e).Blend(
							(bBlendToNeutral) ? BonePoseInfo()->GetPose(0) : m_Expressions(e).GetPose(0),
							LIPSincValues,
							bBlendToNeutral );
					}
				}

				// Blend the expressions in.
				FinalBlend( LIPSincBones );
			}

			unclock(NormalBlendCycles);
			GStats.DWORDStats( GEngineStats.STATS_LIPSinc_TotalNormalBlendCycles ) += NormalBlendCycles;
		}
		else if( BlendInfo.BlendMode == ELBM_BlendOut )
		{
			DOUBLE BlendOutCycles = 0;
			clock(BlendOutCycles);

			if( !BlendInfo.bHasStartedBlendOut )
			{
				BlendInfo.BlendOutStartTime   = CurrentTime;
				BlendInfo.bHasStartedBlendOut = 1;
			}

			DOUBLE DeltaBlendOutTime = (CurrentTime - BlendInfo.BlendOutStartTime);

			if( DeltaBlendOutTime <= BlendOutTime )
			{
				BlendOut( CachedUnrealPositions, CachedUnrealOrientations, LIPSincBones, (DeltaBlendOutTime / BlendOutTime) );
			}
			else
			{
				BlendInfo.BlendMode = ELBM_None;
				bAnimDone = 1;
				OurActor->NotifyLIPSincAnimEnd();  // TODO:  Should we call this when the animation is interrupted, too ??
				
				// Note that we don't remove this 'Talker' from the talker list here because we could be in the middle of
				// a ForceUpdate() call.  The 'Talker' is removed in the TLIPSincTalkerList::ForceUpdate() function when
				// it's animation is complete.
				
				// Clear out the CachedLIPSincValues TArray in USkeletalMeshInstance so that the LIPSinc Browser will display
				// all zero weights after the animation has ended (i.e., reset the list of pose weights so that it doesn't always
				// show the last frame values)
				if( GIsEditor )
				{
					LIPSincValues.Empty();
					LIPSincValues.AddZeroed( 27 );  // FIXME

					for( INT e = 0; e < NumExpressions(); ++e )
					{
						GetExpression( e )->m_fWeight = 0.0;
					}
				}
			}

			unclock(BlendOutCycles);
			GStats.DWORDStats( GEngineStats.STATS_LIPSinc_TotalBlendOutCycles ) += BlendOutCycles;
		}
		else  // ELBM_None
		{
			//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ELBM_None"));
			bAnimDone = 1;
		}
	
		if( !bAnimDone )
		{
			DWORD LIPSincUpdateCycles = 0;
			clock(LIPSincUpdateCycles);
			
			UpdateUnrealBoneCache( UnrealSpaceBases, CachedUnrealPositions, CachedUnrealOrientations, LIPSincBones );

			unclock(LIPSincUpdateCycles);
			GStats.DWORDOldStats( GEngineStats.STATS_LIPSinc_TotalUpdateCycles ) += LIPSincUpdateCycles;
		}
	}
	else
	{
		bAnimDone = 1;
	}

	unclock(BlendCycles);
	GStats.DWORDStats( GEngineStats.STATS_LIPSinc_TotalBlendCycles ) += BlendCycles;

	return bAnimDone;

	unguard;
}

void TLIPSincController::UpdateUnrealBoneCache
(
	TArray<FCoords>				&UnrealSpaceBases,
	TArray<FVector>				&CachedUnrealPositions,
	TArray<FQuat>				&CachedUnrealOrientations,
	TArray<TLIPSincIndexedBone> &LIPSincBones
)
{
	guard(TLIPSincController::UpdateUnrealBoneCache);

	INT NumBones = LIPSincBones.Num();

	for( INT i = 0; i < NumBones; ++i )
	{
		TLIPSincIndexedBone* pBone = &(LIPSincBones( i ));

		FVector Position    = pBone->Position();
		FQuat   Orientation = pBone->Orientation();
		INT     Index       = pBone->GetUnrealIndex();

		//CachedUnrealPositions   ( Index ) = Position;
		//CachedUnrealOrientations( Index ) = Orientation;

		FastQuatToFCoords( Orientation, Position, UnrealSpaceBases( Index ) );
	}

	unguard;
}

// TODO:
// There should be quite a bit of room for optimization in this function.
// For example, there are lots of places where a function is called that
// simply loops over each bone in an expression trying to find a match.
// This data could be precomputed and stored in a bitfield.  That could get
// tricky to do since we don't want to limit the number of 'expressions' that
// a character can contain.  For that reason it is left out of the initial
// release.  Future releases should address this issue.
void TLIPSincController::FinalBlend( TArray<TLIPSincIndexedBone> &LIPSincBones )
{
	guard(TLIPSincController::FinalBlend);

	DWORD LIPSincFinalBlendCycles = 0;
	clock(LIPSincFinalBlendCycles);

	TLIPSincBonePose *pDefPose = m_lbpiBonePoseInfo.GetPose(0);
	
	if( pDefPose )
	{
		INT NumBones	   = LIPSincBones.Num();
		INT NumExpressions = m_Expressions.Num();

		for( INT i = 0; i < NumBones; ++i )
		{
			INT Index = LIPSincBones( i ).GetUnrealIndex();
			
			// pose index 0 is the default pose.
			INT defBoneIndex = m_lbpiBonePoseInfo.FindUnrealBoneIndex( Index );
            TLIPSincBone* pDefaultTransform = NULL;
			
			if( defBoneIndex >= 0 )
			{
				pDefaultTransform = pDefPose->GetBone( defBoneIndex );
			}
			else
			{
				// TODO:
				// It is an error if a bone exists that doesn't also exist in the base set because
				// we have no base bone to blend from.  Perhaps we could get the bone's refpose
				// transform and blend from that ??
				debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ERROR -> bone %d doesn't exist in base set!"), Index);
			}

			FVector tDefault(0,0,0);
			FQuat rDefault(0,0,0,0);

			if( pDefaultTransform )
			{
				tDefault = pDefaultTransform->Position();
				rDefault = pDefaultTransform->Orientation();
			}
			else
			{
				debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pDefaultTransform"));
			}

			FVector tSum = tDefault;
			FQuat rSum   = rDefault;

			FQuat rDefaultInv = -rDefault;

			FLOAT fBoneWeight  = 0.0f;
			FLOAT fTotalWeight = 0.0f;

			for( INT j = 0; j < NumExpressions; ++j )
			{
				if( m_Expressions(j).FindUnrealBoneIndex( Index ) > -1 )
				{
					fTotalWeight += m_Expressions(j).m_fWeight;
				}
			}

			// Do a cummulative, weighted blend for each active expression.
			for(INT iPose=0; iPose<NumExpressions; ++iPose)
			{
				FLOAT fWeight = 0.0f;
				TLIPSincBone* pPoseTransform = 0;

				// Get the weight of the expression.
				fWeight = m_Expressions(iPose).m_fWeight;

				// Get the transform for this bone.
				pPoseTransform = m_Expressions(iPose).FindUnrealBone( Index );
					
				if( fWeight != 0.0 )
				{
					if( fTotalWeight > 1.0f )
					{
						fWeight /= fTotalWeight;
					}

					FVector tPose(0,0,0);
					FQuat rPose(0,0,0,0);

					if( pPoseTransform )
					{
						fBoneWeight += fWeight;

						tPose = pPoseTransform->Position();
						rPose = pPoseTransform->Orientation();

						_Blend( tPose, rPose, tDefault, rDefault, rDefaultInv, tSum, rSum, fWeight );

						LIPSincBones(i).SetPosition( tSum );
						LIPSincBones(i).SetOrientation( rSum );
					}
					else
					{
						//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pPoseTransform"));
						//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Bone %d not in expression %s"), Index, *(m_Expressions(iPose).GetName()) );
					}
				}
			}

			// Bring in the base if this bone's weight doesn't add up to 100%.
			if( fBoneWeight < 1.0f )
			{
				fBoneWeight = 1.0f - fBoneWeight;

				TLIPSincBone *pBaseTransform = m_lbpiBonePoseInfo.FindUnrealBone( Index );

				FVector tBase(0,0,0);
				FQuat rBase(0,0,0,0);

				if( pBaseTransform )
				{
					tBase = pBaseTransform->Position();
					rBase = pBaseTransform->Orientation();

					_Blend( tBase, rBase, tDefault, rDefault, rDefaultInv, tSum, rSum, fBoneWeight );
					
					LIPSincBones(i).SetPosition( tSum );
					LIPSincBones(i).SetOrientation( rSum );
				}
				else
				{
					debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pBaseTransform"));
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: pDefPose"));
	}

	unclock(LIPSincFinalBlendCycles);
	GStats.DWORDOldStats( GEngineStats.STATS_LIPSinc_TotalFinalBlendCycles ) += LIPSincFinalBlendCycles;

	unguard;
}

INT TLIPSincController::InitializeBones( TArray<TLIPSincIndexedBone> &LIPSincBones )
{
	guard(TLIPSincController::InitializeBones);

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Initializing bones..."));

	TArray<INT> UniqueBoneIndices;

	INT Index = 0;
	INT i     = 0;

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Default Poses..."));

	for( i = 0; i < m_lbpiBonePoseInfo.NumBonesPerPose(); ++i )
	{
		if( !UniqueBoneIndices.FindItem( m_lbpiBonePoseInfo.GetUnrealBoneIndex( i ), Index ) )
		{
			//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]:\t Added bone with index %d"), m_lbpiBonePoseInfo.GetUnrealBoneIndex( i ) );
			UniqueBoneIndices.AddItem( m_lbpiBonePoseInfo.GetUnrealBoneIndex( i ) );
		}
	}

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Expressions..."));

	for( i = 0; i < m_Expressions.Num(); ++i )
	{
		//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]:\t Expression %d..."), i );

		for( INT j = 0; j < m_Expressions(i).NumBonesPerPose(); ++j )
		{
			if( !UniqueBoneIndices.FindItem( m_Expressions( i ).GetUnrealBoneIndex( j ), Index ) )
			{
				//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]:\t Added bone with index %d"), m_Expressions( i ).GetUnrealBoneIndex( j ) );
				UniqueBoneIndices.AddItem( m_Expressions( i ).GetUnrealBoneIndex( j ) );
			}
		}
	}

	// Remove head bone.
	UniqueBoneIndices.RemoveItem( m_LookAtInfo.GetUnrealHeadBoneIndex() );
	UniqueBoneIndices.Shrink();

	LIPSincBones.Empty();
	LIPSincBones.AddZeroed( UniqueBoneIndices.Num() );

	for( i = 0; i < UniqueBoneIndices.Num(); ++i )
	{
		LIPSincBones(i).SetUnrealIndex( UniqueBoneIndices( i ) );
	}

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: done with %d unique bones."), UniqueBoneIndices.Num() );

	return UniqueBoneIndices.Num();

	unguard;
}

void TLIPSincController::SaveToDisk ( FString filename )
{
	guard(TLIPSincController::SaveToDisk);

	FArchive* Ar = GFileManager->CreateFileWriter(*(filename));
	
	if( Ar )
	{
		*Ar << *this;
		delete Ar;
		m_bDirty = 0;
	}
	
	unguard;
}

void TLIPSincController::LoadFromDisk ( FString filename )
{
	guard(TLIPSincController::LoadFromDisk);
	
	FArchive* Ar = GFileManager->CreateFileReader(*(filename));
	
	if( Ar )
	{
		*Ar << *this;
		delete Ar;

		for( INT f = 0; f < NumAnimations(); ++f )
		{
			m_aAnimations(f).Digest();
		}
	}
	
	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincDB
// -----------------------------------------------------------------------------

TLIPSincDB::TLIPSincDB( )
{
	guard(TLIPSincDB::TLIPSincDB);
	unguard;
}

TLIPSincDB::~TLIPSincDB( )
{
	guard(TLIPSincDB::~TLIPSincDB);
	m_LoadedControllers.Empty();
	unguard;
}

TLIPSincController* TLIPSincDB::FindControllerByName( FString ControllerName )
{
	guard(TLIPSincDB::FindControllerByName);

	for( INT i=0; i<m_LoadedControllers.Num(); ++i )
	{
		if( !appStrcmp(*(m_LoadedControllers(i).Name()), *(ControllerName)) )
		{
			return &m_LoadedControllers(i);
		}
	}

	return NULL;

	unguard;
}

INT TLIPSincDB::GetControllerIndex( FString ControllerName )
{
	guard(TLIPSincDB::GetControllerIndex);

	for( INT i=0; i<m_LoadedControllers.Num(); ++i )
	{
		if( !appStrcmp(*(m_LoadedControllers(i).Name()), *(ControllerName)) )
		{
			return i;
		}
	}

	return -1;

	unguard;
}

TLIPSincController* TLIPSincDB::GetController( INT index )
{
	guard(TLIPSincDB::GetController);
	
	return ((index >= 0 && index < m_LoadedControllers.Num()) ? &m_LoadedControllers(index) : NULL);
	
	unguard;
}

void TLIPSincDB::AddController( TLIPSincController &controller )
{
	guard(TLIPSincDB::AddController);
	TLIPSincController* newController = new(m_LoadedControllers)TLIPSincController();
	*newController = controller;
	newController->SetDirty();
	unguard;
}

void TLIPSincDB::DeleteController( INT index )
{
	guard(TLIPSincDB::DeleteController);

	if ( m_LoadedControllers.Num() > 0 && index < m_LoadedControllers.Num() && index >= 0 )
	{
		m_LoadedControllers.Remove(index);
	}

	unguard;
}

void TLIPSincDB::LoadLIPSincDBFromDisk( TArray<FString> &Filenames )
{
	guard(TLIPSincDB::LoadLIPSincDBFromDisk);

	INT numFiles = Filenames.Num();

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: LoadLIPSincDBFromDisk with %i files"), numFiles);

	if( numFiles > 0 )
	{
		m_LoadedControllers.AddZeroed( numFiles );

		for( INT i=0; i<numFiles; ++i )
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: %s"), *(Filenames(i)));
			
			m_LoadedControllers(i).LoadFromDisk( Filenames(i) );

			FString ControllerName = Filenames(i);
			
			while( ControllerName.InStr( TEXT("\\") ) != -1 )
				ControllerName = ControllerName.Mid( ControllerName.InStr( TEXT("\\") ) + 1, ControllerName.Len() );
			
			if( ControllerName.InStr( TEXT(".") ) != -1 )
				ControllerName = ControllerName.Left( ControllerName.InStr( TEXT(".") ) );

			m_LoadedControllers(i).SetName( ControllerName );
		}
	}

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincPhonemeMap
// -----------------------------------------------------------------------------

TLIPSincPhonemeMap::TLIPSincPhonemeMap()
{
	guard(TLIPSincPhonemeMap::TLIPSincPhonemeMap);
	unguard;
}

TLIPSincPhonemeMap::~TLIPSincPhonemeMap()
{
	guard(TLIPSincPhonemeMap::~TLIPSincPhonemeMap);
	m_sSpeechTargetNames.Empty();
	m_sPhonemeNames.Empty();
	m_SpeechTargetWeights.Empty();
	unguard;
}

void TLIPSincPhonemeMap::AddSpeechTargetName( FString SpeechTargetName )
{
	guard(TLIPSincPhonemeMap::AddSpeechTargetName);
	FString* speechTargetName = new(m_sSpeechTargetNames)FString();
	*speechTargetName = SpeechTargetName;
	unguard;
}

void TLIPSincPhonemeMap::AddPhonemeMappingEntry( FString PhonemeName, TLIPSincPhonemeWeights &theWeights )
{
	guard(TLIPSincPhonemeMap::AddPhonemeMappingEntry);

	FString* phonemeName = new(m_sPhonemeNames)FString();
	*phonemeName = PhonemeName;

	TLIPSincPhonemeWeights* phonemeWeights = new(m_SpeechTargetWeights)TLIPSincPhonemeWeights();
	*phonemeWeights = theWeights;

	unguard;
}

void TLIPSincPhonemeMap::SaveToDisk( FString filename, FString MeshName )
{
	guard(TLIPSincPhonemeMap::SaveToDisk);
	
	FString MapFile;

	MapFile = FString::Printf(TEXT("; %s.map\r\n;\r\n; LIPSinc Phoneme Mapping File for %s\r\n;\r\n; "), *MeshName, *MeshName);

	for( INT k=0; k<NumSpeechTargets(); ++k )
	{
		MapFile += m_sSpeechTargetNames(k) + FString(TEXT(" "));
	}

	MapFile += FString(TEXT("\r\n\r\n"));

	MapFile += FString::Printf(TEXT("[PhonemeToSpeechTargetMapping]\r\nNumSpeechTargets=%d\r\n"), NumSpeechTargets());

	for( INT i=0; i<NumPhonemeEntries(); ++i )
	{
		FString Entry = FString::Printf(TEXT("%s="), m_sPhonemeNames(i));

		for( INT j=0; j<NumSpeechTargets(); ++j )
		{
			FString Value;
			
			if( j == NumSpeechTargets() - 1 )
			{
				Value = FString::Printf(TEXT("%.2f\r\n"), m_SpeechTargetWeights(i).m_fWeights(j));
			}
			else
			{
				Value = FString::Printf(TEXT("%.2f, "), m_SpeechTargetWeights(i).m_fWeights(j));
			}

			Entry += Value;
		}

		MapFile += Entry;
	}

	appSaveStringToFile(MapFile, *filename);
	
	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincLBPFileParser
// -----------------------------------------------------------------------------

const INT TLIPSincLBPFileParser::kLBPVersion = 2;

UBOOL TLIPSincLBPFileParser::Parse
(
	FString              Filename,
	TLIPSincBonePoseInfo *pbi,
	TLIPSincPhonemeMap   *pm,
	TLIPSincLookAtInfo   *plai,
	USkeletalMesh        *Mesh,
	TArray<FString>      *DanglingBones
)
{
	guard(TLIPSincLBPFileParser::Parse);

	FString Data;

	if( DanglingBones )
	{
		DanglingBones->Empty();
	}
	
	if( appLoadFileToString( Data, *(Filename) ) )
	{
		const TCHAR* Buffer = *Data;
		
		FString StrLine;
		
		// Get LBP Version from the file
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		TArray<FString> Fields;
		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT LBPVersion = appAtoi(*(Fields(0)));

		if( LBPVersion != kLBPVersion )
		{
			// Lame attempt to detect obsolete .LBP files and prevent them
			// from causing problems.  If the .LBP format needs to evolve,
			// the LBPVersion from the file will be used to correctly parse
			// old and new formats.
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Attempt to import an obsolete .LBP file!"));
			return 0;
		}

		Fields.Empty();
		//////////////////////////////////////////////

		// Get number of speech targets from the file
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT NumSpeechTargets = appAtoi(*(Fields(0)));

		Fields.Empty();
		/////////////////////////////////////////////

		// Get the mapping information from the file
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		for( INT stn=0; stn<NumSpeechTargets; ++stn )
		{
			pm->AddSpeechTargetName(Fields(stn));
		}

		Fields.Empty();

		for( INT pme=0; pme<kNumLIPSincPhonemes; ++pme )
		{
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" "), &Fields );

			StrLine.Empty();

			TLIPSincPhonemeMap::TLIPSincPhonemeWeights Weights;
			
			for( INT st=0; st<NumSpeechTargets; ++st )
			{
				// Plus one to account for the phoneme name
				Weights.m_fWeights.AddItem(appAtof(*(Fields(st+1))));
			}

			pm->AddPhonemeMappingEntry( Fields(0), Weights );

			Fields.Empty();
		}
		///////////////////////////////////////////////
			
		// Get the number of bones from the file
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT numBones = appAtoi(*(Fields(0)));
		
		Fields.Empty();
		////////////////////////////////////////////////

		// Read in the bones section of the .lbp file
		for(INT i = 0; i < numBones; ++i)
		{
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" -> "), &Fields );

			StrLine.Empty();

			pbi->AddBone(Fields(0));
		
			Fields.Empty();
		}
		////////////////////////////////////////////////

		// Read the bone assignments in the .lbp file (these aren't used at the moment)
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );
	
		StrLine.ParseIntoArray( TEXT("="), &Fields );

		StrLine.Empty();

		// This is the head bone
		plai->SetHeadBoneName( Fields(1) );
		
		Fields.Empty();

		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT("="), &Fields );

		StrLine.Empty();

		// This is the left eye bone
		plai->SetLeftEyeBoneName( Fields(1) );

		Fields.Empty();

		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT("="), &Fields );

		StrLine.Empty();

		// This is the right eye bone
		plai->SetRightEyeBoneName( Fields(1) );

		Fields.Empty();
		////////////////////////////////////////////////

		// Get the number of bone poses from the file
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT numPoses = appAtoi(*(Fields(0)));

		Fields.Empty();
		////////////////////////////////////////////////
	
		// Read in the poses
		for(INT p = 0; p < numPoses; ++p)
		{
			// Create new bone frame to hold this pose
			TLIPSincBonePose bonePose;
			
			// Read in the bones for this pose
			for(INT b = 0; b < numBones; ++b)
			{
				TLIPSincBone bone;
				
				while(StrLine.Len() == 0)
					ParseLine( &Buffer, StrLine );

				StrLine.ParseIntoArray( TEXT(" "), &Fields );

				StrLine.Empty();
				
				FLOAT v_t_x = 0.0;
				FLOAT v_t_y = 0.0;
				FLOAT v_t_z = 0.0;
				
				FLOAT q_r_w = 0.0;
				FLOAT q_r_x = 0.0;
				FLOAT q_r_y = 0.0;
				FLOAT q_r_z = 0.0;
				
				// Read in the 3D vector translation and the quaternion rotation
				v_t_x = appAtof(*(Fields(16)));
				v_t_y = appAtof(*(Fields(17)));
				v_t_z = appAtof(*(Fields(18)));

				q_r_w = appAtof(*(Fields(22)));
				q_r_x = appAtof(*(Fields(23)));
				q_r_y = appAtof(*(Fields(24)));
				q_r_z = appAtof(*(Fields(25)));

				FVector position;
				FQuat   orientation;
				
				// Put them in Unreal's coordinate system				
				position.X = v_t_x;
				position.Y = -v_t_y;
				position.Z = v_t_z;
				
				orientation.X = q_r_x;
				orientation.Y = -q_r_y;
				orientation.Z = q_r_z;
				orientation.W = -q_r_w;
				
				bone.SetPosition( position );
				bone.SetOrientation( orientation );
				
				bonePose.AddBone( bone );
				
				Fields.Empty();
			}
					
			pbi->AddPose( bonePose );
		}
		///////////////////////////////////////////////////////
	}

	if( !plai->BuildBoneMap( Mesh ) )
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: Bone linkup for look at failed!"));
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Look at linked up correctly."));
	}

	if( !pbi->BuildBoneMap( Mesh, DanglingBones ) )
	{
		pbi->Clear();
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: LIPSinc bone linkup failed!"));
		return 0;
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Parsed %s"), *(Filename));
		return 1;
	}
	
	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincLTFFileParser
// -----------------------------------------------------------------------------

const FLOAT TLIPSincLTFFileParser::kLTFVersion = 1.0f;

UBOOL TLIPSincLTFFileParser::Parse
(
	FString			  Filename,
	TLIPSincAnimation *pa,
	USkeletalMesh	  *Mesh
)
{
	guard(TLIPSincLTFFileParser::Parse);
	
	FString Data;

	if( appLoadFileToString( Data, *(Filename) ) )
	{
		const TCHAR* Buffer = *Data;
		
		FString StrLine;
		
		// Get file type from the file.
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		TArray<FString> Fields;
		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		if( appStricmp( *(Fields(0)), TEXT("LTF") ) )
		{
			// Not a valid LTF file.
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ERROR: %s is not a valid LTF file!"), *(Filename));
			return 0;
		}

		Fields.Empty();
		//////////////////////////////////////////////

		// Get the LTF Version from the file.
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		FLOAT LTFVersion = appAtof(*(Fields(0)));

		if( LTFVersion > kLTFVersion )
		{
			// This is a newer LTF format than we currently support.
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ERROR: %s is a more recent LTF format.  This version can only read version %f and lower LTF files."), *(Filename), kLTFVersion);
			return 0;
		}

		Fields.Empty();
		/////////////////////////////////////////////

		// Get the number of phonemes.
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT NumPhonemes = appAtoi(*(Fields(0)));

		Fields.Empty();
		////////////////////////////////////////////////

		// Read in the phoneme section (skip it)
		for( INT i = 0; i < NumPhonemes; ++i )
		{
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" "), &Fields );

			StrLine.Empty();

			Fields.Empty();
		}
		////////////////////////////////////////////////

		// Get the number of function curves (tracks)
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT NumCurves = appAtoi(*(Fields(0)));

		Fields.Empty();
		////////////////////////////////////////////////

		INT t = 0;

		// Read in the tracks.
		for( t = 0; t < NumCurves; ++t )
		{
			// Get the {
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			// Set up a track to put values in.
			TLIPSincAnimationTrack TempTrack;

			// Get the track name
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			while( StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			FString TrackName = StrLine.Mid( 1 );

			TempTrack.SetName( TrackName );

			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Read Track %s"), TrackName);

			TrackName.Empty();
			StrLine.Empty();

			// Get the number of keys
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" "), &Fields );

			StrLine.Empty();

			INT NumKeys = appAtoi(*(Fields(0)));

			Fields.Empty();

			// Read in the keys
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" "), &Fields );

			StrLine.Empty();

			// Fields now has the first keys line from the LTF File
			// The section is in the format key0_time key0_value key1_time key1_value ... keyN_time keyN_value
			// There are 25 keys (time / value pairs) per line, so that we don't hit the 1024 byte limit for
			// strings in Unreal.
			// So there are 2 * NumKeys in the section, split up into lines of 25.
			INT DoubleNumKeys = 2 * NumKeys;
			INT x = 0;
			for( INT k = 0; k < DoubleNumKeys; k += 2 )
			{
				if( (((k / 2) + 1) % 26) == 0 )
				{
					while(StrLine.Len() == 0)
						ParseLine( &Buffer, StrLine );

					Fields.Empty();

					StrLine.ParseIntoArray( TEXT(" "), &Fields );

					StrLine.Empty();

					x = 0;
				}
				
				// NumKeys       = 3
				// DoubleNumKeys = 6
				// k = 0  -> Fields(k) Fields(k+1) -> Fields(0) Fields(1) -> key0_time key0_value
				// k = 2  -> Fields(k) Fields(k+1) -> Fields(2) Fields(3) -> key1_time key1_value
				// k = 4  -> Fields(k) Fields(k+1) -> Fields(4) Fields(5) -> key2_time key2_value
				
				DOUBLE Time  = appAtof(*(Fields(x)));
				DOUBLE Value = appAtof(*(Fields(x+1)));

				x += 2;

				// Set up the key
				TLIPSincAnimationKey TempKey;
				TempKey.SetTime( (INT)Time );
				TempKey.SetValue( Value );

				// Add the key to the track
				TempTrack.AddKey( TempKey );
			}

			Fields.Empty();

			// Get the }
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			// This was a valid curve definition, so add the track to the animation.
			pa->AddTrack( TempTrack );
		}
		////////////////////////////////////////////////

		// Get the number of expression curves (tracks)
		while(StrLine.Len() == 0)
			ParseLine( &Buffer, StrLine );

		StrLine.ParseIntoArray( TEXT(" "), &Fields );

		StrLine.Empty();

		INT NumExpressionCurves = appAtoi(*(Fields(0)));

		Fields.Empty();
		////////////////////////////////////////////////

		// Read in the tracks.
		for( t = 0; t < NumExpressionCurves; ++t )
		{
			// Get the {
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			// Set up a track to put values in.
			TLIPSincAnimationTrack TempTrack;

			// Get the track name
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			while( StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			FString TrackName = StrLine.Mid( 1 );

			TempTrack.SetName( TrackName );

			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Read Track %s"), TrackName);

			TrackName.Empty();
			StrLine.Empty();
			
			// Get the number of keys
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" "), &Fields );

			StrLine.Empty();

			INT NumKeys = appAtoi(*(Fields(0)));

			Fields.Empty();

			// Read in the keys
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.ParseIntoArray( TEXT(" "), &Fields );

			StrLine.Empty();

			// Fields now has the first keys line from the LTF File
			// The section is in the format key0_time key0_value key1_time key1_value ... keyN_time keyN_value
			// There are 25 keys (time / value pairs) per line, so that we don't hit the 1024 byte limit for
			// strings in Unreal.
			// So there are 2 * NumKeys in the section, split up into lines of 25.
			INT DoubleNumKeys = 2 * NumKeys;
			INT x = 0;
			for( INT k = 0; k < DoubleNumKeys; k += 2 )
			{
				if( (((k / 2) + 1) % 26) == 0 )
				{
					while(StrLine.Len() == 0)
						ParseLine( &Buffer, StrLine );

					Fields.Empty();

					StrLine.ParseIntoArray( TEXT(" "), &Fields );

					StrLine.Empty();

					x = 0;
				}
				
				// NumKeys       = 3
				// DoubleNumKeys = 6
				// k = 0  -> Fields(k) Fields(k+1) -> Fields(0) Fields(1) -> key0_time key0_value
				// k = 2  -> Fields(k) Fields(k+1) -> Fields(2) Fields(3) -> key1_time key1_value
				// k = 4  -> Fields(k) Fields(k+1) -> Fields(4) Fields(5) -> key2_time key2_value
				
				DOUBLE Time  = appAtof(*(Fields(x)));
				DOUBLE Value = appAtof(*(Fields(x+1)));

				x += 2;

				// Set up the key
				TLIPSincAnimationKey TempKey;
				TempKey.SetTime( (INT)Time );
				TempKey.SetValue( Value );

				// Add the key to the track
				TempTrack.AddKey( TempKey );
			}

			Fields.Empty();

			// Get the }
			while(StrLine.Len() == 0)
				ParseLine( &Buffer, StrLine );

			StrLine.Empty();

			// This was a valid curve definition, so add the track to the animation.
			pa->AddExpressionTrack( TempTrack );
		}
		////////////////////////////////////////////////

		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Parsed %s"), *(Filename));
		return 1;
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ERROR: Couldn't load file %s."), *(Filename));
		return 0;
	}

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincTalkerList
// -----------------------------------------------------------------------------

// Clear the whole list.
void TLIPSincTalkerList::Clear( void )
{
	guard(TLIPSincTalkerList::Clear);

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Clearing talker list..."));

	TLIPSincTalker* pTemp = 0;

	while( pHead != 0 )
	{
		pTemp = pHead->pNext;
		delete pHead;
		pHead = pTemp;
	}

	pHead = 0;

	unguard;
}

// Add a new talker to the list.
void TLIPSincTalkerList::Add( USkeletalMeshInstance* pSkeletalMeshInstance )
{
	guard(TLIPSincTalkerList::Add);

	pHead = new TLIPSincTalker( pSkeletalMeshInstance, pHead );

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Added %s to talker list."), pSkeletalMeshInstance->GetName());

	unguard;
}

// Remove a talker from the list.
void TLIPSincTalkerList::Remove( USkeletalMeshInstance* pSkeletalMeshInstance )
{
	guard(TLIPSincTalkerList::Remove);

	//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Removing %s from talker list..."), pSkeletalMeshInstance->GetName());

	TLIPSincTalker* pPrevious = 0;
	TLIPSincTalker* pCurrent  = pHead;

	while( pCurrent != 0 )
	{
		if( pCurrent->pSkeletalMeshInstance == pSkeletalMeshInstance )
		{
			if( pPrevious != 0 )
			{
				pPrevious->pNext = pCurrent->pNext;
			}
			else
			{
				pHead = 0;
			}

			delete pCurrent;

			//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Removed."));

			return;
		}
		else
		{
			pPrevious = pCurrent;
			pCurrent  = pCurrent->pNext;
		}
	}

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: ERROR -> %s not found in list!"), pSkeletalMeshInstance->GetName());

	unguard;
}

// Iterate through all talkers, forcing an update on their mesh whether it is visible or not.
void TLIPSincTalkerList::ForceUpdate( void )
{
	guard(TLIPSincTalkerList::ForceUpdate);

	if( pHead != 0 )
	{
		TLIPSincTalker* pTalker = pHead;

		while( pTalker != 0 )
		{
			//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Updating %s in talker list..."), pTalker->pSkeletalMeshInstance->GetName());
			pTalker->pSkeletalMeshInstance->ForceUpdate();

			if( !pTalker->pSkeletalMeshInstance->IsPlayingLIPSincAnim() )
			{
				TLIPSincTalker* pTemp = pTalker->pNext;
				GLIPSincTalkerList.Remove( pTalker->pSkeletalMeshInstance );
				//GLIPSincTalkerList.PrintList();
				pTalker = pTemp;
			}
			else
			{
				pTalker = pTalker->pNext;
			}
		}
	}

	unguard;
}

/*
// Debugging Only.
void TLIPSincTalkerList::PrintList( void )
{
	guard(TLIPSincTalkerList::PrintList);

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: Begin Talker List"));

	if( pHead != 0 )
	{
		TLIPSincTalker* pTalker = pHead;

		while( pTalker != 0 )
		{
			debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]:\t %s"), pTalker->pSkeletalMeshInstance->GetName());
			pTalker = pTalker->pNext;
		}
	}
	else
	{
		debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]:\t Talker List Empty."));
	}

	debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: End Talker List"));

	unguard;
}
*/

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

TLIPSincLookAtInfo::TLIPSincLookAtInfo( )
: m_nHeadBoneIndex    ( -1 )
, m_nLeftEyeBoneIndex ( -1 )
, m_nRightEyeBoneIndex( -1 )
{
	guard(TLIPSincLookAtInfo::TLIPSincLookAtInfo);
	m_nVersion = kTLIPSincLookAtInfoVersion;
	unguard;
}

TLIPSincLookAtInfo::~TLIPSincLookAtInfo( )
{
	guard(TLIPSincLookAtInfo::~TLIPSincLookAtInfo);
	m_sHeadBone.Empty();
	m_sLeftEyeBone.Empty();
	m_sRightEyeBone.Empty();
	unguard;
}

UBOOL TLIPSincLookAtInfo::BuildBoneMap( USkeletalMesh* Mesh )
{
	guard(TLIPSincLookAtInfo::BuildBoneMap);
	
	INT numValidBones = 0;

	for( INT bone=0; bone < Mesh->RefSkeleton.Num(); bone ++ )
	{
		if ( !appStricmp(*(Mesh->RefSkeleton(bone).Name), *(m_sHeadBone)) )
		{
			m_nHeadBoneIndex = bone;			
			numValidBones++;
		}
		else if ( !appStricmp(*(Mesh->RefSkeleton(bone).Name), *(m_sLeftEyeBone)) )
		{
			m_nLeftEyeBoneIndex = bone;
			numValidBones++;
		}
		else if ( !appStricmp(*(Mesh->RefSkeleton(bone).Name), *(m_sRightEyeBone)) )
		{
			m_nRightEyeBoneIndex = bone;
			numValidBones++;
		}
	}

	if( numValidBones == 3 )
	{
		//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: look at info is valid"));
		return 1;
	}
	else
	{
		//debugf(NAME_DevLIPSinc,TEXT("[LIPSinc]: WARNING: LOOK AT INFO IS NOT VALID!"));
		return 0;
	}

	unguard;
}

// -----------------------------------------------------------------------------
// TLIPSincLookAtController
// -----------------------------------------------------------------------------

TLIPSincLookAtController::TLIPSincLookAtController( )
: m_pLookAtObject( NULL )
, m_LookAtOffsetVector( FVector(0.0f,0.0f,0.0f) )
, m_bCurrentlyLookingAt( 0 )
{
	guard(TLIPSincLookAtController::TLIPSincLookAtController);
	unguard;
}

TLIPSincLookAtController::~TLIPSincLookAtController( )
{
	guard(TLIPSincLookAtController::TLIPSincLookAtController);
	unguard;
}

void TLIPSincLookAtController::StartLookAt( AActor* pInLookAtObject, FVector InLookAtOffsetVector )
{
	guard(TLIPSincLookAtController::StartLookAt);
	unguard;
}

void TLIPSincLookAtController::StopLookAt( void )
{
	guard(TLIPSincLookAtController::StopLookAt);
	unguard;
}

void TLIPSincLookAtController::Update
(
	TLIPSincLookAtInfo    *pLookAtInfo,
	USkeletalMesh         *pMesh,
	TArray<FCoords>       &UnrealSpaceBases,
	TArray<FVector>       &CachedUnrealPositions,
	TArray<FQuat>         &CachedUnrealOrientations,
	FMatrix               &MeshToWorldMatrix,
	USkeletalMeshInstance *pMeshInst
)
{
	guard(TLIPSincLookAtController::Update);
	unguard;
}

// ======================
// END EXPERIMENTAL STUFF
// ======================

#endif