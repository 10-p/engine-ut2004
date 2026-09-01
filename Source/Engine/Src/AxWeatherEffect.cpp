//=============================================================================
// AxWeatherEffect - implementation
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

// Mostly rewritten for performance by Daniel Vogel.

#include "EnginePrivate.h"
#include "UnRender.h"
#include "xParticleMgr.h"

const	FLOAT	InvWeatherParticleMass	= 10.f;

static	INT		tmp_numCols				= 4.0f;    
static	INT		tmp_numRows				= 4.0f;
static	FLOAT	tmp_texU				= 0.0f;
static	FLOAT	tmp_texV				= 0.0f;
static	FLOAT	tmp_ScaleGlow			= 1.f;

#define LerpInOut( r, x ) (((x) < (r)) ? (x)/(r) : (1.f-(x))/(1.f-(r)))

#define USE_SSE_WEATHER __HAS_SSE__

/* ⚠ THE 64-BIT WEATHER PATH IS AN MSVC DIALECT SPLIT, NOT A POINTER-WIDTH ONE. Spelling it
   `_WIN64` is what stopped this file compiling for MinGW x86_64 (ufront 2026-09-01):
   GCC took MSVC's branch and failed on `m128i_u32`, while the helper the OTHER branch
   needs had been #ifdef'd away by the same test. See UT_MSVC_NO_MMX in Core.h. */

#if USE_SSE_WEATHER
const	FLOAT	INV_005					= 1.f / 0.05f;
const	FLOAT	INV_095					= 1.f / 0.95f;
static	FLOAT	FLOAT_255				= 255.f;
static	F32vec4 M00, M10, M20, M30;
static	F32vec4 SSE_ScaleGlow;

#if !UT_MSVC_NO_MMX
// For some reason this didn't get inlined!
static FORCEINLINE __m64 _mm_cvtps_pi16_EPIC(__m128 a)
{
	return _mm_packs_pi32(_mm_cvtps_pi32(a),_mm_cvtps_pi32(_mm_movehl_ps(a, a)));
}
#endif

static FORCEINLINE void TransformFPlaneSSE( FPlane& Plane )
{
	F32vec4 P_xxxx, P_yyyy, P_zzzz, P_wwww;

	P_xxxx = _mm_load_ps1( &Plane.X );
	P_yyyy = _mm_load_ps1( &Plane.Y );
	P_zzzz = _mm_load_ps1( &Plane.Z );
	P_wwww = _mm_load_ps1( &Plane.W );
			
	_mm_store_ps
	(
		&Plane.X
		,
		_mm_add_ps
		(
			_mm_add_ps
			(
				_mm_add_ps
				( 
					_mm_mul_ps( M00, P_xxxx )
					,
					_mm_mul_ps( M10, P_yyyy )
				)
				,
				_mm_mul_ps( M20, P_zzzz )
			)
			,
			_mm_mul_ps( M30, P_wwww )
		)
	);
}
#endif


static inline DWORD MakeAttenuatedColor( INT Attenuation, INT Alpha )
{
#if __INTEL_BYTE_ORDER__
	return Attenuation | (Attenuation << 8) | (Attenuation << 16) | (Alpha << 24);
#else
	if( GIsOpenGLRevComps )  // !!! FIXME: does rev_comps ever exist on x86 MacOS?
		return Attenuation | (Attenuation << 8) | (Attenuation << 16) | (Alpha << 24);
	else
		return Alpha | (Attenuation << 8) | (Attenuation << 16) | (Attenuation << 24);
#endif
}


class FWeatherQuadStream : public FVertexStream
{
public:
	AxWeatherEffect*	pFx;
	QWORD				CacheId;
	INT					Revision,
						Renderable;
	FMatrix				WorldToCamera;

	void Init( AxWeatherEffect* pInFx, int inRenderable, FMatrix& inWorldToCamera )
	{
		pFx				= pInFx;
		WorldToCamera	= inWorldToCamera;
		Renderable		= inRenderable;
		Revision++;
	}

	FWeatherQuadStream()
	{
		CacheId			= MakeCacheID(CID_RenderVertices);
		pFx				= NULL;
		Revision		= 1;    
		Renderable		= 0;
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return Revision;
	}

