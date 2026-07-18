/*=============================================================================
	GeomFitUtils.cpp: Utilities for fitting collision models to static meshes.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

    Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "GeomFitUtils.h"

#define LOCAL_EPS (0.01f)

#ifdef WITH_KARMA

static void AddVertexIfNotPresent(TArray<FVector> &vertices, FVector &newVertex)
{
	guard(AddVertexIfNotPresent);

	UBOOL isPresent = 0;

	for(INT i=0; i<vertices.Num() && !isPresent; i++)
	{
		FLOAT diffSqr = (newVertex - vertices(i)).SizeSquared();
		if(diffSqr < LOCAL_EPS * LOCAL_EPS)
			isPresent = 1;
	}

	if(!isPresent)
		vertices.AddItem(newVertex);

	unguard;
}

// Utility to update the mass properties.
static void MakeConvexHull(TArray<FVector> &HullVerts, TArray<FStaticMeshVertex> &MeshVerts)
{
	guard(MakeConvexHull);

	// Make 'dummy' framework.
	McdFrameworkID frame = McdInit(0, 1, 1, 1);
	McdConvexMeshRegisterType(frame);
	McdConvexMeshPrimitivesRegisterInteractions(frame);
	McdConvexMeshConvexMeshRegisterInteraction(frame);

	// Copy graphics verts into Karma type array
	MeVector3* meVerts = (MeVector3*)appAlloca( MeshVerts.Num() * sizeof(MeVector3) );
	for(INT i=0; i<MeshVerts.Num(); i++)
		KU2MEVecCopy(meVerts[i], MeshVerts(i).Position);

	McdConvexMeshID conv = McdConvexMeshCreateHull(frame, meVerts, MeshVerts.Num(), 0);
	for(INT i=0; i<McdConvexMeshGetPolygonCount(conv); i++)
	{
		for(INT j=0; j<McdConvexMeshGetPolygonVertexCount(conv, i); j++)
		{
			const MeReal* v = McdConvexMeshGetPolygonVertexPtr(conv, i, j);
			AddVertexIfNotPresent( HullVerts, FVector(v[0], v[1], v[2]) );
		}
	}

	McdTerm(frame);

	unguard;
}

/* **************************** JACOBI 3x3 SOLVE ************************ */
// Nicked from Karma :)

// This is a helper function called only by MeMatrix3JacobiSolve.
static void sym_schur2(MeMatrix3 a, int p, int q, MeReal *s, MeReal *c)
{
    *c = 1;
    *s = 0;
    if (a[p][q] != 0.0) 
    {
        MeReal     tau = (MeReal) 0.5 * (a[q][q] - a[p][p]) / a[p][q];
        MeReal     t = 0;
        if (tau > 0) 
            t = (MeReal) 1 / (tau + MeSqrt(1 + tau * tau));
        else 
            t = (MeReal) -1 / (-tau + MeSqrt(1 + tau * tau));

        *c = (MeReal) 1 / MeSqrt(1 + t * t);
        *s = *c * t;
    }
}

// This is a helper function called only by MeMatrix3JacobiSolve.
// update a matrix by the jacobi transform on the right.
// Here, assume that i>j and that we are working _only_ on the elements
// below the diagonal.
// function to update a matrix by multiplying on the right with a Jacobi
// transformation matrix.
static void jacobi_update_cols(MeMatrix3 a, int i, int j, MeReal c, MeReal s)
{
    MeReal     coli[3];
    MeReal     colj[3];
    coli[0] = c * a[0][i] - s * a[0][j];
    coli[1] = c * a[1][i] - s * a[1][j];
    coli[2] = c * a[2][i] - s * a[2][j];
    colj[0] = s * a[0][i] + c * a[0][j];
    colj[1] = s * a[1][i] + c * a[1][j];
    colj[2] = s * a[2][i] + c * a[2][j];
    a[0][i] = coli[0];
    a[1][i] = coli[1];
    a[2][i] = coli[2];
    a[0][j] = colj[0];
    a[1][j] = colj[1];
    a[2][j] = colj[2];
}

