/*============================================================================
	AFluidSurfaceInfo.cpp

============================================================================*/
#include "EnginePrivate.h"


IMPLEMENT_CLASS(AFluidSurfaceInfo);
IMPLEMENT_CLASS(UFluidSurfacePrimitive);
IMPLEMENT_CLASS(AFluidSurfaceOscillator);

#define ROOT3OVER2			(0.866025f)
#define FLUIDBOXHEIGHT		(5)

#define SOFTWARE_NORMALIZE	(0)
#define SHOW_NORMALS		(0)

const DOUBLE MyU2Rad = (DOUBLE)0.000095875262;

inline UBOOL UsingLowDetail(AFluidSurfaceInfo* Fluid)
{
	ULevel* level = Fluid->GetLevel();
	if(!level)
		return 0;

	ALevelInfo* info = level->GetLevelInfo();
	if(!info)
		return 0;

	if(info->PhysicsDetailLevel == PDL_Low)
		return 1;
	else
		return 0;
}

/* ********************* UFLUIDSURFACEPRIM ***************** */

UBOOL UFluidSurfacePrimitive::LineCheck
(FCheckResult	&Result,
 AActor			*Owner,
 const FVector&	End,
 const FVector&	Start,
 const FVector&	Extent,
 DWORD          ExtraNodeFlags,
 DWORD			TraceFlags)
{
	guard(UFluidSurfacePrimitive::LineCheck);
	check(FluidInfo==Owner);

	FMatrix world2Local = Owner->WorldToLocal();
	FVector localExtent;

	// Slightly hacky way to work out local-space bounding extent
	if(!Extent.IsZero())
	{
		FBox ExtentBox(-1.f * Extent, Extent);
		FBox localExtentBox = ExtentBox.TransformBy(world2Local);
		localExtent = 0.5f * (localExtentBox.Max - localExtentBox.Min);
	}
	else
	{
		localExtent = FVector(0, 0, 0);
	}

	FVector localStart = world2Local.TransformFVector(Start);
	FVector localEnd = world2Local.TransformFVector(End);

	FVector hitLoc, hitNormal;
	FLOAT hitTime;
	UBOOL hit = FLineExtentBoxIntersection(FluidInfo->FluidBoundingBox, 
		localStart, localEnd, localExtent, 
		hitLoc, hitNormal, hitTime);

	if(hit)
	{
		// Extent fluid surface bounding box by trace extent
		Result.Actor = FluidInfo;
		Result.Item = 0;
		Result.Location = Owner->LocalToWorld().TransformFVector(hitLoc);
		Result.Material = (FluidInfo->Skins.Num() > 0)?FluidInfo->Skins(0):NULL;
		Result.Normal = hitNormal;
		Result.Time = hitTime;

		//debugf(TEXT("FS HitLoc: %f %f %f"), Result.Location.X, Result.Location.Y, Result.Location.Z);

		return 0;
	}
	else
		return 1;

	unguard;
}

UBOOL UFluidSurfacePrimitive::PointCheck
(FCheckResult	&Result,
 AActor			*Owner,
 const FVector&	Location,
 const FVector& Extent,
 DWORD          ExtraNodeFlags)
{
	guard(UFluidSurfacePrimitive::PointCheck);

	// Create FBox for point
	FBox checkBox(Location - Extent, Location + Extent);
	FBox localCheckBox = checkBox.TransformBy(Owner->ToLocal());

	// Check against water bounding box
	bool result = FluidInfo->FluidBoundingBox.Intersect(localCheckBox);

	if(result)
	{
		Result.Actor = FluidInfo;
		Result.Normal = FVector(0, 0, 1);
		Result.Location = FVector(Location.X, Location.Y, Location.Z - Extent.Z);

		return 0;
	}
	else
		return 1;

	unguard;
}

FBox UFluidSurfacePrimitive::GetRenderBoundingBox( const AActor* Owner )
{
	guard(UFluidSurfacePrimitive::GetRenderBoundingBox);

	return FluidInfo->FluidBoundingBox;
	
	unguard;
}

FSphere UFluidSurfacePrimitive::GetRenderBoundingSphere( const AActor* Owner )
{
	guard(UFluidSurfacePrimitive::GetRenderBoundingBox);

	FSphere s(&FluidInfo->FluidBoundingBox.GetExtrema(0), 2);
	return s;

	unguard;
}

UBOOL UFluidSurfacePrimitive::UseCylinderCollision( const AActor* Owner )
{
	guardSlow(UFluidSurfacePrimitive::UseCylinderCollision);

	return false;
	unguardSlow;
}

FBox UFluidSurfacePrimitive::GetCollisionBoundingBox( const AActor* Owner ) const
{
	guard(UFluidSurfacePrimitive::GetCollisionBoundingBox);

	return FluidInfo->FluidBoundingBox.TransformBy(Owner->ToWorld());

	unguard;
}

void UFluidSurfacePrimitive::Serialize(FArchive& Ar)
{
	guard(UFluidSurfacePrimitive::Serialize);
	Super::Serialize(Ar);
	//Ar << FluidInfo;
	unguard;
}

/* ********************* AFLUIDSURFACEINFO ********************* */

void AFluidSurfaceInfo::execPling( FFrame& Stack, RESULT_DECL )
{
	guard(AFluidSurfaceInfo::execPling);

	P_GET_VECTOR(Position);
	P_GET_FLOAT(Strength);
	P_GET_FLOAT_OPTX(Radius,0);
	P_FINISH;

	Pling(Position, Strength, Radius);

	unguard;
}
IMPLEMENT_FUNCTION( AFluidSurfaceInfo, -1, execPling );

void AFluidSurfaceInfo::Pling(const FVector& Position, FLOAT Strength, FLOAT Radius)
{
	guard(AFluidSurfaceInfo::Pling);

	// Do nothing if on low detail level.
	if( UsingLowDetail(this) )
		return;

	// Transform pling position to local space.
	INT hitY, hitX;
	GetNearestIndex(Position, hitX, hitY);

	FVector localHitPos = WorldToLocal().TransformFVector(Position);

	if(Radius > 0.01f)
	{
		// Calculate square of verts that are affected by this pling!
		INT radX, minX, maxX, radY, minY, maxY;

		radX = appCeil(Radius/FluidGridSpacing);

		if(FluidGridType == FGT_Hexagonal)
			radY = appCeil(Radius/(ROOT3OVER2 * FluidGridSpacing));
		else
			radY = appCeil(Radius/FluidGridSpacing);

		minX = hitX - radX;
		maxX = hitX + radX;

		minY = hitY - radY;
		maxY = hitY + radY;

		for(INT y = minY; y <= maxY; y++)
		{
			for(INT x = minX; x <= maxX; x++)
			{
				FVector localVertPos = GetVertexPosLocal(x, y);
				FLOAT a2 = Square(localVertPos.X - localHitPos.X) + Square(localVertPos.Y - localHitPos.Y);
				
				if(a2 < (Radius * Radius))
				{
					FLOAT h = appSqrt((Radius * Radius) - a2);
					PlingVertex(x, y, h/Radius*Strength);
				}
			}
		}	
	}
	else
	{
		PlingVertex(hitX, hitY, Strength);
	}

	unguard;
}

