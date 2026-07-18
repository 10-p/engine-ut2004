#include "EnginePrivate.h"

#ifdef WITH_KARMA

#include <MeMath.h>
#include <McdBox.h>

#ifndef _WIN64  // !!! FIXME: Merge headers.
#include <McdCheck.h>
#endif

#include <McdInteractionTable.h>
#include <McdModel.h>
#include <McdContact.h>
//#include "lsTransform.h"
#include <McdTriangleList.h>
#include <McdModelPair.h>
#include <McdPrimitives.h>
//#include "vectormath.h"
//#include <GeomUtils.h>

//  Vec x Axis
inline void MeVec3CrossAxis(MeVector3 outVec, const MeVector3 inVec, const INT inAxisN) 
{
	const int axisN1 = NextMod3(inAxisN);
	const int axisN2 = NextMod3(axisN1);

	outVec[inAxisN] = (MeReal)(0.0);
	outVec[axisN1] = inVec[axisN2];
	outVec[axisN2] = -inVec[axisN1];
}

//   Axis x Vec
inline void MeAxisCrossVec3(MeVector3 outVec, const int inAxisN, const MeVector3 inVec) 
{
	const int axisN1 = NextMod3(inAxisN);
	const int axisN2 = NextMod3(axisN1);

	outVec[inAxisN] = (MeReal)(0.0);
	outVec[axisN1] = -inVec[axisN2];
	outVec[axisN2] = inVec[axisN1];
}

inline void MeVector3Abs(MeVector3 outVec, const MeVector3 inVec)
{
	outVec[0] = MeFabs( inVec[0] );
	outVec[1] = MeFabs( inVec[1] );
	outVec[2] = MeFabs( inVec[2] );
}

inline void McdUserTriangleGetExtent( const McdUserTriangle * tri, const MeVector3 e[], MeReal e2[], int coordN, MeReal *minExt, MeReal *maxExt )
{
	guard(McdUserTriangleGetExtent);

	*minExt = -MEINFINITY;
	*maxExt = MEINFINITY;

	for ( int i = 0; i < 3; ++i )
	{
		int j = NextMod3( i );
		MeReal ei = e[i][coordN];
		MeReal ej = e[j][coordN];
		MeBool iValid = (tri->flags & (1<<(i+2))) != 0;
		MeBool jValid = (tri->flags & (1<<(j+2))) != 0;
		MeReal vij = (*tri->vertices[j])[coordN];

		if ( ei*ei > e2[i]*ME_SMALL_EPSILON )
		{
			if (jValid)
			{
				if ( ei > 0 )
				{
					if ( ej*ej > e2[j]*ME_SMALL_EPSILON )
					{
						if ( ej < 0 )
						{
							// +-
							if (iValid)
							{
								*maxExt = vij;
							}
						}
					} else
					{
						// +0
						*maxExt = vij;
					}
				} else
				{
					if ( ej*ej > e2[j]*ME_SMALL_EPSILON )
					{
						if ( ej > 0 )
						{
							// -+
							if (iValid)
							{
								*minExt = vij;
							}
						}
					} else
					{
						// -0
						*minExt = vij;
					}
				}
			}
		} else
		{
			if ( iValid && ej*ej > e2[j]*ME_SMALL_EPSILON )
			{
				if ( ej > 0 )
				{
					// 0+
					*minExt = vij;
				} else
				{
					// 0-
					*maxExt = vij;
				}
			}
		}

	}

	unguard;
}

static inline int McdVanillaSegmentCubeIntersect(
	MeReal *tInMax, MeReal *tOutMin,
	const MeVector3 orig, const MeVector3 disp,
	const MeVector3 invDisp, const MeVector3 inR, const MeReal eps)
{
	guard(McdVanillaSegmentCubeIntersect);

	int j;
	MeReal tIn, tOut;

	for (j = 0; j < 3; j++)
	{ // j = axis of box A

		if(MeFabs(disp[j])<eps)
		{
			if(MeFabs(orig[j]) > inR[j])
				return 0;
		}
		else
		{
			tIn =  (- (orig[j] * invDisp[j])) - inR[j] * MeFabs(invDisp[j]);
			tOut = (- (orig[j] * invDisp[j])) + inR[j] * MeFabs(invDisp[j]);

			if (tIn > *tInMax)
				*tInMax = tIn;
			if (tOut < *tOutMin)
				*tOutMin = tOut;
			if (*tOutMin < *tInMax)
				return 0;
		}
	}
	return 1;

	unguard;
}

