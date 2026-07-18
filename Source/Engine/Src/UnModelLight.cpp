/*=============================================================================
	UnModel.cpp: Unreal model raytracing.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

#ifndef CONSOLE// code size optimization for console runtime

extern ENGINE_API FRebuildTools	GRebuildTools;

/*---------------------------------------------------------------------------------------
   UModel raytracing.
---------------------------------------------------------------------------------------*/

#define FRAMEBUFFER_SIZE		512

//
//	HackVector
//

FVector HackVector(FVector V)
{
	return V / V.SizeSquared();
}

//
//	FProxyRI
//	Renders everything solid black.
//

	class FProxyRI : public FRenderInterface
	{
	public:

		FRenderInterface*	RI;

	// Constructor/destructor.

		FProxyRI(FRenderInterface* InRI)
		{
			RI = InRI;

			RI->PushState();
		RI->EnableLighting(1,0);
			RI->SetAmbientLight(FColor(0,0,0));
		}

		~FProxyRI()
		{
			RI->PopState();
		}

	// FRenderInterface interface.

		virtual void PushState()
		{
			RI->PushState();
		}

		virtual void PopState()
		{
			RI->PopState();
		}

		virtual UBOOL SetRenderTarget(FRenderTarget* RenderTarget)
		{
			return RI->SetRenderTarget(RenderTarget);
		}

		virtual void SetViewport(INT X,INT Y,INT Width,INT Height)
		{
			RI->SetViewport(X,Y,Width,Height);
		}

		virtual void Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil)
		{
			RI->Clear(UseColor,Color,UseDepth,Depth,UseStencil,Stencil);
		}

		virtual void PushHit(const BYTE* Data,INT Count)
		{
			RI->PushHit(Data,Count);
		}

		virtual void PopHit(INT Count,UBOOL Force)
		{
			RI->PopHit(Count,Force);
		}

		virtual void SetCullMode(ECullMode CullMode)
		{
		}

		virtual void SetAmbientLight(FColor Color)
		{
		}

	    virtual void EnableLighting(UBOOL UseDynamic,UBOOL UseStatic,UBOOL Modulate2X,FBaseTexture* LightMap,UBOOL LightingOnly,FSphere LitSphere)
		{
		}

	    virtual void SetLight(INT LightIndex,FDynamicLight* Light, FLOAT Scale ) // sjs
		{
		}

		virtual void SetNPatchTesselation( FLOAT Tesselation )
		{
		}

		virtual void SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor Color)
		{
		}

		virtual void SetGlobalColor(FColor Color)
		{
		}
		
		virtual void SetTransform(ETransformType Type,const FMatrix& Matrix)
		{
			RI->SetTransform(Type,Matrix);
		}

		virtual void SetZBias(INT ZBias)
		{
			RI->SetZBias(ZBias);
		}

		virtual void SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask)
		{
			RI->SetStencilOp(Test,Ref,Mask,FailOp,ZFailOp,PassOp,WriteMask);
		}

	virtual void SetPrecacheMode( EPrecacheMode PrecacheMode )
	{
		RI->SetPrecacheMode( PrecacheMode );
	}

		virtual void SetMaterial(UMaterial* Material,FString* ErrorString, UMaterial** ErrorMaterial, INT* NumPasses)
		{
            // sjs
            BYTE b = 0;
            if ( Material ) 
            {
                b = Material->SurfaceType;
                Material->SurfaceType = 255;
            }

			RI->SetMaterial(Material,ErrorString,ErrorMaterial);

            if ( Material ) // sjs
            {
                Material->SurfaceType = b;
            }
		}

		virtual INT SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams)
		{
			return RI->SetVertexStreams(Shader,Streams,NumStreams);
		}

		virtual INT SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
		{
			return RI->SetDynamicStream(Shader,Stream);
		}

		virtual INT SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseIndex)
		{
			return RI->SetIndexBuffer(IndexBuffer,BaseIndex);
		}

		virtual INT SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseIndex)
		{
			return RI->SetDynamicIndexBuffer(IndexBuffer,BaseIndex);
		}

		virtual void DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex = INDEX_NONE,INT MaxIndex = INDEX_NONE)
		{
			RI->DrawPrimitive(PrimitiveType,FirstIndex,NumPrimitives,MinIndex,MaxIndex);
		}
	};

//
//	FLightMapSceneNode
//	Sets up a projection matrix that transforms from world space into lightmap space, using a perspective
//	projection with the view origin at the light point.
//

class ENGINE_API FLightMapSceneNode : public FLevelSceneNode
{
public:

	FLightMap*	LightMap;

	// Constructor/destructor.

	FLightMapSceneNode(UViewport* InViewport,AActor* LightActor,FLightMap* InLightMap) : FLevelSceneNode(InViewport,&InViewport->RenderTarget)
	{
		LightMap = InLightMap;

		// Determine which zone the light is in.

		FPointRegion	LightZone = Model->PointRegion(LightActor->Level,LightActor->Location);

		ViewActor = LightActor;
		ViewOrigin = LightActor->Location;
		ViewZone = LightZone.ZoneNumber;
	}

	virtual ~FLightMapSceneNode()
	{
	}

	// FilterActor

	virtual UBOOL FilterActor(AActor* Actor)
	{
		if((GRebuildTools.GetCurrent()->Options & REBUILD_OnlyVisible) && Actor->bHidden)
			return 0;

		if(Actor->bShadowCast)
			return 1;

		return 0;
	}

	// Render

	virtual void Render(FRenderInterface* RI)
	{
		RI->Clear(1,FColor(255,255,255),1,1.0f - 1.0f / 65536.0f,1,~DEPTH_COMPLEXITY_MASK(Viewport));

		FProxyRI	ProxyRI(RI);
		FLevelSceneNode::Render(&ProxyRI);
	}
};

//
//	FPointLightMapSceneNode
//

class ENGINE_API FPointLightMapSceneNode : public FLightMapSceneNode
{
public:

	FVector	LightMapCenter;

	// Constructor.

