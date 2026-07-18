#include "UnrealEd.h"
#include "KarmaEditorSupport.h"

#ifdef WITH_KARMA

#pragma pack (push,8)
#include "MeASELoad.h"
#pragma pack (pop)

/* --------------- ASE2ME2 ------------------- */

static Mesh2GeometryType Kname2Type(const char* name)
{
    guard(Kname2Type);
    Mesh2GeometryType type = kMesh2GeometryUnknown;
    char tmpName[256];
    
    /* Copy and push to upper case for case-insensitive match. */
    strncpy(tmpName, name, 256);
    strupr(tmpName);

    if(strncmp(tmpName, "MCDSP", 5) == 0)
        type = kMesh2GeometrySphere;
    else if(strncmp(tmpName, "MCDBX", 5) == 0)
        type = kMesh2GeometryBox;
    else if(strncmp(tmpName, "MCDCY", 5) == 0)
        type = kMesh2GeometryCylinder;
    else if(strncmp(tmpName, "MCDCX", 5) == 0)
        type = kMesh2GeometryConvex;

    return type;
    unguard;
}

static void Ktype2Name(Mesh2GeometryType type, char name[256])
{
    guard(Kname2Type);
    if(type == kMesh2GeometrySphere)
        strncpy(name, "McdSphere", 256);
    else if(type == kMesh2GeometryBox)
        strncpy(name, "McdBox", 256);
    else if(type == kMesh2GeometryCylinder)
        strncpy(name, "McdCylinder", 256);
    else if(type == kMesh2GeometryConvex)
        strncpy(name, "McdConvexHull", 256);
    else if(type == kMesh2GeometryUnknown)
        strncpy(name, "Unknown", 256);
    unguard;
}

// Convert from Karma geometry load format and Unreal geometry format.
static void MeFGeom2FKAggGeom(FKAggregateGeom* uGeom, MeFGeometry* meGeom)
{
	guard(MeFGeom2FKAggGeom);

	// Iterate over each primitive, converting it.
	MeFPrimitiveIt it;
	MeFGeometryInitPrimitiveIterator(meGeom, &it);
	
	MeFPrimitive *prim;
	while (prim = MeFGeometryGetPrimitive(&it))
	{
		MeFPrimitiveType primType = MeFPrimitiveGetType(prim);
		
		if (primType == kMeFPrimitiveTypeBox)
		{
			MeVector3 dims;
			MeFPrimitiveGetDimensions(prim, dims);
			
			int ex = uGeom->BoxElems.AddZeroed();
			FKBoxElem* b = &uGeom->BoxElems(ex);
			b->X = dims[0];
			b->Y = dims[1];
			b->Z = dims[2];
			KME2UMatrixCopy(&b->TM, MeFPrimitiveGetTransformPtr(prim));
		} 
		else if (primType == kMeFPrimitiveTypeSphere)
		{
			int ex = uGeom->SphereElems.AddZeroed();
			FKSphereElem* s = &uGeom->SphereElems(ex);
			s->Radius = MeFPrimitiveGetRadius(prim);
			KME2UMatrixCopy(&s->TM, MeFPrimitiveGetTransformPtr(prim));
		}
		else if (primType == kMeFPrimitiveTypeCylinder)
		{
			int ex = uGeom->CylinderElems.AddZeroed();
			FKCylinderElem* c = &uGeom->CylinderElems(ex);
			c->Radius = MeFPrimitiveGetRadius(prim);
			c->Height = MeFPrimitiveGetHeight(prim);
			KME2UMatrixCopy(&c->TM, MeFPrimitiveGetTransformPtr(prim));
		}
		else if (primType == kMeFPrimitiveTypeConvex)
		{
			// We ignore importing convex here. We would rather convert from collision-model BSP
			// later, because that gives us multiple convex hulls, rather than just one.
			// JTODO: Make this whole process tidier...
#if 0
			int ex = uGeom->ConvexElems.AddZeroed();
			FKConvexElem* c = &uGeom->ConvexElems(ex);

			MeFVertexIt it;
			int i, nVerts = MeFPrimitiveGetVertexCount(prim);
			
			// Iterate over each vertex copying it.
			MeFPrimitiveInitVertexIterator(prim, &it);
			MeReal* cvert = MeFPrimitiveGetVertex(&it);
			for (i = 0; i < nVerts; i++)
			{
				c->VertexData.AddItem(FVector(cvert[0], cvert[1], cvert[2]));
				cvert = MeFPrimitiveGetVertex(&it);
			}

			KME2UMatrixCopy(&c->TM, MeFPrimitiveGetTransformPtr(prim));
#endif
		}
	}

	unguard;
}

