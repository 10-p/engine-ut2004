//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSHoverTank extends ONSTreadCraft;

#exec OBJ LOAD FILE=..\Animations\ONSVehicles-A.ukx
#exec OBJ LOAD FILE=..\Sounds\ONSVehicleSounds-S.uax
#exec OBJ LOAD FILE=InterfaceContent.utx
#exec OBJ LOAD FILE=..\textures\VMVehicles-TX.utx

var()   float   MaxPitchSpeed;
var VariableTexPanner LeftTreadPanner, RightTreadPanner;
var float TreadVelocityScale;
var float MaxGroundSpeed, MaxAirSpeed;

simulated function PostBeginPlay()
{
	super.PostBeginPlay();

	if ( Level.NetMode != NM_DedicatedServer )
		SetupTreads();
}

simulated function Destroyed()
{
	DestroyTreads();
	super.Destroyed();
}

function bool ImportantVehicle()
{
	return true;
}

simulated function SetupTreads()
{
	LeftTreadPanner = VariableTexPanner(Level.ObjectPool.AllocateObject(class'VariableTexPanner'));
	if ( LeftTreadPanner != None )
	{
		LeftTreadPanner.Material = Skins[1];
		LeftTreadPanner.PanDirection = rot(0, 16384, 0);
		LeftTreadPanner.PanRate = 0.0;
		Skins[1] = LeftTreadPanner;
	}
	RightTreadPanner = VariableTexPanner(Level.ObjectPool.AllocateObject(class'VariableTexPanner'));
	if ( RightTreadPanner != None )
	{
		RightTreadPanner.Material = Skins[2];
		RightTreadPanner.PanDirection = rot(0, 16384, 0);
		RightTreadPanner.PanRate = 0.0;
		Skins[2] = RightTreadPanner;
	}
}

simulated function DestroyTreads()
{
	if ( LeftTreadPanner != None )
	{
		Level.ObjectPool.FreeObject(LeftTreadPanner);
		LeftTreadPanner = None;
	}
	if ( RightTreadPanner != None )
	{
		Level.ObjectPool.FreeObject(RightTreadPanner);
		RightTreadPanner = None;
	}
}

simulated event DrivingStatusChanged()
{
    Super.DrivingStatusChanged();

    if (!bDriving)
    {
        if ( LeftTreadPanner != None )
            LeftTreadPanner.PanRate = 0.0;

        if ( RightTreadPanner != None )
            RightTreadPanner.PanRate = 0.0;
    }
}

simulated function Tick(float DeltaTime)
{
    local float EnginePitch;
	local float LinTurnSpeed;
    local KRigidBodyState BodyState;
    local KarmaParams KP;
    local bool bOnGround;
    local int i;

    KGetRigidBodyState(BodyState);

	KP = KarmaParams(KParams);

	// Increase max karma speed if falling
	bOnGround = false;
	for(i=0; i<KP.Repulsors.Length; i++)
	{
        //log("Checking Repulsor "$i);
		if( KP.Repulsors[i] != None && KP.Repulsors[i].bRepulsorInContact )
			bOnGround = true;
		//log("bOnGround: "$bOnGround);
	}

	if (bOnGround)
	   KP.kMaxSpeed = MaxGroundSpeed;
	else
	   KP.kMaxSpeed = MaxAirSpeed;

	if ( Level.NetMode != NM_DedicatedServer )
	{
		LinTurnSpeed = 0.5 * BodyState.AngVel.Z;
		EnginePitch = 64.0 + VSize(Velocity)/MaxPitchSpeed * 64.0;
		SoundPitch = FClamp(EnginePitch, 64, 128);

		if ( LeftTreadPanner != None )
		{
			LeftTreadPanner.PanRate = VSize(Velocity) / TreadVelocityScale;
			if (Velocity Dot Vector(Rotation) > 0)
				LeftTreadPanner.PanRate = -1 * LeftTreadPanner.PanRate;
			LeftTreadPanner.PanRate += LinTurnSpeed;
		}

		if ( RightTreadPanner != None )
		{
			RightTreadPanner.PanRate = VSize(Velocity) / TreadVelocityScale;
			if (Velocity Dot Vector(Rotation) > 0)
				RightTreadPanner.PanRate = -1 * RightTreadPanner.PanRate;
			RightTreadPanner.PanRate -= LinTurnSpeed;
		}
	}

    Super.Tick( DeltaTime );
}


function KDriverEnter(Pawn p)
{
    Super.KDriverEnter(p);

    SVehicleUpdateParams();
}