	FPointLightMapSceneNode(UViewport* InViewport,AActor* LightActor,FBspSurf& Surf,FLightMap* InLightMap,INT MinX,INT MinY,INT MaxX,INT MaxY) : FLightMapSceneNode(InViewport,LightActor,InLightMap)
	{
		guard(FPointLightMapSceneNode::FPointLightMapSceneNode);

		// Initialize the view matrix.

		INT		SizeX = MaxX - MinX + 1,
				SizeY = MaxY - MinY + 1;
		FLOAT	Dot = Surf.Plane.PlaneDot(LightActor->Location) > 0.0f ? 1.0f : -1.0f;
		FMatrix	LightMapToWorld = LightMap->WorldToLightMap.Inverse();
		FVector	WorldSurfaceCenter = LightMapToWorld.TransformFVector(
										FVector(
											MinX + (SizeX + 1.0f) * 0.5f,
											MinY + (SizeY + 1.0f) * 0.5f,
											0
											)
										),
				WorldSurfaceX = LightMapToWorld.TransformNormal(
									FVector(
										SizeX * 0.5f,
										0,
										0
										)
									),
				WorldSurfaceY = LightMapToWorld.TransformNormal(
									FVector(
										0,
										SizeY * 0.5f,
										0
										)
									),
				WorldSurfaceZ = Surf.Plane * -Dot;

		FProjectTextureToPlane(
				WorldSurfaceCenter,
				WorldSurfaceX,
				WorldSurfaceY,
				Surf.Plane
				);

		WorldToCamera = FMatrix::Identity;
		WorldToCamera = WorldToCamera * FTranslationMatrix(-ViewOrigin);
		WorldToCamera = WorldToCamera * FCoords(
											FVector(0,0,0),
											HackVector(WorldSurfaceX),
											HackVector(WorldSurfaceY),
											HackVector(WorldSurfaceZ)
											).Matrix();

		LightMapCenter = WorldSurfaceCenter;
		CameraToWorld = WorldToCamera.Inverse();

		// Initialize the projection matrix.

		FVector	CameraSurfaceCenter = WorldToCamera.TransformFVector(WorldSurfaceCenter),
				CameraSurfaceX = WorldToCamera.TransformNormal(WorldSurfaceX),
				CameraSurfaceY = WorldToCamera.TransformNormal(WorldSurfaceY),
				CameraSurfaceZ = WorldToCamera.TransformNormal(WorldSurfaceZ);
		FLOAT	MinZ = 1.0f,
				MaxZ = CameraSurfaceCenter.Z;

		CameraToScreen = FMatrix::Identity;
		CameraToScreen = CameraToScreen * FMatrix(
											FPlane(1.0f,											0.0f,											0.0f,	0.0f),
											FPlane(0.0f,											1.0f,											0.0f,	0.0f),
											FPlane(-CameraSurfaceCenter.X / CameraSurfaceCenter.Z,	-CameraSurfaceCenter.Y / CameraSurfaceCenter.Z,	1.0f,	0.0f),
											FPlane(0.0f,											0.0f,											0.0f,	1.0f)
											);
		CameraToScreen = CameraToScreen * FMatrix(
											FPlane(CameraSurfaceX.Size() * CameraSurfaceCenter.Z,	0.0f,											0.0f,	0.0f),
											FPlane(0.0f,											CameraSurfaceY.Size() * CameraSurfaceCenter.Z,	0.0f,	0.0f),
											FPlane(0.0f,											0.0f,											1.0f,	0.0f),
											FPlane(0.0f,											0.0f,											0.0f,	1.0f)
											);
		CameraToScreen = CameraToScreen * FMatrix(
											FPlane(1.0f,	0.0f,	0.0f,							0.0f),
											FPlane(0.0f,	-1.0f,	0.0f,							0.0f),
											FPlane(0.0f,	0.0f,	MaxZ / (MaxZ - MinZ),			1.0f),
											FPlane(0.0f,	0.0f,	-MinZ * (MaxZ / (MaxZ - MinZ)),	0.0f)
											);

		ScreenToCamera = CameraToScreen.Inverse();

		WorldToScreen = WorldToCamera * CameraToScreen;
		ScreenToWorld = ScreenToCamera * CameraToWorld;

		unguard;
	}

	// GetViewFrustum

	virtual FConvexVolume GetViewFrustum()
	{
		FConvexVolume	ViewFrustum;
		FVector			ViewSides[4];
		FLOAT			TempSigns[2] = { -1.0f, 1.0f };
		FPlane			ScreenLightMapCenter = Project(LightMapCenter);

		for(INT X = 0;X < 2;X++)
			for(INT Y = 0;Y < 2;Y++)
				ViewSides[X * 2 + Y] = Deproject(FPlane(TempSigns[X],TempSigns[Y],ScreenLightMapCenter.Z,ScreenLightMapCenter.W));

		if(WorldToScreen.Determinant() < 0.0f)
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewOrigin,ViewSides[1],ViewSides[0]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewOrigin,ViewSides[0],ViewSides[2]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewOrigin,ViewSides[2],ViewSides[3]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewOrigin,ViewSides[3],ViewSides[1]);

			ViewFrustum.BoundingPlanes[4] = FPlane(ViewSides[0],ViewSides[1],ViewSides[2]);
		}
		else
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewOrigin,ViewSides[0],ViewSides[1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewOrigin,ViewSides[2],ViewSides[0]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewOrigin,ViewSides[3],ViewSides[2]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewOrigin,ViewSides[1],ViewSides[3]);

			ViewFrustum.BoundingPlanes[4] = FPlane(ViewSides[0],ViewSides[2],ViewSides[1]);
		}

		ViewFrustum.NumPlanes = 5;

		return ViewFrustum;
	}
};

//
//	FDirectionalLightMapSceneNode
//

class ENGINE_API FDirectionalLightMapSceneNode : public FLightMapSceneNode
{
public:

	// Constructor.