// This is a helper function called only by MeMatrix3JacobiSolve.
// same as above but multiplying with the transpose of the Jacobi transform
// from the left
static void jacobi_update_rows(MeMatrix3 a, int i, int j, MeReal c, MeReal s)
{
    MeReal     rowi[3];
    MeReal     rowj[3];
    rowi[0] = c * a[i][0] - s * a[j][0];
    rowi[1] = c * a[i][1] - s * a[j][1];
    rowi[2] = c * a[i][2] - s * a[j][2];
    rowj[0] = s * a[i][0] + c * a[j][0];
    rowj[1] = s * a[i][1] + c * a[j][1];
    rowj[2] = s * a[i][2] + c * a[j][2];
    a[i][0] = rowi[0];
    a[i][1] = rowi[1];
    a[i][2] = rowi[2];
    a[j][0] = rowj[0];
    a[j][1] = rowj[1];
    a[j][2] = rowj[2];
}

// This is a helper macro called only by MeMatrix3JacobiSolve.
// Macro to estimate the size of the diagonal elements of a 3x3 matrix, 
// only considering the lower diagonal ones.
#define OFFDIAG(a) \
(a[1][0]*a[1][0] + a[2][0]*a[2][0] + a[2][1]*a[2][1])

/**
 *  This is the Cyclic-by-Row Jacobi linear transformation diagonalization
 *  based on the method described by Golub & van Loan implemented by 
 *  Claude Lacoursiere and ported by Scott Burlington for MathEngine
 *
 *  Get the eigenvalues and the rotation matrix for a positive definite 3x3
 *  matrix.  A is overwriten with the rotation matrix and i is the vector of
 *  eigenvalues.  We do this using the Jacobi iteration method which should
 *  be quite fast for 3x3 matrices.  We follow the 'cyclic by row' algorithm
 *  from Golub and Van Loan which is optimized a bit here.  The algorithm
 *  relies on the symmetric Schur routine which computes the sine and cosine
 *  of the transformation we want.
 *
 *  Replaces matrix A with approximate diagonal and also return rotation
 *  matrix R. 
 *
 *  Typically maxiter=10 and tolerance=ME_SMALL_EPSILON.
 *  Returns number of unused iterations, or 0 on failure.
 */
int MyMatrix3JacobiSolve(MeMatrix3 a, MeMatrix3 r, int maxiter, MeReal tolerance)
{
	guard(MyMatrix3JacobiSolve);

    int i, j, k;
    MeReal c, s;

#ifdef _MECHECK
    c = MeFabs(a[1][0]-a[0][1]) + MeFabs(a[2][0]-a[0][2]) + MeFabs(a[2][1]-a[1][2]);
    s = MeFabs(a[0][0]) + MeFabs(a[1][1]) + MeFabs(a[2][2]);
    if (c > s*0.0001f)
        MeWarning(2,"MeMatrix3JacobiSolve - the matrix does not appear to be symetric.");
#endif

    MeMatrix3MakeIdentity(r);
    tolerance *= tolerance;

    for (k = maxiter; OFFDIAG(a) > tolerance && k; --k) 
        for (i = 1; i < 3; ++i) 
            for (j = 0; j < i; ++j) 
            {
                // get the sine and cosine for transform
                // NB this is parallelizable -- see Golub & van Loan
                sym_schur2(a, i, j, &s, &c);
                jacobi_update_cols(r, i, j, c, s);
                jacobi_update_cols(a, i, j, c, s);
                jacobi_update_rows(a, i, j, c, s);
            }

    return k;

	unguard;
}
#endif

/* ******************************** KDOP ******************************** */

// This function takes the current collision model, and fits a k-DOP around it.
// It uses the array of k unit-length direction vectors to define the k bounding planes.

