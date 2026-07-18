//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPRV extends ONSWheeledCraft;

#exec OBJ LOAD FILE=..\Animations\ONSVehicles-A.ukx
#exec OBJ LOAD FILE=..\Sounds\ONSVehicleSounds-S.uax
#exec OBJ LOAD FILE=..\textures\VehicleFX.utx
#exec OBJ LOAD FILE=..\textures\EpicParticles.utx
#exec OBJ LOAD FILE=..\textures\VMVehicles-TX.utx
#exec OBJ LOAD FILE=..\textures\2k4Fonts.utx

var ScriptedTexture LicensePlate;
var Material        LicensePlateFallBack;
var Material        LicensePlateBackground;
var string          LicensePlateName;
var Font            LicensePlateFont;

simulated function PostBeginPlay()
{
    Super.PostBeginPlay();

    if (Level.NetMode != NM_DedicatedServer)
    {
        LicensePlate = ScriptedTexture(Level.ObjectPool.AllocateObject(class'ScriptedTexture'));
        LicensePlate.SetSize(256,128);
        LicensePlate.FallBackMaterial = LicensePlateFallBack;
        LicensePlate.Client = Self;
        Skins[3] = LicensePlate;
        LicensePlateFont = Font(DynamicLoadObject("2k4Fonts.Verdana28", class'Font'));
    }
}

simulated event RenderTexture(ScriptedTexture Tex)
{
    local int SizeX,  SizeY;
    local color BackColor, ForegroundColor, HighLightColor;

    HighLightColor.R=100;
    HighLightColor.G=100;
    HighLightColor.B=100;
    HighLightColor.A=255;

    ForegroundColor.R=25;
    ForegroundColor.G=25;
    ForegroundColor.B=25;
    ForegroundColor.A=255;

    BackColor.R=128;
    BackColor.G=128;
    BackColor.B=128;
    BackColor.A=0;

    if (bDriving)
    {
        Tex.TextSize(LicensePlateName, LicensePlateFont, SizeX, SizeY);
        Tex.DrawTile(0, 0, Tex.USize, Tex.VSize, 0, 0, Tex.USize, Tex.VSize, LicensePlateBackground, BackColor);
        Tex.DrawText(((Tex.USize - SizeX) * 0.5) - 1, 40 - 1, LicensePlateName, LicensePlateFont, HighLightColor);
        Tex.DrawText((Tex.USize - SizeX) * 0.5, 40, LicensePlateName, LicensePlateFont, ForegroundColor);
    }
    else
        Tex.DrawTile(0, 0, Tex.USize, Tex.VSize, 0, 0, Tex.USize, Tex.VSize, LicensePlateFallBack, BackColor);
}

simulated event DrivingStatusChanged()
{
    local array<String> Characters;
    local int i, AscCode;

    if (bDriving && LicensePlate != None)
    {
        LicensePlateName = Caps(PlayerReplicationInfo.PlayerName);
        if (Len(LicensePlateName) > 8)
        {
            // First try removing all brackets, braces, greater/less-thans, and parentheses
            LicensePlateName = Repl(LicensePlateName, "[", "", false);
            LicensePlateName = Repl(LicensePlateName, "]", "", false);
            LicensePlateName = Repl(LicensePlateName, "(", "", false);
            LicensePlateName = Repl(LicensePlateName, ")", "", false);
            LicensePlateName = Repl(LicensePlateName, "{", "", false);
            LicensePlateName = Repl(LicensePlateName, "}", "", false);
            LicensePlateName = Repl(LicensePlateName, "<", "", false);
            LicensePlateName = Repl(LicensePlateName, ">", "", false);

            // If still not small enough, remove symbols one by one from right to left
            if (Len(LicensePlateName) > 8)
            {
                // Split into character array this way since Split() won't do this without crashing
                for (i=0; i<Len(LicensePlateName); i++)
                    Characters[i] = Mid(LicensePlateName, i, 1);

                for (i=Characters.Length-1; i>=0; i--)
                {
                    AscCode = Asc(Characters[i]);

                    if (AscCode < 65 || AscCode > 90)
                        Characters.Remove(i, 1);

                    if (Characters.Length <= 8)
                        break;
                }

                // If still not small enough, remove vowels one by one from right to left
                if (Characters.Length > 8)
                {
                    for (i=Characters.Length-1; i>=0; i--)
                    {
                        AscCode = Asc(Characters[i]);

                        switch(AscCode)
                        {
                            case 65:    Characters.Remove(i, 1);
                                        break;
                            case 69:    Characters.Remove(i, 1);
                                        break;
                            case 73:    Characters.Remove(i, 1);
                                        break;
                            case 79:    Characters.Remove(i, 1);
                                        break;
                            case 85:    Characters.Remove(i, 1);
                                        break;
                        }

                        if (Characters.Length <= 8)
                            break;
                    }
                }

                // Rebuild the string from all our munging
                LicensePlateName = "";
                for (i=0; i<Characters.Length; i++)
                    LicensePlateName $= Characters[i];
            }

            // You can't say I didn't try, but just in case your name is insane...
            LicensePlateName = Left(LicensePlateName, 8);
        }
        LicensePlate.Revision++;
    }

    Super.DrivingStatusChanged();
}