	FDirectionalLightMapSceneNode(UViewport* InViewport,AActor* LightActor,FBspSurf& Surf,FLightMap* InLightMap) : FLightMapSceneNode(InViewport,LightActor,InLightMap)
	{
		guard(FDirectionalLightMapSceneNode::FDirectionalLightMapSceneNode);

		// Initialize the view matrix.

		FMatrix	LightMapToWorld = LightMap->WorldToLightMap.Inverse();
		FVector	LightDirection = -LightActor->Rotation.Vector().SafeNormal();
		FLOAT	Dot = (LightDirection | Surf.Plane) > 0.0f ? 1.0f : -1.0f;
		FVector	WorldSurfaceCenter = LightMapToWorld.TransformFVector(
										FVector(
											(LightMap->SizeX + 1.0f) * 0.5f,
											(LightMap->SizeY + 1.0f) * 0.5f,
											0
											)
										),
				WorldSurfaceX = LightMapToWorld.TransformNormal(
									FVector(
										LightMap->SizeX * 0.5f,
										0,
										0
										)
									),
				WorldSurfaceY = LightMapToWorld.TransformNormal(
									FVector(
										0,
										LightMap->SizeY * 0.5f,
										0
										)
									),
				WorldSurfaceZ = Surf.Plane * -Dot;
		FLOAT	LightDistance = 65536.0f / (LightDirection | -WorldSurfaceZ);

		FProjectTextureToPlane(
				WorldSurfaceCenter,
				WorldSurfaceX,
				WorldSurfaceY,
				Surf.Plane
				);

		WorldToCamera = FMatrix::Identity;
		WorldToCamera = WorldToCamera * FTranslationMatrix(-(WorldSurfaceCenter + LightDirection * LightDistance));
		WorldToCamera = WorldToCamera * FCoords(
											FVector(0,0,0),
											HackVector(WorldSurfaceX),
											HackVector(WorldSurfaceY),
											HackVector(WorldSurfaceZ)
											).Matrix();

		CameraToWorld = WorldToCamera.Inverse();

		// Initialize the projection matrix.

		FVector	CameraSurfaceCenter = WorldToCamera.TransformFVector(WorldSurfaceCenter),
				CameraSurfaceX = WorldToCamera.TransformNormal(WorldSurfaceX),
				CameraSurfaceY = WorldToCamera.TransformNormal(WorldSurfaceY),
				CameraSurfaceZ = WorldToCamera.TransformNormal(WorldSurfaceZ);
		FLOAT	MinZ = 1.0f,
				MaxZ = CameraSurfaceCenter.Z;

		CameraToScreen = FMatrix::Identity;
		CameraToScreen = CameraToScreen * FMatrix(
											FPlane(1.0f,											0.0f,											0.0f,	0.0f),
											FPlane(0.0f,											1.0f,											0.0f,	0.0f),
											FPlane(-CameraSurfaceCenter.X / CameraSurfaceCenter.Z,	-CameraSurfaceCenter.Y / CameraSurfaceCenter.Z,	1.0f,	0.0f),
											FPlane(0.0f,											0.0f,											0.0f,	1.0f)
											);
		CameraToScreen = CameraToScreen * FMatrix(
											FPlane(1.0f / CameraSurfaceX.Size(),	0.0f,							0.0f,	0.0f),
											FPlane(0.0f,							1.0f / CameraSurfaceY.Size(),	0.0f,	0.0f),
											FPlane(0.0f,							0.0f,							1.0f,	0.0f),
											FPlane(0.0f,							0.0f,							0.0f,	1.0f)
											);
		CameraToScreen = CameraToScreen * FOrthoMatrix(1.0f,-1.0f,1.0f / (MaxZ - MinZ),-MinZ);

		ScreenToCamera = CameraToScreen.Inverse();

		WorldToScreen = WorldToCamera * CameraToScreen;
		ScreenToWorld = ScreenToCamera * CameraToWorld;

		ViewZone = 0;

		unguard;
	}

	// GetViewFrustum

	virtual FConvexVolume GetViewFrustum()
	{
		FConvexVolume	ViewFrustum;
		FVector	ViewSides[2][2][2];
		FLOAT	TempSigns[2] = { -1.0f, 1.0f };

		for(INT X = 0;X < 2;X++)
			for(INT Y = 0;Y < 2;Y++)
				for(INT Z = 0;Z < 2;Z++)
					ViewSides[X][Y][Z] = Deproject(FPlane(TempSigns[X],TempSigns[Y],Z ? 1.0f : 0.0f,1.0f));

		if(WorldToScreen.Determinant() < 0.0f)
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewSides[0][0][0],ViewSides[0][0][1],ViewSides[1][0][1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewSides[1][0][0],ViewSides[1][0][1],ViewSides[1][1][1]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewSides[1][1][0],ViewSides[1][1][1],ViewSides[0][1][1]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewSides[0][1][0],ViewSides[0][1][1],ViewSides[0][0][1]);

			ViewFrustum.BoundingPlanes[4] = FPlane(ViewSides[0][0][0],ViewSides[1][0][0],ViewSides[1][1][0]);
			ViewFrustum.BoundingPlanes[5] = FPlane(ViewSides[0][0][1],ViewSides[1][1][1],ViewSides[1][0][1]);
		}
		else
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewSides[0][0][0],ViewSides[1][0][1],ViewSides[0][0][1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewSides[1][0][0],ViewSides[1][1][1],ViewSides[1][0][1]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewSides[1][1][0],ViewSides[0][1][1],ViewSides[1][1][1]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewSides[0][1][0],ViewSides[0][0][1],ViewSides[0][1][1]);

			ViewFrustum.BoundingPlanes[4] = FPlane(ViewSides[0][0][0],ViewSides[1][1][0],ViewSides[1][0][0]);
			ViewFrustum.BoundingPlanes[5] = FPlane(ViewSides[0][0][1],ViewSides[1][0][1],ViewSides[1][1][1]);
		}

		ViewFrustum.NumPlanes = 6;

		return ViewFrustum;
	}
};

//
//	FSurfaceLayout
//