void AFluidSurfaceInfo::PlingVertex(INT x, INT y, FLOAT Strength)
{
	guard(AFluidSurfaceInfo::PlingVertex);

	// Do nothing if on low detail level.
	if( UsingLowDetail(this) )
		return;

	// If we are trying to pling outside the fluid surface - do nothing.
	if(x < 1 || x > FluidXSize-2)
		return;
	if(y < 1 || y > FluidYSize-2)
		return;

	// If we are trying to pling a clamped vertex - do nothing.
	if(GetClampedBitmap(x, y))
		return;

	if(LatestVerts == 0)
		Verts1(y * FluidXSize + x) += Strength*(1/UpdateRate);
	else
		Verts0(y * FluidXSize + x) += Strength*(1/UpdateRate);

	unguard;
}

FVector AFluidSurfaceInfo::GetVertexPosLocal(INT x, INT y)
{
	guard(AFluidSurfaceInfo::GetVertexPosLocal);

	FVector localPos;

	// First, find water vertex we are interested in.
	if(FluidGridType == FGT_Hexagonal)
	{
		localPos.X = FluidOrigin.X + x * FluidGridSpacing;
		if(x & 0x01)
			localPos.X += 0.5 * FluidGridSpacing;
		
		localPos.Y = FluidOrigin.Y + ROOT3OVER2 * y * FluidGridSpacing;
		localPos.Z = FluidOrigin.Z;
	}
	else
	{
		localPos.X = FluidOrigin.X + x * FluidGridSpacing;
		localPos.Y = FluidOrigin.Y + y * FluidGridSpacing;
		localPos.Z = FluidOrigin.Z;
	}

	return localPos;

	unguard;
}

FVector AFluidSurfaceInfo::GetVertexPosWorld(INT x, INT y)
{
	guard(AFluidSurfaceInfo::GetVertexPosWorld);

	return LocalToWorld().TransformFVector(GetVertexPosLocal(x,y));

	unguard;
}

void AFluidSurfaceInfo::GetNearestIndex(const FVector& pos, INT& xIndex, INT& yIndex)
{
	guard(AFluidSurfaceInfo::GetNearestIndex);
	
	FVector localPos = WorldToLocal().TransformFVector(pos);

	xIndex = appRound((localPos.X - FluidOrigin.X)/FluidGridSpacing);
	xIndex = Clamp(xIndex, 0, FluidXSize-1);
	
	if(FluidGridType == FGT_Hexagonal)
		yIndex = appRound((localPos.Y - FluidOrigin.Y)/(ROOT3OVER2 * FluidGridSpacing));
	else
		yIndex = appRound((localPos.Y - FluidOrigin.Y)/FluidGridSpacing);

	yIndex = Clamp(yIndex, 0, FluidYSize-1);

	unguard;
}

// Rebuild list of oscillators that affect this fluid surface.
void AFluidSurfaceInfo::UpdateOscillatorList()
{
	guard(AFluidSurfaceInfo::UpdateOscillatorList);

	// Initialise te array of osciallators that affect this fluid surface.
	ULevel* level = GetLevel();
	if(!level)
		return;
	
	for( INT i=0; i<level->Actors.Num(); i++ )
	{
		if( level->Actors(i) && 
			!level->Actors(i)->bDeleteMe && 
			level->Actors(i)->IsA(AFluidSurfaceOscillator::StaticClass()) )
		{
			AFluidSurfaceOscillator* thisOsc = Cast<AFluidSurfaceOscillator>(level->Actors(i));

			if(thisOsc->FluidInfo == this)
				Oscillators.AddItem(thisOsc);
		}
	}	

	unguard;
}


void AFluidSurfaceInfo::RebuildClampedBitmap()
{
	guard(AFluidSurfaceInfo::RebuildClampedBitmap);

	// Only do this in the editor. Also, dont do it if its hidden (for Alan).
	if(!GIsEditor || bHidden)
		return;

	GWarn->BeginSlowTask(TEXT("Rebuilding Fluid Clamp Bitmap"), 1);

	INT x, y;

	// Reset clamp bitmap to zero
	INT BitmapSize = (FluidXSize * FluidYSize + 31 ) / 32;
	appMemset( &ClampBitmap(0), 0, BitmapSize * sizeof(DWORD) );

	// Ensure all heights are zeroed. Looks weird otherwise.
	appMemset( &Verts0(0), 0, FluidXSize * FluidYSize * sizeof(FLOAT) );
	appMemset( &Verts1(0), 0, FluidXSize * FluidYSize * sizeof(FLOAT) );

	// Iterate over 'touching actors' list, checking for blocking volumes.
	// JTODO: This isn't very effecient.
	for (INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		AActor* actor = GetLevel()->Actors(i);

		// If this actor is a blocking volume, or has (and is using) simple collision model, check fluid against it.
		ABlockingVolume* bvol = Cast<ABlockingVolume>(actor);

		if(	actor && !actor->bDeleteMe && (
			( bvol && bvol->bClampFluid ) || 
			(actor->StaticMesh && actor->IsA(AStaticMeshActor::StaticClass()) && actor->StaticMesh->CollisionModel && actor->StaticMesh->UseSimpleBoxCollision) ) )
		{
			for(x=1; x<FluidXSize-1; x++)
			{
				for(y=1; y<FluidYSize-1; y++)
				{
					// Dont bother doing point test if this vertex is already clamped.
					if(!GetClampedBitmap(x,y))
					{
						FCheckResult TestHit(1.f);

						FVector posWorld = GetVertexPosWorld(x, y);

						UBOOL noHit = actor->GetPrimitive()->PointCheck(
							TestHit, actor, posWorld, FVector(0, 0, 0), 0);

						if(!noHit)
							SetClampedBitmap(x, y, 1);
					}
				}
			}
		}
	}


	// Check fluid surface against level BSP and terrain (if present).
	// We dont check the border - those are implicitly clamped.
	INT vCount = 0;

	for(x=1; x<FluidXSize-1; x++)
	{
		for(y=1; y<FluidYSize-1; y++)
		{
			GWarn->StatusUpdatef( vCount, (FluidXSize-2) * (FluidYSize-2), 
				TEXT("Fluid Checking Against Level.") );
			vCount++;

			if(GetClampedBitmap(x,y))
				continue;

			FCheckResult TestHit(1.f);
			UBOOL noHit;

			// Get position of surface vertex.
			FVector posWorld = GetVertexPosWorld(x, y);

			// Check against level BSP.
			if(XLevel && XLevel->Model)
			{
				noHit = XLevel->Model->PointCheck(TestHit, NULL, posWorld, FVector(0, 0, 0), 0);
				if(!noHit)
				{
					SetClampedBitmap(x, y, 1);
					continue;
				}
			}

			// If we have a terrain to check against, do it here.
			if(ClampTerrain && ClampTerrain->IsA(ATerrainInfo::StaticClass()) && !ClampTerrain->bDeleteMe)
			{
				// Do a line check against the terrain from world top to bottom, over the water vertex.
				FVector checkStart = posWorld;
				checkStart.Z = WORLD_MAX;

				FVector checkEnd = posWorld;
				checkEnd.Z = -WORLD_MAX;

				noHit = ClampTerrain->GetPrimitive()->LineCheck(TestHit, ClampTerrain, 
					checkEnd, checkStart, FVector(0, 0, 0), TRACE_World, 0);

				// Hmm.. we should never miss!
				if(!noHit && TestHit.Location.Z > posWorld.Z)
				{
					SetClampedBitmap(x, y, 1);
				}
			}
		}
	}
	
	GWarn->EndSlowTask();

	unguard;
}