void KAse2me2(char* infilename, UStaticMesh* smesh)
{
    guard(KAse2me2);
    char finalmessage[16384]; // this is quite remarkably unpleasant
    int charsleft = 16383;
    char message[512];        // this is slightly less so
    MeBool bHideMessage = 0;
    MeASEObject *aseObject = 0, *tmpAse = 0;
    MyMesh mesh;
    int i, maxFaces, maxGeom;
    MeBool problem = 0;

    MeFGeometry* aggGeometry = 0;

    MeReal scale = K_U2MEScale; // We want final geometry in Karma scale.
    MeBool bFlipX = 1;

    mesh.faceVertex = 0;

    finalmessage[0] = message[0] = 0;

    _snprintf(message, 512, "KARMA: Starting ASE2ME2...\n\n");
    strncat(finalmessage, message, (charsleft-=strlen(message)));    

    aseObject = MeASEObjectLoadParts(infilename, scale, scale, scale, 1);

    if(!aseObject)
    {
        _snprintf(message, 512, "ERROR: Issues opening/loading ASE file: %s.\n", infilename);
        strncat(finalmessage, message, (charsleft-=strlen(message)));
        goto cleanup2;        
    }
      
    // problem is set if non-fatal error occurs
    problem = 0;

    // Work out how big our temporary mesh struct has to be and allocate.
    tmpAse = aseObject;
    maxFaces = 0;
    maxGeom = 0;
    while(tmpAse)
    {
        maxFaces = MeMAX(maxFaces, tmpAse->numFaces);
        maxGeom++;
        tmpAse = tmpAse->nextObject;
    }
    mesh.faceVertex = (int(*)[3])MeMemoryALLOCA(maxFaces * 3 * sizeof(int));  

    // Create aggregate geometry to add each piece to.
    aggGeometry = MeFGeometryCreate("TempGeom");

    // While there are bits of geometry to convert.
    while(aseObject)
    {
        Mesh2GeometryType type;
        MeMatrix4 relTM;
        MeFPrimitive* partGeometry = 0;
        
        /* Convert ASE into generic mesh format for functions. */
        mesh.numFaces = aseObject->numFaces;
        mesh.numVerts = aseObject->numVerts;
        mesh.verts = aseObject->verts;

        if( bFlipX )
            for(i=0; i<mesh.numVerts; i++)
                (*(mesh.verts+i))[0] *= -1;
        

        MEASSERT(mesh.numFaces <= maxFaces);
        
        for(i=0; i<mesh.numFaces; i++)
        {
            mesh.faceVertex[i][0] = aseObject->faces[i].vertexId[0];
            mesh.faceVertex[i][1] = aseObject->faces[i].vertexId[1];
            mesh.faceVertex[i][2] = aseObject->faces[i].vertexId[2];        
        }
        
        // See if the name hints at what kind of shape it is...
        type = Kname2Type(aseObject->name);
        
        // If the name is not a geometry type, move on..
        if(type != kMesh2GeometryUnknown)
        {
            char typeName[256];

            Ktype2Name(type, typeName);

            if(type == kMesh2GeometrySphere)
                partGeometry = MeFSphereCreateFromMesh(aseObject->name, &mesh, relTM);
            else if(type == kMesh2GeometryBox)
                partGeometry = MeFBoxCreateFromMesh(aseObject->name, &mesh, relTM);
            else if(type == kMesh2GeometryCylinder)    
                partGeometry = MeFCylinderCreateFromMesh(aseObject->name, &mesh, relTM);
            else if(type == kMesh2GeometryConvex)
                partGeometry = MeFConvexCreateFromMesh(aseObject->name, &mesh, relTM);

            
            if(!partGeometry)
            {
                _snprintf(message, 512, "ERROR: Converting Part: %s (Geometry Type: %s).\n", aseObject->name, typeName); 
                strncat(finalmessage, message, (charsleft-=strlen(message)));                
                problem = 1;
            }
            else
            {
                _snprintf(message, 512, "Adding Part: %s (Geometry Type: %s).\n", aseObject->name, typeName);
                strncat(finalmessage, message, (charsleft-=strlen(message)));

                // Add this geometry to the aggregate.
				MeFPrimitiveSetTransform(partGeometry, relTM);
                MeFGeometryInsertPrimitive(aggGeometry, partGeometry);
            }
        }
        
        aseObject = aseObject->nextObject;
    }


    // Only bother saving if we actually added some geometry to aggregate.
    guard(SaveAggregate);
    if(MeFGeometryGetPrimitiveCount(aggGeometry) > 0)
    {
		UBOOL doImport = appMsgf(1, TEXT("Karma Collision Data Found. \nDo you want to add to StaticMesh?"));

        if(!doImport)
        {
            bHideMessage = 1;
            goto cleanup2;
        }

		// Construct Karma physics properties (collision & mass props) struct.
		if(!smesh->KPhysicsProps)
			smesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), smesh);

		// Convert from Karma to Unreal geoemtry format (for later serialization).
		MeFGeom2FKAggGeom(&smesh->KPhysicsProps->AggGeom, aggGeometry);

		// Update mass properties (com/inertia tensor) based on collision geometry.
		KUpdateMassProps(smesh->KPhysicsProps);

        // Final message
        _snprintf(message, 512, "\n" );
        strncat(finalmessage, message, (charsleft-=strlen(message)));
        if(problem)
        {
            _snprintf(message, 512, "PARTIAL "); //:)
            strncat(finalmessage, message, (charsleft-=strlen(message)));
        }            
        _snprintf(message, 512, "SUCCESS! Imported %d geometries.\n", MeFGeometryGetPrimitiveCount(aggGeometry)); 
        strncat(finalmessage, message, (charsleft-=strlen(message)));
    }
    else
    {
        bHideMessage = 1; // if nothing is there, don't show anything...
        goto cleanup2;
    }
    unguard;

    // cleanup