class FSurfaceLayout
	{
	public:

	INT							Width,
								Height;
	TArray<INT>					AllocatedTexels;

		// Constructors.

	FSurfaceLayout() {}

	FSurfaceLayout(INT InWidth,INT InHeight)
		{
		Width = InWidth;
		Height = InHeight;
		AllocatedTexels.AddZeroed(InHeight);
		}

	// AddSurface

	UBOOL AddSurface(INT* OffsetX,INT* OffsetY,INT SizeX,INT SizeY)
		{
		guard(FSurfaceLayout::AddSurface);

		INT	BestX = Width,
			BestY = Height,
			BestWaste = Width * Height;

		for(INT Y = 0;Y < Height - SizeY;Y++)
			{
			INT	MaxX = 0,
				Waste = 0;

			for(INT Row = 0;Row < SizeY;Row++)
				MaxX = Max(AllocatedTexels(Y + Row),MaxX);

			for(INT Row = 0;Row < SizeY;Row++)
				Waste += MaxX - AllocatedTexels(Y + Row);

			if(MaxX <= Width - SizeX && Waste < BestWaste)
			{
				BestX = MaxX;
				BestY = Y;
				BestWaste = Waste;
			}
		}

		if(BestX <= Width - SizeX && BestY <= Height - SizeY)
		{
			*OffsetX = BestX;
			*OffsetY = BestY;

			for(INT Row = 0;Row < SizeY;Row++)
				AllocatedTexels(BestY + Row) = BestX + SizeX;

			return 1;
		}
		else
			return 0;

		unguard;
		}
};

//
//	CreateLightMapTexture
//

static FSurfaceLayout	LightMapLayout;

void CreateLightMapTexture(ULevel* Level)
{
	guard(CreateLightMapTexture);

	// Reset the layout state.

	LightMapLayout = FSurfaceLayout(LIGHTMAP_TEXTURE_WIDTH,LIGHTMAP_TEXTURE_HEIGHT);

	FLightMapTexture*	Texture = new(Level->Model->LightMapTextures) FLightMapTexture(Level);

	Texture->Revision++;

		unguard;
	}

//
//	SetupLightMap
//
INT SetupLightMap(ULevel* Level,INT iSurf,INT iZone)
{
	guard(SetupSurfaceLightMap);

	UModel*		Model = Level->Model;
		FBspSurf&		Surf = Model->Surfs(iSurf);

	// Find a matching surface light map.

	for(INT LightMapIndex = 0;LightMapIndex < Model->LightMaps.Num();LightMapIndex++)
	{
		FLightMap&	LightMap = Model->LightMaps(LightMapIndex);

		if(LightMap.iSurf == iSurf && LightMap.iZone == iZone)
			return LightMapIndex;
	}

		// Find minimum and maximum texture coordinates on this surface.

	FVector	AxisU,
			AxisV;

	Model->Vectors(Surf.vNormal).FindBestAxisVectors(AxisU,AxisV);

		FLOAT		MinU = +10000000.0,
					MinV = +10000000.0,
					MaxU = -10000000.0,
					MaxV = -10000000.0;

		for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
		{
			FBspNode&	Node = Model->Nodes(NodeIndex);

		if(Node.iSurf == iSurf)
			{
				for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
				{
				FLOAT	U = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex) | AxisU,
						V = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex) | AxisV;

				MinU = Min(U,MinU);
				MaxU = Max(U,MaxU);
				MinV = Min(V,MinV);
				MaxV = Max(V,MaxV);
				}
			}
		}

	// Allocate a new light map.

	FLightMap*	LightMap = new(Model->LightMaps) FLightMap(Level,iSurf,iZone);
	INT			iLightMap = LightMap - &Model->LightMaps(0);

	LightMap->Revision++;

		// Calculate the lightmap pan and scale relative to the surface texture.

	FLOAT	UScale = Surf.LightMapScale,
			VScale = Surf.LightMapScale;
	FVector	Pan = FVector(0,0,0);
	INT		SizeGranularity = (GRebuildTools.GetCurrent()->LightmapFormat == TEXF_RGBA8) ? 1 : 8;

	for(;;)
	{
		FLOAT	UExtent	= MaxU - MinU;

		LightMap->SizeX	= (appCeil(UExtent / UScale) + 2 + (SizeGranularity - 1)) & ~(SizeGranularity - 1);

		if(LightMap->SizeX <= LIGHTMAP_MAX_WIDTH)
			break;

		UScale *= 2.0f;
		VScale *= 2.0f;
	}

	for(;;)
	{
		FLOAT	VExtent = MaxV - MinV; 

		LightMap->SizeY = (appCeil(VExtent / VScale) + 2 + (SizeGranularity - 1)) & ~(SizeGranularity - 1);

		if(LightMap->SizeY <= LIGHTMAP_MAX_HEIGHT)
			break;

		UScale *= 2.0f;
		VScale *= 2.0f;
	}

	UScale = (MaxU - MinU) / ((FLOAT) LightMap->SizeX - 2);
	VScale = (MaxV - MinV) / ((FLOAT) LightMap->SizeY - 2);

	Pan.X = MinU - UScale;
	Pan.Y = MinV - VScale;

	// Allocate texture space for the lightmap.

	if(!LightMapLayout.AddSurface(&LightMap->OffsetX,&LightMap->OffsetY,LightMap->SizeX,LightMap->SizeY))
	{
		CreateLightMapTexture(Level);
		verify(LightMapLayout.AddSurface(&LightMap->OffsetX,&LightMap->OffsetY,LightMap->SizeX,LightMap->SizeY)); // gam
	}

	LightMap->iTexture = Model->LightMapTextures.Num() - 1;
	Model->LightMapTextures(LightMap->iTexture).LightMaps.AddItem(iLightMap);

		// Calculate the world to light map transform.

	LightMap->WorldToLightMap = FMatrix(
									FPlane(	AxisU.X,	AxisV.X,	AxisU.Y * AxisV.Z - AxisU.Z * AxisV.Y,	0	),
									FPlane(	AxisU.Y,	AxisV.Y,	AxisU.Z * AxisV.X - AxisU.X * AxisV.Z,	0	),
									FPlane(	AxisU.Z,	AxisV.Z,	AxisU.X * AxisV.Y - AxisU.Y * AxisV.X,	0	),
									FPlane(	0,			0,			0,										1	)
									) *
								FTranslationMatrix(
											-FVector(
												Pan.X,
												Pan.Y,
												0
												)
											) *
										FScaleMatrix(
											FVector(
												1.0f / UScale,
												1.0f / VScale,
												1.0f
												)
											);

	// Calculate the lightmap base and axis vectors, and project them onto the surface plane.

	FMatrix	LightMapToWorld = LightMap->WorldToLightMap.Inverse();

	LightMap->LightMapBase = LightMapToWorld.TransformFVector(FVector(0,0,0));
	LightMap->LightMapX = LightMapToWorld.TransformNormal(FVector(1,0,0));
	LightMap->LightMapY = LightMapToWorld.TransformNormal(FVector(0,1,0));

		FProjectTextureToPlane(
		LightMap->LightMapBase,
		LightMap->LightMapX,
		LightMap->LightMapY,
			Surf.Plane
			);

	return iLightMap;

	unguard;
}


