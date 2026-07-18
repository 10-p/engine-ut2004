//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSTankFireTrailEffect extends Emitter;

#exec OBJ LOAD FILE=AS_FX_TX.utx

defaultproperties
{
    Begin Object Class=TrailEmitter Name=TrailEmitter0
        TrailShadeType=PTTST_PointLife
        TrailLocation=PTTL_FollowEmitter
        MaxPointsPerTrail=150
        DistanceThreshold=80.000000
        PointLifeTime=0.800000
        UseCrossedSheets=True
        UseColorScale=True
        UseSizeScale=True
        UseRegularSizeScale=False
        AutomaticInitialSpawning=False
        ColorScale(0)=(Color=(B=100,G=200,R=255))
        ColorScale(1)=(RelativeTime=1.000000,Color=(B=100,G=200,R=255))
        Opacity=0.500000
        MaxParticles=1
        SizeScale(0)=(RelativeSize=1.000000)
        SizeScale(1)=(RelativeTime=1.000000,RelativeSize=5.000000)
        StartSizeRange=(X=(Min=25.000000,Max=25.000000))
        InitialParticlesPerSecond=2000.000000
        DrawStyle=PTDS_Translucent
        Texture=AW-2004Particles.Weapons.TrailBlur
        SecondsBeforeInactive=0.000000
        LifetimeRange=(Min=500.000000,Max=500.000000)
        Name="TrailEmitter6"
    End Object
    Emitters(0)=TrailEmitter'TrailEmitter0'

    Begin Object Class=SpriteEmitter Name=SpriteEmitter22
        ProjectionNormal=(X=1.000000,Z=0.000000)
        UseColorScale=True
        SpinParticles=True
        UniformSize=True
        AutomaticInitialSpawning=False
        ColorMultiplierRange=(Y=(Min=0.800000,Max=0.800000),Z=(Min=0.000000,Max=0.000000))
        CoordinateSystem=PTCS_Relative
        MaxParticles=2
        StartLocationOffset=(X=-10.000000)
        SpinCCWorCW=(X=0.000000,Y=0.000000,Z=0.000000)
        SpinsPerSecondRange=(X=(Min=0.050000,Max=0.200000))
        StartSpinRange=(X=(Max=1.000000))
        SizeScale(0)=(RelativeSize=0.100000)
        SizeScale(1)=(RelativeTime=0.100000,RelativeSize=1.500000)
        SizeScale(2)=(RelativeTime=0.150000,RelativeSize=1.000000)
        SizeScale(3)=(RelativeTime=1.000000,RelativeSize=0.750000)
        StartSizeRange=(X=(Min=20.000000,Max=20.000000))
        InitialParticlesPerSecond=50.000000
        Texture=AW-2004Particles.Weapons.PlasmaStar
        SecondsBeforeInactive=0.000000
        LifetimeRange=(Min=0.100000,Max=0.100000)
        InitialDelayRange=(Min=0.050000,Max=0.050000)
        Name="SpriteEmitter0"
    End Object
    Emitters(1)=SpriteEmitter'SpriteEmitter22'

	bStasis=false
	bDirectional=true
	bHardAttach=true
    bNoDelete=false
	RemoteRole=ROLE_None
	AutoDestroy=True

    Physics=PHYS_Trailer
}