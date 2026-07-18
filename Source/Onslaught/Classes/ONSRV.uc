//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSRV extends ONSWheeledCraft;

#exec OBJ LOAD FILE=..\Animations\ONSVehicles-A.ukx
#exec OBJ LOAD FILE=..\textures\VehicleFX.utx
#exec OBJ LOAD FILE=..\textures\EpicParticles.utx
#exec OBJ LOAD FILE=..\textures\VMVehicles-TX.utx
#exec OBJ LOAD FILE=..\sounds\ONSVehicleSounds-S.uax

var bool bLeftArmBroke;
var bool bRightArmBroke;
var bool bClientLeftArmBroke;
var bool bClientRightArmBroke;

var() sound ArmExtendSound;
var() sound ArmRetractSound;
var() sound BladeBreakSound;

var string ArmExtendForce;
var string ArmRetractForce;

replication
{
    reliable if (bNetDirty && Role == ROLE_Authority)
        bClientLeftArmBroke, bClientRightArmBroke;
}

function ChooseFireAt(Actor A)
{
	if (Pawn(A) != None && Vehicle(A) == None && VSize(A.Location - Location) < 1500 && Controller.LineOfSightTo(A))
	{
		if (!bWeaponIsAltFiring)
			AltFire(0);
	}
	else if (bWeaponIsAltFiring)
		VehicleCeaseFire(true);

	Fire(0);
}

function AltFire(optional float F)
{
	//avoid sending altfire to weapon
	Super(Vehicle).AltFire(F);
}

function ClientVehicleCeaseFire(bool bWasAltFire)
{
	//avoid sending altfire to weapon
	if (bWasAltFire)
		Super(Vehicle).ClientVehicleCeaseFire(bWasAltFire);
	else
		Super.ClientVehicleCeaseFire(bWasAltFire);
}

function VehicleFire(bool bWasAltFire)
{
	if (bWasAltFire)
	{
        PlayAnim('RVArmExtend');
        if (!bLeftArmBroke || !bRightArmBroke)
        {
            PlaySound(ArmExtendSound, SLOT_None, 2.0,,,, False);
            bWeaponIsAltFiring = True;
            ClientPlayForceFeedback(ArmExtendForce);
        }
    }
	else
		Super.VehicleFire(bWasAltFire);
}

function VehicleCeaseFire(bool bWasAltFire)
{
	if (bWasAltFire)
    {
        PlayAnim('RVArmRetract');
        if (!bLeftArmBroke || !bRightArmBroke)
        {
            PlaySound(ArmRetractSound, SLOT_None, 2.0,,,, False);
    		bWeaponIsAltFiring = False;
    		ClientPlayForceFeedback(ArmRetractForce);
    	}
	}
	else
		Super.VehicleCeaseFire(bWasAltFire);
}

function Pawn CheckForHeadShot(Vector loc, Vector ray, float AdditionalScale)
{
    local vector X, Y, Z;

    GetAxes(Rotation,X,Y,Z);

    if (Driver != None && Driver.IsHeadShot(loc, ray, AdditionalScale))
        return Driver;

    return None;
}

simulated function Tick(float DT)
{
    local Coords ArmBaseCoords, ArmTipCoords;
    local vector HitLocation, HitNormal;
    local actor Victim;

    Super.Tick(DT);

    // Left Blade Arm System
    if (Role == ROLE_Authority && bWeaponIsAltFiring && !bLeftArmBroke)
    {
        ArmBaseCoords = GetBoneCoords('CarLShoulder');
        ArmTipCoords = GetBoneCoords('LeftBladeDummy');
        Victim = Trace(HitLocation, HitNormal, ArmTipCoords.Origin, ArmBaseCoords.Origin);

        if (Victim != None && Victim.bBlockActors)
        {
            if (Victim.IsA('Pawn') && !Victim.IsA('Vehicle'))
                Pawn(Victim).TakeDamage(1000, self, HitLocation, Velocity * 100, class'DamTypeONSRVBlade');
            else
            {
                bLeftArmBroke = True;
                bClientLeftArmBroke = True;
                BladeBreakOff(4, 'CarLSlider', class'ONSRVLeftBladeBreakOffEffect');
				// We use slot 4 here because slots 0-3 can be used by BigWheels mutator.
            }
        }
    }
    if (Role < ROLE_Authority && bClientLeftArmBroke)
    {
        bLeftArmBroke = True;
        bClientLeftArmBroke = False;
        BladeBreakOff(4, 'CarLSlider', class'ONSRVLeftBladeBreakOffEffect');
    }

    // Right Blade Arm System
    if (Role == ROLE_Authority && bWeaponIsAltFiring && !bRightArmBroke)
    {
        ArmBaseCoords = GetBoneCoords('CarRShoulder');
        ArmTipCoords = GetBoneCoords('RightBladeDummy');
        Victim = Trace(HitLocation, HitNormal, ArmTipCoords.Origin, ArmBaseCoords.Origin);

        if (Victim != None && Victim.bBlockActors)
        {
            if (Victim.IsA('Pawn') && !Victim.IsA('Vehicle'))
                Pawn(Victim).TakeDamage(1000, self, HitLocation, Velocity * 100, class'DamTypeONSRVBlade');
            else
            {
                bRightArmBroke = True;
                bClientRightArmBroke = True;
                BladeBreakOff(5, 'CarRSlider', class'ONSRVRightBladeBreakOffEffect');
            }
        }
    }
    if (Role < ROLE_Authority && bClientRightArmBroke)
    {
        bRightArmBroke = True;
        bClientRightArmBroke = False;
        BladeBreakOff(5, 'CarRSlider', class'ONSRVRightBladeBreakOffEffect');
    }
}