static inline void McdVanillaAddBoxEndSegmentPoints(MeVector3* &outList,
													const MeVector3 orig, const MeVector3 disp,
													const MeVector3 invDisp, const MeVector3 inR, const MeReal eps)
{
	MeReal tIn = 0.0f;
	MeReal tOut = 1.0f;

	if ( McdVanillaSegmentCubeIntersect(&tIn,&tOut,orig,disp,invDisp,inR,eps) )
	{
		MeVector3Copy(*outList, disp);
		MeVector3Scale(*outList, tIn);
		MeVector3Add(*outList, *outList, orig);
		outList++;

		if (tOut < 1)
		{
			MeVector3Copy(*outList, disp);
			MeVector3Scale(*outList, tOut);
			MeVector3Add(*outList, *outList, orig);
			outList++;
		}
	}
}

static void McdVanillaAddTriBoxSegmentPoints(MeVector3* &outList,
													const MeVector3 inRBox, const int i0, const int i1, const int i2,
													const McdUserTriangle * inTri, const MeVector3 edge[3], const MeVector3 axb[3], const MeReal inTriD)
{
	guard(McdVanillaAddTriBoxSegmentPoints);

	const MeReal r0 = inRBox[i0];
	const MeReal den = (*inTri->normal)[i0];
	const MeReal recipDen = MeSafeRecip(den);
	const bool ccw = den < 0;

	MeVector3 x[3];

	MeVec3CrossAxis(x[0], edge[0], i0);
	x[0][i1] *= inRBox[i1];
	x[0][i2] *= inRBox[i2];

	MeVec3CrossAxis(x[1], edge[1], i0);
	x[1][i1] *= inRBox[i1];
	x[1][i2] *= inRBox[i2];

	MeVec3CrossAxis(x[2], edge[2], i0);
	x[2][i1] *= inRBox[i1];
	x[2][i2] *= inRBox[i2];

	MeVector3 dn;
	dn[i0] = 0;
	dn[i1] = inRBox[i1]*(*inTri->normal)[i1];
	dn[i2] = inRBox[i2]*(*inTri->normal)[i2];

	// ++ edge
	if (axb[0][i0]-x[0][i1]-x[0][i2] < 0 == ccw &&
		axb[1][i0]-x[1][i1]-x[1][i2] < 0 == ccw &&
		axb[2][i0]-x[2][i1]-x[2][i2] < 0 == ccw)
	{   // points at triangle
		MeReal r = (inTriD-dn[i1]-dn[i2])*recipDen;
		if (r >= -r0 && r <= r0)
		{  // intersects segment
			(*outList)[i0] = r;
			(*outList)[i1] = inRBox[i1];
			(*outList)[i2] = inRBox[i2];
			outList++;
		}
	}

	// +- edge
	if (axb[0][i0]-x[0][i1]+x[0][i2] < 0 == ccw &&
		axb[1][i0]-x[1][i1]+x[1][i2] < 0 == ccw &&
		axb[2][i0]-x[2][i1]+x[2][i2] < 0 == ccw)
	{   // points at triangle
		MeReal r = (inTriD-dn[i1]+dn[i2])*recipDen;
		if (r >= -r0 && r <= r0)
		{  // intersects segment
			(*outList)[i0] = r;
			(*outList)[i1] = inRBox[i1];
			(*outList)[i2] = -inRBox[i2];
			outList++;
		}
	}

	// -- edge
	if (axb[0][i0]+x[0][i1]+x[0][i2] < 0 == ccw &&
		axb[1][i0]+x[1][i1]+x[1][i2] < 0 == ccw &&
		axb[2][i0]+x[2][i1]+x[2][i2] < 0 == ccw)
	{   // points at triangle
		MeReal r = (inTriD+dn[i1]+dn[i2])*recipDen;
		if (r >= -r0 && r <= r0)
		{  // intersects segment
			(*outList)[i0] = r;
			(*outList)[i1] = -inRBox[i1];
			(*outList)[i2] = -inRBox[i2];
			outList++;
		}
	}

	// -+ edge
	if (axb[0][i0]+x[0][i1]-x[0][i2] < 0 == ccw &&
		axb[1][i0]+x[1][i1]-x[1][i2] < 0 == ccw &&
		axb[2][i0]+x[2][i1]-x[2][i2] < 0 == ccw)
	{   // points at triangle
		MeReal r = (inTriD+dn[i1]-dn[i2])*recipDen;
		if (r >= -r0 && r <= r0) {  // intersects segment
			(*outList)[i0] = r;
			(*outList)[i1] = -inRBox[i1];
			(*outList)[i2] = inRBox[i2];
			outList++;
		}
	}

	unguard;
}

