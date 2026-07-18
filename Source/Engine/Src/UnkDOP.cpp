/*=============================================================================
	UnkDOP.cpp: k-DOP collision
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Graf
=============================================================================*/

#include "EnginePrivate.h"

#include "UnkDOP.h"


// These are the plane normals for the kDOP that we use (bounding box)
const FVector PlaneNormals[NUM_PLANES] =
{
	FVector(1.f,0.f,0.f),
	FVector(0.f,1.f,0.f),
	FVector(0.f,0.f,1.f)
};

//
//	Andrew's UE3 Separating axis code.
//

static inline void CheckMinIntersectTime(FLOAT& MinIntersectTime,FVector& HitNormal,FLOAT Time,const FVector& Normal)
{
	if(Time > MinIntersectTime)
	{
		MinIntersectTime = Time;
		HitNormal = Normal;
	}
}

static inline UBOOL TestSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Line,
	FLOAT ProjectedStart,
	FLOAT ProjectedEnd,
	FLOAT ProjectedExtent,
	FLOAT& MinIntersectTime,
	FLOAT& MaxIntersectTime,
	FVector& HitNormal
	)
{
	FLOAT	ProjectedDirection = ProjectedEnd - ProjectedStart,
			ProjectedV0 = Line | V0,
			ProjectedV1 = Line | V1,
			ProjectedV2 = Line | V2,
			TriangleMin = Min(ProjectedV0,Min(ProjectedV1,ProjectedV2)) - ProjectedExtent,
			TriangleMax = Max(ProjectedV0,Max(ProjectedV1,ProjectedV2)) + ProjectedExtent;

	if(ProjectedStart < TriangleMin)
	{
		if(ProjectedDirection < DELTA)
			return 0;
		CheckMinIntersectTime(MinIntersectTime,HitNormal,(TriangleMin - ProjectedStart) / ProjectedDirection,-Line);
		MaxIntersectTime = Min(MaxIntersectTime,(TriangleMax - ProjectedStart) / ProjectedDirection);
	}
	else if(ProjectedStart > TriangleMax)
	{
		if(ProjectedDirection > -DELTA)
			return 0;
		CheckMinIntersectTime(MinIntersectTime,HitNormal,(TriangleMax - ProjectedStart) / ProjectedDirection,Line);
		MaxIntersectTime = Min(MaxIntersectTime,(TriangleMin - ProjectedStart) / ProjectedDirection);
	}
	else
	{
		if(ProjectedDirection > DELTA)
			MaxIntersectTime = Min(MaxIntersectTime,(TriangleMax - ProjectedStart) / ProjectedDirection);
		else if(ProjectedDirection < -DELTA)
			MaxIntersectTime = Min(MaxIntersectTime,(TriangleMin - ProjectedStart) / ProjectedDirection);
	}

	return MaxIntersectTime >= MinIntersectTime;
}

static inline UBOOL TestSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Line,
	const FVector& Start,
	const FVector& End,
	FLOAT ProjectedExtent,
	FLOAT& MinIntersectTime,
	FLOAT& MaxIntersectTime,
	FVector& HitNormal
	)
{
	return TestSeparatingAxis(V0,V1,V2,Line,Line | Start,Line | End,ProjectedExtent,MinIntersectTime,MaxIntersectTime,HitNormal);
}

static inline UBOOL TestSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& TriangleEdge,
	const FVector& BoxEdge,
	const FVector& Start,
	const FVector& End,
	const FVector& BoxX,
	const FVector& BoxY,
	const FVector& BoxZ,
	const FVector& BoxExtent,
	FLOAT& MinIntersectTime,
	FLOAT& MaxIntersectTime,
	FVector& HitNormal
	)
{
	FVector	Line = BoxEdge ^ TriangleEdge;

	if(Line.SizeSquared() < DELTA)
		return 1;

	FLOAT	ProjectedExtent = BoxExtent.X * Abs(Line | BoxX) + BoxExtent.Y * Abs(Line | BoxY) + BoxExtent.Z * Abs(Line | BoxZ);
	return TestSeparatingAxis(V0,V1,V2,Line,Line | Start,Line | End,ProjectedExtent,MinIntersectTime,MaxIntersectTime,HitNormal);
}