	virtual INT GetSize()
	{
		return Renderable * sizeof(DynamicVertex);
	}

	virtual INT GetStride()
	{
		return sizeof(DynamicVertex);
	}

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type		= CT_Float3;
		OutComponents[0].Function	= FVF_Position;
		OutComponents[1].Type		= CT_Color;
		OutComponents[1].Function	= FVF_Diffuse;
		OutComponents[2].Type		= CT_Float2;
		OutComponents[2].Function	= FVF_TexCoord0;
		return 3;
	}

	virtual void GetStreamData(void* Dest)
	{
		DynamicVertex*	pVerts		= (DynamicVertex*)Dest;
		FWeatherPcl*	p			= &pFx->pcl(0);
		INT				Rendered	= 0;

#if USE_SSE_WEATHER
		#if UT_MSVC_NO_MMX
		const UBOOL UseScalarFallback = 0;
		M00 = _mm_loadu_ps( &WorldToCamera.M[0][0] );
		M10 = _mm_loadu_ps( &WorldToCamera.M[1][0] );
		M20 = _mm_loadu_ps( &WorldToCamera.M[2][0] );
		M30 = _mm_loadu_ps( &WorldToCamera.M[3][0] );

		int numPcl = pFx->pcl.Num();
		for( int i=0; i<numPcl && Rendered<Renderable; i++ )
		{
			if( !p->Visible )
			{
				p++;
				continue;
			}

			Rendered+=4;

			INT		Col			=	p->frame / tmp_numCols,
					Row			=	p->frame - tmp_numRows * Col;

			FLOAT	LifeScale	=	p->Life < 0.05f ? p->Life * INV_005 : (1.f-p->Life) * INV_095,
					HalfSize	=	p->Size * 127.5f,
					U1			=	Row * tmp_texU,
					U2			=	U1  + tmp_texU,
					V1			=	Col * tmp_texV,
					V2			=	V1  + tmp_texV;
		
			F32vec4	TimeFactor	=	_mm_max_ps
									(
										_mm_min_ps
										(
											_mm_mul_ps
											(
												_mm_mul_ps
												(
													_mm_max_ps
													(
														_mm_load_ps1( &HalfSize )
														,
														_mm_load_ps1( &FLOAT_255 )
													)
													,
													_mm_load_ps1( &p->DistAtten )
												)
												,
												_mm_load_ps1( &LifeScale )
											)
											,
											_mm_load_ps1( &FLOAT_255 )
										)
										,
										_mm_setzero_ps()
									);


			DWORD	Color		=	_mm_packus_epi16   // __m128i _mm_packus_epi16 (__m128i a, __m128i b);
									(
										_mm_packs_epi32      // __m128i _mm_packs_epi32 (__m128i a, __m128i b);
										(
											_mm_cvtps_epi32    // __m128i _mm_cvtps_epi32 (__m128 a);
											(
												_mm_mul_ps
												(
													SSE_ScaleGlow
													,
													TimeFactor
												)
											)
											,
											_mm_setzero_si128()
										)
										,
										_mm_setzero_si128()
									).m128i_u32[0];

			if ( pFx->WeatherType == WT_Rain )
			{
				__declspec(align(16)) FPlane Start	= FPlane( p->pos, 1.f );
				__declspec(align(16)) FPlane End	= FPlane( p->pos + p->Vel * 0.03f, 1.f );

				TransformFPlaneSSE( Start );
				TransformFPlaneSSE( End );

				pVerts->Point.X	= Start.X + p->Size;
				pVerts->Point.Y	= Start.Y;
				pVerts->Point.Z	= Start.Z;
				pVerts->Color	= Color;
				pVerts->U		= U2;
				pVerts->V		= V1;
				pVerts++;

				pVerts->Point.X	= Start.X - p->Size;
				pVerts->Point.Y	= Start.Y;
				pVerts->Point.Z	= Start.Z;
				pVerts->Color	= Color;
				pVerts->U		= U1;
				pVerts->V		= V1;
				pVerts++;

				pVerts->Point.X	= End.X - p->Size;
				pVerts->Point.Y	= End.Y;
				pVerts->Point.Z	= End.Z;
				pVerts->Color	= Color;
				pVerts->U		= U1;
				pVerts->V		= V2;
				pVerts++;

				pVerts->Point.X	= End.X + p->Size;
				pVerts->Point.Y	= End.Y;
				pVerts->Point.Z	= End.Z;
				pVerts->Color	= Color;
				pVerts->U		= U2;
				pVerts->V		= V2;
				pVerts++;
			}
			else
			{
				__declspec(align(16)) FPlane Center	= FPlane( p->pos, 1.f );

				TransformFPlaneSSE( Center );
		
				pVerts->Point.X	= Center.X;
				pVerts->Point.Y	= Center.Y + p->Size;
				pVerts->Point.Z	= Center.Z;
				pVerts->Color	= Color;
				pVerts->U		= U1;
				pVerts->V		= V1;
				pVerts++;

				pVerts->Point.X	= Center.X - p->Size;
				pVerts->Point.Y	= Center.Y;
				pVerts->Point.Z	= Center.Z;
				pVerts->Color	= Color;
				pVerts->U		= U1;
				pVerts->V		= V2;
				pVerts++;

				pVerts->Point.X	= Center.X;
				pVerts->Point.Y	= Center.Y - p->Size;
				pVerts->Point.Z	= Center.Z;
				pVerts->Color	= Color;
				pVerts->U		= U2;
				pVerts->V		= V2;
				pVerts++;

				pVerts->Point.X	= Center.X + p->Size;
				pVerts->Point.Y	= Center.Y;
				pVerts->Point.Z	= Center.Z;
				pVerts->Color	= Color;
				pVerts->U		= U2;
				pVerts->V		= V1;
				pVerts++;
			}

			p++;
		}
		#else
		const UBOOL UseScalarFallback = !GIsSSE;
		if( GIsSSE )
		{
			M00 = _mm_loadu_ps( &WorldToCamera.M[0][0] );
			M10 = _mm_loadu_ps( &WorldToCamera.M[1][0] );
			M20 = _mm_loadu_ps( &WorldToCamera.M[2][0] );
			M30 = _mm_loadu_ps( &WorldToCamera.M[3][0] );

			int numPcl = pFx->pcl.Num();
			for( int i=0; i<numPcl && Rendered<Renderable; i++ )
			{
				if( !p->Visible )
				{
					p++;
					continue;
				}

				Rendered+=4;

				INT		Col			=	p->frame / tmp_numCols,
						Row			=	p->frame - tmp_numRows * Col;

				FLOAT	LifeScale	=	p->Life < 0.05f ? p->Life * INV_005 : (1.f-p->Life) * INV_095,
						HalfSize	=	p->Size * 127.5f,
						U1			=	Row * tmp_texU,
						U2			=	U1  + tmp_texU,
						V1			=	Col * tmp_texV,
						V2			=	V1  + tmp_texV;
		
				F32vec4	TimeFactor	=	_mm_max_ps
										(
											_mm_min_ps
											(
												_mm_mul_ps
												(
													_mm_mul_ps
													(
														_mm_max_ps
														( 
															_mm_load_ps1( &HalfSize )
															, 
															_mm_load_ps1( &FLOAT_255 )
														)
														,
														_mm_load_ps1( &p->DistAtten )
													)
													,
													_mm_load_ps1( &LifeScale )
												)
												,
												_mm_load_ps1( &FLOAT_255 )
											)
											,
											_mm_setzero_ps()
										);

				__m64 __color_pack =	_mm_packs_pu16
										(
											_mm_cvtps_pi16_EPIC
												(
													_mm_mul_ps
													(
														SSE_ScaleGlow
														,
														TimeFactor
													)
												)
												,
											_mm_setzero_si64()
										);
				DWORD	Color		=	_m_to_int(_m_punpckldq(__color_pack, _mm_setzero_si64())); 
										
				_mm_empty();

				if ( pFx->WeatherType == WT_Rain )
				{
					__attribute__((aligned(16))) FPlane Start	= FPlane( p->pos, 1.f );
					__attribute__((aligned(16))) FPlane End	= FPlane( p->pos + p->Vel * 0.03f, 1.f );
	
					TransformFPlaneSSE( Start );
					TransformFPlaneSSE( End );

					pVerts->Point.X	= Start.X + p->Size;
					pVerts->Point.Y	= Start.Y;
					pVerts->Point.Z	= Start.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V1;
					pVerts++;

					pVerts->Point.X	= Start.X - p->Size;
					pVerts->Point.Y	= Start.Y;
					pVerts->Point.Z	= Start.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V1;
					pVerts++;

					pVerts->Point.X	= End.X - p->Size;
					pVerts->Point.Y	= End.Y;
					pVerts->Point.Z	= End.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V2;
					pVerts++;

					pVerts->Point.X	= End.X + p->Size;
					pVerts->Point.Y	= End.Y;
					pVerts->Point.Z	= End.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V2;
					pVerts++;
				}
				else
				{				
					__attribute__((aligned(16))) FPlane Center	= FPlane( p->pos, 1.f );

					TransformFPlaneSSE( Center );
		
					pVerts->Point.X	= Center.X;
					pVerts->Point.Y	= Center.Y + p->Size;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V1;
					pVerts++;

					pVerts->Point.X	= Center.X - p->Size;
					pVerts->Point.Y	= Center.Y;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V2;
					pVerts++;

					pVerts->Point.X	= Center.X;
					pVerts->Point.Y	= Center.Y - p->Size;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V2;
					pVerts++;

					pVerts->Point.X	= Center.X + p->Size;
					pVerts->Point.Y	= Center.Y;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V1;
					pVerts++;
				}

				p++;
			}
		}
		#endif
#else
		// No SSE: always use scalar fallback
		const UBOOL UseScalarFallback = 1;
#endif

		if (UseScalarFallback)
		{
			int numPcl = pFx->pcl.Num();
			for( int i=0; i<numPcl && Rendered<Renderable; i++ )
			{
				if( !p->Visible )
				{
					p++;
					continue;
				}

				Rendered+=4;

				FLOAT	t	= Clamp( 255.f * LerpInOut( 0.05f, p->Life ) * Max( p->Size * 0.5f, 1.0f ) * p->DistAtten, 0.f, 255.f );
							
				INT		Col = p->frame / tmp_numCols,
						Row = p->frame - tmp_numRows * Col;
		
				FLOAT	U1	= Row * tmp_texU,
						U2	= U1  + tmp_texU,
						V1	= Col * tmp_texV,
						V2	= V1  + tmp_texV;
		
				INT		Alpha, 
						Attenuation;

				Alpha		= appTrunc( t );
				Attenuation	= appTrunc( t * tmp_ScaleGlow );

				//DWORD Color = Attenuation | (Attenuation << 8) | (Attenuation << 16) | (Alpha << 24);
				DWORD Color = MakeAttenuatedColor(Attenuation, Alpha);

				if ( pFx->WeatherType == WT_Rain )
				{
					FVector Start	= WorldToCamera.TransformFVector(p->pos);
					FVector End		= WorldToCamera.TransformFVector(p->pos + p->Vel*.03f);

					pVerts->Point.X	= Start.X + p->Size;
					pVerts->Point.Y	= Start.Y;
					pVerts->Point.Z	= Start.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V1;
					pVerts++;

					pVerts->Point.X	= Start.X - p->Size;
					pVerts->Point.Y	= Start.Y;
					pVerts->Point.Z	= Start.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V1;
					pVerts++;

					pVerts->Point.X	= End.X - p->Size;
					pVerts->Point.Y	= End.Y;
					pVerts->Point.Z	= End.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V2;
					pVerts++;

					pVerts->Point.X	= End.X + p->Size;
					pVerts->Point.Y	= End.Y;
					pVerts->Point.Z	= End.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V2;
					pVerts++;
				}
				else
				{				
					FVector Center	= WorldToCamera.TransformFVector(p->pos);
		
					pVerts->Point.X	= Center.X;
					pVerts->Point.Y	= Center.Y + p->Size;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V1;
					pVerts++;

					pVerts->Point.X	= Center.X - p->Size;
					pVerts->Point.Y	= Center.Y;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U1;
					pVerts->V		= V2;
					pVerts++;

					pVerts->Point.X	= Center.X;
					pVerts->Point.Y	= Center.Y - p->Size;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V2;
					pVerts++;

					pVerts->Point.X	= Center.X + p->Size;
					pVerts->Point.Y	= Center.Y;
					pVerts->Point.Z	= Center.Z;
					pVerts->Color	= Color;
					pVerts->U		= U2;
					pVerts->V		= V1;
					pVerts++;
				}

				p++;
			}
		}
	}

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = NULL;
	}
};

