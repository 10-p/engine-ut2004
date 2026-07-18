//=============================================================================
// StaticMeshActor.
// An actor that is drawn using a static mesh(a mesh that never changes, and
// can be cached in video memory, resulting in a speed boost).
//=============================================================================

class StaticMeshActor extends Actor
	native
	placeable;

var() bool bExactProjectileCollision;		// nonzero extent projectiles should shrink to zero when hitting this actor

defaultproperties
{
	DrawType=DT_StaticMesh
	bEdShouldSnap=True
	bStatic=True
	bStaticLighting=True
	bShadowCast=True
	bCollideActors=True
	bBlockActors=True
	bBlockKarma=True
	bWorldGeometry=True
    CollisionHeight=+000001.000000
	CollisionRadius=+000001.000000
	bAcceptsProjectors=True
	bExactProjectileCollision=true
     bUseDynamicLights=true
}