// Render water
void AFluidSurfaceInfo::Render(FDynamicActor* Actor,
							   class FLevelSceneNode* SceneNode,
							   TList<class FDynamicLight*>* Lights,
							   FRenderInterface* RI)
{
	guard(AFluidSurfaceInfo::Render);

	// See if the camera is below the surface of the water, and dont draw it if so.
	if(bUseNoRenderZ)
	{
		FVector CamPos = SceneNode->CameraToWorld.TransformFVector(FVector(0,0,0));
		if(CamPos.Z < NoRenderZ)
			return;
	}

	INT	StartTime = appCycles();

	if( FluidXSize > 0 && FluidYSize > 0 && Verts0.Num() > 0)
	{
		// Create the dynamic vertex stream and index buffer.
		FFluidSurfaceVertexStream FluidSurfaceVertices(this);
		FFluidSurfaceIndexBuffer  FluidSurfaceIndices(this);

		RI->PushState();

		// Rotate & translate if needed.
		RI->SetTransform(TT_LocalToWorld, LocalToWorld());
		
		// Set lighting
		if(GetAmbientLightingActor()->bUnlit || !SceneNode->Viewport->IsLit())
		{
			RI->EnableLighting(0, 0);
		}
		else
		{
			// JTODO: I just can't find a sphere that makes lighting nice and even...
			FLOAT tmpRad = Max(
				0.5f * (FluidBoundingBox.Max.X - FluidBoundingBox.Min.X), 
				0.5f * (FluidBoundingBox.Max.Y - FluidBoundingBox.Min.Y) );

			RI->EnableLighting(1,1,1, NULL, 
				SceneNode->Viewport->Actor->RendMap == REN_LightingOnly, 
				FSphere(FVector(0, 0, 0), tmpRad) );
				//FSphere(Location, 1000) );
				//FSphere(&FluidBoundingBox.GetExtrema(0), 2) );
				//GetPrimitive()->GetRenderBoundingSphere(this) );

			int i=0;
			while(Lights)
			{
				RI->SetLight(i, Lights->Element);
				Lights = Lights->Next;
				i++;
			}
		}
			
		// Set texture and blending.
		if(SceneNode->Viewport->IsWire())
		{
			DECLARE_STATIC_UOBJECT(
			UShader,
			MeshWireframeShader,
			{ MeshWireframeShader->Wireframe = 1; });

			RI->SetMaterial(MeshWireframeShader);
		}
		else
		{
			RI->SetMaterial(Skins.Num()?Skins(0):NULL);
		}

		// We might be underwater - so dont back face cull
		// TODO: Change culling mode when underwater.
		//RI->SetCullMode(CM_None);

		// Set the particle vertex stream and index buffer. The particle vertices aren't actually 
		// generated until now.
		INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&FluidSurfaceVertices);
		INT BaseIndex		= RI->SetDynamicIndexBuffer(&FluidSurfaceIndices,BaseVertexIndex);

		if( UsingLowDetail(this) )
		{
			RI->DrawPrimitive(
				PT_TriangleList, 
				BaseIndex,
				2,
				0,
				3);
		}
		else
		{
			// Two tris per square.
			INT NumTris = (FluidXSize-1) * (FluidYSize-1) * 2;

			RI->DrawPrimitive(
				PT_TriangleList, 
				BaseIndex,
				NumTris,
				0,
				(FluidXSize * FluidYSize) - 1);
		}
		
		RI->PopState();
	}

	if(bShowBoundingBox)
	{
		FLineBatcher nevel(RI);
		nevel.DrawBox(FluidBoundingBox.TransformBy(LocalToWorld()), FColor(255,255,255));
	}

	GStats.DWORDStats( GEngineStats.STATS_Fluid_RenderCycles ) += (appCycles() - StartTime);

	unguard;
}

void AFluidSurfaceInfo::Init()
{
	guard(AFluidSurfaceInfo::Init);

	// Ensure sizes with limits
	if(FluidXSize < 0)
		FluidXSize = 0;
	else if(FluidXSize > 255)
		FluidXSize = 255;

	if(FluidYSize < 0)
		FluidYSize = 0;
	else if(FluidYSize > 255)
		FluidYSize = 255;
	
	// Reallocate height arrays if necessary
	int totalVerts = FluidXSize * FluidYSize;
	if(totalVerts != Verts0.Num())
	{
		if(Verts0.Num() > 0)
		{
			Verts0.Empty();
			Verts1.Empty();
			ClampBitmap.Empty();
			VertAlpha.Empty();
		}

		Verts0.AddZeroed(totalVerts);		
		Verts1.AddZeroed(totalVerts);
		VertAlpha.AddZeroed(totalVerts);

		INT BitmapSize = (totalVerts + 31) / 32;
		ClampBitmap.AddZeroed(BitmapSize);

		LatestVerts = 0;
	}
	
	// Calculate 'origin' aka. bottom left (min) corner
	FLOAT radX;
	FLOAT radY;

	if(FluidGridType == FGT_Hexagonal)
	{
		radX = (0.5f*(FluidXSize-1)*FluidGridSpacing);
		radY = ROOT3OVER2 * (0.5f*(FluidYSize-1)*FluidGridSpacing);
	}
	else
	{
		radX = (0.5f*(FluidXSize-1)*FluidGridSpacing);
		radY = (0.5f*(FluidYSize-1)*FluidGridSpacing);
	}

	FluidOrigin.X = -radX;
	FluidOrigin.Y = -radY;
	FluidOrigin.Z = 0;
	
	// Calc bounding box
	FVector p;
	FluidBoundingBox.Init();		
	
	p = FluidOrigin;
	p.Z = -FLUIDBOXHEIGHT;
	FluidBoundingBox += p;

	p.X = radX;
	p.Y = radY;
	p.Z = FLUIDBOXHEIGHT;
	FluidBoundingBox += p;

	if(GetLevel())
		ClearRenderData();

	unguard;
}

void AFluidSurfaceInfo::PostLoad()
{
	guard(AFluidSurfaceInfo::PostBeginPlay);

	Super::PostLoad();
	Init();

	unguard;
}

void AFluidSurfaceInfo::PostEditChange()
{
	guard(AFluidSurfaceInfo::PostEditChange);

	Super::PostEditChange();
	Init();

	unguard;
}

void AFluidSurfaceInfo::PostEditMove()
{
	guard(AFluidSurfaceInfo::PostEditMove);

	Super::PostEditMove();
	Init();

	unguard;
}

void AFluidSurfaceInfo::Spawned()
{
	guard(AFluidSurfaceInfo::Spawned);

	Super::Spawned();
	Init();

	unguard;
}

void AFluidSurfaceInfo::Destroy()
{
	guard(AFluidSurfaceInfo::Destroy);

	Super::Destroy();

	unguard;
}

UPrimitive* AFluidSurfaceInfo::GetPrimitive()
{	
	guard(AFluidSurfaceInfo::GetPrimitive);

	if(!Primitive)
	{
		Primitive = ConstructObject<UFluidSurfacePrimitive>(
			UFluidSurfacePrimitive::StaticClass(), GetOuter());
		Primitive->FluidInfo = this;
	}

	return Primitive;

	unguard;
};