//
//	CalculateLeafLights
//

void CalculateLeafLights(UModel* Model,INT iLeaf,TArray<AActor*>* OutLeafLights)
{
	guard(CalculateLeafLights);

	FLeaf&	Leaf = Model->Leaves(iLeaf);

	if(Leaf.iPermeating != INDEX_NONE)
	{
		for(INT PermeatingIndex = Leaf.iPermeating;Model->Lights(PermeatingIndex);PermeatingIndex++)
		{
			AActor*	LightActor = Model->Lights(PermeatingIndex);

			if(!LightActor->bDeleteMe)
			{
				if(!LightActor->bDynamicLight)
					OutLeafLights->AddItem(LightActor);
			}
		}
	}

	unguard;
}

//
//	CalculateSurfaceLights
//

void CalculateSurfaceLights(UModel* Model,INT iNode,TArray<AActor*>* Lights,TArray<AActor*>* SurfaceLights)
	{
	guard(CalculateSurfaceLights);

	FBspNode&	Node = Model->Nodes(iNode);
	FBspSurf&	Surf = Model->Surfs(Node.iSurf);
	UBOOL		SpecialLit = (Surf.PolyFlags & PF_SpecialLit) ? 1 : 0;

	for(INT LightIndex = 0;LightIndex < Lights->Num();LightIndex++)
	{
		AActor*	Light = (*Lights)(LightIndex);

		if((Light->bSpecialLit ? 1 : 0) == SpecialLit)
		{
			if(Light->LightEffect == LE_Sunlight)
			{
				if(
					(
						(Cast<ASkyZoneInfo>(Light->Region.Zone) || Node.iZone[1] == Light->Region.ZoneNumber) &&
						(-Light->Rotation.Vector() | Node.Plane) > 0.0f
						) ||
					(
						(Cast<ASkyZoneInfo>(Light->Region.Zone) || Node.iZone[0] == Light->Region.ZoneNumber) &&
						(-Light->Rotation.Vector() | Node.Plane) < 0.0f &&
						(Surf.PolyFlags & PF_TwoSided)
						)
					)
					SurfaceLights[Node.iSurf].AddUniqueItem(Light);
			}
			else
			{
				if((Light->Location - Node.ExclusiveSphereBound).SizeSquared() < Square(Light->WorldLightRadius() + Node.ExclusiveSphereBound.W) && ((Surf.PolyFlags & PF_TwoSided) || Node.Plane.PlaneDot(Light->Location) > 0.0f))
					SurfaceLights[Node.iSurf].AddUniqueItem(Light);
			}
		}
	}

	unguard;
}
		
//
//	CalculateNodeLights
//

void CalculateNodeLights(UModel* Model,INT iNode,TArray<AActor*>* OutNodeLights,TArray<AActor*>* SurfaceLights)
{
	guard(CalculateNodeLights);

	FBspNode&		Node = Model->Nodes(iNode);
	TArray<AActor*>	FrontLights,
					BackLights;

	if(Node.iLeaf[1] != INDEX_NONE)
		CalculateLeafLights(Model,Node.iLeaf[1],&FrontLights);
	else if(Node.iFront != INDEX_NONE)
		CalculateNodeLights(Model,Node.iFront,&FrontLights,SurfaceLights);

	if(Node.iLeaf[0] != INDEX_NONE)
		CalculateLeafLights(Model,Node.iLeaf[0],&BackLights);
	else if(Node.iBack != INDEX_NONE)
		CalculateNodeLights(Model,Node.iBack,&BackLights,SurfaceLights);

	for(INT iCoplanarNode = iNode;iCoplanarNode != INDEX_NONE;iCoplanarNode = Model->Nodes(iCoplanarNode).iPlane)
	{
		CalculateSurfaceLights(Model,iCoplanarNode,&FrontLights,SurfaceLights);

		if(Model->Surfs(Model->Nodes(iCoplanarNode).iSurf).PolyFlags & PF_TwoSided)
			CalculateSurfaceLights(Model,iCoplanarNode,&BackLights,SurfaceLights);
	}

	for(INT LightIndex = 0;LightIndex < FrontLights.Num();LightIndex++)
		OutNodeLights->AddUniqueItem(FrontLights(LightIndex));

	for(INT LightIndex = 0;LightIndex < BackLights.Num();LightIndex++)
		OutNodeLights->AddUniqueItem(BackLights(LightIndex));

	unguard;
}

//
//	FLightBitmapBatcher
//

class FLightBitmapBatcher
{
private:

	struct FLightBitmapRef
	{
		INT	iLightMap,
			iBitmap;
	};

	UModel*					Model;
	UViewport*				Viewport;
	FRenderInterface*		RI;

	TArray<FLightBitmapRef>	Bitmaps;
	FSurfaceLayout*			Layout;

public:

	// Constructor.

	FLightBitmapBatcher(UModel* InModel,UViewport* InViewport)
	{
		Model = InModel;
		Viewport = InViewport;
		RI = NULL;
		Layout = NULL;
	}

	// Flush.
	static FColor	FrameBuffer[FRAMEBUFFER_SIZE * FRAMEBUFFER_SIZE];

