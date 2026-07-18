class ONSTankSecondaryTurret extends ONSWeapon;

var class<Emitter>      mTracerClass;
var() editinline Emitter mTracer;
var() float				mTracerInterval;
var() float				mTracerPullback;
var() float				mTracerMinDistance;
var() float				mTracerSpeed;
var float               mLastTracerTime;

static function StaticPrecache(LevelInfo L)
{
    L.AddPrecacheMaterial(Material'VMparticleTextures.TankFiringP.CloudParticleOrangeBMPtex');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.TracerShot');
}

simulated function UpdatePrecacheMaterials()
{
    Level.AddPrecacheMaterial(Material'VMparticleTextures.TankFiringP.CloudParticleOrangeBMPtex');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.TracerShot');

    Super.UpdatePrecacheMaterials();
}

function byte BestMode()
{
	return 0;
}

simulated function Destroyed()
{
	if (mTracer != None)
		mTracer.Destroy();

	Super.Destroyed();
}

simulated function UpdateTracer()
{
	local vector SpawnDir, SpawnVel;
	local float hitDist;

	if (Level.NetMode == NM_DedicatedServer)
		return;

	if (mTracer == None)
	{
		mTracer = Spawn(mTracerClass);
	}

	if (Level.bDropDetail || Level.DetailMode == DM_Low)
		mTracerInterval = 2 * Default.mTracerInterval;
	else
		mTracerInterval = Default.mTracerInterval;

	if (mTracer != None && Level.TimeSeconds > mLastTracerTime + mTracerInterval)
	{
	        mTracer.SetLocation(WeaponFireLocation);

		hitDist = VSize(LastHitLocation - WeaponFireLocation) - mTracerPullback;

		if (Instigator != None && Instigator.IsLocallyControlled())
			SpawnDir = vector(WeaponFireRotation);
		else
			SpawnDir = Normal(LastHitLocation - WeaponFireLocation);

		if(hitDist > mTracerMinDistance)
		{
			SpawnVel = SpawnDir * mTracerSpeed;

			mTracer.Emitters[0].StartVelocityRange.X.Min = SpawnVel.X;
			mTracer.Emitters[0].StartVelocityRange.X.Max = SpawnVel.X;
			mTracer.Emitters[0].StartVelocityRange.Y.Min = SpawnVel.Y;
			mTracer.Emitters[0].StartVelocityRange.Y.Max = SpawnVel.Y;
			mTracer.Emitters[0].StartVelocityRange.Z.Min = SpawnVel.Z;
			mTracer.Emitters[0].StartVelocityRange.Z.Max = SpawnVel.Z;

			mTracer.Emitters[0].LifetimeRange.Min = hitDist / mTracerSpeed;
			mTracer.Emitters[0].LifetimeRange.Max = mTracer.Emitters[0].LifetimeRange.Min;

			mTracer.SpawnParticle(1);
		}

		mLastTracerTime = Level.TimeSeconds;
	}
}

simulated function FlashMuzzleFlash()
{
	Super.FlashMuzzleFlash();

	if (Role < ROLE_Authority)
		DualFireOffset *= -1;

	UpdateTracer();
}

DefaultProperties
{
    Mesh=Mesh'ONSWeapons-A.TankMachineGun'
    YawBone=Object01
    YawStartConstraint=0
    YawEndConstraint=65535
    PitchBone=Object02
    PitchUpLimit=12500
    PitchDownLimit=59500
    bInstantFire=True
    AmbientEffectEmitterClass=class'Onslaught.ONSRVChainGunFireEffect'
    FireInterval=0.1
    FireSoundClass=sound'ONSVehicleSounds-S.TankMachineGun01'
    SoundVolume=255
    AmbientSoundScaling=1.3
    ShakeOffsetMag=(X=1.0,Y=1.0,Z=1.0)
    ShakeOffsetRate=(X=1000.0,Y=1000.0,Z=1000.0)
    ShakeOffsetTime=2
    ShakeRotMag=(X=50.0,Y=50.0,Z=50.0)
    ShakeRotRate=(X=10000.0,Y=10000.0,Z=10000.0)
    ShakeRotTime=2
    FireForce="minifireb"
    bIsRepeatingFF=True
    bAmbientFireSound=True
    WeaponFireAttachmentBone=Object02
    WeaponFireOffset=85.0
    DualFireOffset=5.0
    bAimable=True
    DamageType=class'DamTypeONSChainGun'
    DamageMin=17
    DamageMax=17
    Spread=0.01
    RotationsPerSecond=2.0
    bInstantRotation=true
    RedSkin=Shader'VMVehicles-TX.HoverTankGroup.HoverTankChassisFinalRED'
    BlueSkin=Shader'VMVehicles-TX.HoverTankGroup.HoverTankChassisFinalBLUE'
    bDoOffsetTrace=true
    TraceRange=15000

	mTracerInterval=0.06
	mTracerClass=class'XEffects.NewTracer'
	mTracerPullback=150.0
	mTracerMinDistance=0.0
	mTracerSpeed=15000.0

    AIInfo(0)=(bInstantHit=true,AimError=750)
    CullDistance=8000.0
}
