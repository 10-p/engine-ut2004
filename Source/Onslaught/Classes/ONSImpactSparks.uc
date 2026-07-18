class ONSImpactSparks extends Emitter
	native;

#exec OBJ LOAD FILE=..\Textures\AW-2004Particles.utx
#exec OBJ LOAD FILE=..\Sounds\ONSVehicleSounds-S.uax

var() float			HorizontalVelocityRange;
var() float			AdditionalVelocityScale;
var() float			MaxAdditionalVelocity;
var() float			SparkRadiusScale;
var() interpcurve	SparkRate; // function of sliding velocity magnitude.

var() sound			ScrapeSound;

var	  bool			bSparksActive;

simulated event UpdateSparks(float SparkRadius, vector VehicleVelocity)
{
	local float UseRadius, VelMag, ParticleRate;
	local vector LocalVehVel, AddVel;

	// Use function to figure out how many particles we should be spewing.
	VelMag = VSize(VehicleVelocity);
	ParticleRate = InterpCurveEval(SparkRate, VelMag);

	if(ParticleRate > 0.001)
	{
		// Transform velocity into particle system space and project into contact plane.
		LocalVehVel = (VehicleVelocity << Rotation);
		LocalVehVel.X = 0;

		// If sparks are just turning on, set OldLocation to current Location to avoid spawn interpolation.
		if(!bSparksActive)
			OldLocation = Location;

		bSparksActive = true;

		UseRadius = SparkRadiusScale * SparkRadius;

		Emitters[0].ParticlesPerSecond = ParticleRate;
		Emitters[0].InitialParticlesPerSecond = ParticleRate;
		Emitters[0].AllParticlesDead = false;

		Emitters[0].StartLocationRange.Y.Min = -UseRadius;
		Emitters[0].StartLocationRange.Y.Max = UseRadius;

		Emitters[0].StartLocationRange.Z.Min = -UseRadius;
		Emitters[0].StartLocationRange.Z.Max = UseRadius;

		// Add velocity of vehicle to sparks.

		AddVel = AdditionalVelocityScale * LocalVehVel;
		if( VSize(AddVel) > MaxAdditionalVelocity )
		{
			AddVel = normal(AddVel) * MaxAdditionalVelocity;
		}

		Emitters[0].StartVelocityRange.Y.Min = (LocalVehVel.Y + AddVel.Y) - HorizontalVelocityRange;
		Emitters[0].StartVelocityRange.Y.Max = (LocalVehVel.Y + AddVel.Y) + HorizontalVelocityRange;

		Emitters[0].StartVelocityRange.Z.Min = (LocalVehVel.Z + AddVel.Z) - HorizontalVelocityRange;
		Emitters[0].StartVelocityRange.Z.Max = (LocalVehVel.Z + AddVel.Z) + HorizontalVelocityRange;

		AmbientSound = ScrapeSound;
	}
	else
	{
		bSparksActive = false;

		Emitters[0].ParticlesPerSecond = 0;
		Emitters[0].InitialParticlesPerSecond = 0;

		AmbientSound = None;
	}
}

defaultproperties
{
	bNoDelete=false
	bBlockActors=False
	RemoteRole=ROLE_None
	Physics=PHYS_None

	HorizontalVelocityRange=500
	AdditionalVelocityScale=0.25
	MaxAdditionalVelocity=200
	SparkRadiusScale=0.05
	SparkRate=(Points=((InVal=0,OutVal=0),(InVal=350,OutVal=0),(InVal=1200,OutVal=600),(InVal=100000,OutVal=600)))

	ScrapeSound=sound'ONSVehicleSounds-S.CollisionSounds.MetalScrape03'
	SoundVolume=80
	SoundRadius=64

	Begin Object Class=SpriteEmitter Name=SpriteEmitter0
		UseDirectionAs=PTDU_Up
		UseCollision=False
		UseColorScale=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		ScaleSizeYByVelocity=True
		AutomaticInitialSpawning=False
		AddVelocityFromOwner=False
		Acceleration=(Z=-600.000000)
		DampingFactorRange=(X=(Min=0.500000,Max=0.500000),Y=(Min=0.500000,Max=0.500000),Z=(Min=0.100000,Max=0.100000))
		StartLocationRange=(X=(Min=-10.00000,Max=10.00000),Y=(Min=0.00000,Max=0.00000),Z=(Min=0.00000,Max=0.00000))
		ColorScale(0)=(Color=(B=174,G=255,R=255))
		ColorScale(1)=(RelativeTime=0.500000,Color=(B=49,G=214,R=242))
		ColorScale(2)=(RelativeTime=1.000000,Color=(G=40,R=102))
		MaxParticles=1000
		UseRotationFrom=PTRS_Actor
		SizeScale(1)=(RelativeTime=0.050000)
		SizeScale(2)=(RelativeTime=0.100000,RelativeSize=2.000000)
		SizeScale(3)=(RelativeTime=1.00000,RelativeSize=2.000000)
		StartSizeRange=(X=(Min=2.000000,Max=4.000000),Y=(Min=2.000000,Max=2.000000),Z=(Min=100.000000,Max=100.000000))
		ScaleSizeByVelocityMultiplier=(X=0.010000,Y=0.003000,Z=1.000)
		ScaleSizeByVelocityMax=1000
		VelocityLossRange=(X=(Min=1.0000000,Max=2.000000),Y=(Min=1.0000000,Max=2.000000),Z=(Min=1.0000000,Max=2.000000))
		MaxAbsVelocity=(X=1000,Y=1000,Z=1000);
		Texture=Texture'AW-2004Particles.Energy.SparkHead'
		LifetimeRange=(Min=0.400000,Max=0.600000)
		StartVelocityRange=(X=(Min=350.000000,Max=500.000000),Y=(Min=-50.000000,Max=50.000000),Z=(Min=-50.000000,Max=50.000000))
		RespawnDeadParticles=False
		ParticlesPerSecond=0.0
		InitialParticlesPerSecond=0.0
		Name="SpriteEmitter0"
	End Object
	Emitters(0)=SpriteEmitter'SpriteEmitter0'
}