cleanup2:
    if(aseObject)
    {
        MeASEObjectDestroy(aseObject);
        aseObject = 0;
    }
    
    if(aggGeometry)
    {
        MeFGeometryDestroy(aggGeometry); /* This destroys all child FGeometry*s */
        aggGeometry = 0;
    }
    
    if(mesh.faceVertex)
    {
        MeMemoryFREEA(mesh.faceVertex);
        mesh.faceVertex = 0;
    }

    if( !bHideMessage )
    {
        TCHAR tmp[16384];
        _snwprintf(tmp, 16384, TEXT("%hs"), finalmessage);        
        appMsgf( 0, tmp );
    }

    unguard;
}

static FMatrix RefMeshToWorld(USkeletalMesh* smesh)
{
	FMatrix NewMatrix = FRotationMatrix( smesh->RotOrigin );

	FVector XAxis( NewMatrix.M[0][0], NewMatrix.M[1][0], NewMatrix.M[2][0] );
	FVector YAxis( NewMatrix.M[0][1], NewMatrix.M[1][1], NewMatrix.M[2][1] );
	FVector ZAxis( NewMatrix.M[0][2], NewMatrix.M[1][2], NewMatrix.M[2][2] );

	NewMatrix.M[3][0] += - smesh->Origin | XAxis;
	NewMatrix.M[3][1] += - smesh->Origin | YAxis;
	NewMatrix.M[3][2] += - smesh->Origin | ZAxis;

	return NewMatrix;
}

