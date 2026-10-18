/*=============================================================================
	UnStats.cpp: In game performance statistics utilities.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
		* Rewritten multiple times by Andrew Scheidecker and Daniel Vogel
		* Last rewrite by Daniel Vogel for .csv support
=============================================================================*/

#ifdef _XBOX
#include <xtl.h>
#endif
#include "EnginePrivate.h"
#include "UnNet.h"

// Global stats.
FStats GStats;
FEngineStats GEngineStats;

FStats::FStats()
{
	for( INT i=0; i<STATSTYPE_MAX; i++ )
		Registered[i] = 0;
}

INT FStats::RegisterStats( EStatsType Type, EStatsDataType DataType, FString Description, FString SectionDescription, EStatsUnit StatsUnit )
{
	guard(FStats::RegisterStats);
	INT Index		= -1;
	switch( DataType )
	{
	case STATSDATATYPE_DWORD:
		Index = DWORDStats.Add( 1 );
		DWORDOldStats.Add( 1 );
		new(DWORDStatsDescriptions)FString(Description);
		new(DWORDStatsSectionDescriptions)FString(SectionDescription + TEXT(" ") + Description);
		break;
	case STATSDATATYPE_FLOAT:
		Index = FLOATStats.Add( 1 );
		FLOATOldStats.Add( 1 );
		new(FLOATStatsDescriptions)FString(Description);
		new(FLOATStatsSectionDescriptions)FString(SectionDescription + TEXT(" ") + Description);
		break;
	case STATSDATATYPE_STRING:
		Index = FSTRINGStats.Add( 1 );
		FSTRINGOldStats.Add( 1 );
		new(FSTRINGStatsDescriptions)FString(Description);
		new(FSTRINGStatsSectionDescriptions)FString(SectionDescription + TEXT(" ") + Description);
		break;
	}

	FSingleStatsInfo SI;
	SI.Index		= Index;
	SI.DataType		= DataType; 
	SI.StatsUnit	= StatsUnit;

	Stats[Type].AddItem( SI );

	return Index;
	unguard;
}

void FStats::UpdateString( FString& Result, UBOOL Descriptions )
{
	guard(FStats::UpdateString);
	DWORD UpdateCycles = 0;
	clock(UpdateCycles);
	if( Descriptions )
	{
		for( INT i=0; i<STATSTYPE_MAX; i++ )
		{
			for( INT n=0; n<Stats[i].Num(); n++ )
			{
				Result += FString::Printf(TEXT("%s,"), *DWORDStatsSectionDescriptions(Stats[i](n).Index) );
			}
		}
	}
	else
	{
		for( INT i=0; i<STATSTYPE_MAX; i++ )
		{
			for( INT n=0; n<Stats[i].Num(); n++ )
			{
				switch( Stats[i](n).DataType )
				{
				case STATSDATATYPE_DWORD:
					{
						DWORD Value = DWORDStats(Stats[i](n).Index);
						switch( Stats[i](n).StatsUnit )
						{
						case STATSUNIT_Default:
						case STATSUNIT_Combined_Default_MSec:
						case STATSUNIT_Combined_Default_Default:
						case STATSUNIT_Byte:
						case STATSUNIT_KByte:
						case STATSUNIT_MByte:
							Result += FString::Printf(TEXT("%u,"), Value );
							break;
						case STATSUNIT_MSec:
							Result += FString::Printf(TEXT("%f,"), (FLOAT) (Value * GSecondsPerCycle * 1000.0f) );
							break;
						}
						break;
					}
				case STATSDATATYPE_FLOAT:
				case STATSDATATYPE_STRING:
					appErrorf(TEXT("Implement me!"));
				}
			}
		}
	}
	unclock(UpdateCycles);
	
	// Stat update time.
	if( Descriptions )
		Result += TEXT("Stats Update");
	else
		Result += FString::Printf(TEXT("%f"), (FLOAT) (UpdateCycles * GSecondsPerCycle * 1000.0f) );
	Result += TEXT("\n");

	unguard;
}

void FStats::Clear()
{
	guard(FStats::Clear);

	// Update old stats.
	if( DWORDStats.Num() )
		appMemcpy( &DWORDOldStats(0), &DWORDStats(0), DWORDStats.Num() * sizeof(DWORD) );
	if( FLOATStats.Num() )
		appMemcpy( &FLOATOldStats(0), &FLOATStats(0), FLOATStats.Num() * sizeof(FLOAT) );
	for( INT i=0; i<FSTRINGStats.Num(); i++ )
		FSTRINGOldStats(i) = FSTRINGStats(i);

	// Clear new ones.
	if( DWORDStats.Num() )
		appMemzero( &DWORDStats(0), DWORDStats.Num() * sizeof(DWORD) );
	if( FLOATStats.Num() )
		appMemzero( &FLOATStats(0), FLOATStats.Num() * sizeof(FLOAT) );
	for( INT i=0; i<FSTRINGStats.Num(); i++ )
		FSTRINGStats(i) = TEXT("");

	unguard;
}