/* *********************** SIMULATION ********************* */


// velocity
//#define GENALPHA(x,y) (Clamp<INT>(Abs(appRound((SURFHEIGHT(OldH, (x), (y)) - SURFHEIGHT(H, (x), (y))) * recipStep * VelocityAlphaScale)), 0, 255))

// height
//#define GENALPHA(x,y) (Clamp<INT>(appRound(SURFHEIGHT(OldH, (x), (y)) * AlphaScale), 0, AlphaMax))

// height and curvature (additive)
#define GENALPHA(C,H) (Clamp<INT>(appRound((C) * AlphaCurveScale + (H) * AlphaHeightScale), 0, AlphaMax))

#define SURFHEIGHT(H,x,y)	( (*H)( (y)*FluidXSize + (x) ) )
#define VERTALPHA(x,y)		( VertAlpha( (y)*FluidXSize + (x) ) )

void AFluidSurfaceInfo::UpdateSimulation( FLOAT DeltaTime )
{
	guard(AFluidSurfaceInfo::UpdateSimulation);

	// Do nothing if on low detail level.
	if( UsingLowDetail(this) )
		return;

	// Add ripples for actors in the water
	for (INT i=0; i<Touching.Num(); i++)
	{
		AActor* actor = Touching(i);
		
		// HACK: Projectiles only make ripples for initial touch.
		if(actor && actor->bDisturbFluidSurface && !actor->bDeleteMe && !actor->GetAProjectile() )
		{
			// Ripple based on velocity in place of fluid. Transform velocity vector into local space
			// and project down. But use magnitude from world space, so drawscale doesn't affect.
			FVector LocalVel = actor->Velocity.TransformVectorBy(ToLocal());
			FLOAT HorzVelMag = LocalVel.Size2D();
			Pling(actor->Location, RippleVelocityFactor * HorzVelMag * actor->FluidSurfaceShootStrengthMod, actor->CollisionRadius);
		}
	}

	// Do surface noise
	INT noisePlings = appFloor(FluidNoiseFrequency * DeltaTime);
	for(INT i=0; i<noisePlings; i++)
	{
		INT nX = 1+(appRand()%(FluidXSize-2));
		INT nY = 1+(appRand()%(FluidYSize-2));
		if(!GetClampedBitmap(nX, nY))
		{
			FLOAT mag = FluidNoiseStrength.GetRand();
			PlingVertex(nX, nY, mag);
		}
	}

	// Do test ripple (moving around in a circle)
	if(GIsEditor && TestRipple)
	{
		TestRippleAng += DeltaTime * MyU2Rad * TestRippleSpeed;
		
		FVector worldRipplePos, localRipplePos;
		
		FLOAT RippleRadius = 0.3f * (FluidXSize-1) * FluidGridSpacing;
		if(FluidGridType == FGT_Hexagonal)
			RippleRadius = Max(RippleRadius, 0.3f * (FluidYSize-1) * FluidGridSpacing * ROOT3OVER2);
		else
			RippleRadius = Max(RippleRadius, 0.3f * (FluidYSize-1) * FluidGridSpacing);

		localRipplePos.X = (RippleRadius * appSin(TestRippleAng));
		localRipplePos.Y = (RippleRadius * appCos(TestRippleAng));
		localRipplePos.Z = 0;

		worldRipplePos = LocalToWorld().TransformFVector(localRipplePos);

		Pling(worldRipplePos, TestRippleStrength, TestRippleRadius);
	}

	// Add oscillator effect.
	for(INT i=0; i<Oscillators.Num(); i++)
	{
		if(Oscillators(i))
			Oscillators(i)->UpdateOscillation(DeltaTime);
	}

	// Flip old/new store
	TArrayNoInit<FLOAT> *H, *OldH; 
	
	if(LatestVerts == 0)
	{
		H = &(Verts0);
		OldH = &(Verts1);
		LatestVerts = 1;
	}
	else
	{
		H = &(Verts1);
		OldH = &(Verts0);
		LatestVerts = 0;
	}

	// Update water surface simulation
	INT	StartUpdateTime = appCycles();

	INT x, y;
	FLOAT curve;
	const FLOAT dampFactor = 1 - (FluidDamping * DeltaTime);
	const FLOAT C2T2 = (FluidSpeed*DeltaTime)*(FluidSpeed*DeltaTime);
	const FLOAT RecipH2 = 1/(FluidGridSpacing*FluidGridSpacing);

	// HEX GRID
	// When we make the hexagonal grid - x keeps unit space, but y rows
	// are only sqrt(3)/2 apart. Odd x rows are shifted one unit over.
	if(FluidGridType == FGT_Hexagonal)
	{
		for(y=1; y<FluidYSize-1; y++)
		{
			if(y & 0x01) // Odd Row
			{
				for(x=1; x<FluidXSize-1; x++)
				{
					// See if we are simulating this vertex.
					if(!GetClampedBitmap(x, y))
					{
						curve = RecipH2 * (
							SURFHEIGHT(OldH,	x-1,	y) + 
							SURFHEIGHT(OldH,	x,		y+1) +
							SURFHEIGHT(OldH,	x+1,	y+1) + 
							SURFHEIGHT(OldH,	x+1,	y) +
							SURFHEIGHT(OldH,	x+1,	y-1) +
							SURFHEIGHT(OldH,	x,		y-1) -
							6*SURFHEIGHT(OldH,  x,		y));
						
						SURFHEIGHT(H,x,y) = C2T2 * curve + 
							(2 * SURFHEIGHT(OldH,x,y)) - SURFHEIGHT(H,x,y);
						
						SURFHEIGHT(H,x,y) *= dampFactor;
	
						VERTALPHA(x,y) = Clamp<INT>( appTrunc( curve * AlphaCurveScale + (SURFHEIGHT(H,x,y)) * AlphaHeightScale ), 0, AlphaMax );
					}
				}
			}
			else
			{
				for(x=1; x<FluidXSize-1; x++)
				{
					if(!GetClampedBitmap(x, y))
					{
						curve = RecipH2 * (
							SURFHEIGHT(OldH,	x-1,	y) + 
							SURFHEIGHT(OldH,	x-1,	y+1) + 
							SURFHEIGHT(OldH,	x,		y+1) +
							SURFHEIGHT(OldH,	x+1,	y) +
							SURFHEIGHT(OldH,	x,		y-1) + 
							SURFHEIGHT(OldH,	x-1,	y-1) -
							6*SURFHEIGHT(OldH,  x,		y));

						SURFHEIGHT(H,x,y) = C2T2 * curve + 
							(2 * SURFHEIGHT(OldH,x,y)) - SURFHEIGHT(H,x,y);
						
						SURFHEIGHT(H,x,y) *= dampFactor;

						VERTALPHA(x,y) = Clamp<INT>( appTrunc( curve * AlphaCurveScale + (SURFHEIGHT(H,x,y)) * AlphaHeightScale ), 0, AlphaMax );
					}
				}	
			}
		}

		GStats.DWORDStats( GEngineStats.STATS_Fluid_SimulateCycles ) += (appCycles() - StartUpdateTime);
	}
	// RECT GRID
	else
	{		
		for(y=1; y<FluidYSize-1; y++)
		{
			for(x=1; x<FluidXSize-1; x++)
			{
				// See if we are simulating this vertex.
				if(!GetClampedBitmap(x, y))
				{
					curve = RecipH2 * (
						SURFHEIGHT(OldH,x-1,y) + 
						SURFHEIGHT(OldH,x+1,y) +
						SURFHEIGHT(OldH,x,y-1) + 
						SURFHEIGHT(OldH,x,y+1) -
						4 * SURFHEIGHT(OldH,x,y));

					SURFHEIGHT(H,x,y) = C2T2 * curve +
						(2 * SURFHEIGHT(OldH,x,y)) - SURFHEIGHT(H,x,y);
					
					SURFHEIGHT(H,x,y) *= dampFactor;

					VERTALPHA(x,y) = Clamp<INT>( appTrunc( curve * AlphaCurveScale + (SURFHEIGHT(H,x,y)) * AlphaHeightScale ), 0, AlphaMax );
				}
			}
		}
		
		
		
		GStats.DWORDStats( GEngineStats.STATS_Fluid_SimulateCycles ) += (appCycles() - StartUpdateTime);
	}

	unguard;
}