function DriverLeft()
{
    Super.DriverLeft();

    SVehicleUpdateParams();
}

function AltFire(optional float F)
{
	local PlayerController PC;

	PC = PlayerController(Controller);
	if (PC == None)
		return;

	bWeaponIsAltFiring = true;
	PC.ToggleZoomWithMax(0.5);
}

function ClientVehicleCeaseFire(bool bWasAltFire)
{
	local PlayerController PC;

	if (!bWasAltFire)
	{
		Super.ClientVehicleCeaseFire(bWasAltFire);
		return;
	}

	PC = PlayerController(Controller);
	if (PC == None)
		return;

	bWeaponIsAltFiring = false;
	PC.StopZoom();
}

simulated function ClientKDriverLeave(PlayerController PC)
{
	Super.ClientKDriverLeave(PC);

	bWeaponIsAltFiring = false;
	PC.EndZoom();
}

function bool RecommendLongRangedAttack()
{
	return true;
}

function TakeDamage(int Damage, Pawn instigatedBy, Vector Hitlocation, Vector Momentum, class<DamageType> DamageType)
{
	if (DamageType == class'DamTypeHoverBikePlasma')
		Damage *= 0.80;

	Super.TakeDamage(Damage, instigatedBy, Hitlocation, Momentum, damageType);
}

static function StaticPrecache(LevelInfo L)
{
    Super.StaticPrecache(L);

	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.TANKexploded.TankTurret');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');
	L.AddPrecacheStaticMesh(StaticMesh'WeaponStaticMesh.RocketProj');

    L.AddPrecacheMaterial(Material'AW-2004Particles.Energy.SparkHead');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.SmokeReOrdered');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.MuchSmoke1');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    L.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.TankColorRED');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.TankColorBLUE');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.TankNoColor');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.tankTreads');
    L.AddPrecacheMaterial(Material'VMParticleTextures.EJECTA.Tex');
	L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.TrailBlur');
    L.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    L.AddPrecacheMaterial(Material'AW-2004Explosions.Fire.Fireball3');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.SmokeFragment');
}

simulated function UpdatePrecacheStaticMeshes()
{
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.TANKexploded.TankTurret');
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
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.SmokeReOrdered');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.MuchSmoke1');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    Level.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.TankColorRED');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.TankColorBLUE');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.TankNoColor');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.HoverTankGroup.tankTreads');
    Level.AddPrecacheMaterial(Material'VMParticleTextures.EJECTA.Tex');
	Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.TrailBlur');
    Level.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    Level.AddPrecacheMaterial(Material'AW-2004Explosions.Fire.Fireball3');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.SmokeFragment');

	Super.UpdatePrecacheMaterials();
}


function ShouldTargetMissile(Projectile P)
{
	if ( (WeaponPawns.Length > 0) && (WeaponPawns[0].Controller == None) )
		Super.ShouldTargetMissile(P);
}

