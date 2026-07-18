/*============================================================================
	Karma Debug Drawing
    
    - Draw models (collision geoemtry, centre-of-mass etc.)
    - Draw constraints (contacts, axis, limits etc.)
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA

#include "KDebugDraw.h"

#pragma warning (disable:4305)

#define CRAZY_COLOR (1)

const MeReal debugAxisSize = 0.5; // ME scale
const MeReal contactScale = 40;

const MeReal contColor[3]       = {1, 0, 0};
const MeReal cont2BodyColor[3]  = {0.5, 0.5, 1};
const MeReal collColor[3]       = {0, 1, 0};
const MeReal COMColor[3]        = {1, 0, 1};
const MeReal axisColor[3]       = {1, 1, 1};
const MeReal hiLimColor[3]      = {1, 0, 0};
const MeReal loLimColor[3]      = {0, 0, 1};
const MeReal currPosColor[3]    = {1, 0, 1};
const MeReal dirColor[3]        = {1, 1, 0};

/* KTODO: This could _CERTAINLY_ be done more efficiently!! */
const MeVector3 sphereDraw[24][2] = 
{
    {{0, 0, -1},            {0, 0.7071, -0.7071}},
    {{0, 0.7071, -0.7071},  {0, 1, 0}},
    {{0, 1, 0},             {0, 0.7071, 0.7071}},
    {{0, 0.7071, 0.7071},   {0, 0, 1}},
    {{0, 0, 1},             {0, -0.7071, 0.7071}},
    {{0, -0.7071, 0.7071},  {0, -1, 0}},
    {{0, -1, 0},            {0, -0.7071, -0.7071}},
    {{0, -0.7071, -0.7071}, {0, 0, -1}},

    {{0, 0, -1},            {0.7071, 0, -0.7071}},
    {{0.7071, 0, -0.7071},  {1, 0, 0}},
    {{1, 0, 0},             {0.7071, 0, 0.7071}},
    {{0.7071, 0, 0.7071},   {0, 0, 1}},
    {{0, 0, 1},             {-0.7071, 0, 0.7071}},
    {{-0.7071, 0, 0.7071},  {-1, 0, 0}},
    {{-1, 0, 0},            {-0.7071, 0, -0.7071}},
    {{-0.7071, 0, -0.7071}, {0, 0, -1}},

    {{0, -1, 0},            {0.7071, -0.7071, 0}},
    {{0.7071, -0.7071, 0},  {1, 0, 0}},
    {{1, 0, 0},             {0.7071, 0.7071, 0}},
    {{0.7071, 0.7071, 0},   {0, 1, 0}},
    {{0, 1, 0},             {-0.7071, 0.7071, 0}},
    {{-0.7071, 0.7071, 0},  {-1, 0, 0}},
    {{-1, 0, 0},            {-0.7071, -0.7071, 0}},
    {{-0.7071, -0.7071, 0}, {0, -1, 0}}
};

const MeVector3 boxDraw[12][2] =
{
    {{1, -1, -1},    {1, 1, -1}},
    {{1, 1, -1},     {1, 1, 1}},
    {{1, 1, 1},      {1, -1, 1}},
    {{1, -1, 1},     {1, -1, -1}},

    {{-1, -1, -1},   {-1, 1, -1}},
    {{-1, 1, -1},    {-1, 1, 1}},
    {{-1, 1, 1},     {-1, -1, 1}},
    {{-1, -1, 1},    {-1, -1, -1}},

    {{-1, -1, -1},   {1, -1, -1}},
    {{-1, 1, -1},    {1, 1, -1}},
    {{-1, 1, 1},     {1, 1, 1}},
    {{-1, -1, 1},    {1, -1, 1}}
};

const MeVector3 cylDraw[20][2] =
{
    {{0, -1, -1},            {0.7071, -0.7071, -1}},
    {{0.7071, -0.7071, -1},  {1, 0, -1}},
    {{1, 0, -1},             {0.7071, 0.7071, -1}},
    {{0.7071, 0.7071, -1},   {0, 1, -1}},
    {{0, 1, -1},             {-0.7071, 0.7071, -1}},
    {{-0.7071, 0.7071, -1},  {-1, 0, -1}},
    {{-1, 0, -1},            {-0.7071, -0.7071, -1}},
    {{-0.7071, -0.7071, -1}, {0, -1, -1}},

    {{0, -1, 1},            {0.7071, -0.7071, 1}},
    {{0.7071, -0.7071, 1},  {1, 0, 1}},
    {{1, 0, 1},             {0.7071, 0.7071, 1}},
    {{0.7071, 0.7071, 1},   {0, 1, 1}},
    {{0, 1, 1},             {-0.7071, 0.7071, 1}},
    {{-0.7071, 0.7071, 1},  {-1, 0, 1}},
    {{-1, 0, 1},            {-0.7071, -0.7071, 1}},
    {{-0.7071, -0.7071, 1}, {0, -1, 1}},
    
    {{0, -1, -1},   {0, -1, 1}},
    {{0, 1, -1},    {0, 1, 1}},
    {{1, 0, -1},    {1, 0, 1}},
    {{-1, 0, -1},   {-1, 0, 1}}
};