void FStats::CalcMovingAverage( INT StatIndex, DWORD CycleSize )
{
#define DISCARDFIRST 3 
	// Automatic custom-size cyclic running average for MSEC timing parameters.

	DWORD* LatestCycles = &(GStats.DWORDStats( StatIndex ));
	
	INT NewElements = (StatIndex+1) -  Averages.Num();
	if( NewElements > 0 )
		Averages.AddZeroed( NewElements );	

	if( Averages(StatIndex).CycBufferSize != (INT)CycleSize )
	{
		Averages(StatIndex).CycBufferSize = CycleSize;
		Averages(StatIndex).Samples.Empty();
		Averages(StatIndex).Samples.AddZeroed( CycleSize );
		Averages(StatIndex).Count = 0;
	}
	
	Averages(StatIndex).Count = Min( Averages(StatIndex).CycBufferSize + DISCARDFIRST, Averages(StatIndex).Count + 1 );

	if( Averages(StatIndex).Count > DISCARDFIRST )
	{
		// Store and compute running average.
		INT TotalElements = Averages(StatIndex).Count - DISCARDFIRST;
				
		Averages(StatIndex).Samples( Averages(StatIndex).CycIndex ) = *LatestCycles;
		SQWORD DAv = 0;
		for( INT i=0; i< TotalElements; i++ )
			DAv += Averages(StatIndex).Samples(i);

		Averages(StatIndex).CycIndex = ( Averages(StatIndex).CycIndex + 1) % Averages(StatIndex).CycBufferSize;
		*LatestCycles = (INT)(DAv/(SQWORD)TotalElements);
	}		
}


#define MAYBE_WRAP				if( (Canvas->CurY + 32) >= Viewport->SizeY ) {CurX += 200; Canvas->CurY = 8;}
#define DRAW_STRING(Arg)		MAYBE_WRAP Canvas->CurX = CurX; Canvas->WrappedPrintf(Canvas->TinyFont,0,*Arg);
#define TIME_PRINTF				FString::Printf(TEXT("%2.2f %s ms")	
#define TIME_ARGUMENTS(Arg)		(FLOAT) (DWORDOldStats(GEngineStats.Arg) * GSecondsPerCycle * 1000.0f), *DWORDStatsDescriptions(GEngineStats.Arg) )
#define DWORD_PRINTF			FString::Printf(TEXT("%u %s")	
#define DWORD_ARGUMENTS(Arg)	DWORDOldStats(GEngineStats.Arg), *DWORDStatsDescriptions(GEngineStats.Arg) )
#define DRAW_HEADLINE(Arg)		Canvas->Color = FColor(0,255,255); Canvas->CurY += 4; Dummy = Arg; DRAW_STRING( Dummy ); Canvas->Color = FColor(0,255,0);

#define DRAW_ALL(Arg)			\
for( INT i=0; i<GStats.Stats[Arg].Num(); i++ )	\
{	\
	FSingleStatsInfo* StatsInfo = &GStats.Stats[Arg](i);	\
	switch( StatsInfo->StatsUnit )	\
	{	\
		case STATSUNIT_Default:	\
		case STATSUNIT_Byte:	\
		{	\
			DRAW_STRING( FString::Printf(TEXT("%u %s"), DWORDOldStats(StatsInfo->Index), *DWORDStatsDescriptions(StatsInfo->Index)) );	\
			break;	\
		}	\
		case STATSUNIT_KByte:	\
		{	\
			DRAW_STRING( FString::Printf(TEXT("%u KByte %s"), DWORDOldStats(StatsInfo->Index) / 1024, *DWORDStatsDescriptions(StatsInfo->Index)) );	\
			break;	\
		}	\
		case STATSUNIT_MByte:	\
		{	\
			DRAW_STRING( FString::Printf(TEXT("%u MByte %s"), DWORDOldStats(StatsInfo->Index) / 1024 / 1024, *DWORDStatsDescriptions(StatsInfo->Index)) );	\
			break;	\
		}	\
		case STATSUNIT_MSec:	\
		{	\
			DRAW_STRING( FString::Printf(TEXT("%2.1f %s ms"), (FLOAT)(DWORDOldStats(StatsInfo->Index) * GSecondsPerCycle * 1000.0f), *DWORDStatsDescriptions(StatsInfo->Index) ));	\
			break;	\
		}	\
		case STATSUNIT_Combined_Default_MSec:	\
		{	\
			FSingleStatsInfo* NextInfo = StatsInfo + 1;	\
			DRAW_STRING( FString::Printf(TEXT("%2.1f %s ms [%u]"), (FLOAT)(DWORDOldStats(NextInfo->Index) * GSecondsPerCycle * 1000.0f), *DWORDStatsDescriptions(StatsInfo->Index), DWORDOldStats(StatsInfo->Index)) );	\
			i++;	\
			break;	\
		}	\
		case STATSUNIT_Combined_Default_Default:	\
		{	\
			FSingleStatsInfo* NextInfo = StatsInfo + 1;	\
			DRAW_STRING( FString::Printf(TEXT("In: %i, Out: %i %s"), DWORDOldStats(StatsInfo->Index), DWORDOldStats(NextInfo->Index), *DWORDStatsDescriptions(StatsInfo->Index) ));	\
			i++;	\
			break;	\
		}	\
	}	\
}