simulated event Destroyed()
{
    if (LicensePlate != None)
    {
        LicensePlate.Client = None;
        Level.ObjectPool.FreeObject(LicensePlate);
    }

    Super.Destroyed();
}

function VehicleFire(bool bWasAltFire)
{
	ServerPlayHorn(0);
}

function float BotDesireability(Actor S, int TeamIndex, Actor Objective)
{
	local Bot B;
	local SquadAI Squad;
	local int Num;

	if ( Level.Game.JustStarted(20) && !Level.Game.IsA('ASGameInfo') )
		return Super.BotDesireability(S, TeamIndex, Objective);

	Squad = SquadAI(S);

	if (Squad.Size == 1)
	{
		if ( (Squad.Team != None) && (Squad.Team.Size == 1) && Level.Game.IsA('ASGameInfo') )
			return Super.BotDesireability(S, TeamIndex, Objective);
		return 0;
	}

	for (B = Squad.SquadMembers; B != None; B = B.NextSquadMember)
		if (Vehicle(B.Pawn) == None && (B.RouteGoal == self || B.Pawn == None || VSize(B.Pawn.Location - Location) < Squad.MaxVehicleDist(B.Pawn)))
			Num++;

	if ( Num < 2 )
		return 0;

	return Super.BotDesireability(S, TeamIndex, Objective);
}

function Vehicle FindEntryVehicle(Pawn P)
{
	local Bot B;
	local int i;

	if ( Level.Game.JustStarted(20) )
		return Super.FindEntryVehicle(P);

	B = Bot(P.Controller);
	if (B == None || WeaponPawns.length == 0 || !IsVehicleEmpty() || ((B.PlayerReplicationInfo.Team != None) && (B.PlayerReplicationInfo.Team.Size == 1) && Level.Game.IsA('ASGameInfo')) )
		return Super.FindEntryVehicle(P);

	for (i = WeaponPawns.length - 1; i >= 0; i--)
		if (WeaponPawns[i].Driver == None)
			return WeaponPawns[i];

	return Super.FindEntryVehicle(P);
}

static function StaticPrecache(LevelInfo L)
{
    Super.StaticPrecache(L);

	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.HELLbenderExploded.HellTire');
	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.HELLbenderExploded.HellDoor');
	L.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.HELLbenderExploded.HellGun');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	L.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');

    L.AddPrecacheMaterial(Material'AW-2004Particles.Energy.SparkHead');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    L.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.MuchSmoke1');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    L.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.newPRVnoColor');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.PRVcolorRED');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.PRVcolorBLUE');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.EnergyEffectMASKtex');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Energy.PowerSwirl');
    L.AddPrecacheMaterial(Material'VMWeaponsTX.ManualBaseGun.baseGunEffectcopy');
    L.AddPrecacheMaterial(Material'VehicleFX.Particles.DustyCloud2');
    L.AddPrecacheMaterial(Material'VMParticleTextures.DirtKICKGROUP.dirtKICKTEX');
    L.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.PRVtagFallBack');
    L.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.prvTAGSCRIPTED');
}

