/*============================================================================
	Karma Integration Support
    
    - TriList BSP/Terrain query
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA

#define USE_EDGES (0)

/*********************** TRI LIST GENERATOR *********************/

/*  Gets the offset in the vertex_memory of a vertex at position
    (lx, ly) within region. */
#define GET_VERT(lx, ly) ((lx) + ((ly)*spanX))

/*  Add all visible tris in region to trilist. */
static void MEAPI KAddHeightmapTriangles(ATerrainInfo* t, 
                                         INT min[2], INT max[2], 
                                         KarmaTriListData *triData)
{
    guard(KAddHeightmapTriangles);
    /* Defines how me make 2 triangles out of a heightfield square.
       First/second triangle, vertex1/2/3, dx/dy */
    static int normalOrder[2][3][2] = 
    {
        {{0, 0}, {1, 1}, {0, 1}},
        {{0, 0}, {1, 0}, {1, 1}}
    };
    static int turnedOrder[2][3][2] = 
    {
        {{0, 0}, {1, 0}, {0, 1}},
        {{1, 0}, {1, 1}, {0, 1}}
    };
	static int normalWinding[3] = 
	{ 0, 1, 2 };
	static int invertedWinding[3] = 
	{ 0, 2, 1 };

    INT ix, iy, k, m, spanX, spanY, vert_count = 0, tri_count = 0;

	INT stride = 1;
	if(t->bKCollisionHalfRes)
	{
		// If using the bKCollisionHalfRes flag, we ensure the sizes of the query region are even.
		if( (max[0] - min[0]) & 0x01 ) // If size is odd.
		{
			if(min[0] > 0)
				min[0] -= 1;
			else if(max[0] < t->HeightmapX-1)
				max[0] += 1;
		}

		if( (max[1] - min[1]) & 0x01 )
		{
			if(min[1] > 0)
				min[1] -= 1;
			else if(max[1] < t->HeightmapY-1)
				max[1] += 1;
		}

		check(((max[0] - min[0]) & 0x01) == 0);
		check(((max[1] - min[1]) & 0x01) == 0);

		stride = 2;

		/* Number of vertices in X and Y directions. */
		spanX = (max[0] - min[0])/2 + 1;
		spanY = (max[1] - min[1])/2 + 1;
	}
	else
	{
		spanX = max[0] - min[0] + 1;
		spanY = max[1] - min[1] + 1;
	}

    /* First, copy all vertices into vertex_memory */
    for(iy=min[1]; iy<=max[1]; iy += stride)
    {
        for(ix=min[0]; ix<=max[0]; ix += stride)
        {
			if(triData->num_vert+vert_count >= KTRILIST_SIZE*3)
				return;

            KU2MEPosition(triData->vertex_memory[triData->num_vert+vert_count],
                t->Vertices(t->GetGlobalVertex(ix, iy)));

            vert_count++;
        }
    }

    /* Then make each triangle. */

	INT lix, liy; // These are indices into the 2-d array of verts generated above.
    for(iy=min[1], liy=0; iy<max[1]; iy += stride, liy++)
    {
        for(ix=min[0], lix=0; ix<max[0]; ix += stride, lix++)
        {
            /* Check if this sqare is 'visible' */
            if(t->bKCollisionHalfRes || t->GetQuadVisibilityBitmap(ix, iy))
            {
				// Here we figure out the material (note for bKCollisionHalfRes, uses bottom-left quad)
				MeReal quadFriction = 1, quadRestitution = 1;
				UMaterial* quadMaterial = t->GetQuadDomMaterialBitmap(ix, iy);

                /* [ix,iy] now points at bottom left-hand corner of possible square (2 triangles). */
                for(k=0; k<2; k++)
				{
					if(triData->num_tri + tri_count >= KTRILIST_SIZE)
						return;

                    McdUserTriangle *tri = &(triData->triangles[triData->num_tri + tri_count]);

					// If bKCollisionHalfRes, ignore edge turn bitmap.
                    MeBool turn;
					if(t->bKCollisionHalfRes)
						turn = 0;
					else
						turn = t->GetEdgeTurnBitmap(ix, iy);

                    for(m=0; m<3; m++)
                    {
						INT vIndex;
						if(t->Inverted)
							vIndex = invertedWinding[m];
						else
							vIndex = normalWinding[m];

                        INT vOffset;				
                        if(turn)
                            vOffset = GET_VERT(lix + turnedOrder[k][vIndex][0], liy + turnedOrder[k][vIndex][1]);
                        else
                            vOffset = GET_VERT(lix + normalOrder[k][vIndex][0], liy + normalOrder[k][vIndex][1]);

						check(vOffset >= 0 && vOffset < vert_count);

                        tri->vertices[m] = &(triData->vertex_memory[triData->num_vert+vOffset]);
                    }
                    
                    /* Calculate Normal -- Must be related to vertices / edges using RH rule */
                    MeVector3 edge1, edge2;
                    
                    MeVector3Subtract(edge1, *(tri->vertices[1]), *(tri->vertices[0]));
                    MeVector3Subtract(edge2, *(tri->vertices[2]), *(tri->vertices[1]));
                    
                    int nix = triData->num_norm + tri_count;

					if(nix >= KTRILIST_SIZE)
						return;

                    MeVector3Cross(triData->normal_memory[nix], edge1, edge2);
                    MeVector3Normalize(triData->normal_memory[nix]);
                    
                    tri->normal = (&triData->normal_memory[nix]);
                    
                    /* KTODO: Check angle between tris to set flags. */
                    tri->flags = (McdTriangleFlags)(kMcdTriangleUseSmallestPenetration);
#if USE_EDGES
                    tri->flags = (McdTriangleFlags)(tri->flags | kMcdTriangleUseEdge0 | kMcdTriangleUseEdge1 | kMcdTriangleUseEdge2 );
#endif

					KarmaTriUserData* tUData = &triData->tri_list_udata[nix];
					tUData->localFriction = quadFriction;
                    tUData->localRestitution = quadRestitution;
					tUData->localMaterial = quadMaterial;
                    tri->triangleData.ptr = tUData;

                    tri_count++;
                } /* for k (each triangle) */
            } /* if visible */
        } /* for ix */
    } /* for iy */

    triData->num_tri += tri_count;
    triData->num_norm += tri_count;
    triData->num_vert += vert_count;
    unguard;
}

