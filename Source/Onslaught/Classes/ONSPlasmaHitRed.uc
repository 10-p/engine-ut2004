//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPlasmaHitRed extends Emitter;

simulated function PostNetBeginPlay()
{
	local PlayerController PC;
	local float MaxDist;
	
	Super.PostNetBeginPlay();
		
	PC = Level.GetLocalPlayerController();
	if ( (Instigator == None) || (PC.Pawn != Instigator) )
		MaxDist = 2000;
	else
		MaxDist = 3000;
	if ( (PC.ViewTarget == None) || (VSize(PC.ViewTarget.Location - Location) > MaxDist) )
		Emitters[2].Disabled = true;
}	

DefaultProperties
{
    Begin Object Class=SpriteEmitter Name=SpriteEmitter22
        UseDirectionAs=PTDU_Normal
        ProjectionNormal=(X=1.000000,Z=0.000000)
        UseColorScale=True
        ColorScale(1)=(RelativeTime=0.250000,Color=(B=64,G=64,R=255))
        ColorScale(2)=(RelativeTime=0.500000,Color=(B=64,G=64,R=255))
        ColorScale(3)=(RelativeTime=1.000000)
        MaxParticles=2
        RespawnDeadParticles=False
        StartLocationOffset=(X=-2.000000)
        UseRotationFrom=PTRS_Actor
        SpinParticles=True
        StartSpinRange=(X=(Max=1.000000))
        UseSizeScale=True
        UseRegularSizeScale=False
        SizeScale(0)=(RelativeSize=0.500000)
        SizeScale(1)=(RelativeTime=1.000000,RelativeSize=1.500000)
        StartSizeRange=(X=(Min=60.000000,Max=120.000000))
        UniformSize=True
        InitialParticlesPerSecond=500.000000
        AutomaticInitialSpawning=False
        Texture=Texture'AW-2004Particles.Weapons.SmokePanels1'
        TextureUSubdivisions=4
        TextureVSubdivisions=4
        UseRandomSubdivision=True
        LifetimeRange=(Min=0.200000,Max=0.200000)
        Name="SpriteEmitter22"
    End Object
    Emitters(0)=SpriteEmitter'SpriteEmitter22'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter23
        UseDirectionAs=PTDU_Normal
        ProjectionNormal=(X=1.000000,Z=0.000000)
        UseColorScale=True
        ColorScale(0)=(Color=(B=64,G=64,R=255))
        ColorScale(1)=(RelativeTime=0.800000,Color=(B=64,G=64,R=255))
        ColorScale(2)=(RelativeTime=1.000000)
        Opacity=0.800000
        MaxParticles=1
        RespawnDeadParticles=False
        StartLocationOffset=(X=-2.000000)
        UseRotationFrom=PTRS_Actor
        SpinParticles=True
        StartSpinRange=(X=(Max=1.000000))
        UseSizeScale=True
        UseRegularSizeScale=False
        SizeScale(0)=(RelativeSize=0.500000)
        SizeScale(1)=(RelativeTime=1.000000,RelativeSize=1.000000)
        StartSizeRange=(X=(Min=200.000000,Max=250.000000))
        UniformSize=True
        InitialParticlesPerSecond=500.000000
        AutomaticInitialSpawning=False
        Texture=Texture'AW-2004Particles.Weapons.PlasmaStar'
        LifetimeRange=(Min=0.200000,Max=0.200000)
        Name="SpriteEmitter23"
    End Object
    Emitters(1)=SpriteEmitter'SpriteEmitter23'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter32
        Acceleration=(Z=-250.000000)
        UseColorScale=True
        ColorScale(1)=(RelativeTime=0.200000,Color=(B=20,G=96,R=220))
        ColorScale(2)=(RelativeTime=0.800000,Color=(R=255))
        ColorScale(3)=(RelativeTime=1.000000)
        ColorScaleRepeats=6.000000
        MaxParticles=20
        DetailMode=DM_High
        RespawnDeadParticles=False
        StartLocationShape=PTLS_Polar
        StartLocationPolarRange=(Y=(Max=65536.000000),Z=(Min=8.000000,Max=64.000000))
        UseRotationFrom=PTRS_Actor
        SpinParticles=True
        RotationOffset=(Yaw=16384)
        SpinsPerSecondRange=(X=(Max=0.050000))
        StartSpinRange=(X=(Max=1.000000))
        StartSizeRange=(X=(Min=5.000000,Max=15.000000))
        UniformSize=True
        InitialParticlesPerSecond=5000.000000
        AutomaticInitialSpawning=False
        Texture=Texture'AW-2004Particles.Weapons.PlasmaStar2'
        LifetimeRange=(Min=0.250000,Max=0.750000)
        StartVelocityRange=(Y=(Min=20.000000,Max=150.000000))
        StartVelocityRadialRange=(Min=-30.000000,Max=-150.000000)
        GetVelocityDirectionFrom=PTVD_AddRadial
        Name="SpriteEmitter32"
    End Object
    Emitters(2)=SpriteEmitter'SpriteEmitter32'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter53
        UseDirectionAs=PTDU_Scale
        UseColorScale=True
        ColorScale(0)=(Color=(B=255,G=255,R=255))
        ColorScale(1)=(RelativeTime=0.700000,Color=(B=32,G=32,R=255))
        ColorScale(2)=(RelativeTime=1.000000)
        MaxParticles=1
        RespawnDeadParticles=False
        StartLocationOffset=(X=-4.000000)
        UseRotationFrom=PTRS_Actor
        SpinParticles=True
        StartSpinRange=(X=(Max=1.000000))
        StartSizeRange=(X=(Min=50.000000,Max=50.000000))
        UniformSize=True
        InitialParticlesPerSecond=5000.000000
        AutomaticInitialSpawning=False
        Texture=Texture'EpicParticles.Flares.FlashFlare1'
        LifetimeRange=(Min=0.300000,Max=0.300000)
        Name="SpriteEmitter53"
    End Object
    Emitters(3)=SpriteEmitter'SpriteEmitter53'
    AutoDestroy=True
    bNoDelete=false
}