UBOOL AFluidSurfaceInfo::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	guard(AFluidSurfaceInfo::Tick);

	if ( !Super::Tick(DeltaTime, TickType) )
		return false;

	if(bDeleteMe || TickType == LEVELTICK_ViewportsOnly && !GIsEditor)
		return true;

	// Do nothing if on low detail level.
	if( UsingLowDetail(this) )
		return true;

	FLOAT simStep = 1/UpdateRate;

	if(!bHasWarmedUp)
	{
		UpdateOscillatorList();
		
		// Run the warm-up for the water after its loaded.
		FLOAT runSoFar = 0;
		while(runSoFar < WarmUpTime * FluidTimeScale)
		{
			UpdateSimulation(simStep);
			runSoFar += simStep;
		}

		bHasWarmedUp = true;
	}

	// If this water hasn't been rendered for a while, stop updating.
	if(Level->TimeSeconds - LastRenderTime > 1)
		return true;

	FLOAT ScaledDeltaTime = DeltaTime * FluidTimeScale;

	// We must run the simulation of the water at a constant rate.
	TimeRollover += ScaledDeltaTime;
	while(TimeRollover > simStep)
	{
		UpdateSimulation(simStep);
		TimeRollover -= simStep;
	}

	return true;

	unguard;
}

/* ********************** FLUID VERTEX STREAM *************************** */

INT FFluidSurfaceVertexStream::GetSize()
{
	if( UsingLowDetail(Fluid) )
		return (2 * 2 * sizeof(FFluidSurfaceVertex));
	else
		return (this->Fluid->FluidXSize * this->Fluid->FluidYSize * sizeof(FFluidSurfaceVertex));
}

INT FFluidSurfaceVertexStream::GetComponents(FVertexComponent* OutComponents)
{
	OutComponents[0].Type = CT_Float3;
	OutComponents[0].Function = FVF_Position;
	OutComponents[1].Type = CT_Float3;
	OutComponents[1].Function = FVF_Normal;
	OutComponents[2].Type = CT_Color;
	OutComponents[2].Function = FVF_Diffuse;
	OutComponents[3].Type = CT_Float2;
	OutComponents[3].Function = FVF_TexCoord0;
	
	return 4;
}

void FFluidSurfaceVertexStream::GetStreamData(void* Dest)
{
	if( UsingLowDetail(Fluid) )
		Fluid->SimpleFillVertexBuffer(Dest);
	else
		Fluid->FillVertexBuffer(Dest);
}


/* ********************** FLUID INDEX STREAM *************************** */

INT FFluidSurfaceIndexBuffer::GetSize()
{
	if( UsingLowDetail(Fluid) )
		return (1 * 1 * 2 * 3 * sizeof(_WORD));
	else
		return ((Fluid->FluidXSize-1) * (Fluid->FluidYSize-1) * 2 * 3 * sizeof(_WORD));
}


void FFluidSurfaceIndexBuffer::GetContents(void* Data)
{
	if( UsingLowDetail(Fluid) )
		Fluid->SimpleFillIndexBuffer(Data);
	else
		Fluid->FillIndexBuffer(Data);
}

/* ********************* BUFFER FILLERS ************************* */

void AFluidSurfaceInfo::SimpleFillVertexBuffer(void* Dest)
{
	guard(AFluidSurfaceInfo::SimpleFillVertexBuffer);

	INT StartVertGenTime = appCycles();

	FFluidSurfaceVertex* Vertex = (FFluidSurfaceVertex*)Dest;
	const FVector defNorm = FVector(0, 0, 1);

	FColor Color = FluidColor;
	Color.A = 0;
    Color = Color.RenderColor();

	// //
	Vertex->Position.X = FluidBoundingBox.Min.X;
	Vertex->Position.Y = FluidBoundingBox.Min.Y;
	Vertex->Position.Z = FluidOrigin.Z;

	Vertex->Normal		= defNorm;
	Vertex->U			= UOffset;
	Vertex->V			= VOffset;
	Vertex->Color		= Color;
	Vertex++;

	// //
	Vertex->Position.X = FluidBoundingBox.Max.X;
	Vertex->Position.Y = FluidBoundingBox.Min.Y;
	Vertex->Position.Z = FluidOrigin.Z;

	Vertex->Normal		= defNorm;
	Vertex->U			= UOffset + UTiles;
	Vertex->V			= VOffset;
	Vertex->Color		= Color;
	Vertex++;

	// //
	Vertex->Position.X = FluidBoundingBox.Min.X;
	Vertex->Position.Y = FluidBoundingBox.Max.Y;
	Vertex->Position.Z = FluidOrigin.Z;

	Vertex->Normal		= defNorm;
	Vertex->U			= UOffset;
	Vertex->V			= VOffset + VTiles;
	Vertex->Color		= Color;
	Vertex++;

	// //
	Vertex->Position.X = FluidBoundingBox.Max.X;
	Vertex->Position.Y = FluidBoundingBox.Max.Y;
	Vertex->Position.Z = FluidOrigin.Z;

	Vertex->Normal		= defNorm;
	Vertex->U			= UOffset + UTiles;
	Vertex->V			= VOffset + VTiles;
	Vertex->Color		= Color;

	GStats.DWORDStats( GEngineStats.STATS_Fluid_VertexGenCycles ) += (appCycles() - StartVertGenTime);

	unguard;
}