simulated function BladeBreakOff(int Slot, name BoneName, class<Emitter> BreakEffect)
{
    PlaySound(BladeBreakSound, SLOT_None, 2.0,,,, False);
    SetBoneScale(Slot, 0.0, BoneName);
    if (Level.NetMode != NM_DedicatedServer)
        spawn(BreakEffect);
}

static function StaticPrecache(LevelInfo L)
{
    Super.StaticPrecache(L);

	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RVexploded.RVgun');
	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RVexploded.RVrail');
	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RVexploded.Rvtire');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');

    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.SmokeReOrdered');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    L.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.RVcolorRED');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.NEWrvNoCOLOR');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.RVblades');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.Environments.ReflectionTexture');
    L.AddPrecacheMaterial(Material'VMWeaponsTX.RVgunGroup.RVnewGUNtex');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.MuzzleSpray');
    L.AddPrecacheMaterial(Material'VehicleFX.Particles.DustyCloud2');
    L.AddPrecacheMaterial(Material'VMParticleTextures.DirtKICKGROUP.dirtKICKTEX');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.RVcolorBlue');
    L.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    L.AddPrecacheMaterial(Material'XEffectMat.Link.link_spark_green');
}

simulated function UpdatePrecacheStaticMeshes()
{
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RVexploded.RVgun');
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RVexploded.RVrail');
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.RVexploded.Rvtire');
	Level.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	Level.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');

    Super.UpdatePrecacheStaticMeshes();
}

simulated function UpdatePrecacheMaterials()
{
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.SmokeReOrdered');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    Level.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.RVcolorRED');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.NEWrvNoCOLOR');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.RVblades');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.Environments.ReflectionTexture');
    Level.AddPrecacheMaterial(Material'VMWeaponsTX.RVgunGroup.RVnewGUNtex');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.MuzzleSpray');
    Level.AddPrecacheMaterial(Material'VehicleFX.Particles.DustyCloud2');
    Level.AddPrecacheMaterial(Material'VMParticleTextures.DirtKICKGROUP.dirtKICKTEX');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.RVGroup.RVcolorBlue');
    Level.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    Level.AddPrecacheMaterial(Material'XEffectMat.Link.link_spark_green');

	Super.UpdatePrecacheMaterials();
}

function ShouldTargetMissile(Projectile P)
{
	if ( (Health < 200) && (Bot(Controller) != None) 
		&& (Level.Game.GameDifficulty > 4 + 4*FRand())
		&& (VSize(P.Location - Location) < VSize(P.Velocity)) )
	{
		KDriverLeave(false);
		TeamUseTime = Level.TimeSeconds + 4;
	}
}