defaultproperties
{
	Mesh=Mesh'ONSNewTank-A.HoverTank'

    RedSkin=Shader'VMVehicles-TX.HoverTankGroup.HoverTankChassisFinalRED'
    BlueSkin=Shader'VMVehicles-TX.HoverTankGroup.HoverTankChassisFinalBLUE'
    Skins(1)=Texture'VMVehicles-TX.HoverTankGroup.tankTreads'
    Skins(2)=Texture'VMVehicles-TX.HoverTankGroup.tankTreads'

	DriverWeapons(0)=(WeaponClass=class'Onslaught.ONSHoverTankCannon',WeaponBone=TankCannonWeapon)
	PassengerWeapons(0)=(WeaponPawnClass=class'ONSTankSecondaryTurretPawn',WeaponBone=MachineGunTurret)

	DestroyedVehicleMesh=StaticMesh'ONSDeadVehicles-SM.TankDead'
    DestructionEffectClass=class'Onslaught.ONSVehicleExplosionEffect'
	DisintegrationEffectClass=class'Onslaught.ONSVehDeathHoverTank'
    DestructionLinearMomentum=(Min=250000,Max=400000)
    DestructionAngularMomentum=(Min=100,Max=300)

	Health=800
	HealthMax=800
	DisintegrationHealth=-125
	CollisionHeight=+60.0
	CollisionRadius=+260.0
	bTurnInPlace=true
	bCanStrafe=true
	bHasAltFire=false
	bSeparateTurretFocus=true
	RanOverDamageType=class'DamTypeTankRoadkill'
	CrushedDamageType=class'DamTypeTankPancake'

	IdleSound=sound'ONSVehicleSounds-S.Tank.TankEng01'
	StartUpSound=sound'ONSVehicleSounds-S.Tank.TankStart01'
	ShutDownSound=sound'ONSVehicleSounds-S.Tank.TankStop01'
	SoundVolume=200
	MaxPitchSpeed=700

	StartUpForce="TankStartUp"
	ShutDownForce="TankShutDown"

	FPCamPos=(X=-70,Y=0,Z=130)
	FPCamViewOffset=(X=90,Y=0,Z=0)
	bFPNoZFromCameraPitch=True
	TPCamLookat=(X=-50,Y=0,Z=0)
	TPCamWorldOffset=(X=0,Y=0,Z=250)
	TPCamDistance=600

	bDrawDriverInTP=False
	bDrawMeshInFP=True
	bPCRelativeFPRotation=false

	MaxViewYaw=16000
	MaxViewPitch=16000

	DrivePos=(X=0.0,Y=0.0,Z=130.0)

	ExitPositions(0)=(X=0,Y=-200,Z=100)
	ExitPositions(1)=(X=0,Y=200,Z=100)

	EntryPosition=(X=0,Y=0,Z=0)
	EntryRadius=375.0

	VehiclePositionString="in a Goliath"
	VehicleNameString="Goliath"

    // Left Treads
	ThrusterOffsets(0)=(X=190,Y=145,Z=10)
	ThrusterOffsets(1)=(X=65,Y=145,Z=10)
	ThrusterOffsets(2)=(X=-20,Y=145,Z=10)
	ThrusterOffsets(3)=(X=-200,Y=145,Z=10)

	// Right Treads
	ThrusterOffsets(4)=(X=190,Y=-145,Z=10)
	ThrusterOffsets(5)=(X=65,Y=-145,Z=10)
	ThrusterOffsets(6)=(X=-20,Y=-145,Z=10)
	ThrusterOffsets(7)=(X=-200,Y=-145,Z=10)

	DamagedEffectOffset=(X=100,Y=20,Z=26)
	DamagedEffectScale=1.5

	HoverSoftness=0.05
	HoverPenScale=1.5
	HoverCheckDist=65

	UprightStiffness=500
	UprightDamping=300

	MaxThrust=65.0
	MaxSteerTorque=100.0
	ForwardDampFactor=0.1
	LateralDampFactor=0.5
    ParkingDampFactor=0.8
	SteerDampFactor=100.0
	PitchTorqueFactor=0.0
	PitchDampFactor=0.0
	BankTorqueFactor=0.0
	BankDampFactor=0.0
	TurnDampFactor=0.0

	InvertSteeringThrottleThreshold=-0.1
	VehicleMass=12.0
	MomentumMult=0.3
	DriverDamageMult=0.0

	Begin Object Class=KarmaParamsRBFull Name=KParams0
		KStartEnabled=True
		KFriction=0.5
		KLinearDamping=0
		KAngularDamping=0
		bKNonSphericalInertia=False
        bHighDetailOnly=False
        bClientOnly=False
		bKDoubleTickRate=True
		bKStayUpright=True
		bKAllowRotate=True
		kMaxSpeed=800.0
		KInertiaTensor(0)=1.3
		KInertiaTensor(1)=0.0
		KInertiaTensor(2)=0.0
		KInertiaTensor(3)=4.0
		KInertiaTensor(4)=0.0
		KInertiaTensor(5)=4.5
		KCOMOffset=(X=0.0,Y=0.0,Z=0.0)
		bDestroyOnWorldPenetrate=True
		bDoSafetime=True
        Name="KParams0"
    End Object
    KParams=KarmaParams'KParams0'

	GroundSpeed=520
	MaxGroundSpeed=800.0
	MaxAirSpeed=5000.0
	TreadVelocityScale=450
	bDriverHoldsFlag=false
	FlagBone=MachineGunTurret
	FlagRotation=(Yaw=32768)

	bEnableProximityViewShake=true
	bOnlyViewShakeIfDriven=true
	ViewShakeRadius=600.0
	ViewShakeOffsetMag=(X=0.5,Y=0.0,Z=2.0)
	ViewShakeOffsetFreq=7.0

	HornSounds(0)=sound'ONSVehicleSounds-S.Horn09'
	HornSounds(1)=sound'ONSVehicleSounds-S.Horn02'

	MaxDesireability=0.8
}