// THIS FUNCTION REPLACES EXISTING KARMA AND COLLISION MODEL WITH KDOP
#define MY_FLTMAX (3.402823466e+38F)

void GenerateKDopAsCollisionModel(TArray<FVector> &dirs)
{
	guard(GenerateKDopAsCollisionModel);

	if( !GUnrealEd->CurrentStaticMesh )
	{
		appMsgf( 0, TEXT("Select a static mesh first.") );
		return;
	}

	UStaticMesh* StaticMesh = GUnrealEd->CurrentStaticMesh;

	// If we already have a collision model for this staticmesh, ask if we want to replace it.
	if(StaticMesh->CollisionModel)
	{
		UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has a collision model. \nDo you want to replace it?"));
		if(doReplace)
			StaticMesh->CollisionModel = NULL;
		else
			return;
	}

	UKMeshProps* mp = StaticMesh->KPhysicsProps;
	if(mp)
	{
		// If we already have some karma collision for this mesh, check user want to replace it with sphere.
		int totalGeoms = 1 + mp->AggGeom.GetElementCount();
		if(totalGeoms > 0)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has Karma collision geoemtry. \n")
				TEXT("Are you sure you want replace it with a K-DOP?"));

			if(doReplace)
				mp->AggGeom.EmptyElements();
			else
				return;
		}
	}
	else
	{
		// Otherwise, create one here.
		StaticMesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), StaticMesh);
		mp = StaticMesh->KPhysicsProps;
	}

	// Do k- specific stuff.
	INT kCount = dirs.Num();
	TArray<FLOAT> maxDist;
	for(INT i=0; i<kCount; i++)
		maxDist.AddItem(-MY_FLTMAX);

	StaticMesh->CollisionModel = new(StaticMesh->GetOuter()) UModel(NULL,1);

	// For each vertex, project along each kdop direction, to find the max in that direction.
	TArray<FStaticMeshVertex>* verts = &StaticMesh->VertexStream.Vertices;
	for(INT i=0; i<verts->Num(); i++)
	{
		for(INT j=0; j<kCount; j++)
		{
			FLOAT dist = (*verts)(i).Position | dirs(j);
			maxDist(j) = Max(dist, maxDist(j));
		}
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane> planes;
	for(INT i=0; i<kCount; i++)
		planes.AddItem( FPlane(dirs(i), maxDist(i)) );

	for(INT i=0; i<planes.Num(); i++)
	{
		FPoly*	Polygon = new(StaticMesh->CollisionModel->Polys->Element) FPoly();
		FVector Base, AxisX, AxisY;

		Polygon->Init();
		Polygon->Normal = planes(i);
		Polygon->NumVertices = 4;
		Polygon->Normal.FindBestAxisVectors(AxisX,AxisY);

		Base = planes(i) * planes(i).W;

		Polygon->Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon->Vertex[1] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon->Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon->Vertex[3] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;

		for(INT j=0; j<planes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon->Split(-FVector(planes(j)), planes(j) * planes(j).W))
				{
					Polygon->NumVertices = 0;
					break;
				}
			}
		}

		if(Polygon->NumVertices < 3)
		{
			// If poly resulted in no verts, remove from array
			StaticMesh->CollisionModel->Polys->Element.Remove(StaticMesh->CollisionModel->Polys->Element.Num()-1);
		}
		else
		{
			// Other stuff...
			Polygon->iLink = i;
			Polygon->CalcNormal(1);
		}
	}

	if(StaticMesh->CollisionModel->Polys->Element.Num() < 4)
	{
		StaticMesh->CollisionModel = NULL;
		return;
	}

	// Build bounding box.
	StaticMesh->CollisionModel->BuildBound();

	// Build BSP for the brush.
	GEditor->bspBuild(StaticMesh->CollisionModel,BSP_Good,15,70,1,0);
	GEditor->bspRefresh(StaticMesh->CollisionModel,1);
	GEditor->bspBuildBounds(StaticMesh->CollisionModel);