void FStats::Render(class UViewport* Viewport, class UEngine* Engine)
{
	guard(FStats::Render);
	UCanvas* Canvas	= Viewport->Canvas;
	FString Dummy;

	#if DISABLE_STATS
		return;
	#endif

	// Render the framerate.
	if(Engine->bShowFrameRate)
	{
		FLOAT FrameTime		= DWORDOldStats(GEngineStats.STATS_Frame_TotalCycles) * GSecondsPerCycle;
		// Divide only when there is something to divide by: `1.f / 0.f` is +inf and `(INT)+inf` is
		// undefined behaviour. A zero-length frame degrades to "0 FPS".
		FPSAvg				= FPSAvg ? FPSAvg * 0.98f + FrameTime * 0.02f : FrameTime;
		INT	AvgFrameRate	= FPSAvg > 0.f ? appRound(1.f / FPSAvg) : 0,
			FrameRate		= FrameTime > 0.f ? appRound(1.f / FrameTime) : 0,
			Width			= 0,
			Height			= 0;

		Canvas->CurX = 0;
		Canvas->CurY = 0;

		Dummy = FString::Printf(TEXT("%u avg FPS"),AvgFrameRate);
		Canvas->WrappedStrLenf(Canvas->MedFont,Width,Height,*Dummy);

		if(FrameRate < 20)
			Canvas->Color = FColor(255,0,0);
		else if ( FrameRate < 30 )
			Canvas->Color = FColor(255,0,255);
		else if(FrameRate < 40)
			Canvas->Color = FColor(255,255,0);
		else
			Canvas->Color = FColor(0,255,0);

		Canvas->CurX = Viewport->SizeX - Width - 16;
		Canvas->CurY = 96;
		Canvas->WrappedPrintf(Canvas->MedFont,0,*FString::Printf(TEXT("%u cur FPS"),FrameRate));

		if(AvgFrameRate < 20)
			Canvas->Color = FColor(255,0,0);
		else if ( AvgFrameRate < 30 )
			Canvas->Color = FColor(255,0,255);
		else if(AvgFrameRate < 40)
			Canvas->Color = FColor(255,255,0);
		else
			Canvas->Color = FColor(0,255,0);

		Canvas->CurX = Viewport->SizeX - Width - 16;
		Canvas->WrappedPrintf(Canvas->MedFont,0,*Dummy);
	}

	INT CurX		= 8;
	Canvas->CurY	= 8;

	// Render stats.
	if( Engine->bShowRenderStats )
	{
		DRAW_HEADLINE( TEXT("- Frame ---------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Frame_TotalCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Frame_RenderCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Stats_RenderCycles			) ));

		DRAW_HEADLINE( TEXT("- BSP -----------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_BSP_RenderCycles				) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_BSP_Sections					) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_BSP_Nodes					) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_BSP_Triangles				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_BSP_DynamicLightingCycles	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_BSP_DynamicLights			) ));

		DRAW_HEADLINE( TEXT("- LightMap ------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LightMap_Cycles				) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_LightMap_Updates				) ));

		DRAW_HEADLINE( TEXT("- Projector -----") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Projector_RenderCycles		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Projector_AttachCycles		) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Projector_Projectors			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Projector_Triangles			) ));
		
		DRAW_HEADLINE( TEXT("- Stencil -------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Stencil_RenderCycles			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Stencil_Nodes				) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Stencil_Triangles			) ));

		DRAW_HEADLINE( TEXT("- Visibility ----") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Visibility_SetupCycles		) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Visibility_MaskTests			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Visibility_MaskRejects		) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Visibility_BoxTests			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Visibility_BoxRejects		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Visibility_TraverseCycles	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Visibility_ScratchBytes		) ));
		DRAW_STRING( (TIME_PRINTF ,	TIME_ARGUMENTS ( STATS_Visibility_MeshLightCycles	) ));

		DRAW_HEADLINE( TEXT("- Terrain ------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Terrain_RenderCycles			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Terrain_Sectors				) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Terrain_Triangles			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Terrain_DrawPrimitives		) ));

		DRAW_HEADLINE( TEXT("- DecoLayer ----") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_DecoLayer_RenderCycles		) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_DecoLayer_Triangles			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_DecoLayer_Decorations		) ));

		DRAW_HEADLINE( TEXT("- Mesh ---------") );
		CalcMovingAverage( GEngineStats.STATS_Mesh_SkinCycles	,40);
		CalcMovingAverage( GEngineStats.STATS_Mesh_ResultCycles	,40);
		CalcMovingAverage( GEngineStats.STATS_Mesh_LODCycles	,40);
		CalcMovingAverage( GEngineStats.STATS_Mesh_PoseCycles	,40);
		CalcMovingAverage( GEngineStats.STATS_Mesh_SkelCycles	,40);
		CalcMovingAverage( GEngineStats.STATS_Mesh_RigidCycles	,40);
		CalcMovingAverage( GEngineStats.STATS_Mesh_DrawCycles	,40);		
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_SkinCycles				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_ResultCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_LODCycles				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_PoseCycles				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_SkelCycles				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_RigidCycles				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Mesh_DrawCycles				) ));
		
		DRAW_HEADLINE( TEXT("- Particle -----") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Particle_SpriteSetupCycles	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Particle_Particles			) ));
        DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Particle_ParticlesTicked 	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Particle_RenderCycles		) ));

		DRAW_HEADLINE( TEXT("- StaticMesh ---") );
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_Triangles		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_StaticMesh_RenderCycles	) ));
		Canvas->CurY += 4;
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_Batches					) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_BatchedSortedTriangles	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_BatchedSortedSections		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_StaticMesh_BatchedSortCycles			) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_BatchedUnsortedSections	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_BatchedUnsortedTriangles	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_StaticMesh_BatchedRenderCycles		) ));
		Canvas->CurY += 4;
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_UnbatchedSortedTriangles	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_UnbatchedSortedSections	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_StaticMesh_UnbatchedSortCycles		) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_UnbatchedUnsortedSections	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_UnbatchedUnsortedTriangles) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_StaticMesh_UnbatchedRenderCycles		) ));

		DRAW_HEADLINE( TEXT("- Batch --------") );
		DRAW_STRING( (DWORD_PRINTF,	DWORD_ARGUMENTS( STATS_Batch_Batches				) ));
		DRAW_STRING( (DWORD_PRINTF,	DWORD_ARGUMENTS( STATS_Batch_Primitives				) ));
		DRAW_STRING( (TIME_PRINTF,	TIME_ARGUMENTS ( STATS_Batch_RenderCycles			) ));
	}
	
	// Render game stats.
	if( Engine->bShowGameStats )
	{
		DRAW_HEADLINE( TEXT("- Collision ----") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_StaticMesh_CollisionCycles	) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_CollisionCacheHits) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_CollisionCacheMisses) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_StaticMesh_CollisionCacheFlushes) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_BSP_CollisionCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Terrain_CollisionCycles		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Corona_CollisionCycles		) ));

		DRAW_HEADLINE( TEXT("- Karma --------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_KTriListQuery			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_KUpdateContacts		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_KIntersect				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_KWorldStepSafeTime		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_PreKarmaStep			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_ConstraintSolver		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_FindModelSafetime		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_PostKarmaStep			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_physKarma				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Karma_Temp					) ));

#ifdef WITH_LIPSINC

		DRAW_HEADLINE( TEXT("- LIPSinc --------") );
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_LIPSinc_NumActiveLIPSincAnims) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalCurveEvalCycles ) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalBlendInCycles   ) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalNormalBlendCycles) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalBlendOutCycles  ) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalBlendCycles		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalUpdateCycles	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalFinalBlendCycles) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_LIPSinc_TotalSetupCycles		) ));