defaultproperties
{
	Mesh=Mesh'ONSVehicles-A.RV'
	VehiclePositionString="in a Scorpion"
	VehicleNameString="Scorpion"
	bReplicateAnimations=True

    RedSkin=Shader'VMVehicles-TX.RVGroup.RVChassisFinalRED'
    BlueSkin=Shader'VMVehicles-TX.RVGroup.RVChassisFinalBLUE'

	DriverWeapons(0)=(WeaponClass=class'Onslaught.ONSRVWebLauncher',WeaponBone=ChainGunAttachment);

    DestroyedVehicleMesh=StaticMesh'ONSDeadVehicles-SM.RVDead'
	DestructionEffectClass=class'Onslaught.ONSSmallVehicleExplosionEffect'
	DisintegrationEffectClass=class'Onslaught.ONSVehDeathRV'
    DestructionLinearMomentum=(Min=200000,Max=300000)
    DestructionAngularMomentum=(Min=100,Max=150)
    DisintegrationHealth=-25
	ImpactDamageMult=0.0010

    ArmExtendSound=sound'ONSVehicleSounds-S.RV.Shing1'
    ArmRetractSound=sound'ONSVehicleSounds-S.RV.Shing2'
    BladeBreakSound=sound'ONSVehicleSounds-S.RV.RVBladeBreakOff'

    ArmExtendForce="RVBladeOpen"
    ArmRetractForce="RVBladeClose"

	Health=300
	HealthMax=300
	CollisionHeight=+40.0
	CollisionRadius=+100.0
	DriverDamageMult=0.80
	bHasAltFire=false
	bSeparateTurretFocus=true
	RanOverDamageType=class'DamTypeRVRoadkill'
	CrushedDamageType=class'DamTypeRVPancake'

	DrawScale=1.0
	DrawScale3D=(X=1.0,Y=1.0,Z=1.0)

	FPCamPos=(X=15,Y=0,Z=25)
	TPCamLookat=(X=0,Y=0,Z=0)
	TPCamWorldOffset=(X=0,Y=0,Z=100)
	TPCamDistance=375

	bDoStuntInfo=true
	DaredevilThreshInAirSpin=180.0
	DaredevilThreshInAirPitch=300.0
	DaredevilThreshInAirRoll=300.0
	DaredevilThreshInAirTime=1.7
	DaredevilThreshInAirDistance=21.0

	AirTurnTorque=35.0
	AirPitchTorque=55.0
	AirPitchDamping=35.0
	AirRollTorque=35.0
	AirRollDamping=35.0

	bDrawDriverInTP=True
	bDrawMeshInFP=True
	bHasHandbrake=true
	bAllowBigWheels=true

	DrivePos=(X=2.0,Y=0.0,Z=38.0)

	MaxViewYaw=16000
	MaxViewPitch=16000

	IdleSound=sound'ONSVehicleSounds-S.RV.RVEng01'
	StartUpSound=sound'ONSVehicleSounds-S.RV.RVStart01'
	ShutDownSound=sound'ONSVehicleSounds-S.RV.RVStop01'
	EngineRPMSoundRange=9000
	SoundVolume=180
	IdleRPM=500
	RevMeterScale=4000

	StartUpForce="RVStartUp"

	SteerBoneName="SteeringWheel"
	SteerBoneAxis=AXIS_Z
	SteerBoneMaxAngle=90

	EntryPosition=(X=0,Y=0,Z=0)
	EntryRadius=160.0

	ExitPositions(0)=(X=0,Y=-165,Z=100)
	ExitPositions(1)=(X=0,Y=165,Z=100)
	ExitPositions(2)=(X=0,Y=-165,Z=-100)
	ExitPositions(3)=(X=0,Y=165,Z=-100)

	HeadlightCoronaOffset(0)=(X=86,Y=30,Z=7)
	HeadlightCoronaOffset(1)=(X=86,Y=-30,Z=7)
	HeadlightCoronaMaterial=Material'EpicParticles.flashflare1'
	HeadlightCoronaMaxSize=65

	bMakeBrakeLights=true
	BrakeLightOffset(0)=(X=-100,Y=23,Z=7)
	BrakeLightOffset(1)=(X=-100,Y=-23,Z=7)
	BrakeLightMaterial=Material'EpicParticles.flashflare1'

	HeadlightProjectorOffset=(X=90,Y=0,Z=7)
	HeadlightProjectorRotation=(Yaw=0,Pitch=-1000,Roll=0)
	HeadlightProjectorMaterial=Texture'VMVehicles-TX.RVGroup.RVProjector'
	HeadlightProjectorScale=0.3

	DamagedEffectOffset=(X=60,Y=10,Z=10)
	DamagedEffectScale=1.0

	WheelPenScale=1.2
	WheelPenOffset=0.01
	WheelSoftness=0.025
	WheelRestitution=0.1
	WheelAdhesion=0.0
	WheelLongFrictionFunc=(Points=((InVal=0,OutVal=0.0),(InVal=100.0,OutVal=1.0),(InVal=200.0,OutVal=0.9),(InVal=10000000000.0,OutVal=0.9)))
	WheelLongFrictionScale=1.1
	WheelLatFrictionScale=1.35
	WheelLongSlip=0.001
	WheelLatSlipFunc=(Points=((InVal=0.0,OutVal=0.0),(InVal=30.0,OutVal=0.009),(InVal=45.0,OutVal=0.00),(InVal=10000000000.0,OutVal=0.00)))
	WheelHandbrakeSlip=0.01
	WheelHandbrakeFriction=0.1
	WheelSuspensionTravel=15.0
	WheelSuspensionOffset=0.0
	WheelSuspensionMaxRenderTravel=15.0
	TurnDamping=35

	HandbrakeThresh=200
	FTScale=0.03
	ChassisTorqueScale=0.4

	MinBrakeFriction=4.0
	MaxBrakeTorque=20.0
	MaxSteerAngleCurve=(Points=((InVal=0,OutVal=25.0),(InVal=1500.0,OutVal=11.0),(InVal=1000000000.0,OutVal=11.0)))
	SteerSpeed=160
	StopThreshold=100
	TorqueCurve=(Points=((InVal=0,OutVal=9.0),(InVal=200,OutVal=10.0),(InVal=1500,OutVal=11.0),(InVal=2800,OutVal=0.0)))
	EngineBrakeFactor=0.0001
	EngineBrakeRPMScale=0.1
	EngineInertia=0.1
	WheelInertia=0.1

	TransRatio=0.15
	GearRatios[0]=-0.5
	GearRatios[1]=0.4
	GearRatios[2]=0.65
	GearRatios[3]=0.85
	GearRatios[4]=1.1
	ChangeUpPoint=2000
	ChangeDownPoint=1000
	LSDFactor=1.0

	VehicleMass=3.5

    Begin Object Class=KarmaParamsRBFull Name=KParams0
		KStartEnabled=True
		KFriction=0.5
		KLinearDamping=0.05
		KAngularDamping=0.05
		KImpactThreshold=700
		bKNonSphericalInertia=True
        bHighDetailOnly=False
        bClientOnly=False
		bKDoubleTickRate=True
		KInertiaTensor(0)=1.0
		KInertiaTensor(1)=0.0
		KInertiaTensor(2)=0.0
		KInertiaTensor(3)=3.0
		KInertiaTensor(4)=0.0
		KInertiaTensor(5)=3.0
		KCOMOffset=(X=-0.25,Y=0.0,Z=-0.4)
		bDestroyOnWorldPenetrate=True
		bDoSafetime=True
        Name="KParams0"
    End Object
    KParams=KarmaParams'KParams0'

	Begin Object Class=SVehicleWheel Name=RRWheel
		BoneName="tire02"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=7.0,Z=0.0)
		WheelRadius=24
		bPoweredWheel=True
		bHandbrakeWheel=True
		SteerType=VST_Fixed
		SupportBoneAxis=AXIS_X
		SupportBoneName="RRearStrut"
	End Object
	Wheels(0)=SVehicleWheel'RRWheel'

	Begin Object Class=SVehicleWheel Name=LRWheel
		BoneName="tire04"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=-7.0,Z=0.0)
		WheelRadius=24
		bPoweredWheel=True
		bHandbrakeWheel=True
		SteerType=VST_Fixed
		SupportBoneAxis=AXIS_X
		SupportBoneName="LRearStrut"
	End Object
	Wheels(1)=SVehicleWheel'LRWheel'

	Begin Object Class=SVehicleWheel Name=RFWheel
		BoneName="tire"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=7.0,Z=0.0)
		WheelRadius=24
		bPoweredWheel=True
		SteerType=VST_Steered
		SupportBoneAxis=AXIS_X
		SupportBoneName="RFrontStrut"
	End Object
	Wheels(2)=SVehicleWheel'RFWheel'

	Begin Object Class=SVehicleWheel Name=LFWheel
		BoneName="tire03"
		BoneRollAxis=AXIS_Y
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=0.0,Y=-7.0,Z=0.0)
		WheelRadius=24
		bPoweredWheel=True
		SteerType=VST_Steered
		SupportBoneAxis=AXIS_X
		SupportBoneName="LFrontStrut"
	End Object
	Wheels(3)=SVehicleWheel'LFWheel'

	GroundSpeed=940
	CenterSpringForce="SpringONSSRV"

	HornSounds(0)=sound'ONSVehicleSounds-S.Horn06'
	HornSounds(1)=sound'ONSVehicleSounds-S.Dixie_Horn'

	MaxDesireability=0.4
	ObjectiveGetOutDist=1500.0
	bCanDoTrickJumps=true
}