#ifdef WITH_KARMA
	KModelToHulls(&mp->AggGeom, StaticMesh->CollisionModel, FVector(0, 0, 0));
	KUpdateMassProps(mp);
#endif

	// Mark staticmesh as dirty, to help make sure it gets saved.
	UObject* Outer = StaticMesh->GetOuter();
	while( Outer && Outer->GetOuter() )
		Outer = Outer->GetOuter();
	if( Outer && Cast<UPackage>(Outer) )
		Cast<UPackage>(Outer)->bDirty = 1;

	unguard;
}

/* ******************************** OBB ******************************** */

// Automatically calculate the principal axis for fitting a Oriented Bounding Box to a static mesh.
// Then use k-DOP above to calculate it.
void GenerateOBBAsCollisionModel()
{
	guard(GenerateOBBAsCollisionModel);

	// 'Fraid you dont get this without Karma...
#ifdef WITH_KARMA
	if( !GUnrealEd->CurrentStaticMesh )
	{
		appMsgf( 0, TEXT("Select a static mesh first.") );
		return;
	}

	// If we already have a collision model for this staticmesh, ask if we want to replace it.
	UStaticMesh* StaticMesh = GUnrealEd->CurrentStaticMesh;
	if(StaticMesh->CollisionModel)
	{
		UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has a collision model. \nDo you want to replace it?"));
		if(doReplace)
			StaticMesh->CollisionModel = NULL;
		else
			return;
	}

	// Wrap convex hull around mesh, rather than using graphics verts directly.
	// This removes internal verts and duplicate face verts that can mess up results.
	TArray<FStaticMeshVertex>* meshVerts = &StaticMesh->VertexStream.Vertices;
	TArray<FVector> hullVerts;
	MakeConvexHull(hullVerts, *meshVerts);


	// Work out average vertex position.
	FVector Average(0, 0, 0);
	
	for(INT i=0; i<hullVerts.Num(); i++)
	{
		Average += hullVerts(i);
	}
	Average /= (FLOAT)hullVerts.Num();

	// Then  calculate covariance matrix for vertex distribution.
	MeMatrix3 Cov;
	appMemset(Cov, 0, sizeof(MeMatrix3));

	for(INT i=0; i<hullVerts.Num(); i++)
	{
		FVector v = hullVerts(i) - Average;
		
		// Only need to calculate the upper diagonal - its symmetric...
		Cov[0][0] += v.X * v.X;
		Cov[0][1] += v.X * v.Y;
		Cov[0][2] += v.X * v.Z;

		Cov[1][0] += v.X * v.X;
		Cov[1][1] += v.Y * v.Y;
		Cov[1][2] += v.Y * v.Z;

		Cov[2][0] += v.Z * v.X;
		Cov[2][1] += v.Z * v.Y;
		Cov[2][2] += v.Z * v.Z;
	}
	MeMatrix3Scale(Cov, 1.f/hullVerts.Num());
	
	// Now solve to find eigenvectors of distribution matrix.
	// This should give us primary direction for fitting OBB.
	MeMatrix3 R;
	MyMatrix3JacobiSolve(Cov, R, 10, ME_SMALL_EPSILON);
	MeMatrix3Transpose(R);

	// Turn rotation matrix into Unreal transform matrix.
	FMatrix OptRot;
	OptRot.SetIdentity();
	for(INT i=0; i<3; i++)
		for(INT j=0; j<3; j++)
			OptRot.M[i][j] = R[i][j];

	TArray<FVector> dirs;
#if 1
	for(INT i=0; i<6; i++)
		dirs.AddItem(OptRot.TransformNormal(KDopDir6[i]));
#else
	dirs.AddItem( FVector(R[0][1], R[0][2], R[0][0]));
	dirs.AddItem(-FVector(R[0][1], R[0][2], R[0][0]));
	dirs.AddItem( FVector(R[1][1], R[1][2], R[1][0]));
	dirs.AddItem(-FVector(R[1][1], R[1][2], R[1][0]));
	dirs.AddItem( FVector(R[2][1], R[2][2], R[2][0]));
	dirs.AddItem(-FVector(R[2][1], R[2][2], R[2][0]));
#endif

	// Then, use these vectors as directions for k-dop.
	GenerateKDopAsCollisionModel(dirs);

#endif

	unguard;
}

