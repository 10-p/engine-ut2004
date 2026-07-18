//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSAttackCraft extends ONSChopperCraft
    placeable;

#exec OBJ LOAD FILE=..\textures\VMVehicles-TX.utx

var()   float							MaxPitchSpeed;

var()   array<vector>					TrailEffectPositions;
var     class<ONSAttackCraftExhaust>	TrailEffectClass;
var     array<ONSAttackCraftExhaust>	TrailEffects;

var()	array<vector>					StreamerEffectOffset;
var     class<ONSAttackCraftStreamer>	StreamerEffectClass;
var		array<ONSAttackCraftStreamer>	StreamerEffect;

var()	range							StreamerOpacityRamp;
var()	float							StreamerOpacityChangeRate;
var()	float							StreamerOpacityMax;
var		float							StreamerCurrentOpacity;
var		bool							StreamerActive;

// AI hint
function bool FastVehicle()
{
	return true;
}

function bool Dodge(eDoubleClickDir DoubleClickMove)
{
	if ( FRand() < 0.7 )
	{
		VehicleMovingTime = Level.TimeSeconds + 1;
		Rise = 1;
	}
	return false;
}

function KDriverEnter(Pawn P)
{
	bHeadingInitialized = False;

	Super.KDriverEnter(P);
}

simulated function ClientKDriverEnter(PlayerController PC)
{
	bHeadingInitialized = False;

	Super.ClientKDriverEnter(PC);
}

simulated function SpecialCalcBehindView(PlayerController PC, out actor ViewActor, out vector CameraLocation, out rotator CameraRotation )
{
	local vector CamLookAt, HitLocation, HitNormal, OffsetVector;
	local Actor HitActor;
    local vector x, y, z;

	if (DesiredTPCamDistance < TPCamDistance)
		TPCamDistance = FMax(DesiredTPCamDistance, TPCamDistance - CameraSpeed * (Level.TimeSeconds - LastCameraCalcTime));
	else if (DesiredTPCamDistance > TPCamDistance)
		TPCamDistance = FMin(DesiredTPCamDistance, TPCamDistance + CameraSpeed * (Level.TimeSeconds - LastCameraCalcTime));

    GetAxes(PC.Rotation, x, y, z);
	ViewActor = self;
	CamLookAt = GetCameraLocationStart() + (TPCamLookat >> Rotation) + TPCamWorldOffset;

	OffsetVector = vect(0, 0, 0);
	OffsetVector.X = -1.0 * TPCamDistance;

	CameraLocation = CamLookAt + (OffsetVector >> PC.Rotation);

	HitActor = Trace(HitLocation, HitNormal, CameraLocation, Location, true, vect(40, 40, 40));
	if ( HitActor != None
	     && (HitActor.bWorldGeometry || HitActor == GetVehicleBase() || Trace(HitLocation, HitNormal, CameraLocation, Location, false, vect(40, 40, 40)) != None) )
			CameraLocation = HitLocation;

    CameraRotation = Normalize(PC.Rotation + PC.ShakeRot);
    CameraLocation = CameraLocation + PC.ShakeOffset.X * x + PC.ShakeOffset.Y * y + PC.ShakeOffset.Z * z;
}

function Died(Controller Killer, class<DamageType> damageType, vector HitLocation)
{
    local int i;

    if(Level.NetMode != NM_DedicatedServer)
	{
    	for(i=0;i<TrailEffects.Length;i++)
        	TrailEffects[i].Destroy();
        TrailEffects.Length = 0;

		for(i=0; i<StreamerEffect.Length; i++)
			StreamerEffect[i].Destroy();
		StreamerEffect.Length = 0;
    }

	Super.Died(Killer, damageType, HitLocation);
}

simulated function Destroyed()
{
    local int i;

    if(Level.NetMode != NM_DedicatedServer)
	{
    	for(i=0;i<TrailEffects.Length;i++)
        	TrailEffects[i].Destroy();
        TrailEffects.Length = 0;

		for(i=0; i<StreamerEffect.Length; i++)
			StreamerEffect[i].Destroy();
		StreamerEffect.Length = 0;
    }

    Super.Destroyed();
}

