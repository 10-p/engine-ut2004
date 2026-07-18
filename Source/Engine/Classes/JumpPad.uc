//=============================
// Jumppad - bounces players/bots up
// not directly placeable.  Make a subclass with appropriate sound effect etc.
//
class JumpPad extends NavigationPoint
	native;

var		vector	JumpVelocity, BACKUP_JumpVelocity;
var		Actor	JumpTarget;
var()	float	JumpZModifier;	// for tweaking Jump, if needed
var()	sound	JumpSound;

cpptext
{
	void addReachSpecs(APawn * Scout, UBOOL bOnlyChanged);
	void RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);
}

function PostBeginPlay()
{
	local NavigationPoint N;
	
	super.PostBeginPlay();

	ForEach AllActors(class'NavigationPoint', N)
		if ( (N != self) && NearSpot(N.Location) )
			N.ExtraCost += 1000;

	if ( JumpVelocity != JumpVelocity )
	{
		log(self$" has illegal jump velocity "$JumpVelocity);
		JumpVelocity = vect(0,0,0);
	}
	BACKUP_JumpVelocity = JumpVelocity;
}


/* Reset() 
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	JumpVelocity = BACKUP_JumpVelocity;
}

event Touch(Actor Other)
{
	if ( (Pawn(Other) == None) || (Other.Physics == PHYS_None) || (Vehicle(Other) != None) )
		return;

	PendingTouch = Other.PendingTouch;
	Other.PendingTouch = self;
}

event PostTouch(Actor Other)
{
	local Pawn P;

	P = Pawn(Other);
	if ( (P == None) || (P.Physics == PHYS_None) || (Vehicle(Other) != None) || (P.DrivenVehicle != None) )
		return;

	if ( AIController(P.Controller) != None )
	{
		P.Controller.Movetarget = JumpTarget;
		P.Controller.Focus = JumpTarget;
		if ( P.Physics != PHYS_Flying )
			P.Controller.MoveTimer = 2.0;
		P.DestinationOffset = JumpTarget.CollisionRadius;
	}
	if ( P.Physics == PHYS_Walking )
		P.SetPhysics(PHYS_Falling);
	P.Velocity =  JumpVelocity;
	P.Acceleration = vect(0,0,0);
	if ( JumpSound != None )
		P.PlaySound(JumpSound);

}

defaultproperties
{
	bDestinationOnly=true
	bCollideActors=true
	JumpVelocity=(x=0.0,y=0.0,z=1200.0)
	JumpZModifier=+1.0
}