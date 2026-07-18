//=============================================================================
// PathNode.
//=============================================================================
class PathNode extends NavigationPoint
	placeable
	native;

cpptext
{
	virtual UBOOL ReviewPath(APawn* Scout);
	virtual void CheckSymmetry(ANavigationPoint* Other);
	virtual INT AddMyMarker(AActor *S);
}

defaultproperties
{
     Texture=S_Pickup
     SoundVolume=128
}