simulated event DrivingStatusChanged()
{
	local vector RotX, RotY, RotZ;
	local int i;

	Super.DrivingStatusChanged();

    if (bDriving && Level.NetMode != NM_DedicatedServer && !bDropDetail)
	{
        GetAxes(Rotation,RotX,RotY,RotZ);

        if (TrailEffects.Length == 0)
        {
            TrailEffects.Length = TrailEffectPositions.Length;

        	for(i=0;i<TrailEffects.Length;i++)
            	if (TrailEffects[i] == None)
            	{
                	TrailEffects[i] = spawn(TrailEffectClass, self,, Location + (TrailEffectPositions[i] >> Rotation) );
                	TrailEffects[i].SetBase(self);
                    TrailEffects[i].SetRelativeRotation( rot(0,32768,0) );
                }
        }

        if (StreamerEffect.Length == 0)
        {
    		StreamerEffect.Length = StreamerEffectOffset.Length;

    		for(i=0; i<StreamerEffect.Length; i++)
        		if (StreamerEffect[i] == None)
        		{
        			StreamerEffect[i] = spawn(StreamerEffectClass, self,, Location + (StreamerEffectOffset[i] >> Rotation) );
        			StreamerEffect[i].SetBase(self);
        		}
    	}
    }
    else
    {
        if (Level.NetMode != NM_DedicatedServer)
    	{
        	for(i=0;i<TrailEffects.Length;i++)
        	   TrailEffects[i].Destroy();

        	TrailEffects.Length = 0;

    		for(i=0; i<StreamerEffect.Length; i++)
                StreamerEffect[i].Destroy();

            StreamerEffect.Length = 0;
        }
    }
}

simulated function Tick(float DeltaTime)
{
    local float EnginePitch, DesiredOpacity, DeltaOpacity, MaxOpacityChange, ThrustAmount;
	local TrailEmitter T;
	local int i;
	local vector RelVel;
	local bool NewStreamerActive, bIsBehindView;
	local PlayerController PC;

    if(Level.NetMode != NM_DedicatedServer)
	{
        EnginePitch = 64.0 + VSize(Velocity)/MaxPitchSpeed * 32.0;
        SoundPitch = FClamp(EnginePitch, 64, 96);

        RelVel = Velocity << Rotation;

        PC = Level.GetLocalPlayerController();
		if (PC != None && PC.ViewTarget == self)
			bIsBehindView = PC.bBehindView;
		else
            bIsBehindView = True;

    	// Adjust Engine FX depending on being drive/velocity
		if (!bIsBehindView)
		{
			for(i=0; i<TrailEffects.Length; i++)
				TrailEffects[i].SetThrustEnabled(false);
		}
        else
        {
			ThrustAmount = FClamp(OutputThrust, 0.0, 1.0);

			for(i=0; i<TrailEffects.Length; i++)
			{
				TrailEffects[i].SetThrustEnabled(true);
				TrailEffects[i].SetThrust(ThrustAmount);
			}
		}

		// Update streamer opacity (limit max change speed)
		DesiredOpacity = (RelVel.X - StreamerOpacityRamp.Min)/(StreamerOpacityRamp.Max - StreamerOpacityRamp.Min);
		DesiredOpacity = FClamp(DesiredOpacity, 0.0, StreamerOpacityMax);

		MaxOpacityChange = DeltaTime * StreamerOpacityChangeRate;

		DeltaOpacity = DesiredOpacity - StreamerCurrentOpacity;
		DeltaOpacity = FClamp(DeltaOpacity, -MaxOpacityChange, MaxOpacityChange);

		if(!bIsBehindView)
            StreamerCurrentOpacity = 0.0;
        else
    		StreamerCurrentOpacity += DeltaOpacity;

		if(StreamerCurrentOpacity < 0.01)
			NewStreamerActive = false;
		else
			NewStreamerActive = true;

		for(i=0; i<StreamerEffect.Length; i++)
		{
			if(NewStreamerActive)
			{
				if(!StreamerActive)
				{
					T = TrailEmitter(StreamerEffect[i].Emitters[0]);
					T.ResetTrail();
				}

				StreamerEffect[i].Emitters[0].Disabled = false;
				StreamerEffect[i].Emitters[0].Opacity = StreamerCurrentOpacity;
			}
			else
			{
				StreamerEffect[i].Emitters[0].Disabled = true;
				StreamerEffect[i].Emitters[0].Opacity = 0.0;
			}
		}

		StreamerActive = NewStreamerActive;
    }

    Super.Tick(DeltaTime);
}

function float ImpactDamageModifier()
{
    local float Multiplier;
    local vector X, Y, Z;

    GetAxes(Rotation, X, Y, Z);
    if (ImpactInfo.ImpactNorm Dot Z > 0)
        Multiplier = 1-(ImpactInfo.ImpactNorm Dot Z);
    else
        Multiplier = 1.0;

    return Super.ImpactDamageModifier() * Multiplier;
}

function bool RecommendLongRangedAttack()
{
	return true;
}