#endif
		
		DRAW_HEADLINE( TEXT("- Fluid  --------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Fluid_SimulateCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Fluid_VertexGenCycles	    ) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Fluid_RenderCycles			) ));

		DRAW_HEADLINE( TEXT("- Game ---------") );
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_ScriptCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_ScriptTickCycles		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_ActorTickCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_FindPathCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_SeePlayerCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_SpawningCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_AudioTickCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_CleanupDestroyedCycles	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_UnusedCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_NetTickCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_ParticleTickCycles		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_CanvasCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_PhysicsCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_MoveCycles				) ));
		DRAW_STRING( (DWORD_PRINTF, DWORD_ARGUMENTS( STATS_Game_NumMoves				) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_MLCheckCycles			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_MPCheckCycles			) ));	
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_UpdateRenderData		) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_HUDPostRender			) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_InteractionPreRender	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_InteractionPostRender	) ));
		DRAW_STRING( (TIME_PRINTF , TIME_ARGUMENTS ( STATS_Game_ScriptDebugTime	) ));


		/* SCRIPTTIME
		if ( GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles) * GSecondsPerCycle * 1000.0f > 30.f )
		{
			debugf(TEXT(" ****************************************************************"));
			debugf(TEXT(" ActorTick Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" Script Cycles %f"), GScriptCycles * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" ScriptTick Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" FindPath Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_FindPathCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" SeePlayer Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_SeePlayerCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" Spawning Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" CleanupDestroyed Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" AudioTick Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_AudioTickCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" Canvas Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_CanvasCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" Physics Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" Move Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_MoveCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" MLCheck Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_MLCheckCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" MPCheck Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_MPCheckCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" UpdateRenderData Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_UpdateRenderData) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" ParticleTick Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_ParticleTickCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" NetTick Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles) * GSecondsPerCycle * 1000.0f);
			debugf(TEXT(" HUD PostRender Cycles %f"), GStats.DWORDStats(GEngineStats.STATS_Game_HUDPostRender) * GSecondsPerCycle * 1000.0f);
		}
		*/
	}

	// Render hardware stats.
	if( Engine->bShowHardwareStats )
	{
		DRAW_HEADLINE( TEXT("- Hardware -----") );
		DRAW_ALL(STATSTYPE_Hardware);
	}

	// Render audio stats.
	if( Engine->bShowAudioStats )
	{
		DRAW_HEADLINE( TEXT("- Audio --------") );
		DRAW_ALL(STATSTYPE_Audio);
	}

	// Render network stats.
	if( Engine->bShowNetStats )
	{	
		// Update net stats (yeah, this is a hack).
		UNetConnection*	Conn = NULL;
		if( Viewport->Actor->XLevel->NetDriver && ((Conn = Viewport->Actor->XLevel->NetDriver->ServerConnection) != NULL) )
		{
			INT	NumChannels = 0;

			for(INT ChannelIndex = 0;ChannelIndex < UNetConnection::MAX_CHANNELS;ChannelIndex++)
				NumChannels += (Conn->Channels[ChannelIndex] != NULL);

			if ( Viewport->Actor->PlayerReplicationInfo )
				DWORDStats(GEngineStats.STATS_Net_Ping			) = (DWORD) 4.f*Viewport->Actor->PlayerReplicationInfo->Ping;
			else
				DWORDStats(GEngineStats.STATS_Net_Ping			) = (DWORD) 1000.0f*Conn->BestLag;
			DWORDStats(GEngineStats.STATS_Net_Channels		) = (DWORD) NumChannels;
			DWORDStats(GEngineStats.STATS_Net_InUnordered	) = (DWORD) Conn->InOrder;
			DWORDStats(GEngineStats.STATS_Net_OutUnordered	) = (DWORD) Conn->OutOrder;
			DWORDStats(GEngineStats.STATS_Net_InPacketLoss	) = (DWORD) Conn->InLoss;
			DWORDStats(GEngineStats.STATS_Net_OutPacketLoss	) = (DWORD) Conn->OutLoss;
			DWORDStats(GEngineStats.STATS_Net_InPackets		) = (DWORD) Conn->InPackets;
			DWORDStats(GEngineStats.STATS_Net_OutPackets	) = (DWORD) Conn->OutPackets;
			DWORDStats(GEngineStats.STATS_Net_InBunches		) = (DWORD) Conn->InBunches;
			DWORDStats(GEngineStats.STATS_Net_OutBunches	) = (DWORD) Conn->OutBunches;
			DWORDStats(GEngineStats.STATS_Net_InBytes		) = (DWORD) Conn->InRate;
			DWORDStats(GEngineStats.STATS_Net_OutBytes		) = (DWORD) Conn->OutRate;
			DWORDStats(GEngineStats.STATS_Net_Speed			) = (DWORD) Conn->CurrentNetSpeed;			
		}
		DRAW_HEADLINE( TEXT("- Net ----------") );
		DRAW_ALL(STATSTYPE_Net);
	}

	unguard;
}