simulated function UpdatePrecacheStaticMeshes()
{
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.HELLbenderExploded.HellTire');
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.HELLbenderExploded.HellDoor');
	Level.AddPrecacheStaticMesh(StaticMesh'ONSDeadVehicles-SM.HELLbenderExploded.HellGun');
	Level.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris2');
	Level.AddPrecacheStaticMesh(StaticMesh'AW-2004Particles.Debris.Veh_Debris1');

    Super.UpdatePrecacheStaticMeshes();
}

simulated function UpdatePrecacheMaterials()
{
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Energy.SparkHead');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp2_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.exp1_frames');
    Level.AddPrecacheMaterial(Material'ExplosionTex.Framed.we1_frames');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.MuchSmoke1');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Fire.NapalmSpot');
    Level.AddPrecacheMaterial(Material'EpicParticles.Fire.SprayFire1');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.newPRVnoColor');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.PRVcolorRED');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.PRVcolorBLUE');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.EnergyEffectMASKtex');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Energy.PowerSwirl');
    Level.AddPrecacheMaterial(Material'VMWeaponsTX.ManualBaseGun.baseGunEffectcopy');
    Level.AddPrecacheMaterial(Material'VehicleFX.Particles.DustyCloud2');
    Level.AddPrecacheMaterial(Material'VMParticleTextures.DirtKICKGROUP.dirtKICKTEX');
    Level.AddPrecacheMaterial(Material'Engine.GRADIENT_Fade');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.PRVtagFallBack');
    Level.AddPrecacheMaterial(Material'VMVehicles-TX.NEWprvGroup.prvTAGSCRIPTED');

	Super.UpdatePrecacheMaterials();
}