#pragma warning (default:4305)


void KSphereDraw(const McdSphereID sphere, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KSphereDraw);
    if(McdGeometryGetTypeId(sphere) != kMcdGeometryTypeSphere)
        return;
    
    int i;
    MeReal radius = McdSphereGetRadius(sphere);

    for(i=0; i<24; i++)
    {
        MeVector4 lv1, lv2, wv1, wv2;

        MeVector3Copy(lv1, sphereDraw[i][0]);
        MeVector3Scale(lv1, radius);
        lv1[3] = 1;
        MeMatrix4MultiplyVector(wv1, tm, lv1);

        MeVector3Copy(lv2, sphereDraw[i][1]);
        MeVector3Scale(lv2, radius);
        lv2[3] = 1;
        MeMatrix4MultiplyVector(wv2, tm, lv2);

        drawFn(wv1, wv2, collColor[0], collColor[1], collColor[2]);
    }
    unguard;
}

void KSphylDraw(const McdSphereID sphyl, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KSphylDraw);
    if(McdGeometryGetTypeId(sphyl) != kMcdGeometryTypeSphyl)
        return;
    
    int i;
    MeReal radius = McdSphylGetRadius(sphyl);
    MeReal halfH = (MeReal)0.5 * McdSphylGetHeight(sphyl);
    MeVector4 lv1, lv2, wv1, wv2;


	// Draw the two hemisphere ends

	// -Z end
    for(i=0; i<24; i++)
    {
		if(sphereDraw[i][0][2] > 0 || sphereDraw[i][1][2] > 0)
			continue;

        MeVector3Copy(lv1, sphereDraw[i][0]);
        MeVector3Scale(lv1, radius);
        lv1[3] = 1;
		lv1[2] -= halfH;
        MeMatrix4MultiplyVector(wv1, tm, lv1);

        MeVector3Copy(lv2, sphereDraw[i][1]);
        MeVector3Scale(lv2, radius);
        lv2[3] = 1;
		lv2[2] -= halfH;
        MeMatrix4MultiplyVector(wv2, tm, lv2);

        drawFn(wv1, wv2, collColor[0], collColor[1], collColor[2]);
    }

	// Cylinder section lines.
	for(i=16; i<20; i++)
	{
        MeVector3Copy(wv1, tm[3]);
        MeVector3MultiplyAdd(wv1, cylDraw[i][0][0] * radius, tm[0]);
        MeVector3MultiplyAdd(wv1, cylDraw[i][0][1] * radius, tm[1]);
        MeVector3MultiplyAdd(wv1, cylDraw[i][0][2] * halfH, tm[2]);
    
        MeVector3Copy(wv2, tm[3]);
        MeVector3MultiplyAdd(wv2, cylDraw[i][1][0] * radius, tm[0]);
        MeVector3MultiplyAdd(wv2, cylDraw[i][1][1] * radius, tm[1]);
        MeVector3MultiplyAdd(wv2, cylDraw[i][1][2] * halfH, tm[2]);

        drawFn(wv1, wv2, collColor[0], collColor[1], collColor[2]);
	}

	// +Z end
    for(i=0; i<24; i++)
    {
		if(sphereDraw[i][0][2] < 0 || sphereDraw[i][1][2] < 0)
			continue;

        MeVector3Copy(lv1, sphereDraw[i][0]);
        MeVector3Scale(lv1, radius);
        lv1[3] = 1;
		lv1[2] += halfH;
        MeMatrix4MultiplyVector(wv1, tm, lv1);

        MeVector3Copy(lv2, sphereDraw[i][1]);
        MeVector3Scale(lv2, radius);
        lv2[3] = 1;
		lv2[2] += halfH;
        MeMatrix4MultiplyVector(wv2, tm, lv2);

        drawFn(wv1, wv2, collColor[0], collColor[1], collColor[2]);
    }

    unguard;
}