	void Flush()
	{
		if(Layout)
		{
			delete Layout;
			Layout = NULL;
		}

		if(RI)
		{
			Viewport->Unlock();
			RI = NULL;

			// Read the framebuffer back from the hardware.
			Viewport->RenDev->ReadPixels(Viewport,FrameBuffer);

			// Build light visibility bitmaps from the framebuffer.

			for(INT BitmapIndex = 0;BitmapIndex < Bitmaps.Num();BitmapIndex++)
			{
				FLightMap*		LightMap = &Model->LightMaps(Bitmaps(BitmapIndex).iLightMap);
				FLightBitmap*	LightBitmap = &LightMap->Bitmaps(Bitmaps(BitmapIndex).iBitmap);
				FVector			BitmapBase = LightMap->LightMapBase +
												LightBitmap->MinX * LightMap->LightMapX +
												LightBitmap->MinY * LightMap->LightMapY +
												1.0f * Model->Vectors(Model->Surfs(LightMap->iSurf).vNormal);

				for(INT Y = 0;Y < LightBitmap->SizeY;Y++)
				{
					FColor*	FrameBufferPtr = FrameBuffer + LightBitmap->OffsetX + (LightBitmap->OffsetY + Y) * FRAMEBUFFER_SIZE;
					BYTE*	BitmapPtr = &LightBitmap->Bits(Y * LightBitmap->Stride);
					BYTE	Mask = 1;

					for(INT X = 0;X < LightBitmap->SizeX;X++)
					{
						if(FrameBufferPtr[X].R == 255 && FrameBufferPtr[X].G == 255 && FrameBufferPtr[X].B == 255)
						{
							if(LightBitmap->LightActor->LightEffect != LE_Sunlight)
								*BitmapPtr |= Mask;
							else if(Model->PointRegion(Viewport->Actor->Level,BitmapBase + LightMap->LightMapX * X + LightMap->LightMapY * Y).ZoneNumber != 0)
								*BitmapPtr |= Mask;
							else
								*BitmapPtr &= ~Mask;
						}
						else
							*BitmapPtr &= ~Mask;

						Mask <<= 1;

						if(!Mask)
						{
							BitmapPtr++;
							Mask = 1;
						}
					}
				}
			}

			Bitmaps.Empty();
		}
	}

	// Destructor.

	~FLightBitmapBatcher()
	{
		Flush();
	}

	// AddBitmap.

	void AddBitmap(INT LightMapIndex,INT BitmapIndex)
	{
		if(!Layout)
			Layout = new FSurfaceLayout(FRAMEBUFFER_SIZE,FRAMEBUFFER_SIZE);

		FLightMap*		LightMap = &Model->LightMaps(LightMapIndex);
		FLightBitmap*	LightBitmap = &LightMap->Bitmaps(BitmapIndex);

		if(!Layout->AddSurface(&LightBitmap->OffsetX,&LightBitmap->OffsetY,LightBitmap->SizeX,LightBitmap->SizeY))
		{
			Flush();
			Layout = new FSurfaceLayout(FRAMEBUFFER_SIZE,FRAMEBUFFER_SIZE);
			verify(Layout->AddSurface(&LightBitmap->OffsetX,&LightBitmap->OffsetY,LightBitmap->SizeX,LightBitmap->SizeY));
		}

		if(!RI)
		{
			while(!Viewport->Lock())
				appSleep(0.1f);
			RI = Viewport->RI;
		}

		RI->PushState();

		RI->SetViewport(
			LightBitmap->OffsetX,
			LightBitmap->OffsetY,
			LightBitmap->SizeX,
			LightBitmap->SizeY
			);

		TArray<INT>	SkySurfaces;

		if(LightBitmap->LightActor->LightEffect == LE_Sunlight)
		{
			// Hide fake backdrop polygons.

			for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
				if((Model->Surfs(SurfaceIndex).PolyFlags & PF_FakeBackdrop) && !(Model->Surfs(SurfaceIndex).PolyFlags & PF_Invisible))
				{
					Model->Surfs(SurfaceIndex).PolyFlags |= PF_Invisible;
					SkySurfaces.AddItem(SurfaceIndex);
				}
		}

		// Render the light visibility.

		if(LightBitmap->LightActor->LightEffect == LE_Sunlight)
			FDirectionalLightMapSceneNode(
				Viewport,
				LightBitmap->LightActor,
				Model->Surfs(LightMap->iSurf),
				LightMap
				).Render(Viewport->RI);
		else
			FPointLightMapSceneNode(
				Viewport,
				LightBitmap->LightActor,
				Model->Surfs(LightMap->iSurf),
				LightMap,
				LightBitmap->MinX,
				LightBitmap->MinY,
				LightBitmap->MaxX,
				LightBitmap->MaxY
				).Render(Viewport->RI);

		// Restore the visibility of fake backdrop polygons.

		for(INT SurfaceIndex = 0;SurfaceIndex < SkySurfaces.Num();SurfaceIndex++)
			Model->Surfs(SkySurfaces(SurfaceIndex)).PolyFlags &= ~PF_Invisible;

		RI->PopState();

		FLightBitmapRef	Ref = { LightMapIndex, BitmapIndex };

		Bitmaps.AddItem(Ref);
	}
};

// MacOSX linker doesn't like this being in the class's inlined method. --ryan.
FColor	FLightBitmapBatcher::FrameBuffer[FRAMEBUFFER_SIZE * FRAMEBUFFER_SIZE];


INT Compare(AActor* A, AActor* B) // sjs - test
{
    if( A->LightEffect == LE_Negative && B->LightEffect != LE_Negative )
        return 1;
    if( B->LightEffect == LE_Negative && A->LightEffect != LE_Negative )
        return -1;
    return 0;
}