static UBOOL MEAPI KSphereCheck(const FSphere* s1, const FSphere* s2)
{
    FVector delta = *s2 - *s1;
    if(delta.SizeSquared() < (s1->W + s2->W) * (s1->W + s2->W))
        return 1;
    else
        return 0;
}

/* Traverse BSP tree (iterate/recurse) adding relevant tris */
static void MEAPI KAddBSPTriangles(UModel* model, INT iNode, const FSphere* Sphere, KarmaTriListData *triData)
{
    guard(KAddBSPTriangles);
	do
	{
		FBspNode* Node   = &model->Nodes( iNode );
		FLOAT Dist       = Node->Plane.PlaneDot(*Sphere);
        
		if( Dist < -Sphere->W ) /* Entirely behind plane */
			iNode = Node->iBack;
		else if(Dist > Sphere->W) /* Entirely in front of plane */
            iNode = Node->iFront;
        else /* Overlapping plane - add all coplanar polys.. */
        {
            INT iPNode = iNode;
            while(iPNode != INDEX_NONE)
            {
                FBspNode* PNode = &model->Nodes( iPNode );
    			FBspSurf* Surf = &model->Surfs(PNode->iSurf);

                int i, fVert = PNode->iVertPool;
                
                UBOOL overlap = KSphereCheck(&PNode->ExclusiveSphereBound, Sphere);
                
                if(overlap && PNode->NumVertices > 0 && !(Surf->PolyFlags & PF_NotSolid)) /* If there are any triangles to add. */
                {
                    /* Add Vertices */
                    for(i=0; i<PNode->NumVertices; i++)
                    {
						FVector uPoint = model->Points(model->Verts(fVert+i).pVertex);
						//FVector uPoint = model->Sections(PNode->iSection).Vertices.Vertices(PNode->iFirstVertex + i).Position;

						if(triData->num_vert+i >= KTRILIST_SIZE*3)
							return;

                        KU2MEPosition(triData->vertex_memory[triData->num_vert+i],
                            uPoint);
                    }

					if(triData->num_norm >= KTRILIST_SIZE)
						return;

                    /* Add Normal */
                    triData->normal_memory[triData->num_norm][0] = PNode->Plane.X;
                    triData->normal_memory[triData->num_norm][1] = PNode->Plane.Y;
                    triData->normal_memory[triData->num_norm][2] = PNode->Plane.Z;
                    
                    /* Add Triangles (as a fan about first vertex) */
                    for(i=0; i<PNode->NumVertices-2; i++)
                    {
						if(triData->num_tri >= KTRILIST_SIZE)
								return;

                        McdUserTriangle *tri = &(triData->triangles[triData->num_tri]);

                        tri->vertices[0] = &(triData->vertex_memory[triData->num_vert+0]);
                        tri->vertices[1] = &(triData->vertex_memory[triData->num_vert+i+1]);
                        tri->vertices[2] = &(triData->vertex_memory[triData->num_vert+i+2]);
                        
						// Dont add a triangle if its degenerate and has no area.
						MeVector3 edge1, edge2, triNorm;
						MeVector3Subtract(edge1, *(tri->vertices[1]), *(tri->vertices[0]));
						MeVector3Subtract(edge2, *(tri->vertices[2]), *(tri->vertices[1]));
						MeVector3Cross(triNorm, edge1, edge2);
						if( MeVector3MagnitudeSqr(triNorm) < (MeReal)0.01 * (MeReal)0.01 )
							continue;

                        tri->normal = &triData->normal_memory[triData->num_norm];
                        
                        // We only want 'edge' contacts for interior edges of the polygon. 
                        // KTODO: Dont use edges for edges beteen co-planar polys (tricky).
						tri->flags = (McdTriangleFlags)0;
						tri->flags = (McdTriangleFlags)(tri->flags | kMcdTriangleUseSmallestPenetration);

#if USE_EDGES
                        tri->flags = (McdTriangleFlags)(tri->flags | kMcdTriangleUseEdge1);

                        if(i == 0)
                            tri->flags = (McdTriangleFlags)(tri->flags | kMcdTriangleUseEdge0);
                    
                        if(i == PNode->NumVertices-2)
                            tri->flags = (McdTriangleFlags)(tri->flags | kMcdTriangleUseEdge2);
#endif

						KarmaTriUserData* tUData = &triData->tri_list_udata[triData->num_tri];
						tUData->localFriction = 1;
						tUData->localRestitution = 1;
						tUData->localMaterial = Surf->Material;
						tri->triangleData.ptr = tUData;

						triData->num_tri++;
                    }

                    //triData->num_tri += PNode->NumVertices-2;
                    triData->num_vert += PNode->NumVertices;
                    triData->num_norm += 1;
                }
                iPNode = PNode->iPlane;
            }

			if( Node->iBack != INDEX_NONE )
			    KAddBSPTriangles(model, Node->iBack, Sphere, triData);
			iNode = Node->iFront;
		}
	}
	while(iNode != INDEX_NONE);
    unguard;
}