defaultproperties
{
	Mesh=Mesh'ONSVehicles-A.PRVchassis'
	VehiclePositionString="in a HellBender"
	VehicleNameString="HellBender"

    RedSkin=Shader'VMVehicles-TX.NEWprvGroup.newPRVRedshad'
    BlueSkin=Shader'VMVehicles-TX.NEWprvGroup.newPRVshad'
    LicensePlate=Material'VMVehicles-TX.PRVTag'
    LicensePlateBackground=Material'VMVehicles-TX.PRVTagScripted'
    LicensePlateFallBack=Material'VMVehicles-TX.PRVTagFallBack'

	PassengerWeapons(0)=(WeaponPawnClass=class'Onslaught.ONSPRVSideGunPawn',WeaponBone=Dummy01);
	PassengerWeapons(1)=(WeaponPawnClass=class'Onslaught.ONSPRVRearGunPawn',WeaponBone=Dummy02);

	DestroyedVehicleMesh=StaticMesh'ONSDeadVehicles-SM.NewPRVDead'
    DestructionEffectClass=class'Onslaught.ONSVehicleExplosionEffect'
	DisintegrationEffectClass=class'Onslaught.ONSVehDeathPRV'
    DestructionLinearMomentum=(Min=250000,Max=400000)
    DestructionAngularMomentum=(Min=100,Max=150)
    DisintegrationHealth=-100
	ImpactDamageMult=0.0010

	Health=600
	HealthMax=600
	DriverDamageMult=0.4
	MomentumMult=2.0
	RanOverDamageType=class'DamTypePRVRoadkill'
	CrushedDamageType=class'DamTypePRVPancake'

	DrawScale=0.8
	DrawScale3D=(X=1.0,Y=1.0,Z=1.0)
	CollisionRadius=175.0

	FPCamPos=(X=20,Y=-40,Z=50)
	TPCamLookat=(X=0,Y=0,Z=0)
	TPCamWorldOffset=(X=0,Y=0,Z=100)
	TPCamDistance=375

	bDoStuntInfo=true
	DaredevilThreshInAirSpin=90.0
	DaredevilThreshInAirPitch=300.0
	DaredevilThreshInAirRoll=300.0
	DaredevilThreshInAirTime=1.2
	DaredevilThreshInAirDistance=17.0

	AirTurnTorque=35.0
	AirPitchTorque=55.0
	AirPitchDamping=35.0
	AirRollTorque=35.0
	AirRollDamping=35.0

	bDrawDriverInTP=True
	bDrawMeshInFP=True
	bHasHandbrake=true
	bAllowBigWheels=true

	MaxViewYaw=16000
	MaxViewPitch=16000

	DrivePos=(X=16.921,Y=-40.284,Z=65.794)
	DriveRot=(Pitch=0)

	IdleSound=sound'ONSVehicleSounds-S.PRV.PRVEng01'
	StartUpSound=sound'ONSVehicleSounds-S.PRV.PRVStart01'
	ShutDownSound=sound'ONSVehicleSounds-S.PRV.PRVStop01'
	EngineRPMSoundRange=10000
	IdleRPM=500
	RevMeterScale=4000
	SoundVolume=180
	SoundRadius=200

	StartUpForce="PRVStartUp"
	ShutDownForce="PRVShutDown"

	SteerBoneName=""
	SteerBoneAxis=AXIS_Z
	SteerBoneMaxAngle=90

	EntryPosition=(X=20,Y=-60,Z=10)
	EntryRadius=190.0

	ExitPositions(0)=(X=0,Y=-165,Z=100)
	ExitPositions(1)=(X=0,Y=165,Z=100)
	ExitPositions(2)=(X=0,Y=-165,Z=-100)
	ExitPositions(3)=(X=0,Y=165,Z=-100)

	HeadlightCoronaOffset(0)=(X=140,Y=45,Z=11)
	HeadlightCoronaOffset(1)=(X=140,Y=-45,Z=11)
	HeadlightCoronaMaterial=Material'EpicParticles.flashflare1'
	HeadlightCoronaMaxSize=100

	bMakeBrakeLights=true
	BrakeLightOffset(0)=(X=-173,Y=73,Z=30)
	BrakeLightOffset(1)=(X=-173,Y=-73,Z=30)
	BrakeLightMaterial=Material'EpicParticles.flashflare1'

	HeadlightProjectorOffset=(X=145,Y=0,Z=11)
	HeadlightProjectorRotation=(Yaw=0,Pitch=-1000,Roll=0)
	HeadlightProjectorMaterial=Texture'VMVehicles-TX.NewPRVGroup.PRVProjector'
	HeadlightProjectorScale=0.65

	DamagedEffectOffset=(X=100,Y=-10,Z=35)
	DamagedEffectScale=1.2

	WheelPenScale=1.5
	WheelPenOffset=0.01
	WheelSoftness=0.04
	WheelRestitution=0.1
	WheelAdhesion=0.0
	WheelLongFrictionFunc=(Points=((InVal=0,OutVal=0.0),(InVal=100.0,OutVal=1.0),(InVal=200.0,OutVal=0.9),(InVal=10000000000.0,OutVal=0.9)))
	WheelLongFrictionScale=1.1
	WheelLatFrictionScale=1.5
	WheelLongSlip=0.001
	WheelLatSlipFunc=(Points=((InVal=0.0,OutVal=0.0),(InVal=30.0,OutVal=0.009),(InVal=45.0,OutVal=0.00),(InVal=10000000000.0,OutVal=0.00)))

	WheelHandbrakeSlip=0.01
	WheelHandbrakeFriction=0.1
	WheelSuspensionTravel=25.0
	WheelSuspensionOffset=-10.0
	WheelSuspensionMaxRenderTravel=25.0

	TurnDamping=35

	HandbrakeThresh=200
	FTScale=0.03
	ChassisTorqueScale=0.7

	MinBrakeFriction=4.0
	MaxBrakeTorque=20.0
	MaxSteerAngleCurve=(Points=((InVal=0,OutVal=25.0),(InVal=1500.0,OutVal=8.0),(InVal=1000000000.0,OutVal=8.0)))
	SteerSpeed=110
	StopThreshold=100
	TorqueCurve=(Points=((InVal=0,OutVal=9.0),(InVal=200,OutVal=10.0),(InVal=1500,OutVal=11.0),(InVal=2500,OutVal=0.0)))
	EngineBrakeFactor=0.0001
	EngineBrakeRPMScale=0.1
	EngineInertia=0.1
	WheelInertia=0.1

	TransRatio=0.11
	GearRatios[0]=-0.5
	GearRatios[1]=0.4
	GearRatios[2]=0.65
	GearRatios[3]=0.85
	GearRatios[4]=1.1
	ChangeUpPoint=2000
	ChangeDownPoint=1000
	LSDFactor=1.0

	VehicleMass=4.0

    Begin Object Class=KarmaParamsRBFull Name=KParams0
		KStartEnabled=True
		KFriction=0.5
		KLinearDamping=0.05
		KAngularDamping=0.05
		KImpactThreshold=500
		bKNonSphericalInertia=True
        bHighDetailOnly=False
        bClientOnly=False
		bKDoubleTickRate=True
		KInertiaTensor(0)=1.0
		KInertiaTensor(1)=0.0
		KInertiaTensor(2)=0.0
		KInertiaTensor(3)=3.0
		KInertiaTensor(4)=0.0
		KInertiaTensor(5)=3.5
		KCOMOffset=(X=-0.3,Y=0.0,Z=-0.5)
		bDestroyOnWorldPenetrate=True
		bDoSafetime=True
        Name="KParams0"
    End Object
    KParams=KarmaParams'KParams0'

	Begin Object Class=SVehicleWheel Name=RRWheel
		BoneName="RightRearTire"
		BoneRollAxis=AXIS_X
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=-15.0,Y=0.0,Z=0.0)
		WheelRadius=34
		bPoweredWheel=True
		bHandbrakeWheel=True
		SteerType=VST_Fixed
		SupportBoneName="RightRearStrut"
		SupportBoneAxis=AXIS_Y
	End Object
	Wheels(0)=SVehicleWheel'RRWheel'

	Begin Object Class=SVehicleWheel Name=LRWheel
		BoneName="LeftRearTire"
		BoneRollAxis=AXIS_X
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=15.0,Y=0.0,Z=0.0)
		WheelRadius=34
		bPoweredWheel=True
		bHandbrakeWheel=True
		SteerType=VST_Fixed
		SupportBoneName="LeftRearStrut"
		SupportBoneAxis=AXIS_Y
	End Object
	Wheels(1)=SVehicleWheel'LRWheel'

	Begin Object Class=SVehicleWheel Name=RFWheel
		BoneName="RightFrontTire"
		BoneRollAxis=AXIS_X
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=-15.0,Y=0.0,Z=0.0)
		WheelRadius=34
		bPoweredWheel=True
		SteerType=VST_Steered
		SupportBoneName="RightFrontStrut"
		SupportBoneAxis=AXIS_Y
	End Object
	Wheels(2)=SVehicleWheel'RFWheel'

	Begin Object Class=SVehicleWheel Name=LFWheel
		BoneName="LeftFrontTire"
		BoneRollAxis=AXIS_X
		BoneSteerAxis=AXIS_Z
		BoneOffset=(X=15.0,Y=0.0,Z=0.0)
		WheelRadius=34
		bPoweredWheel=True
		SteerType=VST_Steered
		SupportBoneName="LeftFrontStrut"
		SupportBoneAxis=AXIS_Y
	End Object
	Wheels(3)=SVehicleWheel'LFWheel'

	GroundSpeed=840
	bDriverHoldsFlag=false
	FlagBone=Dummy01
	FlagRotation=(Yaw=32768)

	HornSounds(0)=sound'ONSVehicleSounds-S.Horn09'
	HornSounds(1)=sound'ONSVehicleSounds-S.Horn04'
	//VehicleIcon=(Material=Texture'AS_FX_TX.HUD.AssaultHUD',X=380,Y=83,SizeX=130,SizeY=64)
	VehicleIcon=(Material=Texture'AS_FX_TX.Icons.OBJ_HellBender',X=0,Y=0,SizeX=64,SizeY=64,bIsGreyScale=true)

	ObjectiveGetOutDist=1500.0
	bCanDoTrickJumps=true
}