static void McdVanillaBoxTriIntersect(MeVector3* &outList,
											 const MeVector3 inRBox, const McdUserTriangle *inTri, const MeVector3 edge[3], const MeReal scale)
{
	guard(McdVanillaBoxTriIntersect);

	if(inTri->flags&kMcdTriangleUseEdge0)
	{
		MeVector3 invE;
		invE[0] = MeSafeRecip(edge[0][0]);
		invE[1] = MeSafeRecip(edge[0][1]);
		invE[2] = MeSafeRecip(edge[0][2]);

		McdVanillaAddBoxEndSegmentPoints(outList, *inTri->vertices[0], edge[0], invE, inRBox, (MeReal)1e-6 * scale);
	}

	if(inTri->flags&kMcdTriangleUseEdge1)
	{
		MeVector3 invE;
		invE[0] = MeSafeRecip(edge[1][0]);
		invE[1] = MeSafeRecip(edge[1][1]);
		invE[2] = MeSafeRecip(edge[1][2]);

		McdVanillaAddBoxEndSegmentPoints(outList, *inTri->vertices[1], edge[1], invE, inRBox, (MeReal)1e-6 * scale);
	}

	if(inTri->flags&kMcdTriangleUseEdge2)
	{
		MeVector3 invE;
		invE[0] = MeSafeRecip(edge[2][0]);
		invE[1] = MeSafeRecip(edge[2][1]);
		invE[2] = MeSafeRecip(edge[2][2]);

		McdVanillaAddBoxEndSegmentPoints(outList, *inTri->vertices[2], edge[2], invE, inRBox, (MeReal)1e-6 * scale);
	}

	MeVector3 axb[3];
	MeVector3Cross(axb[0], *inTri->vertices[0], *inTri->vertices[1]);
	MeVector3Cross(axb[1], *inTri->vertices[1], *inTri->vertices[2]);
	MeVector3Cross(axb[2], *inTri->vertices[2], *inTri->vertices[0]);

	const MeReal triD = MeVector3Dot(*inTri->vertices[0], *inTri->normal);

	if ((*inTri->normal)[0] != 0)  // x-direction edges
		McdVanillaAddTriBoxSegmentPoints(outList,inRBox,0,1,2,inTri,edge,axb,triD);

	if ((*inTri->normal)[1] != 0)  // y-direction edges
		McdVanillaAddTriBoxSegmentPoints(outList,inRBox,1,2,0,inTri,edge,axb,triD);

	if ((*inTri->normal)[2] != 0)  // z-direction edges
		McdVanillaAddTriBoxSegmentPoints(outList,inRBox,2,0,1,inTri,edge,axb,triD);

	unguard;
}