// Query this actor against its level to find all the triangles near it that karma should collide against.
// This is called either as a callback from inside the nearfield test in Karma,
// or ahead of time.
void MEAPI KTriListQuery(ULevel* level, FSphere* sphere, KarmaTriListData* triData)
{
	guard(KTriListQuery);

	check(level);


	triData->num_tri = 0;
    triData->num_norm = 0;
    triData->num_vert = 0;

	if(KGData && KGData->bDisableTriCollision)
		return;

    /* ***** TERRAIN TRIANGLES ***** */
    guard(AddTerrain);
    for( INT z=0; z<64; z++ )
    {
        AZoneInfo* Z = level->GetZoneActor(z);
        if(Z && Z->bTerrainZone) 
        {
            for(INT t=0; t<Z->Terrains.Num(); t++)
            {
                ATerrainInfo* tInfo = Z->Terrains(t);
                INT min[2], max[2];

                FVector hPos = sphere->TransformPointBy(tInfo->ToHeightmap);

                MeReal rangeX = fabs((sphere->W)/tInfo->TerrainScale.X);
                MeReal rangeY = fabs((sphere->W)/tInfo->TerrainScale.Y);

                min[0] = MeCLAMP(appFloor(hPos[0] - rangeX), 0, tInfo->HeightmapX-1);
                max[0] = MeCLAMP(appCeil(hPos[0] + rangeX), 0, tInfo->HeightmapX-1);

                min[1] = MeCLAMP(appFloor(hPos[1] - rangeY), 0, tInfo->HeightmapY-1);
                max[1] = MeCLAMP(appCeil(hPos[1] + rangeY), 0, tInfo->HeightmapY-1);

                /* If there are some triangles under this object, add to list. */
                if((max[0] > min[0]) || (max[1] > min[1]))
                {
                    KAddHeightmapTriangles(tInfo, min, max, triData);
                }
            }
        }
    }
    unguard;

    /* ***** BSP TRIANGLES ***** */
	guard(AddBSP);
    KAddBSPTriangles(level->Model, 0, sphere, triData);
	unguard;


    /* ***** STATIC-MESH TRIANGLES ***** */
	guard(AddStaticMesh);

	FMemMark Mark(GMem);

	// Query to find all actors we are overlapping.
	FVector bMin = FVector(sphere->X - sphere->W, sphere->Y - sphere->W, sphere->Z - sphere->W);
	FVector bMax = FVector(sphere->X + sphere->W, sphere->Y + sphere->W, sphere->Z + sphere->W);
	FBox box = FBox(bMin, bMax);

	// We use a box check, with the extent the radius of the sphere
	FCheckResult* first = level->Hash->ActorOverlapCheck(GMem, NULL, &box, 1);
	for(FCheckResult* result = first; result; result=result->GetNext())
	{
		AActor* nearActor = result->Actor;

		// If this actor is set up so we shoul collide against it per-triangle, add triangles to the list here.
		if(	nearActor && nearActor->StaticMesh &&
			nearActor->bBlockKarma &&
			!nearActor->StaticMesh->UseSimpleKarmaCollision
		)
		{
			KGData->StaticMeshTris.Empty();
			FkDOPSphereQuery QueryInfo(nearActor, nearActor->StaticMesh, *sphere, KGData->StaticMeshTris);
			nearActor->StaticMesh->kDOPTree.SphereQuery(QueryInfo);

			FMatrix local2World = nearActor->LocalToWorld();
			FLOAT   l2w_det = local2World.Determinant();
			FMatrix l2w_TA = local2World.TransposeAdjoint();
			TArray<FStaticMeshVertex>& meshVerts = nearActor->StaticMesh->VertexStream.Vertices;

			// Then transform to Karma space and add to tri-list data.
			for(INT i=0; i<KGData->StaticMeshTris.Num(); i++)
			{
				if(	triData->num_tri >= KTRILIST_SIZE || 
					triData->num_vert+2 >= KTRILIST_SIZE*3 ||
					triData->num_norm >= KTRILIST_SIZE)
					return;

				McdUserTriangle *tri = &(triData->triangles[triData->num_tri]);

				tri->vertices[0] = &(triData->vertex_memory[triData->num_vert+0]);
				tri->vertices[1] = &(triData->vertex_memory[triData->num_vert+1]);
				tri->vertices[2] = &(triData->vertex_memory[triData->num_vert+2]);
                tri->normal = &triData->normal_memory[triData->num_norm];

				INT CollTriangleIndex = KGData->StaticMeshTris(i);
				FkDOPCollisionTriangle* meshTri =  &nearActor->StaticMesh->kDOPTree.Triangles( CollTriangleIndex );
				const FPlane& LocalNormal = ((meshVerts(meshTri->v2).Position - meshVerts(meshTri->v3).Position) ^ (meshVerts(meshTri->v1).Position - meshVerts(meshTri->v3).Position)).SafeNormal();

				// transform vertices and normal into world space.
				// note - this takes into account the static mesh scaling.
				FVector tmp;

				// If negative draw-scale - reverse triangle winding/direction
				if(l2w_det > 0.0f) // NORMAL
				{
					tmp = local2World.TransformFVector(meshVerts(meshTri->v1).Position);
					KU2MEPosition(*tri->vertices[0], tmp);

					tmp = local2World.TransformFVector(meshVerts(meshTri->v3).Position);
					KU2MEPosition(*tri->vertices[1], tmp);

					tmp = local2World.TransformFVector(meshVerts(meshTri->v2).Position);
					KU2MEPosition(*tri->vertices[2], tmp);
				}
				else // FLIP
				{
					tmp = local2World.TransformFVector(meshVerts(meshTri->v1).Position);
					KU2MEPosition(*tri->vertices[0], tmp);

					tmp = local2World.TransformFVector(meshVerts(meshTri->v2).Position);
					KU2MEPosition(*tri->vertices[1], tmp);

					tmp = local2World.TransformFVector(meshVerts(meshTri->v3).Position);
					KU2MEPosition(*tri->vertices[2], tmp);
				}

				// TransformByUsingAdjointT already handles flipping the normal if det is negative..
				tmp = LocalNormal.TransformByUsingAdjointT(local2World, l2w_det, l2w_TA);
				KU2MEVecCopy(*tri->normal, tmp);


				// one sided, use smallest penetration direction
				tri->flags = (McdTriangleFlags)(kMcdTriangleUseSmallestPenetration );
#if USE_EDGES
				tri->flags = (McdTriangleFlags)(tri->flags | kMcdTriangleUseEdge0 | kMcdTriangleUseEdge1 | kMcdTriangleUseEdge2);
#endif

				// default tri list data.
				KarmaTriUserData* tUData = &triData->tri_list_udata[triData->num_tri];
				tUData->localFriction = 1;
				tUData->localRestitution = 1;
				tUData->localMaterial = nearActor->StaticMesh->GetSkin(nearActor, meshTri->MaterialIndex);
				tri->triangleData.ptr = tUData;

				(triData->num_tri) += 1;
				(triData->num_vert) += 3;
				(triData->num_norm) += 3;
			}
		}
	}

	Mark.Pop();

	unguard;

    /* debug drawing */
    if(KGData->DebugDrawOpt & KDRAW_Triangles)
    {
        for(INT t=0; t<triData->num_tri; t++)
        {
            McdUserTriangle *tri = &(triData->triangles[t]);
            
            MeReal coffset = (MeReal)t/triData->num_tri;

			MeVector3 normStart, normEnd;

			// Find point on surface of triangle.
			MeVector3Add(normStart, *(tri->vertices[0]), *(tri->vertices[1]));
			MeVector3Add(normStart, normStart, *(tri->vertices[2]));
			MeVector3Scale(normStart, (MeReal)0.333333);

			MeVector3ScaleAndAdd(normEnd, normStart, 1, *(tri->normal));

            KLineDraw(*(tri->vertices[0]), *(tri->vertices[1]), coffset, 0, 1-coffset);
            KLineDraw(*(tri->vertices[1]), *(tri->vertices[2]), coffset, 0, 1-coffset);
            KLineDraw(*(tri->vertices[2]), *(tri->vertices[0]), coffset, 0, 1-coffset);

			KLineDraw(normStart, normEnd, coffset, 0, 1-coffset);

#if 0
			// Draw line from centre of sphere to each triangle.
			MeVector3 sphereCent;
			KU2MEPosition(sphereCent, *sphere);
			KLineDraw(sphereCent, normStart, 0, 1, 0);
#endif
        }
    }

	unguard;
}