static INT FindSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Start,
	const FVector& End,
	const FVector& BoxExtent,
	const FVector& BoxX,
	const FVector& BoxY,
	const FVector& BoxZ,
	FLOAT& HitTime,
	FVector& HitNormal
	)
{
	FLOAT	MinIntersectTime = -1.0f,
			MaxIntersectTime = HitTime;

	// Box faces.

	if(!TestSeparatingAxis(V0,V1,V2,BoxX,Start,End,BoxExtent.X * BoxX.SizeSquared(),MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,BoxY,Start,End,BoxExtent.Y * BoxY.SizeSquared(),MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,BoxZ,Start,End,BoxExtent.Z * BoxZ.SizeSquared(),MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	// Triangle normal.

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),(V1 - V0),Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	// Box faces +/- X x triangle edges.

	if(!TestSeparatingAxis(V0,V1,V2,(V1 - V0),BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V0 - V2),BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	// Box faces +/- Y x triangle edges.

	if(!TestSeparatingAxis(V0,V1,V2,(V1 - V0),BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;
	
	if(!TestSeparatingAxis(V0,V1,V2,(V0 - V2),BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;
	
	// Box faces +/- Z x triangle edges.

	if(!TestSeparatingAxis(V0,V1,V2,(V1 - V0),BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;
	
	if(!TestSeparatingAxis(V0,V1,V2,(V0 - V2),BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(MinIntersectTime >= 0.0f)
	{
		HitTime = MinIntersectTime;
		return 1;
	}
	else
		//TEMP Hack
		return 2;
}

//
//	FSeparatingAxisPointCheck - Checks for intersection between an oriented bounding box and a triangle.
//	HitNormal: The normal of the separating axis the bounding box is penetrating the least.
//	BestDist: The amount the bounding box is penetrating the axis defined by HitNormal.
//

struct FSeparatingAxisPointCheck
{
	FVector	HitNormal;
	FLOAT	BestDist;
	UBOOL	Hit;

	const FVector&	V0,
					V1,
					V2;

	UBOOL TestSeparatingAxis( const FVector& Axis, FLOAT ProjectedPoint, FLOAT ProjectedExtent )
	{
		FLOAT	ProjectedV0 = Axis | V0,
				ProjectedV1 = Axis | V1,
				ProjectedV2 = Axis | V2,
				TriangleMin = Min(ProjectedV0,Min(ProjectedV1,ProjectedV2)) - ProjectedExtent,
				TriangleMax = Max(ProjectedV0,Max(ProjectedV1,ProjectedV2)) + ProjectedExtent;

		if(ProjectedPoint >= TriangleMin && ProjectedPoint <= TriangleMax)
		{
			FLOAT	AxisMagnitude = Axis.Size(),
					MinPenetrationDist = ProjectedPoint - TriangleMin,
					MaxPenetrationDist = TriangleMax - ProjectedPoint;
			if(MinPenetrationDist < BestDist * AxisMagnitude)
			{
				check(AxisMagnitude > 0.0f);
				FLOAT	InvAxisMagnitude = 1.0f / AxisMagnitude;
				BestDist = MinPenetrationDist * InvAxisMagnitude;
				HitNormal = -Axis * InvAxisMagnitude;
			}
			if(MaxPenetrationDist < BestDist * AxisMagnitude)
			{
				check(AxisMagnitude > 0.0f);
				FLOAT	InvAxisMagnitude = 1.0f / AxisMagnitude;
				BestDist = MaxPenetrationDist * InvAxisMagnitude;
				HitNormal = Axis * InvAxisMagnitude;
			}
			return 1;
		}
		else
			return 0;
	}

	UBOOL TestSeparatingAxis( const FVector& Axis, const FVector& Point, FLOAT ProjectedExtent )
	{
		return TestSeparatingAxis(Axis,Axis | Point,ProjectedExtent);
	}

	UBOOL TestSeparatingAxis( const FVector& Axis, const FVector& Point, const FVector& BoxX, const FVector& BoxY, const FVector& BoxZ, const FVector& BoxExtent )
	{
		FLOAT	ProjectedExtent = BoxExtent.X * Abs(Axis | BoxX) + BoxExtent.Y * Abs(Axis | BoxY) + BoxExtent.Z * Abs(Axis | BoxZ);
		return TestSeparatingAxis(Axis,Axis | Point,ProjectedExtent);
	}

	UBOOL FindSeparatingAxis( const FVector& Point, const FVector& BoxExtent, const FVector& BoxX, const FVector& BoxY, const FVector& BoxZ )
	{
		// Box faces.
		if(!TestSeparatingAxis(BoxX,Point,BoxExtent.X * BoxX.SizeSquared()))
			return 0;

		if(!TestSeparatingAxis(BoxY,Point,BoxExtent.Y * BoxY.SizeSquared()))
			return 0;

		if(!TestSeparatingAxis(BoxZ,Point,BoxExtent.Z * BoxZ.SizeSquared()))
			return 0;

		// Triangle normal.
		if(!TestSeparatingAxis((V2 - V1) ^ (V1 - V0),Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Box faces +/- X x triangle edges.
		if(!TestSeparatingAxis((V1 - V0) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V0 - V2) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Box faces +/- Y x triangle edges.
		if(!TestSeparatingAxis((V1 - V0) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		if(!TestSeparatingAxis((V0 - V2) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		// Box faces +/- Z x triangle edges.
		if(!TestSeparatingAxis((V1 - V0) ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		if(!TestSeparatingAxis((V0 - V2) ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		return 1;
	}

	FSeparatingAxisPointCheck(const FVector& InV0,const FVector& InV1,
		const FVector& InV2,const FVector& Point,const FVector& BoxExtent,
		const FVector& BoxX,const FVector& BoxY,const FVector& BoxZ,
		FLOAT InBestDist) :
		BestDist(InBestDist),HitNormal(0,0,0),Hit(0),V0(InV0),V1(InV1),
		V2(InV2)
	{
		Hit = FindSeparatingAxis(Point,BoxExtent,BoxX,BoxY,BoxZ);
	}
};


/* scion ======================================================================
 * FkDOP::FkDOP
 * Author: jg
 * 
 * Copies the passed in FkDOP and expands it by the extent. Note assumes AABB.
 *
 * input:	kDOP -- The kDOP to copy
 *			Extent -- The extent to expand it by
 *
 * ============================================================================
 */
FkDOP::FkDOP(const FkDOP& kDOP,const FVector& Extent)
{
	Min[0] = kDOP.Min[0] - Extent.X;
	Min[1] = kDOP.Min[1] - Extent.Y;
	Min[2] = kDOP.Min[2] - Extent.Z;
	Max[0] = kDOP.Max[0] + Extent.X;
	Max[1] = kDOP.Max[1] + Extent.Y;
	Max[2] = kDOP.Max[2] + Extent.Z;
}

/* scion ======================================================================
 * FkDOP::AddPoint
 * Author: jg
 * 
 * Adds a new point to the kDOP volume, expanding it if needed.
 *
 * input:	Point The vector to add to the volume
 *
 * ============================================================================
 */
void FkDOP::AddPoint(const FVector& Point)
{
	// Dot against each plane and expand the volume out to encompass it if the
	// point is further away than either (min/max) of the previous points
    for (INT nPlane = 0; nPlane < NUM_PLANES; nPlane++)
    {
		// Project this point onto the plane normal
		FLOAT Dot = Point | PlaneNormals[nPlane];
		// Move the plane out as needed
		if (Dot < Min[nPlane])
		{
			Min[nPlane] = Dot;
		}
		if (Dot > Max[nPlane])
		{
			Max[nPlane] = Dot;
		}
	}
}

/* scion ======================================================================
 * FkDOP::AddTriangles
 * Author: jg
 * 
 * Adds all of the triangle data to the kDOP volume
 *
 * input:	Vertices -- The mesh's rendering vertices
 *			StartIndex -- The triangle index to start processing with
 *			NumTris -- The number of triangles to process
 *			BuildTriangles -- The list of triangles to use for the build process
 *
 * ============================================================================
 */
void FkDOP::AddTriangles(TArray<FStaticMeshVertex>& Vertices,INT StartIndex,INT NumTris,TArray<FkDOPBuildCollisionTriangle>& BuildTriangles)
{
	guard(FkDOP::AddTriangles);
	// Reset the min/max planes
	Init();
	// Go through the list and add each of the triangle verts to our volume
	for( INT nTriangle = StartIndex; nTriangle < StartIndex + NumTris;nTriangle++ )
	{
		AddPoint(Vertices(BuildTriangles(nTriangle).v1).Position);
		AddPoint(Vertices(BuildTriangles(nTriangle).v2).Position);
		AddPoint(Vertices(BuildTriangles(nTriangle).v3).Position);
	}
	unguard;
}

/* scion ======================================================================
 * FkDOP::LineCheck
 * Author: jg
 * 
 * Checks a line against this kDOP. Note this assumes a AABB. If more planes
 * are to be used, this needs to be rewritten. Also note, this code is Andrew's
 * original code modified to work with FkDOP
 *
 * input:	Check The aggregated line check structure
 *			HitTime The out value indicating hit time
 *
 * ============================================================================
 */
UBOOL FkDOP::LineCheck(FkDOPLineCollisionCheck& Check,FLOAT& HitTime)
{
	guard(FkDOP::LineCheck);
	FVector	Time(0.f,0.f,0.f);
	UBOOL Inside = 1;

	HitTime = 0.0f;  // always initialize (prevent valgrind whining) --ryan.

	if(Check.LocalStart.X < Min[0])
	{
		if(Check.LocalDir.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Min[0] - Check.LocalStart.X) * Check.LocalOneOverDir.X;
		}
	}
	else if(Check.LocalStart.X > Max[0])
	{
		if(Check.LocalDir.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Max[0] - Check.LocalStart.X) * Check.LocalOneOverDir.X;
		}
	}

	if(Check.LocalStart.Y < Min[1])
	{
		if(Check.LocalDir.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Min[1] - Check.LocalStart.Y) * Check.LocalOneOverDir.Y;
		}
	}
	else if(Check.LocalStart.Y > Max[1])
	{
		if(Check.LocalDir.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Max[1] - Check.LocalStart.Y) * Check.LocalOneOverDir.Y;
		}
	}

	if(Check.LocalStart.Z < Min[2])
	{
		if(Check.LocalDir.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Min[2] - Check.LocalStart.Z) * Check.LocalOneOverDir.Z;
		}
	}
	else if(Check.LocalStart.Z > Max[2])
	{
		if(Check.LocalDir.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Max[2] - Check.LocalStart.Z) * Check.LocalOneOverDir.Z;
		}
	}

	if(Inside)
	{
		HitTime = 0.f;
		return 1;
	}

	HitTime = Time.GetMax();

	if(HitTime >= 0.0f && HitTime <= 1.0f)
	{
		const FVector& Hit = Check.LocalStart + Check.LocalDir * HitTime;

		return (Hit.X > Min[0] - FUDGE_SIZE && Hit.X < Max[0] + FUDGE_SIZE &&
				Hit.Y > Min[1] - FUDGE_SIZE && Hit.Y < Max[1] + FUDGE_SIZE &&
				Hit.Z > Min[2] - FUDGE_SIZE && Hit.Z < Max[2] + FUDGE_SIZE);
	}
	unguard;
	return 0;
}

/* scion ======================================================================
 * FkDOP::PointCheck
 * Author: jg
 * 
 * Checks a point with extent against this kDOP. The extent is already added in
 * to the kDOP being tested (Minkowski sum), so this code just checks to see if
 * the point is inside the kDOP. Note this assumes a AABB. If more planes are 
 * to be used, this needs to be rewritten.
 *
 * input:	Check The aggregated point check structure
 *
 * ============================================================================
 */
UBOOL FkDOP::PointCheck(FkDOPPointCollisionCheck& Check)
{
	return Check.LocalStart.X >= Min[0] && Check.LocalStart.X <= Max[0] 
		&& Check.LocalStart.Y >= Min[1] && Check.LocalStart.Y <= Max[1] 
		&& Check.LocalStart.Z >= Min[2] && Check.LocalStart.Z <= Max[2];
}

/* scion ======================================================================
 * FkDOP::PointCheck
 * Author: jag
 * 
 * Check (local space) AABB against this kDOP.
 *
 * input:	LocalAABB
 *
 * ============================================================================
 */
UBOOL FkDOP::AABBOverlapCheck(const FBox& LocalAABB)
{
	if( Min[0] > LocalAABB.Max.X || LocalAABB.Min.X > Max[0] )
		return 0;
	if( Min[1] > LocalAABB.Max.Y || LocalAABB.Min.Y > Max[1] )
		return 0;
	if( Min[2] > LocalAABB.Max.Z || LocalAABB.Min.Z > Max[2] )
		return 0;

	return 1;
}

/* scion ======================================================================
 * FkDOPTree::Build
 * Author: jg
 * 
 * Creates the root node and recursively splits the triangles into smaller
 * volumes
 *
 * input:	Vertices -- The mesh's vertex data
 *			BuildTriangles -- The list of triangles to use for the build process
 *
 * ============================================================================
 */
void FkDOPTree::Build(TArray<FStaticMeshVertex>& Vertices,TArray<FkDOPBuildCollisionTriangle>& BuildTriangles)
{
	guard(FkDOPTree::Build);
	// Empty the current set of nodes and preallocate the memory so it doesn't
	// reallocate memory while we are recursively walking the tree
	Nodes.Empty(BuildTriangles.Num()*2);
	// Add the root node
	Nodes.Add();
	// Now tell that node to recursively subdivide the entire set of triangles
	Nodes(0).SplitTriangleList(Vertices,0,BuildTriangles.Num(),BuildTriangles,Nodes);
	// Don't waste memory.
	Nodes.Shrink();
	// Copy over the triangle information afterward, since they will have
	// been sorted into their bounding volumes at this point
	Triangles.Empty();
	Triangles.Add(BuildTriangles.Num());
	// Copy the triangles from the build list into the full list
	for (INT nIndex = 0; nIndex < BuildTriangles.Num(); nIndex++)
	{
		Triangles(nIndex) = BuildTriangles(nIndex);
	}
	unguard;
}

/* scion ======================================================================
 * FkDOPNode::SplitTriangleList
 * Author: jg
 * 
 * Determines if the node is a leaf or not. If it is not a leaf, it subdivides
 * the list of triangles again adding two child nodes and splitting them on
 * the mean (splatter method). Otherwise it sets up the triangle information.
 *
 * input:	Vertices -- The mesh's rendering data
 *			Start -- The triangle index to start processing with
 *			NumTris -- The number of triangles to process
 *			BuildTriangles -- The list of triangles to use for the build process
 *			Nodes -- The list of nodes in this tree
 *
 * ============================================================================
 */
void FkDOPNode::SplitTriangleList(TArray<FStaticMeshVertex>& Vertices,INT Start,INT NumTris,TArray<FkDOPBuildCollisionTriangle>& BuildTriangles,TArray<FkDOPNode>& Nodes)
{
	guard(FkDOPTree::SplitTriangleList);
	// Add all of the triangles to the bounding volume
	BoundingVolume.AddTriangles(Vertices,Start,NumTris,BuildTriangles);

	// Figure out if we are a leaf node or not
	if (NumTris > MAX_TRIS_PER_LEAF)
	{
		// Still too many triangles, so continue subdividing the triangle list
		bIsLeaf = 0;
        INT BestPlane = -1;
        FLOAT BestMean = 0.f;
        FLOAT BestVariance = 0.f;
        // Determine how to split using the splatter algorithm
        for (INT nPlane = 0; nPlane < NUM_PLANES; nPlane++)
        {
            FLOAT Mean = 0.f;
			FLOAT Variance = 0.f;
            // Compute the mean for the triangle list
            for (INT nTriangle = Start; nTriangle < Start + NumTris;nTriangle++)
            {
				// Project the centroid of the triangle against the plane
				// normals and accumulate to find the total projected
				// weighting
                Mean += BuildTriangles(nTriangle).Centroid | PlaneNormals[nPlane];
            }
			// Divide by the number of triangles to get the average
            Mean /= FLOAT(NumTris);
            // Compute variance of the triangle list
            for (INT nTriangle = Start; nTriangle < Start + NumTris;nTriangle++)
            {
				// Project the centroid again
                FLOAT Dot = BuildTriangles(nTriangle).Centroid | PlaneNormals[nPlane];
				// Now calculate the variance and accumulate it
                Variance += (Dot - Mean) * (Dot - Mean);
            }
			// Get the average variance
            Variance /= FLOAT(NumTris);
            // Determine if this plane is the best to split on or not
            if (Variance >= BestVariance)
            {
                BestPlane = nPlane;
                BestVariance = Variance;
                BestMean = Mean;
            }
        }
		// Now that we have the plane to split on, work through the triangle
		// list placing them on the left or right of the splitting plane
        INT Left = Start - 1;
        INT Right = Start + NumTris;
		// Keep working through until the left index passes the right
        while (Left < Right)
        {
            FLOAT Dot;
			// Find all the triangles to the "left" of the splitting plane
            do
			{
                Dot = BuildTriangles(++Left).Centroid | PlaneNormals[BestPlane];
			}
            while (Dot < BestMean && Left < Right);
			// Find all the triangles to the "right" of the splitting plane
            do
			{
                Dot = BuildTriangles(--Right).Centroid | PlaneNormals[BestPlane];
			}
            while (Dot >= BestMean && Right > 0 && Left < Right);
			// Don't swap the triangle data if we just hit the end
            if (Left < Right)
			{
				// Swap the triangles since they are on the wrong sides of the
				// splitting plane
				FkDOPBuildCollisionTriangle Temp = BuildTriangles(Left);
				BuildTriangles(Left) = BuildTriangles(Right);
				BuildTriangles(Right) = Temp;
			}
        }
		// Check for wacky degenerate case where more than MAX_TRIS_PER_LEAF
		// fall all in the same kDOP
		if (Left == Start + NumTris || Right == Start)
		{
			Left = Start + (NumTris / 2);
		}
		// Add the two child nodes
		n.LeftNode = Nodes.Add(2);
		n.RightNode = n.LeftNode + 1;
		// Have the left node recursively subdivide it's list
		Nodes(n.LeftNode).SplitTriangleList(Vertices,Start,Left - Start,BuildTriangles,Nodes);
		// And now have the right node recursively subdivide it's list
		Nodes(n.RightNode).SplitTriangleList(Vertices,Left,Start + NumTris - Left,BuildTriangles,Nodes);
	}
	else
	{
		// No need to subdivide further so make this a leaf node
		bIsLeaf = 1;
		// Copy in the triangle information
		t.StartIndex = Start;
		t.NumTriangles = NumTris;
	}
	unguard;
}

/* scion ======================================================================
 * FkDOPNode::LineCheck
 * Author: jg
 * 
 * Determines the line in the FkDOPLineCollisionCheck intersects this node. It
 * also will check the child nodes if it is not a leaf, otherwise it will check
 * against the triangle data.
 *
 * input:	Check -- The aggregated line check data
 *
 * ============================================================================
 */
UBOOL FkDOPNode::LineCheck(FkDOPLineCollisionCheck& Check)
{
	UBOOL bHit = 0;
	guard(FkDOPNode::LineCheck);
	// If this is a node, check the two child nodes and pick the closest one
	// to recursively check against and only check the second one if there is
	// not a hit or the hit returned is further out than the second node
	if (bIsLeaf == 0)
	{
		// Holds the indices for the closest and farthest nodes
		INT NearNode = -1;
		INT FarNode = -1;
		// Holds the hit times for the child nodes
		FLOAT Child1, Child2;
		// Assume the left node is closer (it will be adjusted later)
		if (Check.Nodes(n.LeftNode).BoundingVolume.LineCheck(Check,Child1))
		{
			NearNode = n.LeftNode;
		}
		// Find out if the second node is closer
		if (Check.Nodes(n.RightNode).BoundingVolume.LineCheck(Check,Child2))
		{
			// See if the left node was a miss and make the right the near node
			if (NearNode == -1)
			{
				NearNode = n.RightNode;
			}
			else
			{
				FarNode = n.RightNode;
			}
		}
		// Swap the Near/FarNodes if the right node is closer than the left
		if (NearNode != -1 && FarNode != -1 && Child2 < Child1)
		{
			Exchange(NearNode,FarNode);
			Exchange(Child1,Child2);
		}
		// See if we need to search the near node or not
		if (NearNode != -1 && Check.Result->Time > Child1)
		{
			bHit = Check.Nodes(NearNode).LineCheck(Check);
		}
		// Now do the same for the far node. This will only happen if a miss in
		// the near node or the nodes overlapped and this one is closer
		if (FarNode != -1 && Check.Result->Time > Child2)
		{
			bHit |= Check.Nodes(FarNode).LineCheck(Check);
		}
	}
	else
	{
		// This is a leaf, check the triangles for a hit
		bHit = LineCheckTriangles(Check);
	}
	unguard;
	return bHit;
}

/* scion ======================================================================
 * FkDOPNode::LineCheckTriangles
 * Author: jg
 * 
 * Works through the list of triangles in this node checking each one for a
 * collision.
 *
 * input:	Check -- The aggregated line check data
 *
 * ============================================================================
 */
UBOOL FkDOPNode::LineCheckTriangles(FkDOPLineCollisionCheck& Check)
{
	// Assume a miss
	UBOOL bHit = 0;
	guard(FkDOPNode::LineCheckTriangles);
	// Loop through all of our triangles. We need to check them all in case
	// there are two (or more) potential triangles that would collide and let
	// the code choose the closest
	for( INT nCollTriIndex = t.StartIndex; nCollTriIndex < t.StartIndex + t.NumTriangles;	nCollTriIndex++ )
	{
		// Get the collision triangle that we are checking against
		const FkDOPCollisionTriangle& CollTri =	Check.CollisionTriangles(nCollTriIndex);
		// Now get refs to the 3 verts to check against
		const FStaticMeshVertex& v1 = Check.Triangles(CollTri.v1);
		const FStaticMeshVertex& v2 = Check.Triangles(CollTri.v2);
		const FStaticMeshVertex& v3 = Check.Triangles(CollTri.v3);
		// Now check for an intersection
		bHit |= LineCheckTriangle(Check,v1,v2,v3,CollTri.MaterialIndex);
	}
	unguard;
	return bHit;
}

/* scion ======================================================================
 * FkDOPNode::LineCheckTriangle
 * Author: jg
 * 
 * Performs collision checking against the triangle using the old collision
 * code to handle it. This is done to provide consistency in collision.
 *
 * input:	Check -- The aggregated line check data
 *			v1 -- The first vertex of the triangle
 *			v2 -- The second vertex of the triangle
 *			v3 -- The third vertex of the triangle
 *			MaterialIndex -- The material for this triangle if it is hit
 *
 * ============================================================================
 */
UBOOL FkDOPNode::LineCheckTriangle(FkDOPLineCollisionCheck& Check,const FStaticMeshVertex& v1,const FStaticMeshVertex& v2,const FStaticMeshVertex& v3,INT MaterialIndex)
{
	guard(FkDOPNode::LineCheckTriangle);
	// Calculate the hit normal the same way the old code
	// did so things are the same
	const FVector& LocalNormal = ((v2.Position - v3.Position) ^ (v1.Position - v3.Position)).SafeNormal();
	// Calculate the hit time the same way the old code
	// did so things are the same
	FPlane TrianglePlane(v1.Position,LocalNormal);
	FLOAT StartDist = TrianglePlane.PlaneDot(Check.LocalStart);
	FLOAT EndDist = TrianglePlane.PlaneDot(Check.LocalEnd);
	if ((StartDist > -0.001f && EndDist > -0.001f) || (StartDist < 0.001f && EndDist < 0.001f))
	{
		return 0;
	}
	// Figure out when it will hit the triangle
	FLOAT Time = -StartDist / (EndDist - StartDist);
	// If this triangle is not closer than the previous hit, reject it
	if (Time >= Check.Result->Time)
		return 0;
	// Calculate the line's point of intersection with the node's plane
	const FVector& Intersection = Check.LocalStart + Check.LocalDir * Time;
	const FVector* Verts[3] = 
	{ 
		&v1.Position, 
		&v2.Position, 
		&v3.Position
	};
	// Check if the point of intersection is inside the triangle's edges.
	for( INT SideIndex = 0; SideIndex < 3; SideIndex++ )
	{
		const FVector& SideDirection = LocalNormal ^ (*Verts[(SideIndex + 1) % 3] - *Verts[SideIndex]);
		FLOAT SideW = SideDirection | *Verts[SideIndex];
		if( ((SideDirection | Intersection) - SideW) >= 0.f )
		{
			return 0;
		}
	}
	// Return results
	Check.LocalHitNormal = LocalNormal;
	Check.Result->Time		= Time;
	Check.Result->Material	= Check.StaticMesh->GetSkin(Check.Owner,MaterialIndex);
	unguard;
	return 1;
}

/* scion ======================================================================
 * FkDOPNode::BoxCheck
 * Author: jg
 * 
 * Determines the line + extent in the FkDOPBoxCollisionCheck intersects this
 * node. It also will check the child nodes if it is not a leaf, otherwise it
 * will check against the triangle data.
 *
 * input:	Check -- The aggregated box check data
 *
 * ============================================================================
 */
UBOOL FkDOPNode::BoxCheck(FkDOPBoxCollisionCheck& Check)
{
	UBOOL bHit = 0;
	guard(FkDOPNode::BoxCheck);
	// If this is a node, check the two child nodes and pick the closest one
	// to recursively check against and only check the second one if there is
	// not a hit or the hit returned is further out than the second node
	if (bIsLeaf == 0)
	{
		// Holds the indices for the closest and farthest nodes
		INT NearNode = -1;
		INT FarNode = -1;
		// Holds the hit times for the child nodes
		FLOAT Child1, Child2;
		// Update the kDOP with the extent and test against that
		FkDOP kDOPNear(Check.Nodes(n.LeftNode).BoundingVolume,Check.LocalExtent);
		// Assume the left node is closer (it will be adjusted later)
		if (kDOPNear.LineCheck(Check,Child1))
		{
			NearNode = n.LeftNode;
		}
		// Update the kDOP with the extent and test against that
		FkDOP kDOPFar(Check.Nodes(n.RightNode).BoundingVolume,Check.LocalExtent);
		// Find out if the second node is closer
		if (kDOPFar.LineCheck(Check,Child2))
		{
			// See if the left node was a miss and make the right the near node
			if (NearNode == -1)
			{
				NearNode = n.RightNode;
			}
			else
			{
				FarNode = n.RightNode;
			}
		}
		// Swap the Near/FarNodes if the right node is closer than the left
		if (NearNode != -1 && FarNode != -1 && Child2 < Child1)
		{
			Exchange(NearNode,FarNode);
			Exchange(Child1,Child2);
		}
		// See if we need to search the near node or not
		if (NearNode != -1)
		{
			bHit = Check.Nodes(NearNode).BoxCheck(Check);
		}
		// Now do the same for the far node. This will only happen if a miss in
		// the near node or the nodes overlapped and this one is closer
		if (FarNode != -1 && (Check.Result->Time > Child2 || bHit == 0))
		{
			bHit |= Check.Nodes(FarNode).BoxCheck(Check);
		}
	}
	else
	{
		// This is a leaf, check the triangles for a hit
		bHit = BoxCheckTriangles(Check);
	}
	unguard;
	return bHit;
}

/* scion ======================================================================
 * FkDOPNode::BoxCheckTriangles
 * Author: jg
 * 
 * Works through the list of triangles in this node checking each one for a
 * collision.
 *
 * input:	Check -- The aggregated box check data
 *
 * ============================================================================
 */
UBOOL FkDOPNode::BoxCheckTriangles(FkDOPBoxCollisionCheck& Check)
{
	guard(FkDOPNode::BoxCheckTriangles);
	// Assume a miss
	UBOOL bHit = 0;
	// Loop through all of our triangles. We need to check them all in case
	// there are two (or more) potential triangles that would collide and let
	// the code choose the closest
	for( INT nCollTriIndex = t.StartIndex; nCollTriIndex < t.StartIndex + t.NumTriangles;	nCollTriIndex++ )
	{
		// Get the collision triangle that we are checking against
		const FkDOPCollisionTriangle& CollTri = Check.CollisionTriangles(nCollTriIndex);
		// Now get refs to the 3 verts to check against
		const FVector& v1 = Check.Triangles(CollTri.v1).Position;
		const FVector& v2 = Check.Triangles(CollTri.v2).Position;
		const FVector& v3 = Check.Triangles(CollTri.v3).Position;
		// Now check for an intersection using the Separating Axis Theorem
		bHit |= BoxCheckTriangle(Check,v1,v2,v3,CollTri.MaterialIndex);
	}
	return bHit;
	unguard;
}

/* scion ======================================================================
 * FkDOPNode::BoxCheckTriangle
 * Author: jg
 * 
 * Uses the separating axis theorem to check for triangle box collision.
 *
 * input:	Check -- The aggregated box check data
 *			v1 -- The first vertex of the triangle
 *			v2 -- The second vertex of the triangle
 *			v3 -- The third vertex of the triangle
 *			MaterialIndex -- The material for this triangle if it is hit
 *
 * ============================================================================
 */
UBOOL FkDOPNode::BoxCheckTriangle(FkDOPBoxCollisionCheck& Check,const FVector& v1,const FVector& v2,const FVector& v3,INT MaterialIndex)
{
	FLOAT HitTime = 1.f;
	FVector HitNormal(0.f,0.f,0.f);
	// Now check for an intersection using the Separating Axis Theorem
	INT Result = FindSeparatingAxis(v1,v2,v3,Check.LocalStart,Check.LocalEnd,Check.Extent,Check.LocalBoxX,Check.LocalBoxY,Check.LocalBoxZ,HitTime,HitNormal);
	if (Result == 1)
	{
		if (HitTime < Check.Result->Time)
		{
			// Store the better time
			Check.Result->Time = HitTime;
			// Get the material that was hit
			Check.Result->Material = Check.StaticMesh->GetSkin(
			Check.Owner,MaterialIndex);
			// Normal will get transformed to world space at end of check
			Check.LocalHitNormal = HitNormal;

			//TEMP
			Check.bUseHack = 0;
			return 1;
		}
	}
	//TEMP Hack
	else if (Result == 2)
	{
		Check.bUseHack = 1;
	}
	return 0;
}

/* scion ======================================================================
 * FkDOPNode::PointCheck
 * Author: jg
 * 
 * Determines the point + extent in the FkDOPPointCollisionCheck intersects
 * this node. It also will check the child nodes if it is not a leaf, otherwise
 * it will check against the triangle data.
 *
 * input:	Check -- The aggregated point check data
 *
 * ============================================================================
 */
UBOOL FkDOPNode::PointCheck(FkDOPPointCollisionCheck& Check)
{
	UBOOL bHit = 0;
	guard(FkDOPNode::PointCheck);
	// If this is a node, check the two child nodes recursively
	if (bIsLeaf == 0)
	{
		// Holds the indices for the closest and farthest nodes
		INT NearNode = -1;
		INT FarNode = -1;
		// Update the kDOP with the extent and test against that
		FkDOP kDOPNear(Check.Nodes(n.LeftNode).BoundingVolume,Check.LocalExtent);
		// Assume the left node is closer (it will be adjusted later)
		if (kDOPNear.PointCheck(Check))
		{
			NearNode = n.LeftNode;
		}
		// Update the kDOP with the extent and test against that
		FkDOP kDOPFar(Check.Nodes(n.RightNode).BoundingVolume,Check.LocalExtent);
		// Find out if the second node is closer
		if (kDOPFar.PointCheck(Check))
		{
			// See if the left node was a miss and make the right the near node
			if (NearNode == -1)
			{
				NearNode = n.RightNode;
			}
			else
			{
				FarNode = n.RightNode;
			}
		}
		// See if we need to search the near node or not
		if (NearNode != -1)
		{
			bHit = Check.Nodes(NearNode).PointCheck(Check);
		}
		// Now do the same for the far node
		if (FarNode != -1)
		{
			bHit |= Check.Nodes(FarNode).PointCheck(Check);
		}
	}
	else
	{
		// This is a leaf, check the triangles for a hit
		bHit = PointCheckTriangles(Check);
	}
	unguard;
	return bHit;
}

/* ===========================================================================
 * FkDOPNode::SphereQuery
 * Author: jag
 * 
 * Find triangles that overlap the given sphere. We assume that the supplied box overlaps this node.
 *
 * input:	Query -- Query information
 *
 * ============================================================================
 */
void FkDOPNode::SphereQuery(FkDOPSphereQuery& Query)
{
	guard(FkDOPNode::SphereQuery);

	// If not leaf, check against each child.
	if(bIsLeaf == 0)
	{
		if( Query.Nodes(n.LeftNode).BoundingVolume.AABBOverlapCheck(Query.LocalBox) )
			Query.Nodes(n.LeftNode).SphereQuery(Query);

		if( Query.Nodes(n.RightNode).BoundingVolume.AABBOverlapCheck(Query.LocalBox) )
			Query.Nodes(n.RightNode).SphereQuery(Query);
	}
	else // Otherwise, add all the triangles in this node to the list.
	{
		for(INT i=t.StartIndex; i<t.StartIndex+t.NumTriangles; i++)
		{
			Query.ReturnTriangles.AddItem( i );
		}
	}

	unguard;
}

/* scion ======================================================================
 * FkDOPNode::PointCheckTriangles
 * Author: jg
 * 
 * Works through the list of triangles in this node checking each one for a
 * collision.
 *
 * input:	Check -- The aggregated point check data
 *
 * ============================================================================
 */
UBOOL FkDOPNode::PointCheckTriangles(FkDOPPointCollisionCheck& Check)
{
	// Assume a miss
	UBOOL bHit = 0;
	guard(FkDOPNode::PointCheckTriangles);
	// Loop through all of our triangles. We need to check them all in case
	// there are two (or more) potential triangles that would collide and let
	// the code choose the closest
	for( INT nCollTriIndex = t.StartIndex; nCollTriIndex < t.StartIndex + t.NumTriangles;	nCollTriIndex++ )
	{
		// Get the collision triangle that we are checking against
		const FkDOPCollisionTriangle& CollTri =	Check.CollisionTriangles(nCollTriIndex);
		// Now get refs to the 3 verts to check against
		const FVector& v1 = Check.Triangles(CollTri.v1).Position;
		const FVector& v2 = Check.Triangles(CollTri.v2).Position;
		const FVector& v3 = Check.Triangles(CollTri.v3).Position;
		// Now check for an intersection using the Separating Axis Theorem
		bHit |= PointCheckTriangle(Check,v1,v2,v3,CollTri.MaterialIndex);
	}
	unguard;
	return bHit;
}

/* scion ======================================================================
 * FkDOPNode::PointCheckTriangle
 * Author: jg
 * 
 * Uses the separating axis theorem to check for triangle box collision.
 *
 * input:	Check -- The aggregated box check data
 *			v1 -- The first vertex of the triangle
 *			v2 -- The second vertex of the triangle
 *			v3 -- The third vertex of the triangle
 *			MaterialIndex -- The material for this triangle if it is hit
 *
 * ============================================================================
 */
UBOOL FkDOPNode::PointCheckTriangle(FkDOPPointCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3,INT MaterialIndex)
{
	// Use the separating axis thereom to see if we hit
	FSeparatingAxisPointCheck PointCheck(v1,v2,v3,Check.LocalStart,Check.Extent,Check.LocalBoxX,Check.LocalBoxY,Check.LocalBoxZ,Check.BestDistance);
	// If we hit and it is closer update the out values
	if (PointCheck.Hit && PointCheck.BestDist < Check.BestDistance)
	{
		// Get the material that was hit
		Check.Result->Material = Check.StaticMesh->GetSkin(Check.Owner,MaterialIndex);
		// Normal will get transformed to world space at end of check
		Check.LocalHitNormal = PointCheck.HitNormal;
		// Copy the distance for push out calculations
		Check.BestDistance = PointCheck.BestDist;
		return 1;
	}
	return 0;
}

/* scion ======================================================================
 * FkDOPTree::LineCheck
 * Author: jg
 * 
 * Figures out whether the check even hits the root node's bounding volume. If
 * it does, it recursively searches for a triangle to hit.
 *
 * input:	Check -- The aggregated line check data
 *
 * ============================================================================
 */
UBOOL FkDOPTree::LineCheck(FkDOPLineCollisionCheck& Check)
{
	UBOOL bHit = 0;
	guard(FkDOPTree::LineCheck);
	FLOAT HitTime;
	// Check against the first bounding volume and decide whether to go further
	if (Nodes(0).BoundingVolume.LineCheck(Check,HitTime))
	{
		// Recursively check for a hit
		bHit = Nodes(0).LineCheck(Check);
	}
	unguard;
	return bHit;
}

/* scion ======================================================================
 * FkDOPTree::BoxCheck
 * Author: jg
 * 
 * Figures out whether the check even hits the root node's bounding volume. If
 * it does, it recursively searches for a triangle to hit.
 *
 * input:	Check -- The aggregated box check data
 *
 * ============================================================================
 */
UBOOL FkDOPTree::BoxCheck(FkDOPBoxCollisionCheck& Check)
{
	UBOOL bHit = 0;
	guard(FkDOPTree::BoxCheck);
	FLOAT HitTime;
	// Check the root node's bounding volume expanded by the extent
	FkDOP kDOP(Nodes(0).BoundingVolume,Check.LocalExtent);
	// Check against the first bounding volume and decide whether to go further
	if (kDOP.LineCheck(Check,HitTime))
	{
		// Recursively check for a hit
		bHit = Nodes(0).BoxCheck(Check);
	}
	unguard;
	return bHit;
}

/* scion ======================================================================
 * FkDOPTree::PointCheck
 * Author: jg
 * 
 * Figures out whether the check even hits the root node's bounding volume. If
 * it does, it recursively searches for a triangle to hit.
 *
 * input:	Check -- The aggregated point check data
 *
 * ============================================================================
 */
UBOOL FkDOPTree::PointCheck(FkDOPPointCollisionCheck& Check)
{
	UBOOL bHit = 0;
	guard(FkDOPTree::PointCheck);
	// Check the root node's bounding volume expanded by the extent
	FkDOP kDOP(Nodes(0).BoundingVolume,Check.LocalExtent);
	// Check against the first bounding volume and decide whether to go further
	if (kDOP.PointCheck(Check))
	{
		// Recursively check for a hit
		bHit = Nodes(0).PointCheck(Check);
	}
	unguard;
	return bHit;
}

/* ===========================================================================
 * FkDOPTree::SphereQuery
 * Author: jag
 * 
 * Find all triangles in static mesh that overlap a supplied bounding sphere.
 *
 * input:	Query -- The aggregated sphere query data
 *
 * ============================================================================
 */

void FkDOPTree::SphereQuery(FkDOPSphereQuery& Query)
{
	guard(FkDOPTree::SphereQuery);

	// Check the query box overlaps the root node KDOP. If so, run query recursively.
	if( Query.Nodes(0).BoundingVolume.AABBOverlapCheck( Query.LocalBox ) )
	{
		Query.Nodes(0).SphereQuery( Query );
	}

	unguard;
}

/* scion ======================================================================
 * FkDOPLineCollisionCheck::FkDOPLineCollisionCheck
 * Author: jg
 * 
 * Sets up the FkDOPLineCollisionCheck structure for performing line checks
 * against a kDOPTree. Initializes all of the variables that are used
 * throughout the line check.
 *
 * input:	InResult -- The out param for hit result information
 *			InOwner -- The owning actor being checked
 *			InStaticMesh -- The static mesh being checked
 *			InStart -- The starting point of the trace
 *			InEnd -- The ending point of the trace
 *
 * ============================================================================
 */
FkDOPLineCollisionCheck::FkDOPLineCollisionCheck(
	FCheckResult* InResult,
	AActor* InOwner,UStaticMesh* InStaticMesh,const FVector& InStart,
	const FVector& InEnd) :
	FCollisionCheck(InResult,InOwner,InStaticMesh),
	Start(InStart), End(InEnd), WorldToLocal(InOwner->WorldToLocal()),
	kDOPTree(InStaticMesh->kDOPTree), Nodes(InStaticMesh->kDOPTree.Nodes),
	CollisionTriangles(InStaticMesh->kDOPTree.Triangles),
	Triangles(InStaticMesh->VertexStream.Vertices)
{
	guard(FkDOPLineCollisionCheck::FkDOPLineCollisionCheck);
	// Move start and end to local space
	LocalStart = WorldToLocal.TransformFVector(Start);
	LocalEnd = WorldToLocal.TransformFVector(End);
	// Calculate the vector's direction in local space
	LocalDir = LocalEnd - LocalStart;
	// Build the one over dir
	LocalOneOverDir.X = LocalDir.X ? 1.f / LocalDir.X : 0.f;
	LocalOneOverDir.Y = LocalDir.Y ? 1.f / LocalDir.Y : 0.f;
	LocalOneOverDir.Z = LocalDir.Z ? 1.f / LocalDir.Z : 0.f;
	// For calculating hit normals
	LocalToWorld = InOwner->LocalToWorld();
	LocalToWorldTA = LocalToWorld.TransposeAdjoint();
	Determinant = LocalToWorld.Determinant();
	// Clear the closest hit time
	Result->Time = MAX_FLT;
	unguard;
}

/* scion ======================================================================
 * FkDOPBoxCollisionCheck::FkDOPBoxCollisionCheck
 * Author: jg
 * 
 * Sets up the FkDOPBoxCollisionCheck structure for performing swept box checks
 * against a kDOPTree. Initializes all of the variables that are used
 * throughout the check.
 *
 * input:	InResult -- The out param for hit result information
 *			InOwner -- The owning actor being checked
 *			InStaticMesh -- The static mesh being checked
 *			InStart -- The starting point of the trace
 *			InEnd -- The ending point of the trace
 *			InExtent -- The extent to check
 *
 * ============================================================================
 */
FkDOPBoxCollisionCheck::FkDOPBoxCollisionCheck(FCheckResult* InResult,AActor* InOwner,UStaticMesh* InStaticMesh,const FVector& InStart,const FVector& InEnd,const FVector& InExtent) :
	FkDOPLineCollisionCheck(InResult,InOwner,InStaticMesh,InStart,InEnd),Extent(InExtent)
{
	guard(FkDOPBoxCollisionCheck::FkDOPBoxCollisionCheck);
	// Move extent to local space
	LocalExtent = FBox(-Extent,Extent).TransformBy(WorldToLocal).GetExtent();
	// Transform the PlaneNormals into local space.
	LocalBoxX = WorldToLocal.TransformNormal(PlaneNormals[0]);
	LocalBoxY = WorldToLocal.TransformNormal(PlaneNormals[1]);
	LocalBoxZ = WorldToLocal.TransformNormal(PlaneNormals[2]);
	bUseHack = 0;
	unguard;
}

/* scion ======================================================================
 * FkDOPPointCollisionCheck::FkDOPPointCollisionCheck
 * Author: jg
 * 
 * Sets up the FkDOPPointCollisionCheck structure for performing point checks
 * (point plus extent) against a kDOPTree. Initializes all of the variables
 * that are used throughout the check.
 *
 * input:	InResult -- The out param for hit result information
 *			InOwner -- The owning actor being checked
 *			InStaticMesh -- The static mesh being checked
 *			InLocation -- The point to check for intersection
 *			InExtent -- The extent to check
 *
 * ============================================================================
 */
FkDOPPointCollisionCheck::FkDOPPointCollisionCheck(FCheckResult* InResult,AActor* InOwner,UStaticMesh* InStaticMesh,const FVector& InLocation,const FVector& InExtent) :
	FkDOPBoxCollisionCheck(InResult,InOwner,InStaticMesh,InLocation,InLocation,InExtent),
	BestDistance(100000.f)
{
}

/* ===========================================================================
* FkDOPSphereQuery::FkDOPSphereQuery
* Author: jag
* 
* Sets up the FkDOPSphereQuery structure for finding the set of triangles
* in the static mesh that overlap the give sphere.
*
* input:	InOwner -- The owning actor being checked
*			InStaticMesh -- The static mesh being checked
*			InSphere -- Sphere to query against 
* output:   OutTriangles -- Array of collision triangles that overlap sphere
*
* ============================================================================
*/

FkDOPSphereQuery::FkDOPSphereQuery(AActor* InOwner,UStaticMesh* InStaticMesh,const FSphere& InSphere,TArray<INT>& OutTriangles) :
	Nodes( InStaticMesh->kDOPTree.Nodes ),
	CollisionTriangles( InStaticMesh->kDOPTree.Triangles ),
	ReturnTriangles( OutTriangles )
{
	guard(FkDOPSphereQuery::FkDOPSphereQuery);
	// Find bounding box we are querying triangles against in local space.
	FMatrix w2l = InOwner->WorldToLocal();
	FVector radiusVec(InSphere.W, InSphere.W, InSphere.W);
	FBox WorldBox(InSphere - radiusVec, InSphere + radiusVec);
	LocalBox = WorldBox.TransformBy(w2l);
	unguard;
}