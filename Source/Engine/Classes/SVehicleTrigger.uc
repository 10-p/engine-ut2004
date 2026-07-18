//=============================================================================
// SVehicleTrigger
//=============================================================================
// General trigger to possess vehicles
// This trigger is automatically spawned by the vehicle and cannot be placed
// in levels.
//=============================================================================

class SVehicleTrigger extends Triggers
	notplaceable
	native;

var()	bool	bEnabled;
var		bool	BACKUP_bEnabled; // Backup
var		bool	bMarkWithPath;

var NavigationPoint myMarker;

cpptext
{
	virtual UBOOL ReachedBy(APawn * P, FVector Loc);
}

function PostBeginPlay()
{
	super.PostBeginPlay();

	BACKUP_bEnabled = bEnabled;
}

event Trigger( Actor Other, Pawn EventInstigator )
{
	bEnabled = !bEnabled;
}

function UsedBy( Pawn user )
{
	if ( !bEnabled )
		return;

	Vehicle(Owner).TryToDrive( User );
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	super.Reset();

	bEnabled = BACKUP_bEnabled;
}

//=============================================================================
// defaultproperties
//=============================================================================

defaultproperties
{
	bEnabled=true
	bHardAttach=true
    bHidden=true
	bCollideActors=false
    bStatic=false
    CollisionRadius=+0080.000000
	CollisionHeight=+0400.000000
	bCollideWhenPlacing=false
	bOnlyAffectPawns=true
    RemoteRole=ROLE_None
}