FEngineStats::FEngineStats()
{
	guard(FEngineStats::FEngineStats);
	appMemset( &STATS_FirstEntry, 0xFF, (PTRINT) &STATS_LastEntry - (PTRINT) &STATS_FirstEntry );
	unguard;
}

void FEngineStats::Init()
{
	guard(FEngineStats::Init);

	// If already initialized retrieve indices from GStats.
	if( GStats.Registered[STATSTYPE_Render] )
	{
		INT* Dummy = &STATS_Frame_TotalCycles;
		for( INT i=0; i<GStats.Stats[STATSTYPE_Render].Num(); i++ )
			*(Dummy++) = GStats.Stats[STATSTYPE_Render](i).Index;

		Dummy = &STATS_Game_ScriptCycles;
		for( INT i=0; i<GStats.Stats[STATSTYPE_Game].Num(); i++ )
			*(Dummy++) = GStats.Stats[STATSTYPE_Game](i).Index;

		Dummy = &STATS_Net_Ping;
		for( INT i=0; i<GStats.Stats[STATSTYPE_Net].Num(); i++ )
			*(Dummy++) = GStats.Stats[STATSTYPE_Net](i).Index;

		return;
	}

	// Register all stats with GStats.
	STATS_Frame_TotalCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Frame"				), TEXT("Frame"		), STATSUNIT_MSec					);
	STATS_Frame_RenderCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Frame"		), STATSUNIT_MSec					);
	
	STATS_BSP_RenderCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("BSP"		), STATSUNIT_MSec					);
	STATS_BSP_Sections						= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Sections"			), TEXT("BSP"		), STATSUNIT_Default				);
	STATS_BSP_Nodes							= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Nodes"				), TEXT("BSP"		), STATSUNIT_Default				);
	STATS_BSP_Triangles						= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"			), TEXT("BSP"		), STATSUNIT_Default				);
	STATS_BSP_DynamicLightingCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("DynamicLighting"	), TEXT("BSP"		), STATSUNIT_MSec					);
	STATS_BSP_DynamicLights					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("DynamicLights"		), TEXT("BSP"		), STATSUNIT_Default				);

	STATS_LightMap_Updates					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Updates"			), TEXT("LightMap"	), STATSUNIT_Default				);
	STATS_LightMap_Cycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Time"				), TEXT("LightMap"	), STATSUNIT_MSec					);

	STATS_Projector_RenderCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Projector"	), STATSUNIT_MSec					);
	STATS_Projector_AttachCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Attach"			), TEXT("Projector"	), STATSUNIT_MSec					);
	STATS_Projector_Projectors				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Projectors"		), TEXT("Projector"	), STATSUNIT_Default				);
	STATS_Projector_Triangles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"			), TEXT("Projector"	), STATSUNIT_Default				);
			
	STATS_Stencil_RenderCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Stencil"	), STATSUNIT_MSec					);
	STATS_Stencil_Nodes						= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Nodes"				), TEXT("Stencil"	), STATSUNIT_Default				);
	STATS_Stencil_Triangles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"			), TEXT("Stencil"	), STATSUNIT_Default				);

	STATS_Visibility_SetupCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Setup"				), TEXT("Visibility"), STATSUNIT_MSec					);
	STATS_Visibility_MaskTests				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("MaskTests"			), TEXT("Visibility"), STATSUNIT_Default				);
	STATS_Visibility_MaskRejects			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("MaskRejects"		), TEXT("Visibility"), STATSUNIT_Default				);
	STATS_Visibility_BoxTests				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BoxTests"			), TEXT("Visibility"), STATSUNIT_Default				);
	STATS_Visibility_BoxRejects				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BoxRejects"		), TEXT("Visibility"), STATSUNIT_Default				);
	STATS_Visibility_TraverseCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Traverse"			), TEXT("Visibility"), STATSUNIT_MSec					);
	STATS_Visibility_ScratchBytes			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("ScratchBytes"		), TEXT("Visibility"), STATSUNIT_KByte					);
	STATS_Visibility_MeshLightCycles		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("MeshLight"			), TEXT("Visibility"), STATSUNIT_MSec					);

	STATS_Terrain_RenderCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Terrain"	), STATSUNIT_MSec					);
	STATS_Terrain_Sectors					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Sectors"			), TEXT("Terrain"	), STATSUNIT_Default				);
	STATS_Terrain_Triangles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"			), TEXT("Terrain"	), STATSUNIT_Default				);
	STATS_Terrain_DrawPrimitives			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("DrawPrimitives"	), TEXT("Terrain"	), STATSUNIT_Default				);

	STATS_DecoLayer_RenderCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("DecoLayer"	), STATSUNIT_MSec					);
	STATS_DecoLayer_Triangles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"			), TEXT("DecoLayer"	), STATSUNIT_Default				);
	STATS_DecoLayer_Decorations				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Decorations"		), TEXT("DecoLayer"	), STATSUNIT_Default				);

	STATS_Matinee_TickCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Tick"				), TEXT("Matinee"	), STATSUNIT_MSec					);

	STATS_Mesh_SkinCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Skin"				), TEXT("Mesh"		), STATSUNIT_MSec					);
	STATS_Mesh_ResultCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Result"			), TEXT("Mesh"		), STATSUNIT_MSec					);
	STATS_Mesh_LODCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("LOD"				), TEXT("Mesh"		), STATSUNIT_MSec					);
	STATS_Mesh_SkelCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Skel"				), TEXT("Mesh"		), STATSUNIT_MSec					);
	STATS_Mesh_PoseCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Pose"				), TEXT("Mesh"		), STATSUNIT_MSec					);
	STATS_Mesh_RigidCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Rigid"				), TEXT("Mesh"		), STATSUNIT_MSec					);
	STATS_Mesh_DrawCycles					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Draw"				), TEXT("Mesh"		), STATSUNIT_MSec					);
	
	STATS_Particle_SpriteSetupCycles		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("SpriteSetup"		), TEXT("Particle"	), STATSUNIT_MSec					);
	STATS_Particle_Particles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Particles"			), TEXT("Particle"	), STATSUNIT_Default				);
    STATS_Particle_ParticlesTicked  		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("ParticlesTicked"	), TEXT("Particle"	), STATSUNIT_Default				);
	STATS_Particle_RenderCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Particle"	), STATSUNIT_MSec					);

	STATS_StaticMesh_Triangles	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"), TEXT("StaticMesh"), STATSUNIT_Default );
	STATS_StaticMesh_RenderCycles	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Triangles"), TEXT("StaticMesh"), STATSUNIT_MSec );

	STATS_StaticMesh_Batches					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Batches"					), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_BatchedSortedSections		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BatchedSortedSections"		), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_BatchedSortedTriangles		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BatchedSortedTriangles"	), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_BatchedSortCycles			= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BatchedSort"				), TEXT("StaticMesh"), STATSUNIT_MSec		);
	STATS_StaticMesh_BatchedUnsortedSections	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BatchedUnsortedSections"	), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_BatchedUnsortedTriangles	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BatchedUnsortedTriangles"	), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_BatchedRenderCycles		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("BatchedRender"				), TEXT("StaticMesh"), STATSUNIT_MSec		);

	STATS_StaticMesh_UnbatchedSortedSections	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("UnbatchedSortedSections"	), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_UnbatchedSortedTriangles	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("UnbatchedSortedTriangles"	), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_UnbatchedSortCycles		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("UnbatchedSort"				), TEXT("StaticMesh"), STATSUNIT_MSec		);
	STATS_StaticMesh_UnbatchedUnsortedSections	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("UnbatchedUnsortedSections"	), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_UnbatchedUnsortedTriangles	= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("UnbatchedUnsortedTriangles"), TEXT("StaticMesh"), STATSUNIT_Default	);
	STATS_StaticMesh_UnbatchedRenderCycles		= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("UnbatchedRender"			), TEXT("StaticMesh"), STATSUNIT_MSec		);

	STATS_Batch_Batches						= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Batches"			), TEXT("Batch"		), STATSUNIT_Default				);
	STATS_Batch_Primitives					= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Primitives"		), TEXT("Batch"		), STATSUNIT_Default				);
	STATS_Batch_RenderCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Batch"		), STATSUNIT_MSec					);

	STATS_Stats_RenderCycles				= GStats.RegisterStats( STATSTYPE_Render, STATSDATATYPE_DWORD, TEXT("Stats Render"		), TEXT("Frame"		), STATSUNIT_MSec					);

	STATS_StaticMesh_CollisionCycles		= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("StaticMesh"				), TEXT("Collision" ),	STATSUNIT_MSec			);
	STATS_StaticMesh_CollisionCacheHits		= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("StaticMesh Cache Hits"		), TEXT("" ),			STATSUNIT_Default		);
	STATS_StaticMesh_CollisionCacheMisses	= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("StaticMesh Cache Misses"	), TEXT("" ),			STATSUNIT_Default		);
	STATS_StaticMesh_CollisionCacheFlushes	= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("StaticMesh Cache Flushes"	), TEXT("" ),			STATSUNIT_Default		);
	STATS_BSP_CollisionCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("BSP"						), TEXT("Collision"	),	STATSUNIT_MSec			);
	STATS_Terrain_CollisionCycles			= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Terrain"					), TEXT("Collision"	),	STATSUNIT_MSec			);
	STATS_Corona_CollisionCycles			= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Corona"					), TEXT("Collision"	),	STATSUNIT_MSec			);

	STATS_Karma_KTriListQuery				= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("KTriListQuery"				), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_KUpdateContacts				= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("KUpdateContacts"			), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_KIntersect					= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("  KIntersect"				), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_KWorldStepSafeTime			= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("KWorldStepSafeTime"		), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_PreKarmaStep				= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("  PreKarmaStep"			), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_ConstraintSolver			= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("  ConstraintSolver"		), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_FindModelSafetime			= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("  FindModelSafetime"		), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_PostKarmaStep				= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("  PostKarmaStep"			), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_physKarma					= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("physKarma"					), TEXT("Karma"	    ), STATSUNIT_MSec					);
	STATS_Karma_Temp						= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Temp"						), TEXT("Karma"	    ), STATSUNIT_MSec					);