static FWeatherQuadStream weatherStream;

void AxWeatherEffect::CacheBlockers()
{
	// will this work in the postload?
	pclBlockers.Empty();
	checkSlow(XLevel);
	for( int i=0; i<XLevel->Actors.Num(); i++ )
		if ( XLevel->Actors(i) && (XLevel->Actors(i)->Tag == Tag) && XLevel->Actors(i)->IsA(AVolume::StaticClass()) )
				pclBlockers.AddItem( (AVolume*)XLevel->Actors(i) );
}

void AxWeatherEffect::UpdateViewer( FLevelSceneNode* SceneNode )
{
	FLOAT DeltaTime = Clamp<FLOAT>( GetLevel()->TimeSeconds - LastRenderTime, 0.0001f, 10.f );
	LastRenderTime	= GetLevel()->TimeSeconds;

	// calc velocity
	eyeDir		= FVector(SceneNode->CameraToWorld.M[2][0], SceneNode->CameraToWorld.M[2][1], SceneNode->CameraToWorld.M[2][2]);
	eyeMoveVec	= SceneNode->ViewOrigin - eyePos;
	eyePos		= SceneNode->ViewOrigin;
	eyeVel		= eyeMoveVec.Size();
	if ( eyeVel > 0.001f )
	{
		eyeMoveVec *= 1.0f / eyeVel;
	}
	else
	{
		eyeVel		= 0.0f;
		eyeMoveVec	= FVector(0,0,0);
	}
	
	// move the spawn origin according to velocity and fudge
	spawnOrigin = SceneNode->ViewOrigin;    
	spawnVecU	= FVector( 280, 0, 0 );
	spawnVecV	= FVector( 0, 0, 280 );

	// extrapolate next eye position and bias spawn positions in that dir
	if ( eyeVel > 0.0f )
		spawnOrigin += eyeMoveVec * Clamp( eyeVel * (1.0f / DeltaTime), 0.f, 300.f );

	if ( PhysicsVolume && PhysicsVolume->Brush )
	{
		if ( !PhysicsVolume->IsA(ADefaultPhysicsVolume::StaticClass()) )
		{
			FCheckResult Hit(0);
			// check for containment
			if ( PhysicsVolume->Brush->PointCheck(Hit,PhysicsVolume,spawnOrigin,FVector(0,0,0),0)!=0 )
			{
				// if not contained, find nearest point on the volume for spawn origin
				FCheckResult Hit(0);
				FVector Start	= spawnOrigin,
						End		= Location;
				if( PhysicsVolume->Brush->LineCheck(Hit,PhysicsVolume,End,Start,FVector(0,0,0),0,0)==0 )
					spawnOrigin = Hit.Location;
			}
		}
	}
}