void KBoxDraw(const McdBoxID box, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KBoxDraw);
    if(McdGeometryGetTypeId(box) != kMcdGeometryTypeBox)
        return;

    int i;
    MeVector3 rads;
    MeVector3 v1, v2;

    McdBoxGetDimensions(box, &rads[0], &rads[1], &rads[2]);
    MeVector3Scale(rads, 0.5); /* change from lengths to radii */
    
    for(i=0; i<12; i++)
    {
        MeVector3Copy(v1, tm[3]);
        MeVector3MultiplyAdd(v1, boxDraw[i][0][0] * rads[0], tm[0]);
        MeVector3MultiplyAdd(v1, boxDraw[i][0][1] * rads[1], tm[1]);
        MeVector3MultiplyAdd(v1, boxDraw[i][0][2] * rads[2], tm[2]);
    
        MeVector3Copy(v2, tm[3]);
        MeVector3MultiplyAdd(v2, boxDraw[i][1][0] * rads[0], tm[0]);
        MeVector3MultiplyAdd(v2, boxDraw[i][1][1] * rads[1], tm[1]);
        MeVector3MultiplyAdd(v2, boxDraw[i][1][2] * rads[2], tm[2]);

        drawFn(v1, v2, collColor[0], collColor[1], collColor[2]);
    }
    unguard;
}

void KCylinderDraw(const McdCylinderID cyl, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KCylinderDraw);
    if(McdGeometryGetTypeId(cyl) != kMcdGeometryTypeCylinder)
        return;

    int i;
    MeReal rad, height;
    MeVector3 v1, v2;

    height = 0.5 * McdCylinderGetHeight(cyl);
    rad = McdCylinderGetRadius(cyl);

    for(i=0; i<20; i++)
    {
        MeVector3Copy(v1, tm[3]);
        MeVector3MultiplyAdd(v1, cylDraw[i][0][0] * rad, tm[0]);
        MeVector3MultiplyAdd(v1, cylDraw[i][0][1] * rad, tm[1]);
        MeVector3MultiplyAdd(v1, cylDraw[i][0][2] * height, tm[2]);
    
        MeVector3Copy(v2, tm[3]);
        MeVector3MultiplyAdd(v2, cylDraw[i][1][0] * rad, tm[0]);
        MeVector3MultiplyAdd(v2, cylDraw[i][1][1] * rad, tm[1]);
        MeVector3MultiplyAdd(v2, cylDraw[i][1][2] * height, tm[2]);

        drawFn(v1, v2, collColor[0], collColor[1], collColor[2]);
    }
    unguard;
}

void KConvexDraw(const McdConvexMeshID convex, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KConvexDraw);
    if(McdGeometryGetTypeId(convex) != kMcdGeometryTypeConvexMesh)
        return;

    int i;
    for(i=0; i<McdConvexMeshGetPolygonCount(convex); i++)
    {
        int j, vcount = McdConvexMeshGetPolygonVertexCount(convex, i);
        for(j=0; j<vcount;j++)
        {
            MeVector4 lv1, lv2, wv1, wv2;
            const MeReal* v1 = McdConvexMeshGetPolygonVertexPtr(convex, i, j);
            const MeReal* v2 = McdConvexMeshGetPolygonVertexPtr(convex, i, (j+1)%vcount);
            
            MeVector3Copy(lv1, v1);
            lv1[3] = 1;
            MeVector3Copy(lv2, v2);
            lv2[3] = 1;

            MeMatrix4MultiplyVector(wv1, tm, lv1);
            MeMatrix4MultiplyVector(wv2, tm, lv2);

#if CRAZY_COLOR
			FColor crazy((PTRINT)convex);
            drawFn(wv1, wv2, crazy.R/255.0f, crazy.G/255.0f, crazy.B/255.0f);
#else
            drawFn(wv1, wv2, collColor[0], collColor[1], collColor[2]);
#endif
        }
    }
    unguard;
}

void KAggregateDraw(const McdAggregateID agg, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KAggregateDraw);
    if(McdGeometryGetTypeId(agg) != kMcdGeometryTypeAggregate)
        return;
    
    int i;
    for(i=0; i<McdAggregateGetElementCount(agg); i++)
    {
        McdGeometryID instgeom = McdAggregateGetElementGeometry(agg, i);
        MeMatrix4Ptr insttm = McdAggregateGetElementTransformPtr(agg, i);
        MeMatrix4 totaltm;

        MeMatrix4MultiplyMatrix(totaltm, insttm, tm);

        KGeometryDraw(instgeom, totaltm, drawFn);
    }
    unguard;
}