//FIXME Fix to not be specific to this class after demo
function bool PlaceExitingDriver()
{
	local int i;
	local vector tryPlace, Extent, HitLocation, HitNormal, ZOffset;

	Extent = Driver.default.CollisionRadius * vect(1,1,0);
	Extent *= 2;
	Extent.Z = Driver.default.CollisionHeight;
	ZOffset = Driver.default.CollisionHeight * vect(0,0,1);
	if (Trace(HitLocation, HitNormal, Location + (ZOffset * 6), Location, false, Extent) != None)
		return false;

	//avoid running driver over by placing in direction perpendicular to velocity
	if ( VSize(Velocity) > 100 )
	{
		tryPlace = Normal(Velocity cross vect(0,0,1)) * (CollisionRadius + Driver.default.CollisionRadius ) * 1.25 ;
		if ( FRand() < 0.5 )
			tryPlace *= -1; //randomly prefer other side
		if ( (Trace(HitLocation, HitNormal, Location + tryPlace + ZOffset, Location + ZOffset, false, Extent) == None && Driver.SetLocation(Location + tryPlace + ZOffset))
		     || (Trace(HitLocation, HitNormal, Location - tryPlace + ZOffset, Location + ZOffset, false, Extent) == None && Driver.SetLocation(Location - tryPlace + ZOffset)) )
			return true;
	}

	for( i=0; i<ExitPositions.Length; i++)
	{
		if ( ExitPositions[0].Z != 0 )
			ZOffset = Vect(0,0,1) * ExitPositions[0].Z;
		else
			ZOffset = Driver.default.CollisionHeight * vect(0,0,2);

		if ( bRelativeExitPos )
			tryPlace = Location + ( (ExitPositions[i]-ZOffset) >> Rotation) + ZOffset;
		else
			tryPlace = ExitPositions[i];

		// First, do a line check (stops us passing through things on exit).
		if ( bRelativeExitPos && Trace(HitLocation, HitNormal, tryPlace, Location + ZOffset, false, Extent) != None )
			continue;

		// Then see if we can place the player there.
		if ( !Driver.SetLocation(tryPlace) )
			continue;

		return true;
	}
	return false;
}

static function StaticPrecache(LevelInfo L)
{
    Super.StaticPrecache(L);

	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RAPTORexploded.RaptorWing');
	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RAPTORexploded.RaptorTailWing');
	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RAPTORexploded.RaptorGun');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');
	L.AddPrecacheStaticMesh(StaticMesh'WeaponStaticMesh.RocketProj');

    L.AddPrecacheMaterial(Material'AW-2004Particles.Energy.SparkHead');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.SmokePanels2');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    L.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.RaptorColorRed');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.RaptorColorBlue');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.AttackCraftNoColor');
	L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.TrailBlura');
    L.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.raptorCOLORtest');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.SmokeFragment');
}

simulated function UpdatePrecacheStaticMeshes()
{
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RAPTORexploded.RaptorWing');
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RAPTORexploded.RaptorTailWing');
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RAPTORexploded.RaptorGun');
	Level.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	Level.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');
	Level.AddPrecacheStaticMesh(StaticMesh'WeaponStaticMesh.RocketProj');

	Super.UpdatePrecacheStaticMeshes();
}

simulated function UpdatePrecacheMaterials()
{
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Energy.SparkHead');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.SmokePanels2');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    Level.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.RaptorColorRed');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.RaptorColorBlue');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.AttackCraftNoColor');
	Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.TrailBlura');
    Level.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.AttackCraftGroup.raptorCOLORtest');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.SmokeFragment');

	Super.UpdatePrecacheMaterials();
}