// Take any collision data in the skeletal mesh, and make a UKMeshProps out of it.
void KUpdateSkeletalPhysicsProps(USkeletalMesh* smesh)
{
	guard(KUpdateSkeletalPhysicsProps);

	INT PrimCount = 0;
	for( INT i=0; i<smesh->BoneCollisionBoxes.Num(); i++)
	{
		if( smesh->BoneCollisionBoxes(i).bBlockKarma )
			PrimCount++;
	}

	for( INT i=0; i<smesh->BoneCollisionSpheres.Num(); i++)
	{
		if( smesh->BoneCollisionSpheres(i).bBlockKarma )
			PrimCount++;
	}

	// If no primitives, or skeletal mesh has non-uniform scaling, remove existing karma collision.
	if(PrimCount == 0 || !smesh->Scale.IsUniform())
	{
		if(smesh->KPhysicsProps)
		{
			delete smesh->KPhysicsProps;
			smesh->KPhysicsProps = NULL;
		}

		return;
	}

	// Get UKMeshProps. If there isn't one, create one now
	UKMeshProps* mp = smesh->KPhysicsProps;
	if(!mp)
	{
		smesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), smesh);
		mp = smesh->KPhysicsProps;
	}

	// Calculate mesh-space bone transforms in the default pose.
	TArray<FCoords> RefBases;
	RefBases.Add( smesh->RefSkeleton.Num() );

	for( INT b=0; b<smesh->RefSkeleton.Num(); b++)
	{
		// Render the default pose.
		FQuatToFCoordsFast( smesh->RefSkeleton(b).BonePos.Orientation, smesh->RefSkeleton(b).BonePos.Position, RefBases(b));

		// Construct mesh-space skeletal hierarchy.
		if( b>0 )
		{
			INT Parent = smesh->RefSkeleton(b).ParentIndex;
			RefBases(b) = RefBases(b).ApplyPivot(RefBases(Parent));
		} 
	}	

	// Empty existing aggregate geometry
	mp->AggGeom.BoxElems.Empty();
	mp->AggGeom.ConvexElems.Empty();
	mp->AggGeom.CylinderElems.Empty();
	mp->AggGeom.SphereElems.Empty();

	// // SPHERES // //
	for( INT i=0; i<smesh->BoneCollisionSpheres.Num(); i++)
	{
		FSkelBoneSphere* s = &smesh->BoneCollisionSpheres(i);

		if(!s->bBlockKarma)
			continue;

		int ex = mp->AggGeom.SphereElems.AddZeroed();
		FKSphereElem* se = &mp->AggGeom.SphereElems(ex);

		se->Radius = K_U2MEScale * s->Radius * smesh->Scale.X;
		
		// Get bone->mesh transform
		INT BoneIndex = smesh->MatchRefBone(s->BoneName);
		check(BoneIndex != INDEX_NONE); // Any incorrect bone names should be removed in SaveMeshProperties

		// Combine offset from bone, skeletal mesh rotation/translation offset and bone->mesh transform
		// RefBases do not include smesh->Scale
		FMatrix MeshToWorldMatrix = RefMeshToWorld(smesh);

		FMatrix BoneMatrix = RefBases(BoneIndex).Matrix() * MeshToWorldMatrix; // Bone -> World TM
		FMatrix BoneMatrix2 = RefBases(BoneIndex).Matrix().Inverse() * MeshToWorldMatrix;

		BoneMatrix.M[3][0] = BoneMatrix2.M[3][0];
		BoneMatrix.M[3][1] = BoneMatrix2.M[3][1];
		BoneMatrix.M[3][2] = BoneMatrix2.M[3][2];

		FVector newOrigin = BoneMatrix.TransformFVector(s->Offset);

		BoneMatrix.M[3][0] = newOrigin.X * K_U2MEScale * smesh->Scale.X;
		BoneMatrix.M[3][1] = newOrigin.Y * K_U2MEScale * smesh->Scale.X;
		BoneMatrix.M[3][2] = newOrigin.Z * K_U2MEScale * smesh->Scale.X;

		se->TM = BoneMatrix;
	}

	// // BOXES // //
	for( INT i=0; i<smesh->BoneCollisionBoxes.Num(); i++)
	{
		FSkelBoneBox* b = &smesh->BoneCollisionBoxes(i);

		if(!b->bBlockKarma)
			continue;

		int ex = mp->AggGeom.BoxElems.AddZeroed();
		FKBoxElem* be = &mp->AggGeom.BoxElems(ex);

		be->X = 2.0f * K_U2MEScale * b->Radii.X * smesh->Scale.X; // Not a bug! Scale.X == Y == Z
		be->Y = 2.0f * K_U2MEScale * b->Radii.Y * smesh->Scale.X;
		be->Z = 2.0f * K_U2MEScale * b->Radii.Z * smesh->Scale.X;
		
		INT BoneIndex = smesh->MatchRefBone(b->BoneName);
		check(BoneIndex != INDEX_NONE);

		FMatrix MeshToWorldMatrix = RefMeshToWorld(smesh);

		FMatrix BoneMatrix = RefBases(BoneIndex).Matrix() * MeshToWorldMatrix; // Bone -> World TM
		FMatrix BoneMatrix2 = RefBases(BoneIndex).Matrix().Inverse() * MeshToWorldMatrix;

		BoneMatrix.M[3][0] = BoneMatrix2.M[3][0];
		BoneMatrix.M[3][1] = BoneMatrix2.M[3][1];
		BoneMatrix.M[3][2] = BoneMatrix2.M[3][2];

		FVector newOrigin = BoneMatrix.TransformFVector(b->Offset);

		BoneMatrix.M[3][0] = newOrigin.X * K_U2MEScale * smesh->Scale.X;
		BoneMatrix.M[3][1] = newOrigin.Y * K_U2MEScale * smesh->Scale.X;
		BoneMatrix.M[3][2] = newOrigin.Z * K_U2MEScale * smesh->Scale.X;

		be->TM = BoneMatrix;
	}

	// Finally, update the mass properties (MOI & COM)
	KUpdateMassProps(mp);

	unguard;
}
	
#endif // WITH_KARMA