static bool McdVanillaOverlapOBBTri_13(MeReal &outSep,
										   MeVector3 outN, MeReal &outPN, MeVector3* &outPos, MeI16 &outDims,
										   const MeReal inEps, const MeVector3 inR, const McdUserTriangle *inTri, const MeVector3 edge[3],
										   const MeReal scale)
{
	guard(McdVanillaOverlapOBBTri_13);

	int i;
	int j;

	const MeReal eps = inEps<0?0:inEps;
	const MeReal eps2 = eps*eps;
	MeReal strictSep;

	// Find maximum separation (early-out for positive separation)
	MeReal nRLen = 1;

	MeVector3 aNorm;
	aNorm[0] = MeFabs( (*inTri->normal)[0] );
	aNorm[1] = MeFabs( (*inTri->normal)[1] );
	aNorm[2] = MeFabs( (*inTri->normal)[2] );

	/*
	normInfo = (inClientInfo<<2) | thisClientInfo;
	xInfo = { 0,1,2 => axis #, 3 => other info gives normal }
	If both thisClientInfo and inClientInfo are less than 3,
	then normal is cross product of two axes.
	*/

	// Face separation (face of triangle):
	MeReal sumR = MeVector3Dot(inR, aNorm);
	MeReal normD = MeVector3Dot(*inTri->normal, *inTri->vertices[0]);

	MeReal aNormD = MeFabs(normD);
	MeReal maxSeparation = aNormD-sumR;
	MeReal PN = -aNormD;
	MeReal normalSign = normD > 0 ? -1.0f : 1.0f;
	MeU8 normInfo = 3;

	if (maxSeparation > inEps)
		return false;

	if (!(inTri->flags & kMcdTriangleTwoSided) && normalSign < 0)
	{
		normalSign = 1;
		maxSeparation = -maxSeparation - 2*sumR;
	}

	strictSep = maxSeparation * 0.3f;

	/* calculate the default separating plane */
	MeVector3Copy(outN, *inTri->normal);
	MeVector3Scale(outN, normalSign);

	outPN = MeVector3Dot(outN, *inTri->vertices[0]);

	outDims = (2<<8)|((MeI16)(aNorm[0] < (MeReal)(1.0e-4))+
		(MeI16)(aNorm[1] < (MeReal)(1.0e-4))+
		(MeI16)(aNorm[2] < (MeReal)(1.0e-4)));

	// Edge lengths squared
	MeReal sqE[3];
	sqE[0] = MeVector3Dot(edge[0], edge[0]);
	sqE[1] = MeVector3Dot(edge[1], edge[1]);
	sqE[2] = MeVector3Dot(edge[2], edge[2]);

	// Face separation (face of OBB):
	for (i = 0; i < 3; i++) 
	{
		if (MeFabs((*inTri->normal)[i]) > 1-inEps)
		{
			// already handled by triangle normal
			continue;
		}

		MeReal minCoord, maxCoord;
		McdUserTriangleGetExtent( inTri, edge, sqE, i, &minCoord, &maxCoord );

		MeReal r12 = -inR[i] - maxCoord;
		MeReal r21 = minCoord - inR[i];
		MeReal separation = MeMAX( r12, r21 );

		// strict test the separation if triangle is onesided and normal is inward
		if (!(inTri->flags & kMcdTriangleTwoSided))
		{
			MeReal d = inTri->normal[0][i];
			if ((d > 0 && r12 > r21 || d < 0 && r12 < r21) && separation < strictSep)
				continue;
		}

		if (separation > maxSeparation)
		{
			maxSeparation = separation;
			PN = -inR[i]-separation;
			normalSign = r12 > r21 ? 1.0f : -1.0f;
			normInfo = 0xC|i;
			if(separation > inEps)
				return false;
		}
	}

	MeVector3 aE[3];
	MeVector3Abs(aE[0], edge[0]);
	MeVector3Abs(aE[1], edge[1]);
	MeVector3Abs(aE[2], edge[2]);

	// Edge separation:
	for (i = 0; i < 3; i++)
	{ 
		// Triangle edges
		if ( !(inTri->flags & (1<<(i+2))) ) 
			continue;

		int i1 = NextMod3(i);
		for (j = 0; j < 3; j++)
		{ // Box axes
			int j1 = NextMod3(j);
			int j2 = NextMod3(j1);
			MeReal sR = (MeReal)(0.5)*(edge[i1][j1]*edge[i][j2] - edge[i1][j2]*edge[i][j1]);
			MeReal rB = MeFabs(sR);

			sumR = inR[j1]*aE[i][j2] + inR[j2]*aE[i][j1] + rB;
			normD = ((*inTri->vertices[i])[j1] * edge[i][j2]) - ((*inTri->vertices[i])[j2] * edge[i][j1]) + sR;

			MeReal rLen = sqE[i]-aE[i][j]*aE[i][j];
			if (rLen > eps2 * sqE[i])
			{
				rLen = MeSafeRecip(MeSqrt(rLen));
				/*
				This test is redundant, since  the conditional if (separation-maxSeparation > eps), below will eliminate
				edge collisions which are too close to being a face collision.  I'm chucking it because the test seems
				to be failing, and throwing out needed edge normals.  BRG 24Apr02
				if (MeFabs(rLen*(*(lsVec3*)inTri->normal).dot(Vec3CrossAxis(*(lsVec3*)edge[i],j))) > 1-inEps)
				{
				// already handled by triangle normal
				continue;
				}
				*/
				MeReal aNormD = MeFabs(normD)*rLen;
				MeReal separation = aNormD-sumR*rLen;

				// strict test the separation if triangle is onesided and normal is inward
				if (!(inTri->flags & kMcdTriangleTwoSided))
				{
					MeVector3 CA;
					MeVec3CrossAxis(CA, edge[i], j);

					MeReal d = MeVector3Dot(*inTri->normal, CA);
					d *= rLen;

					if (normD < 0 && d < -0.99f ||
						normD > 0 && d > 0.99f)
						continue;

					if (normD*d > eps && separation < strictSep)
						continue;

				}

				if (separation-maxSeparation > eps)
				{
					maxSeparation = separation;
					PN = rB*rLen-aNormD;
					normalSign = normD > 0 ? -1.0f : 1.0f;
					normInfo = (i<<2)|j;
					nRLen = rLen;
					if (separation > inEps)
						return false;
				}
			}
		}
	}

	outSep = maxSeparation;

	// normal points from Tri to OBB
	if ((normInfo&0xC) == 0xC)
	{ // Normal is from OBB
		MeI8 axis = normInfo&3;

		MeVectorSetZero(outN,3);
		outN[axis] = normalSign;

		outPN = -inR[axis]-maxSeparation;

		MeI16 dimB = MeFabs( (*inTri->normal)[axis] ) > (1-ME_SMALL_EPSILON) ? 2 : 0;
		outDims = (dimB<<8)|2;
	}
	else if(normInfo!=3)
	{ // Normal is from crossed edges
		if (normalSign > 0)
			MeVec3CrossAxis(outN, edge[(normInfo&0xC)>>2], normInfo&3);
		else
			MeAxisCrossVec3(outN, normInfo&3, edge[(normInfo&0xC)>>2]);

		outPN = PN;
		outDims = (1<<8)|1;
	
		MeVector3Scale(outN, nRLen);
	}

	/*
	all the contacts are generated by intersections with the
	triangle, so if the plane is the minimal separating distance,
	they'll all have penetration zero. So we hack PN. But this is
	only a good idea if the minimal separating plane really is the
	triangle face, otherwise it generates artifically large contacts
	if we hit the triangle edge-on.
	*/
	if(normInfo==3)
		outPN -= maxSeparation;


	MeVector3 *posList = outPos;
	McdVanillaBoxTriIntersect(outPos,inR,inTri,edge,scale);

	return outPos != posList;

	unguard;
}