#ifdef WITH_KARMA

/* ******************************** KARMA SPHERE ******************************** */

// Can do bounding circles as well... Set elements of limitVect to 1.f for directions to consider, and 0.f to not consider.
// Have 2 algorithms, seem better in different cirumstances

// This algorithm taken from Ritter, 1990
// This one seems to do well with asymmetric input.
static void CalcBoundingSphere(TArray<FStaticMeshVertex>& verts, FSphere& sphere, FVector& LimitVec)
{
	FBox Box;

	if(verts.Num() == 0)
		return;

	INT minIx[3], maxIx[3]; // Extreme points.

	// First, find AABB, remembering furthest points in each dir.
	Box.Min = verts(0).Position * LimitVec;
	Box.Max = Box.Min;

	minIx[0] = minIx[1] = minIx[2] = 0;
	maxIx[0] = maxIx[1] = maxIx[2] = 0;

	for(INT i=1; i<verts.Num(); i++) 
	{
		FVector p = verts(i).Position * LimitVec;

		// X //
		if(p.X < Box.Min.X)
		{
			Box.Min.X = p.X;
			minIx[0] = i;
		}
		else if(p.X > Box.Max.X)
		{
			Box.Max.X = p.X;
			maxIx[0] = i;
		}

		// Y //
		if(p.Y < Box.Min.Y)
		{
			Box.Min.Y = p.Y;
			minIx[1] = i;
		}
		else if(p.Y > Box.Max.Y)
		{
			Box.Max.Y = p.Y;
			maxIx[1] = i;
		}

		// Z //
		if(p.Z < Box.Min.Z)
		{
			Box.Min.Z = p.Z;
			minIx[2] = i;
		}
		else if(p.Z > Box.Max.Z)
		{
			Box.Max.Z = p.Z;
			maxIx[2] = i;
		}
	}

	//  Now find extreme points furthest apart, and initial centre and radius of sphere.
	FLOAT d2 = 0.f;
	for(INT i=0; i<3; i++)
	{
		FVector diff = (verts(maxIx[i]).Position - verts(minIx[i]).Position) * LimitVec;
		FLOAT tmpd2 = diff.SizeSquared();

		if(tmpd2 > d2)
		{
			d2 = tmpd2;
			FVector centre = verts(minIx[i]).Position + (0.5f * diff);
			centre *= LimitVec;
			sphere.X = centre.X;
			sphere.Y = centre.Y;
			sphere.Z = centre.Z;
			sphere.W = 0.f;
		}
	}

	// radius and radius squared
	FLOAT r = 0.5f * appSqrt(d2);
	FLOAT r2 = r * r;

	// Now check each point lies within this sphere. If not - expand it a bit.
	for(INT i=0; i<verts.Num(); i++) 
	{
		FVector cToP = (verts(i).Position * LimitVec) - sphere;
		FLOAT pr2 = cToP.SizeSquared();

		// If this point is outside our current bounding sphere..
		if(pr2 > r2)
		{
			// ..expand sphere just enough to include this point.
			FLOAT pr = appSqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = r * r;

			sphere += (pr-r)/pr * cToP;
		}
	}

	sphere.W = r;
}