defaultproperties
{
	Mesh=Mesh'ONSVehicles-A.AttackCraft'

    RedSkin=Shader'VMVehicles-TX.AttackCraftGroup.AttackCraftChassisFinalRED'
    BlueSkin=Shader'VMVehicles-TX.AttackCraftGroup.AttackCraftChassisFinalBLUE'

	DriverWeapons(0)=(WeaponClass=class'Onslaught.ONSAttackCraftGun',WeaponBone=PlasmaGunAttachment);

	DestroyedVehicleMesh=StaticMesh'ONSDeadVehicles-SM.AttackCraftDead'
    DestructionEffectClass=class'Onslaught.ONSVehicleExplosionEffect'
	DisintegrationEffectClass=class'Onslaught.ONSVehDeathAttackCraft'
    DestructionLinearMomentum=(Min=50000,Max=150000)
    DestructionAngularMomentum=(Min=100,Max=300)

	Health=300
	HealthMax=300
	DriverDamageMult=0.0
	CollisionHeight=+70.0
	CollisionRadius=150.0
	ImpactDamageMult=0.0010
	RanOverDamageType=class'DamTypeAttackCraftRoadkill'
	CrushedDamageType=class'DamTypeAttackCraftPancake'

	IdleSound=sound'ONSVehicleSounds-S.AttackCraft.AttackCraftIdle'
	StartUpSound=sound'ONSVehicleSounds-S.AttackCraft.AttackCraftStartUp'
	ShutDownSound=sound'ONSVehicleSounds-S.AttackCraft.AttackCraftShutDown'
	MaxPitchSpeed=2000
	SoundVolume=160
	SoundRadius=200

	StartUpForce="AttackCraftStartUp"
	ShutDownForce="AttackCraftShutDown"

	TrailEffectPositions(0)=(X=-148,Y=-26,Z=51);
   	TrailEffectPositions(1)=(X=-148,Y=26,Z=51);
	TrailEffectClass=class'Onslaught.ONSAttackCraftExhaust'

	StreamerEffectOffset(0)=(X=-219,Y=-35,Z=57);
	StreamerEffectOffset(1)=(X=-219,Y=35,Z=57);
	StreamerEffectOffset(2)=(X=-52,Y=-24,Z=142);
	StreamerEffectOffset(3)=(X=-52,Y=24,Z=142);
	StreamerOpacityRamp=(Min=1200.000000,Max=1600.000000)
	StreamerOpacityChangeRate=1.0
	StreamerOpacityMax=0.7
	StreamerEffectClass=class'Onslaught.ONSAttackCraftStreamer'

	bShowDamageOverlay=True

	TPCamDistance=500
	TPCamLookAt=(X=0.0,Y=0.0,Z=0)
	TPCamWorldOffset=(X=0,Y=0,Z=200)

	bDrawDriverInTP=False
	bDrawMeshInFP=False
	bTurnInPlace=true
	bCanStrafe=true

	UprightStiffness=500
	UprightDamping=300

	MaxThrustForce=100.0
	LongDamping=0.05

	MaxStrafeForce=80.0
	LatDamping=0.05

	MaxRiseForce=50.0
	UpDamping=0.05

	TurnTorqueFactor=600.0
	TurnTorqueMax=200.0
	TurnDamping=50.0
	MaxYawRate=1.5

	PitchTorqueFactor=200.0
	PitchTorqueMax=35.0
	PitchDamping=20.0

	RollTorqueTurnFactor=450.0
	RollTorqueStrafeFactor=50.0
	RollTorqueMax=50.0
	RollDamping=30.0

	MaxRandForce=3.0
	RandForceInterval=0.75

	StopThreshold=100
	VehicleMass=4.0

	EntryPosition=(X=-40,Y=0,Z=0)
	EntryRadius=210.0

	ExitPositions(0)=(X=0,Y=-165,Z=100)
	ExitPositions(1)=(X=0,Y=165,Z=100)

	HeadlightCoronaOffset(0)=(X=76,Y=14,Z=-24)
	HeadlightCoronaOffset(1)=(X=76,Y=-14,Z=-24)
	HeadlightCoronaMaterial=Material'EpicParticles.flashflare1'
	HeadlightCoronaMaxSize=60

	DamagedEffectOffset=(X=-120,Y=10,Z=65)
	DamagedEffectScale=1.0

    Begin Object Class=KarmaParamsRBFull Name=KParams0
		KStartEnabled=True
		KFriction=0.5
		KLinearDamping=0.0
		KAngularDamping=0.0
		KImpactThreshold=300
		bKNonSphericalInertia=True
        bHighDetailOnly=False
        bClientOnly=False
		bKDoubleTickRate=True
		bKStayUpright=True
		bKAllowRotate=True
		KInertiaTensor(0)=1.0
		KInertiaTensor(1)=0.0
		KInertiaTensor(2)=0.0
		KInertiaTensor(3)=3.0
		KInertiaTensor(4)=0.0
		KInertiaTensor(5)=3.5
		KCOMOffset=(X=-0.25,Y=0.0,Z=0.0)
		KActorGravScale=0.0
		bDestroyOnWorldPenetrate=True
		bDoSafetime=True
        Name="KParams0"
    End Object
    KParams=KarmaParams'KParams0'
	VehiclePositionString="in a Raptor"
	VehicleNameString="Raptor"
	GroundSpeed=2000
	bDriverHoldsFlag=false
	FlagOffset=(Z=80.0)
	FlagBone=PlasmaGunAttachment
	FlagRotation=(Yaw=32768)
	bCanCarryFlag=false

	HornSounds(0)=sound'ONSVehicleSounds-S.Horn03'
	HornSounds(1)=sound'ONSVehicleSounds-S.Horn07'
	MaxDesireability=0.6
}