void AxWeatherEffect::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	Super::SetZone(bTest,bForceRefresh);
/*
	if ( GIsEditor && XLevel )
	{
		APhysicsVolume* NewVolume = PhysicsVolume;
		for ( INT i=0; i<XLevel->Actors.Num(); i++ )
		{
			APhysicsVolume *Next = Cast<APhysicsVolume>(XLevel->Actors(i));
			if ( Next && Next->Encompasses(Location) && (Next->Priority > NewVolume->Priority) )
				NewVolume = Next;
		}
		PhysicsVolume = NewVolume;
	}

	CacheBlockers();
*/
}

void AxWeatherEffect::PreCalc()
{
	if ( pcl.Num() != numParticles )
	{
		pcl.Empty();
		pcl.AddZeroed( numParticles );
	}

	eyePos		= Location;
	spawnOrigin = Location;
   	numFrames	= (int)(numCols * numRows);
	texU		= 1.0f / numCols;
	texV		= 1.0f / numRows;

	if ( XLevel )
		CacheBlockers();
}

void AxWeatherEffect::PostEditLoad()
{
	SetZone(0,0);
	PreCalc();
	CacheBlockers();
}

void AxWeatherEffect::PostEditChange()
{
	Super::PostEditChange();
	PreCalc();
	SetZone(0,0);
}