// This is the one thats already used by unreal.
// Seems to do better with more symmetric input...
static void CalcBoundingSphere2(TArray<FStaticMeshVertex>& verts, FSphere& sphere, FVector& LimitVec)
{
	FBox Box(0);
	
	for(INT i=0; i<verts.Num(); i++)
	{
		Box += verts(i).Position * LimitVec;
	}

	FVector centre, extent;
	Box.GetCenterAndExtents(centre, extent);

	sphere.X = centre.X;
	sphere.Y = centre.Y;
	sphere.Z = centre.Z;
	sphere.W = 0;

	for( INT i=0; i<verts.Num(); i++ )
	{
		FLOAT Dist = FDistSquared(verts(i).Position * LimitVec, sphere);
		if( Dist > sphere.W )
			sphere.W = Dist;
	}
	sphere.W = appSqrt(sphere.W);
}

// // //

#endif

// THIS FUNCTION REPLACES EXISTING KARMA WITH SPHERE, BUT DOES NOT CHANGE COLLISION MODEL

void GenerateSphereAsKarmaCollision()
{
	guard(GenerateSphereAsKarmaCollision);

#ifdef WITH_KARMA

	if( !GUnrealEd->CurrentStaticMesh )
	{
		appMsgf( 0, TEXT("Select a static mesh first.") );
		return;
	}

	UStaticMesh* StaticMesh = GUnrealEd->CurrentStaticMesh;
	UKMeshProps* mp = StaticMesh->KPhysicsProps;

	if(mp)
	{
		// If we already have some karma collision for this mesh, check user want to replace it with sphere.
		int totalGeoms = 1 + mp->AggGeom.GetElementCount();
		if(totalGeoms > 0)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has Karma collision geoemtry. \n")
				TEXT("Are you sure you want replace it with a sphere?"));

			if(doReplace)
				mp->AggGeom.EmptyElements();
			else
				return;
		}
	}
	else
	{
		// Otherwise, create one here.
		StaticMesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), StaticMesh);
		mp = StaticMesh->KPhysicsProps;
	}

	// Calculate bounding sphere.
	TArray<FStaticMeshVertex>* verts = &StaticMesh->VertexStream.Vertices;

	FSphere bSphere, bSphere2, bestSphere;
	CalcBoundingSphere(*verts, bSphere, FVector(1,1,1));
	CalcBoundingSphere2(*verts, bSphere2, FVector(1,1,1));

	if(bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;

	// Dont use if radius is zero.
	if(bestSphere.W <= 0.f)
	{
		appMsgf(0, TEXT("Could not create geometry."));
		return;
	}

	int ex = mp->AggGeom.SphereElems.AddZeroed();
	FKSphereElem* s = &mp->AggGeom.SphereElems(ex);
	s->TM = FMatrix::Identity;
	s->TM.M[3][0] = bestSphere.X * K_U2MEScale;
	s->TM.M[3][1] = bestSphere.Y * K_U2MEScale;
	s->TM.M[3][2] = bestSphere.Z * K_U2MEScale;
	s->Radius = bestSphere.W * K_U2MEScale;

	// Update inertia/com given new sphere geometry.
	KUpdateMassProps(StaticMesh->KPhysicsProps);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	UObject* Outer = StaticMesh->GetOuter();
	while( Outer && Outer->GetOuter() )
		Outer = Outer->GetOuter();
	if( Outer && Cast<UPackage>(Outer) )
		Cast<UPackage>(Outer)->bDirty = 1;

#endif // WITH_KARMA


	unguard;
}



/* ******************************** KARMA CYLINDER ******************************** */

 // X = 0, Y = 1, Z = 2
// THIS FUNCTION REPLACES EXISTING KARMA WITH CYLINDER, BUT DOES NOT CHANGE COLLISION MODEL

