class ONSAttackCraftMissle extends Projectile;

var Emitter		TrailEmitter;
var class<Emitter>	TrailClass;

var float AccelRate;

var Vehicle HomingTarget;

var vector InitialDir;

replication
{
	reliable if (bNetInitial && Role==ROLE_Authority)
		HomingTarget;
}

simulated function Destroyed()
{
	if ( TrailEmitter != None )
		TrailEmitter.Destroy();
	if (Role == ROLE_Authority && HomingTarget != None)
		HomingTarget.NotifyEnemyLostLock();

	Super.Destroyed();
}

simulated function PostBeginPlay()
{
	super.PostBeginPlay();

	InitialDir = vector(Rotation);
	Velocity = InitialDir * Speed;

	if ( PhysicsVolume.bWaterVolume )
		Velocity = 0.6 * Velocity;

	if (Level.NetMode != NM_DedicatedServer)
	{
		TrailEmitter = Spawn(TrailClass, self,, Location - 15 * InitialDir);
		TrailEmitter.SetBase(self);
	}

	SetTimer(0.1, true);
}

simulated function PostNetBeginPlay()
{
	Super.PostNetBeginPlay();

	Acceleration = Normal(Velocity) * AccelRate;
}

function SetHomingTarget(Vehicle NewTarget)
{
	if (HomingTarget != None)
		HomingTarget.NotifyEnemyLostLock();

	HomingTarget = NewTarget;
	if (HomingTarget != None)
		HomingTarget.NotifyEnemyLockedOn();
}

simulated function Timer()
{
	local float VelMag;
	local vector ForceDir;

	if (HomingTarget == None)
		return;

	ForceDir = Normal(HomingTarget.Location - Location);
	if (ForceDir dot InitialDir > 0)
	{
	    	// Do normal guidance to target.
	    	VelMag = VSize(Velocity);

	    	ForceDir = Normal(ForceDir * 0.7 * VelMag + Velocity);
		Velocity =  VelMag * ForceDir;
    		Acceleration = Normal(Velocity) * AccelRate;

	    	// Update rocket so it faces in the direction its going.
		SetRotation(rotator(Velocity));
	}
}

simulated function Landed( vector HitNormal )
{
	Explode(Location,HitNormal);
}

simulated function ProcessTouch (Actor Other, Vector HitLocation)
{
	if ( (Other != instigator) && (!Other.IsA('Projectile') || Other.bProjTarget) )
		Explode(HitLocation,Vect(0,0,1));
}

simulated function Explode(vector HitLocation, vector HitNormal)
{
	local PlayerController PC;

	PlaySound(sound'WeaponSounds.BExplosion3',, 2.5*TransientSoundVolume);

	if ( TrailEmitter != None )
	{
		TrailEmitter.Kill();
		TrailEmitter = None;
	}

    if ( EffectIsRelevant(Location,false) )
    {
    	Spawn(class'NewExplosionA',,,HitLocation + HitNormal*16,rotator(HitNormal));
		PC = Level.GetLocalPlayerController();
		if ( (PC.ViewTarget != None) && VSize(PC.ViewTarget.Location - Location) < 5000 )
			Spawn(class'ExplosionCrap',,, HitLocation, rotator(HitNormal));

		if ( (ExplosionDecal != None) && (Level.NetMode != NM_DedicatedServer) )
			Spawn(ExplosionDecal,self,,Location, rotator(-HitNormal));
    }

	BlowUp(HitLocation+HitNormal*2.f);
	Destroy();
}

function BlowUp(vector HitLocation)
{
	HurtRadius(Damage, DamageRadius, MyDamageType, MomentumTransfer, Location );
	MakeNoise(1.0);
}

defaultproperties
{
    TrailClass=class'ONSAvrilSmokeTrail'

    speed=2000.0
    MaxSpeed=4000.0
    AccelRate=2000.0
    LifeSpan=10.0

    Damage=100.0
    DamageRadius=150.0

    MomentumTransfer=50000
    MyDamageType=class'DamTypeAttackCraftMissle'
    ExplosionDecal=class'ONSRocketScorch'
    RemoteRole=ROLE_SimulatedProxy
    bCollideWorld=true

    AmbientSound=Sound'WeaponSounds.RocketLauncher.RocketLauncherProjectile'
    SoundVolume=255
    SoundRadius=100

    DrawType=DT_StaticMesh
    StaticMesh=StaticMesh'WeaponStaticMesh.RocketProj'
    DrawScale=0.5
    AmbientGlow=32
    bUnlit=true

    LightType=LT_None
    bDynamicLight=false

    bBounce=false
    bFixedRotationDir=True
    RotationRate=(Roll=50000)
    DesiredRotation=(Roll=30000)

    ForceType=FT_Constant
    ForceScale=5.0
    ForceRadius=100.0
}