void AxWeatherEffect::PostLoad()
{
	Super::PostLoad();
	PreCalc();
}

void AxWeatherEffect::Destroy()
{
	Super::Destroy();
}

void AxWeatherEffect::Spawned()
{
	Super::Spawned();
	PreCalc();
	for ( int i=0; i<pcl.Num(); i++ )
		InitParticle( pcl(i) );
}

FORCEINLINE void AxWeatherEffect::InitParticle( FWeatherPcl& t )
{
	t.pos			= spawnOrigin + Position.GetRand();
	t.Vel			= spawnVel * Speed.GetRand();
	t.Life			= 1.0f;
	FLOAT LifeSecs	= Life.GetRand();
	t.InvLifeSpan	= 1.0f / LifeSecs;
	t.Size			= Size.GetRand();
	t.HitTime		= 0.0f;
	t.frame			= qRand() % (int)numFrames;

	for( int i=0; i<pclBlockers.Num(); i++ )
	{
		// this does not support rotated volumes for extra speed
		FCheckResult Hit(0);
		FVector Start	= t.pos - pclBlockers(i)->Location + pclBlockers(i)->PrePivot,
				End		= Start + t.Vel * LifeSecs;
		if ( pclBlockers(i)->Brush->LineCheck(Hit,NULL,End,Start,FVector(0,0,0),0,0)==0 )
		{
			t.HitTime = 1.0f - Hit.Time;
			break; // this is questionable, should check for closer impacts, but I don't want to slow this down anymore
		}
	}
}

