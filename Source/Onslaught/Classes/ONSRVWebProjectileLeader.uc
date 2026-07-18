class ONSRVWebProjectileLeader extends  ONSRVWebProjectile
	native
	nativereplication;

var() float			SpringLength;
var() float			SpringStiffness;
var() float			StuckSpringStiffness; //when a projectile in this web gets stuck, SpringStiffness is set to this
var() float			SpringDamping;
var() float			SpringMaxForce;
var() float			SpringExplodeLength; // Max stretch length before web explodes.
var() float			ProjVelDamping; //
var() float			ProjStuckNeighbourVelDamping; // Extra damping applied when a neighbour is attached to something.
var() InterpCurve	ProjGravityScale; // Function of time since launched.

// To make the web stuff particularly effective against Mantas, it is sucked into the top of the fans.
var() bool						bEnableSuckTargetForce;
var() bool						bSymmetricSuckTarget; // Suck to SuckTargetOffset mirrored across Y as well (eg. for manta fans)
var() bool						bSuckFriendlyActor; // Don't get sucked towards actors on own team.
var() bool						bNoSuckFromBelow; // If the projectile is 'below' the suck target (ie. negative local Z) it won't get sucked.
var() bool						bOnlySuckToDriven; // Only suck to an actor if bDriving is true.
var() array< class<Vehicle> >	SuckTargetClasses; // Class of actors that projectile should get sucked towards.
var() float						SuckTargetRange; // Distance from SuckTarget before projectile starts getting sucked.
var() float						SuckTargetForce; // Force applied to suck projectile towards target.
var() vector					SuckTargetOffset; // Location in target ref frame that projectile will be sucked too
var() float						SuckReduceVelFactor; // Once a particle is getting sucked, how much to kill velocity that is not in the suck direction.

var byte						ProjTeam;
var float						FireTime;
var	array<ONSRVWebProjectile>	Projectiles;


cpptext
{
	INT* GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel);
	void TryPreAllProjectileTick(FLOAT DeltaSeconds);
	void UpdateBeams(FLOAT DeltaSeconds);
	void ApplySpringForces(FLOAT DeltaSeconds);
}

replication
{
	reliable if (bNetInitial && Role == ROLE_Authority)
		ProjTeam;
}

simulated function PostBeginPlay()
{
	Super.PostBeginPlay();

	FireTime = Level.TimeSeconds;
}

// Walk over entire web, detonating projectiles. Only called on server.
event DetonateWeb()
{
	local int i;

	// We want to destroy ourself last!
	for(i=1; i<Projectiles.Length; i++)
	{
		if( Projectiles[i] != None )
			Projectiles[i].Explode( Projectiles[i].Location, Projectiles[i].StuckNormal );
	}

	self.Explode( self.Location, self.StuckNormal );
}

//one of the projectiles just got stuck to something
simulated function NotifyStuck()
{
	SpringStiffness = StuckSpringStiffness;
}

defaultproperties
{
	SpringStiffness=5.0
	StuckSpringStiffness=50.0
	SpringDamping=6.0
	SpringLength=50.0
	SpringMaxForce=4500.0
	SpringExplodeLength=1250.0
	ProjVelDamping=0.0
	ProjStuckNeighbourVelDamping=2.0
	ProjGravityScale=(Points=((InVal=0.0,OutVal=0.0),(InVal=4.0,OutVal=0.0),(InVal=5.0,OutVal=0.5),(InVal=1000000.0,OutVal=0.5)))

	bEnableSuckTargetForce=true
	bSymmetricSuckTarget=true
	bSuckFriendlyActor=false
	bOnlySuckToDriven=true
	bNoSuckFromBelow=false
	SuckTargetClasses(0)=class'Onslaught.ONSHoverBike'
	SuckTargetRange=275.0
	SuckTargetForce=6500.0
	SuckTargetOffset=(X=25,Y=80,Z=-10)
	SuckReduceVelFactor=0.9
}