void AFluidSurfaceInfo::FillVertexBuffer(void* Dest)
{
	guard(AFluidSurfaceInfo::FillVertexBuffer);

	INT StartVertGenTime = appCycles();

	FFluidSurfaceVertex* VArray = (FFluidSurfaceVertex*)Dest;
	FFluidSurfaceVertex* Vertex = VArray;
	INT x, y;
	
	// Flip old/new store
	TArrayNoInit<FLOAT> *H, *OldH; 
	if(LatestVerts == 0)
	{
		H = &(Verts0);
		OldH = &(Verts1);
	}
	else
	{
		H = &(Verts1);
		OldH = &(Verts0);
	}

	FColor ColorA	= FluidColor;
	FColor Color	= FluidColor;
	Color.A = 0;
	Color = Color.RenderColor();

	// used for velocity alpha
	//FLOAT recipStep = 1.f/AverageTimeStep;
	
	/* ******************** HEX GRID ********************** */
	if(FluidGridType == FGT_Hexagonal)
	{
		FVector tmpNorm;
		const FVector defNorm = FVector(0, 0, 1);
		const FLOAT recipXSizeMinusHalf = 1.f/((FLOAT)FluidXSize-0.5f);

		const FLOAT yMin = FluidOrigin.Y;
		const FLOAT yMax = FluidOrigin.Y + (ROOT3OVER2*(FluidYSize-1)*FluidGridSpacing);
		const FLOAT xMinEven = FluidOrigin.X;
		const FLOAT xMaxEven = FluidOrigin.X + (FluidXSize-1)*FluidGridSpacing;
		const FLOAT xMinOdd = FluidOrigin.X + (0.5f * FluidGridSpacing);
		const FLOAT xMaxOdd = FluidOrigin.X + ((FLOAT)FluidXSize-0.5f)*FluidGridSpacing;

		FLOAT dY = ROOT3OVER2 * FluidGridSpacing;
		FLOAT normalZ = 4 * dY * FluidGridSpacing;

		// Y == 0 ROW ////////////////////////////////////////////////////////////////
		for(x=0; x<FluidXSize; x++)
		{
			Vertex->Position.X = FluidOrigin.X + (x*FluidGridSpacing);
			Vertex->Position.Y = yMin;
			Vertex->Position.Z = FluidOrigin.Z;
			
			Vertex->Normal		= defNorm;
			Vertex->U			= UOffset + (UTiles * (FLOAT)x * recipXSizeMinusHalf);
			Vertex->V			= VOffset;
			Vertex->Color		= Color;
			Vertex++;
		}

		// Y == 1 -> FluidYSize-2 ROWS //////////////////////////////////////////////////
		for(y=1; y<FluidYSize-1; y++)
		{
			FLOAT rowV = VOffset + (VTiles * (FLOAT)y/(FluidYSize-1));
			FLOAT rowY = FluidOrigin.Y + (ROOT3OVER2 * y * FluidGridSpacing);

			// ODD ROW
			if(y & 0x01)
			{
				// X == 0 ////////////////////////////////////////////////////////////////
				Vertex->Position.X = xMinOdd;								
				Vertex->Position.Y = rowY;
				Vertex->Position.Z = FluidOrigin.Z;
				
				Vertex->Normal		= defNorm;
				Vertex->U			= UOffset + (UTiles * 0.5f * recipXSizeMinusHalf);
				Vertex->V			= rowV;
				Vertex->Color		= Color;
				Vertex++;
				
				// X == 1 -> FluidXSize-2 ////////////////////////////////////////////////
				for(x=1; x<FluidXSize-1; x++)
				{
					Vertex->Position.X = FluidOrigin.X + (((FLOAT)x+0.5f) * FluidGridSpacing);					
					Vertex->Position.Y = rowY;
					Vertex->Position.Z = FluidOrigin.Z + (SURFHEIGHT(H,x,y)*FluidHeightScale);
					
					Vertex->U			= UOffset + (UTiles * ((FLOAT)x+0.5f) * recipXSizeMinusHalf);
					Vertex->V			= rowV;
					ColorA.A			= VERTALPHA(x,y);
					Vertex->Color		= ColorA.RenderColor();
					
					// Generate normal from surrounding points
					FLOAT dZ[6];
					dZ[0] = FluidHeightScale * (SURFHEIGHT(H,x-1,y  )	- SURFHEIGHT(H,x,y));
					dZ[1] = FluidHeightScale * (SURFHEIGHT(H,x  ,y+1)	- SURFHEIGHT(H,x,y));
					dZ[2] = FluidHeightScale * (SURFHEIGHT(H,x+1,y+1)	- SURFHEIGHT(H,x,y));
					dZ[3] = FluidHeightScale * (SURFHEIGHT(H,x+1,y  )	- SURFHEIGHT(H,x,y));
					dZ[4] = FluidHeightScale * (SURFHEIGHT(H,x+1,y-1)	- SURFHEIGHT(H,x,y));
					dZ[5] = FluidHeightScale * (SURFHEIGHT(H,x  ,y-1)	- SURFHEIGHT(H,x,y));
					
#if SOFTWARE_NORMALIZE
					FVector tmpN;

					tmpN.X = (2*dZ[0]*dY) - (2*dZ[3]*dY) + (dZ[1]*dY) - (dZ[2]*dY) + (dZ[5]*dY) - (dZ[4]*dY);
					tmpN.Y = 1.5f*FluidGridSpacing*( -dZ[1] -dZ[2] + dZ[4] + dZ[5] );
					tmpN.Z = normalZ;

					Vertex->Normal = tmpN.SafeNormal();
#else
					Vertex->Normal.X = (2*dZ[0]*dY) - (2*dZ[3]*dY) + (dZ[1]*dY) - (dZ[2]*dY) + (dZ[5]*dY) - (dZ[4]*dY);
					Vertex->Normal.Y = 1.5f*FluidGridSpacing*( -dZ[1] -dZ[2] + dZ[4] + dZ[5] );
					Vertex->Normal.Z = normalZ;
#endif

#if SHOW_NORMALS
					FVector unitNorm = Vertex->Normal.SafeNormal();
					GTempLineBatcher->AddLine(
						LocalToWorld().TransformFVector(Vertex->Position), 
						LocalToWorld().TransformFVector(Vertex->Position + (10 * unitNorm)), 
						FColor(VERTALPHA(x,y), 255, 0));
#endif

					Vertex++;
				}
				
				// X == FluidXSize - 1 ///////////////////////////////////////////////////
				Vertex->Position.X = xMaxOdd;								
				Vertex->Position.Y = rowY;
				Vertex->Position.Z = FluidOrigin.Z;
				
				Vertex->Normal		= defNorm;
				Vertex->U			= UOffset + UTiles;
				Vertex->V			= rowV;
				Vertex->Color		= Color;
				
				Vertex++;
			}
			// EVEN ROW /////////////////////////////////////////////////////////
			else 
			{
				// X == 0 ////////////////////////////////////////////////////////////////
				Vertex->Position.X = xMinEven;								
				Vertex->Position.Y = rowY;
				Vertex->Position.Z = FluidOrigin.Z;
				
				Vertex->Normal		= defNorm;
				Vertex->U			= UOffset;;
				Vertex->V			= rowV;
				Vertex->Color		= Color;

				Vertex++;
				
				// X == 1 -> FluidXSize-2 ////////////////////////////////////////////////
				for(x=1; x<FluidXSize-1; x++)
				{
					Vertex->Position.X = FluidOrigin.X + ((FLOAT)x * FluidGridSpacing);					
					Vertex->Position.Y = rowY;
					Vertex->Position.Z = FluidOrigin.Z + (SURFHEIGHT(H,x,y)*FluidHeightScale);
					
					Vertex->U			= UOffset + (UTiles * (FLOAT)x * recipXSizeMinusHalf);
					Vertex->V			= rowV;
					ColorA.A			= VERTALPHA(x,y);
					Vertex->Color		= ColorA.RenderColor();
								
					// Generate normal from surrounding points
					FLOAT dZ[6];					
					dZ[0] = FluidHeightScale * (SURFHEIGHT(H,x-1,y  )	- SURFHEIGHT(H,x,y));
					dZ[1] = FluidHeightScale * (SURFHEIGHT(H,x-1,y+1)	- SURFHEIGHT(H,x,y));
					dZ[2] = FluidHeightScale * (SURFHEIGHT(H,x  ,y+1)	- SURFHEIGHT(H,x,y));
					dZ[3] = FluidHeightScale * (SURFHEIGHT(H,x+1,y  )	- SURFHEIGHT(H,x,y));
					dZ[4] = FluidHeightScale * (SURFHEIGHT(H,x  ,y-1)	- SURFHEIGHT(H,x,y));
					dZ[5] = FluidHeightScale * (SURFHEIGHT(H,x-1,y-1)	- SURFHEIGHT(H,x,y));
					
#if SOFTWARE_NORMALIZE
					FVector tmpN;

					tmpN.X = (2*dZ[0]*dY) - (2*dZ[3]*dY) + (dZ[1]*dY) - (dZ[2]*dY) + (dZ[5]*dY) - (dZ[4]*dY);
					tmpN.Y = 1.5f*FluidGridSpacing*( -dZ[1] -dZ[2] + dZ[4] + dZ[5] );
					tmpN.Z = normalZ;

					Vertex->Normal = tmpN.SafeNormal();
#else
					Vertex->Normal.X = (2*dZ[0]*dY) - (2*dZ[3]*dY) + (dZ[1]*dY) - (dZ[2]*dY) + (dZ[5]*dY) - (dZ[4]*dY);
					Vertex->Normal.Y = 1.5f*FluidGridSpacing*( -dZ[1] -dZ[2] + dZ[4] + dZ[5] );
					Vertex->Normal.Z = normalZ;
#endif

#if SHOW_NORMALS
					FVector unitNorm = Vertex->Normal.SafeNormal();
					GTempLineBatcher->AddLine(
						LocalToWorld().TransformFVector(Vertex->Position), 
						LocalToWorld().TransformFVector(Vertex->Position + (10 * unitNorm)), 
						FColor(VERTALPHA(x,y), 255, 0));
#endif

					Vertex++;
				}
				
				// X == FluidXSize - 1 ///////////////////////////////////////////////////
				Vertex->Position.X = xMaxEven;								
				Vertex->Position.Y = rowY;
				Vertex->Position.Z = FluidOrigin.Z;
				
				Vertex->Normal		= defNorm;
				Vertex->U			= UOffset + (UTiles * (FluidXSize-1) * recipXSizeMinusHalf);
				Vertex->V			= rowV;
				Vertex->Color		= Color;

				Vertex++;
			}
		}

		// Y == FLUIDYSIZE-1 ROW ////////////////////////////////////////////////

		// if last row is odd
		if((FluidYSize-1) & 0x01)
		{
			for(x=0; x<FluidXSize; x++)
			{
				Vertex->Position.X = FluidOrigin.X + (((FLOAT)x+0.5f) * FluidGridSpacing);				
				Vertex->Position.Y = yMax;
				Vertex->Position.Z = FluidOrigin.Z;
								
				Vertex->Normal		= defNorm;
				Vertex->U			= UOffset + (UTiles * ((FLOAT)x+0.5f) * recipXSizeMinusHalf);
				Vertex->V			= VOffset + VTiles;
				Vertex->Color		= Color;

				Vertex++;
			}
		}
		else // if even
		{
			for(x=0; x<FluidXSize; x++)
			{
				Vertex->Position.X = FluidOrigin.X + (x*FluidGridSpacing);				
				Vertex->Position.Y = yMax;
				Vertex->Position.Z = FluidOrigin.Z;
								
				Vertex->Normal		= defNorm;
				Vertex->U			= UOffset + (UTiles * (FLOAT)x * recipXSizeMinusHalf);
				Vertex->V			= VOffset + VTiles;
				Vertex->Color		= Color;

				Vertex++;
			}
		}
		
	} 
	else 
	{
		/* ******************** RECT GRID ********************** */

		FVector tmpNorm;
		const FVector defNorm = FVector(0, 0, 1);

		const FLOAT recipFluidXSizeMin1 = (1.f/((FLOAT)FluidXSize-1));
		const FLOAT normalZ = -4 * (FluidGridSpacing * FluidGridSpacing);

		// Y == 0 ROW ////////////////////////////////////////////////////////////////
		for(x=0; x<FluidXSize; x++)
		{
			Vertex->Position.X = FluidOrigin.X + (x*FluidGridSpacing);
			Vertex->Position.Y = FluidOrigin.Y;
			Vertex->Position.Z = FluidOrigin.Z;
			
			Vertex->Normal		= defNorm;
			Vertex->U			= UOffset + (UTiles * (FLOAT)x * recipFluidXSizeMin1);
			Vertex->V			= VOffset;
			Vertex->Color		= Color;

			Vertex++;
		}

		// Y == 1 -> FluidYSize-2 ROWS //////////////////////////////////////////////////
		for(y=1; y<FluidYSize-1; y++)
		{
			FLOAT rowV = VOffset + (VTiles * (FLOAT)y/(FluidYSize-1));
			FLOAT rowY = FluidOrigin.Y + (y * FluidGridSpacing);
			
			// X == 0 ////////////////////////////////////////////////////////////////
			Vertex->Position.X = FluidOrigin.X;								
			Vertex->Position.Y = rowY;
			Vertex->Position.Z = FluidOrigin.Z;
			
			Vertex->Normal		= defNorm;
			Vertex->U			= UOffset;
			Vertex->V			= rowV;
			Vertex->Color		= Color;
			Vertex++;
			
			// X == 1 -> FluidXSize-2 ////////////////////////////////////////////////
			for(x=1; x<FluidXSize-1; x++)
			{
				Vertex->Position.X = FluidOrigin.X + ((FLOAT)x * FluidGridSpacing);					
				Vertex->Position.Y = rowY;
				Vertex->Position.Z = FluidOrigin.Z + (SURFHEIGHT(H,x,y)*FluidHeightScale);
				
				Vertex->U			= UOffset + (UTiles * (FLOAT)x * recipFluidXSizeMin1);
				Vertex->V			= rowV;
				ColorA.A			= VERTALPHA(x,y);
				Vertex->Color		= ColorA.RenderColor();
				
				FLOAT dZ[4];
				dZ[0] = FluidHeightScale*(SURFHEIGHT(H,x-1,y  ) - SURFHEIGHT(H,x,y));
				dZ[1] = FluidHeightScale*(SURFHEIGHT(H,x  ,y+1)	- SURFHEIGHT(H,x,y));
				dZ[2] = FluidHeightScale*(SURFHEIGHT(H,x+1,y  ) - SURFHEIGHT(H,x,y));
				dZ[3] = FluidHeightScale*(SURFHEIGHT(H,x  ,y-1)	- SURFHEIGHT(H,x,y));
				
#if SOFTWARE_NORMALIZE
				FVector tmpN;
				
				tmpN.X = 2 * FluidGridSpacing * (dZ[0] - dZ[2]);
				tmpN.Y = 2 * FluidGridSpacing * (dZ[3] - dZ[1]);
				tmpN.Z = normalZ;
				
				Vertex->Normal = tmpN.SafeNormal();
#else
				Vertex->Normal.X = 2 * FluidGridSpacing * (dZ[0] - dZ[2]);
				Vertex->Normal.Y = 2 * FluidGridSpacing * (dZ[3] - dZ[1]);
				Vertex->Normal.Z = normalZ;
#endif
				
#if SHOW_NORMALS
				FVector unitNorm = Vertex->Normal.SafeNormal();
				GTempLineBatcher->AddLine(
					LocalToWorld().TransformFVector(Vertex->Position), 
					LocalToWorld().TransformFVector(Vertex->Position + (10 * unitNorm)), 
					FColor(VERTALPHA(x,y), 255, 0));
#endif

				Vertex++;
			}
			
			// X == FluidXSize - 1 ///////////////////////////////////////////////////
			Vertex->Position.X = FluidOrigin.X + (FluidXSize-1) * FluidGridSpacing;								
			Vertex->Position.Y = rowY;
			Vertex->Position.Z = FluidOrigin.Z;
			
			Vertex->Normal		= defNorm;
			Vertex->U			= UOffset + UTiles;
			Vertex->V			= rowV;
			Vertex->Color		= Color;
			
			Vertex++;
		}

		// Y == FLUIDYSIZE-1 ROW ////////////////////////////////////////////////
		for(x=0; x<FluidXSize; x++)
		{
			Vertex->Position.X = FluidOrigin.X + (x*FluidGridSpacing);				
			Vertex->Position.Y = FluidOrigin.Y + (FluidYSize-1) * FluidGridSpacing;
			Vertex->Position.Z = FluidOrigin.Z;
			
			Vertex->Normal		= defNorm;
			Vertex->U			= UOffset + (UTiles * (FLOAT)x * recipFluidXSizeMin1);
			Vertex->V			= VOffset + VTiles;
			Vertex->Color		= Color;
			
			Vertex++;
		}				
	}

	GStats.DWORDStats( GEngineStats.STATS_Fluid_VertexGenCycles ) += (appCycles() - StartVertGenTime);

	unguard;
}