UBOOL AxWeatherEffect::Tick( FLOAT deltaTime, ELevelTick TickType )
{
	guard(AxWeatherEffect::Tick);
	if ( bHidden || (Level->NetMode == NM_DedicatedServer) || (Level->DetailMode == DM_Low) || (bSuperHighDetail && (Level->DetailMode != DM_SuperHigh)) || (UTexture::__Client && !UTexture::__Client->WeatherEffects) )
		return Super::Tick( deltaTime, TickType );

	INT SetupStartTime = appCycles();

	Box.IsValid	= 1;
	Box.Min		= Location;
	Box.Max		= Location;
	numActive	= 0;
	
	FWeatherPcl*	p				= &pcl(0);
	FLOAT			invEyeDistSqr	= 1.0f / Square(maxPclEyeDist);
	INT				numPcl			= pcl.Num();
	for( INT i=0; i<numPcl; i++ )
	{
		p->Life -= deltaTime * p->InvLifeSpan;
	    
		// recycle expired
		if ( p->Life <= 0.0f )
		{
			p->pos			= spawnOrigin + Position.GetRand();
			p->Vel			= spawnVel * Speed.GetRand();
			p->Life			= 1.0f;
			FLOAT LifeSecs	= Life.GetRand();
			p->InvLifeSpan	= 1.0f / LifeSecs;
			p->Size			= Size.GetRand();
			p->HitTime		= 0.0f;
			p->frame		= qRand() % (int)numFrames;

			for ( int j=0; j<pclBlockers.Num(); j++ )
			{
				// this does not support rotated volumes for extra speed
				FCheckResult Hit(0);
				FVector Start	= p->pos - pclBlockers(j)->Location + pclBlockers(j)->PrePivot,
						End		= Start + p->Vel * LifeSecs;
				if ( pclBlockers(j)->Brush->LineCheck(Hit,NULL,End,Start,FVector(0,0,0),0,0)==0 )
				{
					p->HitTime = 1.0f - Hit.Time;
					break; // this is questionable, should check for closer impacts, but I don't want to slow this down anymore
				}
			}
       		numActive++;
			Box += p->pos;
			p++;
			continue;
		}

		// skip updates for particles in collision stasis
		if ( p->Life < p->HitTime )
		{
			numActive++;
			p++;
			continue;
		}

		// do update
		p->pos += p->Vel * deltaTime;
		p->DistAtten = 1.0f - ((p->pos - eyePos).SizeSquared()*invEyeDistSqr);
		if ( p->DistAtten <= 0.0f )
			p->Life = 0.0f;

		// noise
		if ( WeatherType == WT_Snow && qFRand() <= deviation)
		{
			float burst = 20.0f;
			p->Vel.X += -burst + qFRand() * ( burst + burst );
			p->Vel.Y += -burst + qFRand() * ( burst + burst );
			if ( Abs(p->Vel.X) > 40.0f )
				p->Vel.X *= 0.75f;
			if ( Abs(p->Vel.Y) > 40.0f )
				p->Vel.Y *= 0.75f;
		}

		numActive++;
		Box += p->pos;
		p++;
	}

	GStats.DWORDStats( GEngineStats.STATS_Particle_SpriteSetupCycles ) += appCycles() - SetupStartTime;

	unguard;

	return Super::Tick( deltaTime, TickType );
	
}