//
//	UModel::Illuminate
//
void UModel::Illuminate(AActor* Owner,UBOOL ChangedOnly)
{
	guard(UModel::Illuminate);

#if !CONSOLE

	ULevel*	Level = Owner->XLevel;

	if(this == Level->Model && Nodes.Num() > 0)
	{
		DOUBLE	StartSeconds = appSeconds();
		INT		NumRays = 0;

		GWarn->BeginSlowTask(TEXT("Illuminating BSP"),1);

		// Create a lighting viewport.

		UViewport*	LightingViewport = Level->Engine->Client->NewViewport(FName(TEXT("LightingViewport")));

		Level->SpawnViewActor(LightingViewport);
		LightingViewport->Input->Init(LightingViewport);
		LightingViewport->OpenWindow(0,0,128,128,64,64);

		LightingViewport->Actor->RendMap = REN_PlainTex;
		LightingViewport->Actor->ShowFlags = (LightingViewport->Actor->ShowFlags | SHOW_PlayerCtrl) & ~SHOW_Backdrop;

		if(LightingViewport->Lock(NULL,NULL))
		{
			LightingViewport->RI->Clear(1,FColor(0,0,0),1,1.0f);

			LightingViewport->Unlock();
			LightingViewport->Present();
		}

		LightingViewport->SizeX = FRAMEBUFFER_SIZE;
		LightingViewport->SizeY = FRAMEBUFFER_SIZE;

		DynamicLightMaps.Empty();

		if(!ChangedOnly)
		{
			// Empty lighting.

			LightMaps.Empty();
			LightMapTextures.Empty();

			for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
				Nodes(NodeIndex).iLightMap = INDEX_NONE;

			for(INT SectionIndex = 0;SectionIndex < Sections.Num();SectionIndex++)
				Sections(SectionIndex).iLightMapTexture = INDEX_NONE;

			// Build lightmap layouts.

			CreateLightMapTexture(Level);

			for(INT SectionIndex = 0;SectionIndex < Sections.Num();SectionIndex++)
			{
				for(INT ZoneIndex = 0;ZoneIndex < NumZones;ZoneIndex++)
				{
					for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
					{
						FBspNode&	Node = Nodes(NodeIndex);

						if(Node.iZone[1] == ZoneIndex && Node.iSection == SectionIndex)
						{
							FBspSurf&	Surf = Surfs(Node.iSurf);

							if(!(Surf.PolyFlags & PF_NoShadows))
								Node.iLightMap = SetupLightMap(Level,Node.iSurf,Node.iZone[1]);
						}
					}
				}
			}
		}

		// Determine relevant lights.

		TArray<AActor*>*	SurfaceLights;
		TArray<AActor*>		RootNodeLights;

		SurfaceLights = new TArray<AActor*>[Surfs.Num()];
		CalculateNodeLights(this,0,&RootNodeLights,SurfaceLights);

		// Prepare bitmaps for raytracing.

		FLightBitmapBatcher	BitmapBatcher(this,LightingViewport);

		for(INT LightMapIndex = 0;LightMapIndex < LightMaps.Num();LightMapIndex++)
		{
			FLightMap*	LightMap = &LightMaps(LightMapIndex);
			FBspSurf&	Surf = Surfs(LightMap->iSurf);

			GWarn->StatusUpdatef(LightMapIndex,LightMaps.Num(),TEXT("Illuminating BSP"));

			LightMap->DynamicLights.Empty();

			// Remove old light bitmaps.

			if(ChangedOnly)
			{
				for(INT BitmapIndex = 0;BitmapIndex < LightMap->Bitmaps.Num();BitmapIndex++)
					if(LightMap->Bitmaps(BitmapIndex).LightActor->bLightChanged || LightMap->Bitmaps(BitmapIndex).LightActor->bDeleteMe)
					{
						LightMap->Bitmaps.Remove(BitmapIndex--);
						LightMap->Revision++;
						LightMapTextures(LightMap->iTexture).Revision++;
					}
		    }

            Sort(&SurfaceLights[LightMap->iSurf](0), SurfaceLights[LightMap->iSurf].Num()); // sjs - sort negative lights

			// Add new light bitmaps.
			for(INT LightIndex = 0;LightIndex < SurfaceLights[LightMap->iSurf].Num();LightIndex++)
			{
				AActor*			LightActor = SurfaceLights[LightMap->iSurf](LightIndex);

                // gam ---
                if( LightActor->bHiddenEd || LightActor->bHiddenEdGroup )
                    continue;
                // --- gam

				if(!ChangedOnly || LightActor->bLightChanged)
				{
					// Calculate the area of the lightmap affected by the light.

					INT		MinX,
							MinY,
							MaxX,
							MaxY;

					if(LightActor->LightEffect == LE_Sunlight)
					{
						MinX = 0;
						MinY = 0;
						MaxX = LightMap->SizeX - 1;
						MaxY = LightMap->SizeY - 1;
					}
					else
					{
						FLOAT	PlaneDot = Surf.Plane.PlaneDot(LightActor->Location),
								PlaneRadius = appSqrt(Max<FLOAT>(Square(LightActor->WorldLightRadius()) * 1.05f - Square(PlaneDot),0.0));
						FVector	WorldCenter = LightActor->Location - Surf.Plane * PlaneDot,
								LightMapCenter = LightMap->WorldToLightMap.TransformFVector(WorldCenter);
						FLOAT	RadiusX = PlaneRadius / LightMap->LightMapX.Size(),
								RadiusY = PlaneRadius / LightMap->LightMapY.Size();

						MinX = Clamp<INT>((INT)(LightMapCenter.X - RadiusX),0,LightMap->SizeX - 1);
						MaxX = Clamp<INT>((INT)(LightMapCenter.X + RadiusX),0,LightMap->SizeX - 1);
						MinY = Clamp<INT>((INT)(LightMapCenter.Y - RadiusY),0,LightMap->SizeY - 1);
						MaxY = Clamp<INT>((INT)(LightMapCenter.Y + RadiusY),0,LightMap->SizeY - 1);
					}

					// Raytrace the new light bitmap.

					new(LightMap->Bitmaps) FLightBitmap(LightActor,MinX,MinY,MaxX,MaxY);
					BitmapBatcher.AddBitmap(LightMapIndex,LightMap->Bitmaps.Num() - 1);
					NumRays += (MaxX - MinX + 1) * (MaxY - MinY + 1);

					LightMap->Revision++;
					LightMapTextures(LightMap->iTexture).Revision++;
				}
			}
		}

		BitmapBatcher.Flush();

		// Reject totally black bitmaps.

		for(INT LightMapIndex = 0;LightMapIndex < LightMaps.Num();LightMapIndex++)
		{
			FLightMap*	LightMap = &LightMaps(LightMapIndex);

			for(INT BitmapIndex = 0;BitmapIndex < LightMap->Bitmaps.Num();BitmapIndex++)
			{
				FLightBitmap*	LightBitmap = &LightMap->Bitmaps(BitmapIndex);
				UBOOL			Hit = 0;

				for(INT Index = 0;Index < LightBitmap->Bits.Num();Index++)
					if(LightBitmap->Bits(Index))
					{
						Hit = 1;
						break;
		}

				if(!Hit)
					LightMap->Bitmaps.Remove(BitmapIndex--);
			}
		}

		// Cleanup the relevant light lists.

		delete [] SurfaceLights;

		// Close the lighting viewport.

		delete LightingViewport;

		if(!ChangedOnly)
		{
			// Rebuild vertex buffers.

			ClearRenderData(NULL);
		}

		GWarn->EndSlowTask();

		debugf(TEXT("UModel::Illuminate: %u rays, %f seconds"),NumRays,appSeconds() - StartSeconds);
	}
#else
	check(0);
#endif

		unguard;
	}

