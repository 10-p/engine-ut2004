//=============================================================================
// Event.
//=============================================================================
class Triggers extends Actor
	abstract
	placeable
	native;

cpptext
{
	virtual UBOOL ReachedBy(APawn * P, FVector Loc);
}

defaultproperties
{
     bHidden=True
     CollisionRadius=+00040.000000
     CollisionHeight=+00040.000000
     bCollideActors=True
     RemoteRole=ROLE_None
}