void AFluidSurfaceInfo::SimpleFillIndexBuffer(void* Data)
{
	guard(AFluidSurfaceInfo::SimpleFillIndexBuffer);

	_WORD* WordData = (_WORD*)Data;

	*(WordData++) = 0;
	*(WordData++) = 3;
	*(WordData++) = 1;

	*(WordData++) = 0;
	*(WordData++) = 2;
	*(WordData++) = 3;	

	unguard;
}

void AFluidSurfaceInfo::FillIndexBuffer(void* Data)
{
	guard(AFluidSurfaceInfo::FillIndexBuffer);

	INT x, y;
	_WORD* WordData = (_WORD*)Data;
	
	if(FluidGridType == FGT_Hexagonal)
	{
		// For each row
		for(y=0; y<FluidYSize-1; y++)
		{
			if(y & 0x01) // Odd row
			{
				// For each element
				for(x=0; x<FluidXSize-1; x++)
				{
					*(WordData++) = (y+0)*FluidXSize+(x+0);
					*(WordData++) = (y+1)*FluidXSize+(x+1);
					*(WordData++) = (y+0)*FluidXSize+(x+1);
					
					*(WordData++) = (y+0)*FluidXSize+(x+0);
					*(WordData++) = (y+1)*FluidXSize+(x+0);
					*(WordData++) = (y+1)*FluidXSize+(x+1);
				}
			}
			else
			{
				for(x=0; x<FluidXSize-1; x++)
				{
					*(WordData++) = (y+0)*FluidXSize+(x+0);
					*(WordData++) = (y+1)*FluidXSize+(x+0);
					*(WordData++) = (y+0)*FluidXSize+(x+1);
					
					*(WordData++) = (y+1)*FluidXSize+(x+0);
					*(WordData++) = (y+1)*FluidXSize+(x+1);
					*(WordData++) = (y+0)*FluidXSize+(x+1);
				}
			}
		}
	}
	else
	{
		for(y=0; y<FluidYSize-1; y++)
		{
			for(x=0; x<FluidXSize-1; x++)
			{
				*(WordData++) = (y+0)*FluidXSize+(x+0);
				*(WordData++) = (y+1)*FluidXSize+(x+1);
				*(WordData++) = (y+0)*FluidXSize+(x+1);
				
				*(WordData++) = (y+0)*FluidXSize+(x+0);
				*(WordData++) = (y+1)*FluidXSize+(x+0);
				*(WordData++) = (y+1)*FluidXSize+(x+1);
			}
		}
	}	

	unguard;
}

