class ONSRVWebProjectileEffect extends Emitter;

#exec OBJ LOAD FILE=..\Textures\AW-2004Particles.utx
#exec OBJ LOAD FILE=..\Textures\EpicParticles.utx

defaultproperties
{
	bNoDelete=false
	bBlockActors=False
	RemoteRole=ROLE_None
	Physics=PHYS_None
	bHardAttach=True

	Begin Object Class=BeamEmitter Name=BeamEmitter8
		BeamDistanceRange=(Min=32.000000,Max=64.000000)
		DetermineEndPointBy=PTEP_Distance
		LowFrequencyNoiseRange=(X=(Min=-5.000000,Max=5.000000),Y=(Min=-5.000000,Max=5.000000),Z=(Min=-5.000000,Max=5.000000))
		HighFrequencyNoiseRange=(X=(Min=-0.700000,Max=0.700000),Y=(Min=-0.700000,Max=0.700000),Z=(Min=-0.700000,Max=0.700000))
		HighFrequencyPoints=5
		BranchProbability=(Min=1.000000,Max=1.000000)
		BranchHFPointsRange=(Min=1.000000,Max=3.000000)
		BranchSpawnAmountRange=(Min=1.000000,Max=1.000000)
		LinkupLifetime=True
		UseColorScale=True
		ColorScale(1)=(RelativeTime=0.200000,Color=(B=255,G=255,R=255))
		ColorScale(2)=(RelativeTime=0.800000,Color=(B=255,G=255,R=255))
		ColorScale(3)=(RelativeTime=1.000000)
		ColorMultiplierRange=(X=(Min=0.200000,Max=0.200000),Z=(Min=0.200000,Max=0.200000))
		CoordinateSystem=PTCS_Relative
		MaxParticles=5
		StartLocationShape=PTLS_Sphere
		SphereRadiusRange=(Min=0.100000,Max=0.100000)
		StartSizeRange=(X=(Min=10.000000,Max=20.000000))
		Texture=AW-2004Particles.Energy.PowerBolt
		LifetimeRange=(Min=0.200000,Max=0.200000)
		StartVelocityRadialRange=(Min=1.000000,Max=1.000000)
		GetVelocityDirectionFrom=PTVD_AddRadial
		Name="BeamEmitter8"
	End Object
	Emitters(0)=BeamEmitter'BeamEmitter8'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter11
		SpinParticles=True
		UniformSize=True
		AutomaticInitialSpawning=False
		UseRandomSubdivision=True
		ColorMultiplierRange=(X=(Min=0.200000,Max=0.200000),Z=(Min=0.200000,Max=0.200000))
		CoordinateSystem=PTCS_Relative
		MaxParticles=1
		StartSpinRange=(X=(Max=1.000000))
		StartSizeRange=(X=(Min=20.000000,Max=25.000000))
		InitialParticlesPerSecond=500.000000
		Texture=AW-2004Particles.Energy.ElecPanels
		TextureUSubdivisions=2
		TextureVSubdivisions=2
		LifetimeRange=(Min=0.100000,Max=0.100000)
		Name="SpriteEmitter11"
	End Object
	Emitters(1)=SpriteEmitter'SpriteEmitter11'

	Begin Object Class=BeamEmitter Name=BeamEmitter12
		BeamTextureUScale=2.000000
		LowFrequencyNoiseRange=(X=(Min=-5.000000,Max=5.000000),Y=(Min=-5.000000,Max=5.000000),Z=(Min=-10.000000,Max=10.000000))
		LowFrequencyPoints=4
		ColorMultiplierRange=(X=(Min=0.200000,Max=0.200000),Z=(Min=0.200000,Max=0.200000))
		MaxParticles=3
		StartSizeRange=(X=(Min=5.000000,Max=10.000000))
		Texture=EpicParticles.Beams.HotBolt03aw
		LifetimeRange=(Min=0.010000,Max=0.010000)
		DetermineEndPointBy=PTEP_OffsetAsAbsolute
		BeamEndPoints(0)=(ActorTag="Second",Weight=1.000000)
		Disabled=True
		Name="BeamEmitter12"
	End Object
	Emitters(2)=BeamEmitter'BeamEmitter12'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter12
		SpinParticles=True
		UniformSize=True
		AutomaticInitialSpawning=False
		ColorMultiplierRange=(X=(Min=0.200000,Max=0.200000),Z=(Min=0.200000,Max=0.200000))
		Opacity=0.200000
		CoordinateSystem=PTCS_Relative
		MaxParticles=2
		StartSpinRange=(X=(Max=1.000000))
		StartSizeRange=(X=(Min=50.000000,Max=50.000000))
		InitialParticlesPerSecond=20.000000
		Texture=AW-2004Particles.Weapons.HardSpot
		LifetimeRange=(Min=0.100000,Max=0.100000)
		Name="SpriteEmitter12"
	End Object
	Emitters(3)=SpriteEmitter'SpriteEmitter12'
}