//
//	DXT compress hacks.
//
#if _MSC_VER && !_XBOX && !_WIN64
#pragma pack (push,8)
typedef long HRESULT; 
#include "../../nvDXT/Inc/dxtlib.h"
#pragma pack (pop)

static FStaticLightMapTexture*	CurrentLightMapTexture = NULL;
static HRESULT LightMapCompressionCallback(void* Data,INT MipIndex,DWORD Size,INT Width, INT Height, void* UserData )
{
	if(CurrentLightMapTexture && MipIndex < 2)
	{
		CurrentLightMapTexture->Data[MipIndex].Empty(Size);
		CurrentLightMapTexture->Data[MipIndex].Add(Size);
		appMemcpy(&CurrentLightMapTexture->Data[MipIndex](0),Data,Size);
	}
	return 0;
}
#endif

//
//	UModel::CompressLightmaps
//
void UModel::CompressLightmaps()
{
	guard(UModel::CompressLightmaps);
#if !CONSOLE && !__UNIX__ && !_WIN64
	GWarn->BeginSlowTask(TEXT("Compressing lightmaps..."),1);
	for(INT TextureIndex = 0;TextureIndex < LightMapTextures.Num();TextureIndex++)
	{
		FLightMapTexture*	Texture		= &LightMapTextures(TextureIndex);
	
		if( Texture->StaticTexture.Revision != Texture->Revision )
		{
			if(GRebuildTools.GetCurrent()->LightmapFormat == TEXF_RGBA8)
			{
				Texture->StaticTexture.Data[0].Empty();
				Texture->StaticTexture.Data[1].Empty();
			}
			else
			{
				FColor*				TextureData = new FColor[LIGHTMAP_TEXTURE_WIDTH * LIGHTMAP_TEXTURE_HEIGHT];

				GWarn->StatusUpdatef(TextureIndex,LightMapTextures.Num(),TEXT("Compressing lightmaps..."));

				// Calculate the lightmap.

				for(INT LightMapIndex = 0;LightMapIndex < Texture->LightMaps.Num();LightMapIndex++)
				{
					FLightMap*		LightMap	= &LightMaps(Texture->LightMaps(LightMapIndex));
					FColor*			LightMapData= TextureData + LightMap->OffsetX + LightMap->OffsetY * LIGHTMAP_TEXTURE_WIDTH;

					LightMap->GetTextureData(0,LightMapData,LIGHTMAP_TEXTURE_WIDTH * 4,TEXF_RGBA8,0);
				}
				
				// Setup compression options.
				FRebuildOptions*	Options		= GRebuildTools.GetCurrent();
				UBOOL				UseDXT3		= Options->LightmapFormat == TEXF_DXT3 ? 1 : 0;
				CompressionOptions	nvOptions;

				// Save out lightmaps for debugging purposes.
				if( Options->SaveOutLightmaps )
					appCreateBitmap( TEXT("LM-"), LIGHTMAP_TEXTURE_WIDTH, LIGHTMAP_TEXTURE_HEIGHT, (DWORD*) TextureData );
						
				nvOptions.bMipMapsInImage		= 0;
				nvOptions.MipMapType			= dGenerateMipMaps;
				nvOptions.MIPFilterType			= kMIPFilterCubic;
				nvOptions.bDitherColor			= Options->DitherLightmaps ? 1 : 0;
				nvOptions.bDitherEachMIPLevel	= Options->DitherLightmaps ? 1 : 0;
				nvOptions.bGreyScale			= Options->DitherLightmaps ? 1 : 0;
				nvOptions.TextureType			= dTextureType2D;
				nvOptions.TextureFormat			= UseDXT3 ? kDXT3 : kDXT1;

				// Compress...
				CurrentLightMapTexture			= &Texture->StaticTexture;
				nvDXTcompress(
					(BYTE*) TextureData,			// src
					LIGHTMAP_TEXTURE_WIDTH,			// width
					LIGHTMAP_TEXTURE_HEIGHT,		// height
					LIGHTMAP_TEXTURE_WIDTH * 4,		// pitch
					&nvOptions,						// compression options
					4,								// depth
					LightMapCompressionCallback		// callback
				);

				CurrentLightMapTexture			= NULL;

				Texture->StaticTexture.Width	= LIGHTMAP_TEXTURE_WIDTH;
				Texture->StaticTexture.Height	= LIGHTMAP_TEXTURE_HEIGHT;
				Texture->StaticTexture.Format	= UseDXT3 ? TEXF_DXT3 : TEXF_DXT1;
				Texture->StaticTexture.Revision	= Texture->Revision;

				delete [] TextureData;
			}
		}		
	}		
	GWarn->EndSlowTask();
#endif
	unguard;
}

#else // sjs - CONSOLE

void UModel::Illuminate(AActor* Owner,UBOOL ChangedOnly)
{
    appErrorf(TEXT("Console called UModel::Illuminate!"));
}

#endif 