/*
* Box to Tri List collision detection
*/

MeBool KBoxTriangleListIntersect( McdModelPair* p, McdIntersectResult *result ) 
{
	guard(KBoxTriangleListIntersect);

	MeVector3 boxCentre;
	MeMatrix4 triToBox;
	MeVector3 boxPosTrans, vector[4];
	McdUserTriangle ct;
	MeVector3 edge[3];
	MeI16 dims;
	MeVector3 footprint[18], *verts, normal;
	MeReal separation, PN, boxRadius;
	McdUserTriangle *triNE;
	int count;
	int j = 0;

	McdBoxID boxgeom = (McdBoxID)McdModelGetGeometry(p->model1);
	McdTriangleListID trilistgeom = (McdTriangleListID)McdModelGetGeometry(p->model2);

	const MeReal eps = McdModelGetContactTolerance( p->model1 ) + McdModelGetContactTolerance( p->model2 );

	// Box's global information
	MeMatrix4Ptr boxTM = McdModelGetTransformPtr(p->model1);
	MeMatrix4Ptr triListTM = McdModelGetTransformPtr(p->model2);

	McdFramework *fwk = p->model1->frame;

	result->contactCount = 0;
	result->touch = 0;

	MeReal* boxRadiiTmp = McdBoxGetRadii( boxgeom );
	MeVector3 boxRadii;
	boxRadii[0] = boxRadiiTmp[0];
	boxRadii[1] = boxRadiiTmp[1];
	boxRadii[2] = boxRadiiTmp[2];

	/* Transform box into tri list space    */
	MeMatrix4TMInverseTransform(boxPosTrans, triListTM, boxTM[3]);

	McdBoxGetBSphere(boxgeom, boxCentre, &boxRadius);
	McdTriangleList* triList = (McdTriangleList*)trilistgeom;

	triList->list = (McdUserTriangle *)MeMemoryALLOCA(triList->triangleMaxCount * sizeof(McdUserTriangle));

	count = (*triList->triangleListGenerator)(p, triList->list, boxPosTrans, boxRadius+eps, triList->triangleMaxCount);

	MeVectorSetZero(result->normal, 3);

	if(count==0)
		return 0;

	ct.normal = (MeVector3*)&vector[0];
	ct.vertices[0] = (MeVector3*)&vector[1];
	ct.vertices[1] = (MeVector3*)&vector[2];
	ct.vertices[2] = (MeVector3*)&vector[3];

	McdContact *c = result->contacts;

	MeMatrix4 InvBoxTM;
	MeMatrix4Copy(InvBoxTM, boxTM);
	MeMatrix4TMInvert(InvBoxTM);
	
	MeMatrix4MultiplyMatrix(triToBox, triListTM, InvBoxTM);

	for(j=0; j<count; j++)
	{
		triNE = triList->list+j;

		/* Map vertices back into collision then box space  */
		MeMatrix4TMTransform(*ct.vertices[0], triToBox, *triNE->vertices[0]);
		MeMatrix4TMTransform(*ct.vertices[1], triToBox, *triNE->vertices[1]);
		MeMatrix4TMTransform(*ct.vertices[2], triToBox, *triNE->vertices[2]);
		MeMatrix4TMRotate(*ct.normal, triToBox, *triNE->normal);

		ct.flags = triNE->flags;
		ct.triangleData = triNE->triangleData;

		/* Edges */
		MeVector3Subtract(edge[0], *ct.vertices[1], *ct.vertices[0]);
		MeVector3Subtract(edge[1], *ct.vertices[2], *ct.vertices[1]);
		MeVector3Subtract(edge[2], *ct.vertices[0], *ct.vertices[2]);

		//verts = footprint;
		//McdVanillaOverlapOBBTri_13(separation,normal,PN,verts,dims,eps,boxRadii,&ct,edge,fwk->mScale);

		verts = footprint;

		if ( McdVanillaOverlapOBBTri_13(separation,normal,PN,verts,dims,eps,boxRadii,&ct,edge,fwk->mScale) )
		{
			// Transform normal into global frame
			MeVector3 globalNormal;

			MeMatrix4TMRotate(globalNormal, boxTM, normal);
			
			// Transform footprint into global frame, and find depths
			MeVector3* v = footprint;
			while (v != verts && result->contactCount < 400) 
			{
				MeMatrix4TMTransform(c->position, boxTM, *v);

				MeVector3Copy(c->normal, globalNormal);

				c->dims = dims;
				c->separation = separation;

				/* Copy user data into McdContact */
				c->element2.ptr = triNE->triangleData.ptr;

				c++;
				v++;
				result->contactCount++;
			}

			MeVector3Add(result->normal, result->normal, globalNormal);
		}
	}
	if (result->contactCount > 0)
	{
		MeVector3Normalize(result->normal);
		result->touch = 1;
	}
	else
		result->touch = 0;   

	return result->touch;

	unguard;
}

#endif // WITH_KARMA