/* *********************** AFLUIDSURFACEOSCILLATOR ************* */

void AFluidSurfaceOscillator::UpdateOscillation( FLOAT DeltaTime )
{
	guard(AFluidSurfaceOscillator::UpdateOscillation);

	if( bDeleteMe || !FluidInfo || !FluidInfo->IsA(AFluidSurfaceInfo::StaticClass()) || FluidInfo->bDeleteMe)
		return;

	OscTime += DeltaTime;

	// If frequency is zero - just set velocity to 'strength'
	FLOAT period, amp, currPhase;
	
	if(Frequency > 0.0001f)
	{
		period = 1.f/Frequency;
		currPhase = (fmod(OscTime, period)*Frequency) + (FLOAT)Phase/255.f;
		amp = Strength * appSin(currPhase * PI * 2);
	}
	else
		amp = Strength;

	FluidInfo->Pling(Location, amp, Radius);

	unguard;
}

void AFluidSurfaceOscillator::PostEditChange()
{
	guard(AFluidSurfaceOscillator::PostEditChange);

	ULevel* level = GetLevel();
	if(!level)
		return;

	// In case we changed the water we affect - Remove this oscillator from any
	// fluid surface that have it in their list.

	// First - remove all references to this oscillator...
	for( INT i=0; i<level->Actors.Num(); i++ )
	{
		if( level->Actors(i) && 
			!level->Actors(i)->bDeleteMe && 
			level->Actors(i)->IsA(AFluidSurfaceInfo::StaticClass()) )
		{
			AFluidSurfaceInfo* info = Cast<AFluidSurfaceInfo>(level->Actors(i));
			info->Oscillators.RemoveItem(this);
		}
	}

	// Then add it to the fluid surface we now affect.
	if(FluidInfo)
		FluidInfo->Oscillators.AddItem(this);

	unguard;
}

// When we destroy an oscillator, we need to remove it from the fluids list.
void AFluidSurfaceOscillator::Destroy()
{
	guard(AFluidSurfaceOscillator::Destroy);
	Super::Destroy();
	if(FluidInfo)
		FluidInfo->Oscillators.RemoveItem(this);
	unguard;
}

UBOOL AFluidSurfaceInfo::ShouldTrace(AActor *SourceActor, DWORD TraceFlags)
{
	guard(AFluidSurfaceInfo::ShouldTrace);

	if ( (TraceFlags & TRACE_Others) 
		&& (TraceFlags & TRACE_OnlyProjActor) 
		&& (SourceActor && SourceActor->bTraceWater) )
		return true;

	return Super::ShouldTrace(SourceActor, TraceFlags);
	unguard;
}
