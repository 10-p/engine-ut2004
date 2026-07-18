//=============================================================================
// InventorySpot.
//=============================================================================
class InventorySpot extends SmallNavigationPoint
	native;

cpptext
{
	virtual UBOOL IsIdentifiedAs(FName ActorName);
    virtual AInventorySpot* GetAInventorySpot() { return this; } 
}

var Pickup markedItem;
var xPickupBase myPickupBase;

var bool bSuperPickup; // set in xPickupBase PostBeginPlay()

/* GetMoveTargetFor()
Possibly return pickup rather than self as movetarget
*/
function Actor GetMoveTargetFor(AIController B, float MaxWait)
{
	if ( (markedItem != None) && markedItem.ReadyToPickup(MaxWait) && (B.Desireability(markedItem) > 0) )
		return markedItem;
	
	return self;
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
event float DetourWeight(Pawn Other,float PathWeight)
{
	if ( (markedItem != None) && markedItem.ReadyToPickup(0) )
		return markedItem.DetourWeight(Other,PathWeight);
}	

defaultproperties
{
     bCollideWhenPlacing=False
	 bHiddenEd=true
}