#ifdef WITH_LIPSINC

	STATS_LIPSinc_NumActiveLIPSincAnims = GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Active"			), TEXT("LIPSinc"	), STATSUNIT_Default				);
	STATS_LIPSinc_TotalCurveEvalCycles	= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("CurveEval"			), TEXT("LIPSinc"	), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalBlendInCycles    = GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("BlendIn"			), TEXT("LIPSinc"	), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalNormalBlendCycles= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("NormalBlend"		), TEXT("LIPSinc"	), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalBlendOutCycles   = GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("BlendOut"			), TEXT("LIPSinc"	), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalBlendCycles		= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("TotalBlend"		), TEXT("LIPSinc"	), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalUpdateCycles		= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Update"			), TEXT("LIPSinc"	), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalFinalBlendCycles = GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("FinalBlend"        ), TEXT("LIPSinc"   ), STATSUNIT_MSec					);
	STATS_LIPSinc_TotalSetupCycles		= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Total"				), TEXT("LIPSinc"	), STATSUNIT_MSec					);

#endif

	STATS_Game_ScriptCycles					= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Script"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_ScriptTickCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("ScriptTick"		), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_ActorTickCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Actor"				), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_FindPathCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Path"				), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_SeePlayerCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("See"				), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_SpawningCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Spawning"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_AudioTickCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Audio"				), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_CleanupDestroyedCycles		= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("CleanupDestroyed"	), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_UnusedCycles					= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Unused"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_NetTickCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Net"				), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_ParticleTickCycles			= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Particle"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_CanvasCycles					= GStats.RegisterStats( STATSTYPE_Game,   STATSDATATYPE_DWORD, TEXT("Canvas"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_NumMoves						= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Move"				), TEXT("Game"		), STATSUNIT_Combined_Default_MSec	);
	STATS_Game_MoveCycles					= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Move"				), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_PhysicsCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Physics"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_MLCheckCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("MLChecks"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_MPCheckCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("MPChecks"			), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_UpdateRenderData				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("RenderData"		), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_HUDPostRender				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("HUD PostRender"	), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_InteractionPreRender			= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Interaction Pre"	), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_InteractionPostRender		= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Interaction Post"	), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Game_ScriptDebugTime				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("ScriptDebug"		), TEXT("Game"		), STATSUNIT_MSec					);
	STATS_Fluid_SimulateCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Simulate"			), TEXT("Fluid"		), STATSUNIT_MSec					);
	STATS_Fluid_VertexGenCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("VertexGen"			), TEXT("Fluid"		), STATSUNIT_MSec					);
	STATS_Fluid_RenderCycles				= GStats.RegisterStats( STATSTYPE_Game,	  STATSDATATYPE_DWORD, TEXT("Render"			), TEXT("Fluid"		), STATSUNIT_MSec					);

	STATS_Net_Ping							= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Ping"				), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_Channels						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Channels"			), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_InUnordered					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Unorderd"			), TEXT("Net"		), STATSUNIT_Combined_Default_Default	);
	STATS_Net_OutUnordered					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Unordered"			), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_InPacketLoss					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("PacketLoss"		), TEXT("Net"		), STATSUNIT_Combined_Default_Default	);
	STATS_Net_OutPacketLoss					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("PacketLoss"		), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_InPackets						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Packets"			), TEXT("Net"		), STATSUNIT_Combined_Default_Default	);
	STATS_Net_OutPackets					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Packets"			), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_InBunches						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Bunches"			), TEXT("Net"		), STATSUNIT_Combined_Default_Default	);
	STATS_Net_OutBunches					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Bunches"			), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_InBytes						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Bytes"				), TEXT("Net"		), STATSUNIT_Combined_Default_Default	);
	STATS_Net_OutBytes						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Bytes"				), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_Speed							= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Speed"				), TEXT("Net"		), STATSUNIT_Default					);
	STATS_Net_NumReps						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("Reps"				), TEXT("Game"		), STATSUNIT_Default				);
	STATS_Net_NumRPC						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("RPC"				), TEXT("Game"		), STATSUNIT_Default				);
	STATS_Net_NumPV							= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("PV"				), TEXT("Game"		), STATSUNIT_Default				);
	STATS_Net_NumVoiceBunches				= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("VoiceBunches"		), TEXT("Net"		), STATSUNIT_Default				);
	STATS_Net_VoiceTime						= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("VoiceTime"			), TEXT("Net"		), STATSUNIT_MSec					);
	STATS_Net_ControlTime					= GStats.RegisterStats( STATSTYPE_Net,	  STATSDATATYPE_DWORD, TEXT("ControlTime"		), TEXT("Net"		), STATSUNIT_MSec					);

	// Initialized.
	GStats.Registered[STATSTYPE_Render] = 1;
	GStats.Registered[STATSTYPE_Game]	= 1;
	GStats.Registered[STATSTYPE_Net]	= 1;

	unguard;
}