void KGeometryDraw(const McdGeometryID geom, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn)
{
    guard(KGeometryDraw);
    switch(McdGeometryGetTypeId(geom))
    {
        case kMcdGeometryTypeSphere:
            KSphereDraw(geom, tm, drawFn);
            break;
        
        case kMcdGeometryTypeBox:
            KBoxDraw(geom, tm, drawFn);
            break;
        
        case kMcdGeometryTypeCylinder:
            KCylinderDraw(geom, tm, drawFn);
            break;
        
        case kMcdGeometryTypeConvexMesh:
            KConvexDraw(geom, tm, drawFn);
            break;    
        
		case kMcdGeometryTypeSphyl:
			KSphylDraw(geom, tm, drawFn);
			break;

        case kMcdGeometryTypeAggregate:
            KAggregateDraw(geom, tm, drawFn);
            break;

        case kMcdGeometryTypeNull:
            break;
        
        default:
            MeWarning(0, "KModelDraw: Don't know how to draw this geometry type!\n");
            break;
    }
    unguard;
}

void KModelDraw(const McdModelID model, int opt, const MeDebugLineFuncPtr drawFn)
{
    guard(KModelDraw);
    if(opt & KDRAW_Collision)
    {
        McdGeometryID geom = McdModelGetGeometry(model);
        MeMatrix4Ptr tm = McdModelGetTransformPtr(model);
        
        KGeometryDraw(geom, tm, drawFn);
    }

    if(opt & KDRAW_COM)
    {
        MdtBodyID body = McdModelGetBody(model);
        if(body)
        {
            MeVector3 start, end;
            int i;

            for(i=0; i<3; i++)
            {
                MeVector3Copy(start, body->comTM[3]);
                MeVector3MultiplyAdd(start, (MeReal)0.5 * debugAxisSize, body->comTM[i]);
                MeVector3Copy(end, body->comTM[3]);
                MeVector3MultiplyAdd(end, (MeReal)-0.5 * debugAxisSize, body->comTM[i]);
                drawFn(start, end, COMColor[0], COMColor[1], COMColor[2]);
            }
        }
    }

	if(opt & KDRAW_Origin)
	{
		MeMatrix4Ptr tm = McdModelGetTransformPtr(model);
		MeVector3 start, end;
        int i;
		MeReal axisColors[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

        MeVector3Copy(start, tm[3]);

		for(i=0; i<3; i++)
        {
            MeVector3Copy(end, start);
            MeVector3MultiplyAdd(end, (MeReal)0.5 * debugAxisSize, tm[i]);
            drawFn(start, end, axisColors[i][0], axisColors[i][1], axisColors[i][2]);
        }
	}
    unguard;
}

void KConstraintDraw(const MdtConstraintID con, int opt, 
                     const MeDebugLineFuncPtr drawFn)
{
    guard(KConstraintDraw);
    MdtBSJointID bs = MdtConstraintDCastBSJoint(con);
    if(bs && (opt & KDRAW_Axis))
    {
        guard(DrawBSJoint);
        MeVector3 pos, v1, v2, x={1, 0, 0}, z={0, 0, 1};
        MdtConstraintBodyGetPosition(con, 1, pos);
        MeVector3Copy(v1, pos);
        MeVector3MultiplyAdd(v1, (MeReal)0.5 * debugAxisSize, x);
        MeVector3Copy(v2, pos);
        MeVector3MultiplyAdd(v2, (MeReal)-0.5 * debugAxisSize, x);
        drawFn(v1, v2, axisColor[0], axisColor[1], axisColor[2]);
        
        MeVector3Copy(v1, pos);
        MeVector3MultiplyAdd(v1, (MeReal)0.5 * debugAxisSize, z);
        MeVector3Copy(v2, pos);
        MeVector3MultiplyAdd(v2, (MeReal)-0.5 * debugAxisSize, z);
        drawFn(v1, v2, axisColor[0], axisColor[1], axisColor[2]);

#if 1
		MeReal tmpColor[3] = {0.5, 0.5, 1};
		MdtConstraintBodyGetPosition(con, 0, pos);
        MeVector3Copy(v1, pos);
        MeVector3MultiplyAdd(v1, (MeReal)0.5 * debugAxisSize, x);
        MeVector3Copy(v2, pos);
        MeVector3MultiplyAdd(v2, (MeReal)-0.5 * debugAxisSize, x);
        drawFn(v1, v2, tmpColor[0], tmpColor[1], tmpColor[2]);
        
        MeVector3Copy(v1, pos);
        MeVector3MultiplyAdd(v1, (MeReal)0.5 * debugAxisSize, z);
        MeVector3Copy(v2, pos);
        MeVector3MultiplyAdd(v2, (MeReal)-0.5 * debugAxisSize, z);
        drawFn(v1, v2, tmpColor[0], tmpColor[1], tmpColor[2]);
#endif
        unguard;
    }
    
    MdtHingeID hinge = MdtConstraintDCastHinge(con);
    if(hinge)
    {
        guard(DrawHinge);

        MeVector3 hpos, haxis, v1, v2;
        MdtHingeGetPosition(hinge, hpos);
        MdtHingeGetAxis(hinge, haxis);
        
        if(opt & KDRAW_Axis)
        {
            MeVector3Copy(v1, hpos);
            MeVector3MultiplyAdd(v1, (MeReal)0.5 * debugAxisSize, haxis);
            MeVector3Copy(v2, hpos);
            MeVector3MultiplyAdd(v2, (MeReal)-0.5 * debugAxisSize, haxis);
            drawFn(v1, v2, axisColor[0], axisColor[1], axisColor[2]);
        }

        /* Only draw limits if they are turned on. */
        MdtLimitID l = MdtHingeGetLimit(hinge);
        if(opt & KDRAW_Limits && MdtLimitIsActive(l))
        {
            /* Hinge limits - v2 is orthogonal to hinge axis. */
            MeVector4 q;
            MeVector3 lim, currpos, tmp;
            MdtConstraintBodyGetAxes(con, 1, v1, v2);
            MdtConstraintBodyGetAxes(con, 0, tmp, currpos);
            MeVector3Scale(v2, debugAxisSize);
            MeVector3Scale(currpos, debugAxisSize);
            MeVector3Add(currpos, hpos, currpos);
            drawFn(hpos, currpos, currPosColor[0], currPosColor[1], currPosColor[2]); /* Current position */
            
            MeQuaternionMake(q, v1, MdtSingleLimitGetStop(MdtLimitGetUpperLimit(l)));
            MeQuaternionRotateVector3(lim, q, v2);
            MeVector3Add(lim, hpos, lim);
            drawFn(hpos, lim, hiLimColor[0], hiLimColor[1], hiLimColor[2]); /* upper limit */
            
            MeQuaternionMake(q, v1, MdtSingleLimitGetStop(MdtLimitGetLowerLimit(l)));
            MeQuaternionRotateVector3(lim, q, v2);
            MeVector3Add(lim, hpos, lim);
            drawFn(hpos, lim, loLimColor[0], loLimColor[1], loLimColor[2]); /* lower limit */
        }
        unguard;
    }

    MdtCarWheelID cw = MdtConstraintDCastCarWheel(con);
    if(cw && (opt & KDRAW_Limits))
    {
        guard(DrawCarWheel);
        MeVector3 cpos, wpos, hinge, steer, start, end, x = {1, 0, 0};
		MdtConstraintBodyGetPosition(con, 0, cpos); // We want the position in the chassis ref frame.
        //MdtCarWheelGetPosition(cw, cpos);
        MdtCarWheelGetSteeringAxis(cw, steer);
        MdtCarWheelGetHingeAxis(cw, hinge);
        MeReal low = MdtCarWheelGetSuspensionLowLimit(cw);
        MeReal high = MdtCarWheelGetSuspensionHighLimit(cw);

        /* Draw suspension line */
        MeVector3Copy(start, cpos);
        MeVector3MultiplyAdd(start, low, steer);
        MeVector3Copy(end, cpos);
        MeVector3MultiplyAdd(end, high, steer);
        drawFn(start, end, axisColor[0], axisColor[1], axisColor[2]);

        /* Draw top, bottom and connection point ticks */
        MeVector3Copy(start, cpos);
        MeVector3MultiplyAdd(start, debugAxisSize*0.5, x);
        MeVector3Copy(end, cpos);
        MeVector3MultiplyAdd(end, -debugAxisSize*0.5, x);
        drawFn(start, end, currPosColor[0], currPosColor[1], currPosColor[2]);
        
        MeVector3Copy(start, cpos);
        MeVector3MultiplyAdd(start, high, steer);
        MeVector3MultiplyAdd(start, debugAxisSize*0.5, x);
        MeVector3Copy(end, cpos);
        MeVector3MultiplyAdd(end, high, steer);
        MeVector3MultiplyAdd(end, -debugAxisSize*0.5, x);
        drawFn(start, end, hiLimColor[0], hiLimColor[1], hiLimColor[2]);
        
        MeVector3Copy(start, cpos);
        MeVector3MultiplyAdd(start, low, steer);
        MeVector3MultiplyAdd(start, debugAxisSize*0.5, x);
        MeVector3Copy(end, cpos);
        MeVector3MultiplyAdd(end, low, steer);
        MeVector3MultiplyAdd(end, -debugAxisSize*0.5, x);
        drawFn(start, end, loLimColor[0], loLimColor[1], loLimColor[2]);

        /* Draw hinge axis */
        MdtBodyID wheel = MdtCarWheelGetBody(cw, 1); /* first body chassis, second body wheel */
        MdtBodyGetPosition(wheel, wpos);

        MeVector3Copy(start, wpos);
        MeVector3MultiplyAdd(start, debugAxisSize*0.5, hinge);
        MeVector3Copy(end, wpos);
        MeVector3MultiplyAdd(end, -debugAxisSize*0.5, hinge);
        drawFn(start, end, 1, 1, 1);
        unguard;
    }

	MdtContactGroupID congroup = MdtConstraintDCastContactGroup(con);
	if(congroup && KGData->DebugDrawOpt & KDRAW_Contacts)
	{
        guard(DrawContactGroup);

		MdtContactID contact = MdtContactGroupGetFirstContact(congroup);
		while(contact)
		{
			KConstraintDraw(MdtContactQuaConstraint(contact), opt, drawFn);
			contact = MdtContactGroupGetNextContact(congroup, contact);
		}

		unguard;
	}

    MdtContactID contact = MdtConstraintDCastContact(con);
    if(contact && KGData->DebugDrawOpt & KDRAW_Contacts)
    {
        guard(DrawContact);
        MeVector3 pos, normal, end;

        /* Debug contact line drawing */
        MdtContactGetPosition(contact, pos);
        MdtContactGetNormal(contact, normal);
        
        MeVector3Scale(normal, contactScale * MdtContactGetPenetration(contact));
        MeVector3Add(end, normal, pos);
        
        drawFn(pos, end, contColor[0], contColor[1], contColor[2]);
        
        /* Draw line from each body origin to the contact. */
        //MdtBodyID body = MdtContactGetBody(contact, 0);
        //drawFn(body->comTM[3], pos, cont2BodyColor[0], cont2BodyColor[1], cont2BodyColor[2]);
        
        //body = MdtContactGetBody(contact, 1);
        //if(body)
        //    drawFn(body->comTM[3], pos, cont2BodyColor[0], cont2BodyColor[1], cont2BodyColor[2]);

        /* Draw contact direction if used */
        if(contact->params.options & MdtBclContactOptionUseDirection)
        {
            MeVector3 dir;
            MdtContactGetDirection(contact, dir);
            MeVector3Copy(end, pos);
            MeVector3MultiplyAdd(end, debugAxisSize, dir);
            drawFn(end, pos, dirColor[0], dirColor[1], dirColor[2]);
        }
        unguard;
    }
	
	MdtConeLimitID cl = MdtConstraintDCastConeLimit(con);
    if(cl && (opt & KDRAW_Limits))
    {
		guard(DrawConeLimit);
		MeVector3 pos, p, o, o2, axis1, axis2, end, last;	
		MeReal s =  debugAxisSize; // scale		
		MeReal angle = MdtConeLimitGetConeHalfAngle(cl);
		MdtConeLimitGetAxes(cl, p, o);
		MeVector3Cross(o2, p, o);
		MdtConstraintGetPosition((MdtConstraintID)cl,pos);
		
		MdtConeLimitBodyGetAxes(cl, 0, axis1, axis2);
		MeVector3Copy(end, pos);
		MeVector3MultiplyAdd(end, 2 * s, axis1);
		drawFn(pos, end, 1, 0, 1);
	
		for(int i = 0; i < 31; i++)
		{
			MeVector3Copy(end, pos);
			MeVector3MultiplyAdd(end, s, p);
			MeVector3MultiplyAdd(end, s * sin(angle) * cos(i * 2 * 3.14159 / 30.0), o);
			MeVector3MultiplyAdd(end, s * sin(angle) * sin(i * 2 * 3.14159/ 30.0), o2);
			drawFn(pos, end, 0, 1, 1);
			if(i)
				drawFn(last, end, 0, 1, 1);			
			MeVector3Copy(last, end);
		}
		unguard;
	}

    unguard;
}

void UKMeshProps::Draw(FRenderInterface* RI, INT ShowFlags)
{
	guard(UKMeshProps::Draw);

	FLineBatcher LineBatcher(RI);
	int i, j;
	
	if(ShowFlags & SHOW_KarmaPrimitives)
	{
		for(i=0; i<AggGeom.BoxElems.Num(); i++)
		{
			FKBoxElem* boxElem = &AggGeom.BoxElems(i);
			FVector rads = 0.5 * FVector(boxElem->X, boxElem->Y, boxElem->Z);
			FMatrix tempTM = boxElem->TM;
			tempTM.M[3][0] *= K_ME2UScale;
			tempTM.M[3][1] *= K_ME2UScale;
			tempTM.M[3][2] *= K_ME2UScale;

			for(j=0; j<12; j++)
			{
				FVector boxDraw1, boxDraw2;

				KME2UVecCopy(&boxDraw1, boxDraw[j][0]);
				boxDraw1 *= (rads * K_ME2UScale);

				KME2UVecCopy(&boxDraw2, boxDraw[j][1]);
				boxDraw2 *= (rads * K_ME2UScale);

				LineBatcher.DrawLine(
					tempTM.TransformFVector(boxDraw1), 
					tempTM.TransformFVector(boxDraw2), 
					FColor(255*collColor[0], 255*collColor[1], 255*collColor[2])
					);
			}
		}

		for(i=0; i<AggGeom.SphereElems.Num(); i++)
		{
			FKSphereElem* sphereElem = &AggGeom.SphereElems(i);
			FMatrix tempTM = sphereElem->TM;
			tempTM.M[3][0] *= K_ME2UScale;
			tempTM.M[3][1] *= K_ME2UScale;
			tempTM.M[3][2] *= K_ME2UScale;

			for(j=0; j<24; j++)
			{
				FVector sphereDraw1, sphereDraw2;

				KME2UVecCopy(&sphereDraw1, sphereDraw[j][0]);
				sphereDraw1 *= (sphereElem->Radius * K_ME2UScale);

				KME2UVecCopy(&sphereDraw2, sphereDraw[j][1]);
				sphereDraw2 *= (sphereElem->Radius * K_ME2UScale);

				LineBatcher.DrawLine(
					tempTM.TransformFVector(sphereDraw1), 
					tempTM.TransformFVector(sphereDraw2), 
					FColor(255*collColor[0], 255*collColor[1], 255*collColor[2])
					);
			}
		}

		for(i=0; i<AggGeom.CylinderElems.Num(); i++)
		{
			FKCylinderElem* cylElem = &AggGeom.CylinderElems(i);
			FVector rads = FVector(cylElem->Radius, cylElem->Radius, 0.5 * cylElem->Height);
			FMatrix tempTM = cylElem->TM;
			tempTM.M[3][0] *= K_ME2UScale;
			tempTM.M[3][1] *= K_ME2UScale;
			tempTM.M[3][2] *= K_ME2UScale;

			for(j=0; j<20; j++)
			{
				FVector cylDraw1, cylDraw2;

				KME2UVecCopy(&cylDraw1, cylDraw[j][0]);
				cylDraw1 *= (rads * K_ME2UScale);

				KME2UVecCopy(&cylDraw2, cylDraw[j][1]);
				cylDraw2 *= (rads * K_ME2UScale);

				LineBatcher.DrawLine(
					tempTM.TransformFVector(cylDraw1), 
					tempTM.TransformFVector(cylDraw2), 
					FColor(255*collColor[0], 255*collColor[1], 255*collColor[2])
					);
			}

		}

		// Currently, because we dont have any connectiviy information, we can only draw convex vertices.
		for(i=0; i<AggGeom.ConvexElems.Num(); i++)
		{
			FKConvexElem* conElem = &AggGeom.ConvexElems(i);
			FMatrix tempTM = conElem->TM;
			tempTM.M[3][0] *= K_ME2UScale;
			tempTM.M[3][1] *= K_ME2UScale;
			tempTM.M[3][2] *= K_ME2UScale;

#if CRAZY_COLOR
			FColor vertColor = FColor((PTRINT)conElem);
#else
			FLOAT colorScale = 1.0f - (i/AggGeom.ConvexElems.Num() * 0.5f);
			FColor vertColor = FColor(255.f*colorScale*collColor[0], 255.f*colorScale*collColor[1], 255.f*colorScale*collColor[2]);
#endif

			// If all we have are the vertices.
			if(conElem->PlaneData.Num() == 0)
			{
				for(INT j=0; j<conElem->VertexData.Num(); j++)
				{
					FVector conDraw;
					conDraw = conElem->VertexData(j) * K_ME2UScale;

					// DrawPoint needs a SceneNode, which we dont have, so we just draw a little line.
					LineBatcher.DrawLine(
						tempTM.TransformFVector( conDraw - FVector(5, 0, 0) ), 
						tempTM.TransformFVector( conDraw + FVector(5, 0, 0) ), 
						vertColor );

					LineBatcher.DrawLine(
						tempTM.TransformFVector( conDraw - FVector(0, 0, 5) ), 
						tempTM.TransformFVector( conDraw + FVector(0, 0, 5) ), 
						vertColor );
				}
			}
			// If we have the (temporary) plane data as well.
			else
			{
				for(INT k=0; k<conElem->PlaneData.Num(); k++)
				{
					FPoly	Polygon;
					FVector Base, AxisX, AxisY;

					Polygon.Normal = conElem->PlaneData(k);
					Polygon.NumVertices = 4;
					Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

					Base = conElem->PlaneData(k) * conElem->PlaneData(k).W;

					Polygon.Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
					Polygon.Vertex[1] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
					Polygon.Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
					Polygon.Vertex[3] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;

					for(INT j=0; j<conElem->PlaneData.Num(); j++)
					{
						if(k != j)
						{
							if(!Polygon.Split(-FVector(conElem->PlaneData(j)), conElem->PlaneData(j) * conElem->PlaneData(j).W))
							{
								Polygon.NumVertices = 0;
								break;
							}
						}
					}


					// Draw
					for(INT VertexIndex = 0;VertexIndex < Polygon.NumVertices;VertexIndex++)
					{
						LineBatcher.DrawLine(
							tempTM.TransformFVector(Polygon.Vertex[VertexIndex]),
							tempTM.TransformFVector(Polygon.Vertex[(VertexIndex + 1) % Polygon.NumVertices]),
							vertColor);
					}
				}
			}
		}
	}

	if(ShowFlags & SHOW_KarmaMassProps)
	{
		// Draw centre of mass position as a star
		FVector UComPos = COMOffset*K_ME2UScale;
		FLOAT lineLength = (MeReal)0.5 * debugAxisSize * K_ME2UScale;
		FColor lineColor = FColor(255*COMColor[0], 255*COMColor[1], 255*COMColor[2]);

		LineBatcher.DrawLine(
			UComPos + (lineLength * FVector(1, 0, 0)), 
			UComPos - (lineLength * FVector(1, 0, 0)), 
			lineColor);

		LineBatcher.DrawLine(
			UComPos + (lineLength * FVector(0, 1, 0)), 
			UComPos - (lineLength * FVector(0, 1, 0)), 
			lineColor);

		LineBatcher.DrawLine(
			UComPos + (lineLength * FVector(0, 0, 1)), 
			UComPos - (lineLength * FVector(0, 0, 1)), 
			lineColor);

		// Draw inertia tensor as equivalent sized box.
		// JTODO: This currently ignores off-diagnol tensor elements.
		FVector InertiaRadii;
		InertiaRadii.X = K_ME2UScale * appSqrt( (1.5f)*(InertiaTensor[3]+InertiaTensor[5]-InertiaTensor[0]) );
		InertiaRadii.Y = K_ME2UScale * appSqrt( (1.5f)*(InertiaTensor[0]+InertiaTensor[5]-InertiaTensor[3]) );
		InertiaRadii.Z = K_ME2UScale * appSqrt( (1.5f)*(InertiaTensor[0]+InertiaTensor[3]-InertiaTensor[5]) );   

		for(j=0; j<12; j++)
		{
			FVector boxDraw1, boxDraw2;

			KME2UVecCopy(&boxDraw1, boxDraw[j][0]);
			boxDraw1 *= InertiaRadii;
			boxDraw1 += UComPos;

			KME2UVecCopy(&boxDraw2, boxDraw[j][1]);
			boxDraw2 *= InertiaRadii;
			boxDraw2 += UComPos;

			LineBatcher.DrawLine(boxDraw1, boxDraw2, lineColor);
		}
	}

	unguard;
}

#endif // WITH_KARMA