/* Fill in tri_list_memory with 'relevant' triangles */
int MEAPI KTriListGenerator(McdModelPair* modelTriListPair,
							McdUserTriangle *triangles,
							MeVector3 pos, 
							MeReal radius,
							int maxTriangles)
{
    guard(KTriListGenerator);

    /* Transform sphere from ME scale to Unreal scale */
    FVector fpos = FVector(0, 0, 0);
    KME2UPosition(&fpos, pos);

    FSphere sphere = FSphere(fpos, K_ME2UScale * radius);

	// Get the userdata from the triangle-list geometry, which is the level in question.
	McdModelID m1, m2;
	McdModelPairGetModels(modelTriListPair, &m1, &m2);
	McdGeometryID geom1 = McdModelGetGeometry(m1);
	McdGeometryID geom2 = McdModelGetGeometry(m2);

	// Find the level, actor and triangle-list geometry involved in this interaction.
	McdTriangleList* tlGeom = 0;
    ULevel* level = 0;
	AActor* actor = 0;
	if(McdGeometryGetTypeId(geom1) == kMcdGeometryTypeTriangleList)
	{
		level = (ULevel*)(McdTriangleListGetUserData(geom1));
		actor = (AActor*)KModelGetActor(m2);
		tlGeom = (McdTriangleList*)geom1;
	}
	else if(McdGeometryGetTypeId(geom2) == kMcdGeometryTypeTriangleList)
	{
		level = (ULevel*)(McdTriangleListGetUserData(geom2));
		actor = (AActor*)KModelGetActor(m1);
		tlGeom = (McdTriangleList*)geom2;
	}
    
	check(level && actor && tlGeom);

	KarmaTriListData* triData = 0;
	UKarmaParams* kp = 0;
	if(actor->KParams) 
		kp = Cast<UKarmaParams>(actor->KParams);

	if(kp && kp->KTriList)
	{
		// Now, if this actor already has an associated triangle list, we use that, and dont do the query.
		triData = (KarmaTriListData*)kp->KTriList;
	}
	else
	{
		// Otherwise, use the temporary triangle list, and do the query now.
		triData = &(KGData->TriListData);

		clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KTriListQuery));
		KTriListQuery(level, &sphere, triData);
		unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KTriListQuery));
	}
   
	tlGeom->list = triData->triangles;

    return triData->num_tri;

    unguard;
}

#endif // WITH_KARMA

