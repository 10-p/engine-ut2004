class ONSAttackCraftStreamer extends Emitter;

#exec OBJ LOAD FILE=..\Textures\AW-2004Particles.utx

defaultproperties
{
	bNoDelete=false
	bBlockActors=False
	RemoteRole=ROLE_None
	Physics=PHYS_None
	bHardAttach=True
	CullDistance=8000.0

	Begin Object Class=TrailEmitter Name=TrailEmitter0
		TrailShadeType=PTTST_Linear
		TrailLocation=PTTL_FollowEmitter
		MaxPointsPerTrail=100
		DistanceThreshold=50.000000
		PointLifeTime=1.000000
		AlphaTest=False
		AutomaticInitialSpawning=False
		ColorScale(0)=(Color=(B=64,G=64,R=64,A=255))
		ColorScale(1)=(RelativeTime=1.000000,Color=(B=64,G=64,R=64,A=255))
		MaxParticles=1
		SizeScale(0)=(RelativeSize=1.000000)
		SizeScale(1)=(RelativeTime=1.000000)
		StartSizeRange=(X=(Min=5.000000,Max=5.000000),Y=(Min=10.000000,Max=10.000000))
		InitialParticlesPerSecond=5000.000000
		DrawStyle=PTDS_AlphaBlend
		Texture=Texture'AW-2004Particles.Weapons.TrailBlura'
		LifetimeRange=(Min=50.000000,Max=50.000000)
		SecondsBeforeInactive=0.0
		Name="TrailEmitter0"
	End Object
	Emitters(0)=TrailEmitter'TrailEmitter0'
}