void GenerateCylinderAsKarmaCollision(INT dir)
{
	guard(GenerateCylinderAsKarmaCollision);

#ifdef WITH_KARMA

	check(dir >=0 && dir < 3);

	if( !GUnrealEd->CurrentStaticMesh )
	{
		appMsgf( 0, TEXT("Select a static mesh first.") );
		return;
	}

	UStaticMesh* StaticMesh = GUnrealEd->CurrentStaticMesh;
	UKMeshProps* mp = StaticMesh->KPhysicsProps;

	if(mp)
	{
		// If we already have some karma collision for this mesh, check user want to replace it with sphere.
		int totalGeoms = 1 + mp->AggGeom.GetElementCount();
		if(totalGeoms > 0)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has Karma collision geoemtry. \n")
				TEXT("Are you sure you want replace it with a cylinder?"));

			if(doReplace)
				mp->AggGeom.EmptyElements();
			else
				return;
		}
	}
	else
	{
		// Otherwise, create one here.
		StaticMesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), StaticMesh);
		mp = StaticMesh->KPhysicsProps;
	}

	// Calculate bounding circle in direction specifyed.
	TArray<FStaticMeshVertex>* verts = &StaticMesh->VertexStream.Vertices;

	FVector LimitVec = FVector(1, 1, 1);
	if(dir == 0)
		LimitVec.X = 0.f;
	else if(dir == 1)
		LimitVec.Y = 0.f;
	else if(dir == 2)
		LimitVec.Z = 0.f;

	FSphere bSphere, bSphere2, bestSphere;
	CalcBoundingSphere(*verts, bSphere, LimitVec);
	CalcBoundingSphere2(*verts, bSphere2, LimitVec);

	if(bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;


	// Dont use if radius is zero.
	if(bestSphere.W <= 0.f)
	{
		appMsgf(0, TEXT("Could not create geometry."));
		return;
	}

	// Use bounding box to figure out height.
	FBox box(0);
	for(INT i=0; i<verts->Num(); i++)
	{
		box += (*verts)(i).Position;
	}
	FVector centre, extent;
	box.GetCenterAndExtents(centre, extent);

	// Create new cylinder geometry
	int ex = mp->AggGeom.CylinderElems.AddZeroed();
	FKCylinderElem* c = &mp->AggGeom.CylinderElems(ex);
	c->TM = FMatrix::Identity;
	
	// Set up matrix so to transform Z (karma cyl axis) to desired direction.
	if(dir == 0)
	{
		c->TM.M[0][0] = c->TM.M[2][2] = 0.f;		
		c->TM.M[0][2] = 1.f;
		c->TM.M[2][0] = -1.f;

		c->TM.M[3][0] = centre.X * K_U2MEScale;
		c->TM.M[3][1] = bestSphere.Y * K_U2MEScale;
		c->TM.M[3][2] = bestSphere.Z * K_U2MEScale;

		c->Height = 2 * extent.X * K_U2MEScale;
	}
	else if(dir == 1)
	{
		c->TM.M[1][1] = c->TM.M[2][2] = 0.f;
		c->TM.M[1][2] = 1.f;
		c->TM.M[2][1] = -1.f;

		c->TM.M[3][0] = bestSphere.X * K_U2MEScale;
		c->TM.M[3][1] = centre.Y * K_U2MEScale;
		c->TM.M[3][2] = bestSphere.Z * K_U2MEScale;

		c->Height = 2 * extent.Y * K_U2MEScale;
	}
	else if(dir == 2)
	{
		// transform ok!

		c->TM.M[3][0] = bestSphere.X * K_U2MEScale;
		c->TM.M[3][1] = bestSphere.Y * K_U2MEScale;
		c->TM.M[3][2] = centre.Z * K_U2MEScale;

		c->Height = 2 * extent.Z * K_U2MEScale;
	}

	c->Radius = bestSphere.W * K_U2MEScale;

	// Update inertia/com given new sphere geometry.
	KUpdateMassProps(StaticMesh->KPhysicsProps);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	UObject* Outer = StaticMesh->GetOuter();
	while( Outer && Outer->GetOuter() )
		Outer = Outer->GetOuter();
	if( Outer && Cast<UPackage>(Outer) )
		Cast<UPackage>(Outer)->bDirty = 1;


#endif // WITH_KARMA

	unguard;
}
