#ifndef _KDEBUGDRAW_H_
#define _KDEBUGDRAW_H_

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include "MeDebugDraw.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

typedef enum
{
    /* Model drawing options */
    KDRAW_Collision = 0x0001,    /* Draw Karma collision geometry */
    KDRAW_COM       = 0x0002,	/* Draw dynamics centre-of-mass */
	KDRAW_Origin	= 0x0004,	/* Draw model/body origin */
	
    /* Constraint drawing options */
    KDRAW_Axis      = 0x0010,    /* Draw constraint axis (where appropriate) */
    KDRAW_Limits    = 0x0020,    /* Draw constraint limits (where appropriate) */

    /* Other */
    KDRAW_Contacts  = 0x0100,    /* Draw Karma contacts */
    KDRAW_Triangles = 0x0200,    /* Draw potentially-colliding triangles */

	KDRAW_WheelPhys = 0x0400,	/* Draw SVehicleWheel debug info */
	KDRAW_SafeTime  = 0x0800,	/* Draw Safetime line checks */
	KDRAW_Repulsors = 0x1000	/* Draw KRepulsors */
}
EKDebugDrawOptions;



void KSphereDraw    (const McdSphereID sphere, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);
void KSphylDraw     (const McdSphereID sphyl, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);
void KBoxDraw       (const McdBoxID box, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);
void KCylinderDraw  (const McdCylinderID cyl, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);
void KConvexDraw    (const McdConvexMeshID convex, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);
void KAggregateDraw (const McdAggregateID agg, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);
void KGeometryDraw  (const McdGeometryID geom, const MeMatrix4 tm, const MeDebugLineFuncPtr drawFn);

void KModelDraw(const McdModelID model, int opt, const MeDebugLineFuncPtr drawFn);
void KConstraintDraw(const MdtConstraintID con, int opt, const MeDebugLineFuncPtr drawFn);

#endif