void AxWeatherEffect::Render( FLevelSceneNode* SceneNode, FRenderInterface* RI )
{
	if ( SceneNode->Viewport->IsOrtho() || (UTexture::__Client && !UTexture::__Client->WeatherEffects) )
		return;

	INT	RenderStartTime = appCycles();

	UpdateViewer( SceneNode );
	LastRenderTime = GetLevel()->TimeSeconds;

	if( !GIsEditor )
		GetLevel()->FarMoveActor( this, SceneNode->ViewOrigin);

	if ( Skins.Num() == 0 || !Skins(0) )
		return;

	tmp_texU			= texU;
	tmp_texV			= texV;
	tmp_numCols			= numCols;
	tmp_numRows			= numRows;

#if USE_SSE_WEATHER
	if( GIsSSE )
	{
		FPlane Dummy	= FPlane( ScaleGlow, ScaleGlow, ScaleGlow, 1.f );
		appMemcpy( &SSE_ScaleGlow, &Dummy, sizeof(F32vec4) );
	}
#endif
	tmp_ScaleGlow		= ScaleGlow;

	RI->SetMaterial( Skins(0) );
	RI->EnableLighting(0);

	RI->SetTransform(TT_WorldToCamera,FMatrix::Identity);
	RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);

	// cull behind particles, need to first determine 'renderable' amount for dynamic buffer lock.
	FPlane			eyePlane( SceneNode->ViewOrigin, eyeDir);
	INT				NumVisible	= 0;
	FWeatherPcl*	p			= &pcl(0);
	
	int pclNum = pcl.Num();
	for( INT i=0; i<pclNum; i++ )
	{
		if( p->Life <= 0.0f || eyePlane.PlaneDot( p->pos ) < 0.0f || (p->Life < p->HitTime) )
		{
			p->Visible = 0;
		}
		else
		{
			p->Visible = 1;
			NumVisible++;
		}
		p++;
	}

	if( NumVisible )
	{
		weatherStream.Init(this,4*Min(5400,NumVisible),SceneNode->WorldToCamera);
		INT	FirstVertex = SceneNode->Viewport->RI->SetDynamicStream(VS_FixedFunction,&weatherStream);
		SceneNode->Viewport->RI->DrawQuads( FirstVertex, NumVisible );

		// Reset the camera transform.
		RI->SetTransform(TT_WorldToCamera, SceneNode->WorldToCamera);
	}

	GStats.DWORDStats( GEngineStats.STATS_Particle_Particles	) += NumVisible;
	GStats.DWORDStats( GEngineStats.STATS_Particle_RenderCycles ) += (appCycles() - RenderStartTime);
}

IMPLEMENT_CLASS(AxWeatherEffect);

