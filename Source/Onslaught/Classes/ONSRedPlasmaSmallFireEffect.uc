//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSRedPlasmaSmallFireEffect extends Emitter;

simulated function PostNetBeginPlay()
{
	local PlayerController PC;
	
	Super.PostNetBeginPlay();
		
	PC = Level.GetLocalPlayerController();
	if ( (PC.ViewTarget == None) || (VSize(PC.ViewTarget.Location - Location) > 5000) )
		Emitters[2].Disabled = true;
}	

DefaultProperties
{
    Begin Object Class=SpriteEmitter Name=SpriteEmitter37
        UseDirectionAs=PTDU_Scale
        UseColorScale=True
        ColorScale(0)=(Color=(B=96,G=96,R=128,A=128))
        ColorScale(1)=(RelativeTime=0.800000,Color=(B=96,G=96,R=128))
        ColorScale(2)=(RelativeTime=1.000000,Color=(B=96,G=96,R=128,A=128))
        CoordinateSystem=PTCS_Relative
        MaxParticles=2
        StartLocationOffset=(X=92.500000)
        SpinParticles=True
        StartSpinRange=(X=(Max=1.000000))
        StartSizeRange=(X=(Min=35.000000,Max=50.000000))
        UniformSize=True
        InitialParticlesPerSecond=8000.000000
        Texture=Texture'AW-2004Particles.Weapons.PlasmaStarRed'
        LifetimeRange=(Min=0.200000,Max=0.200000)
        WarmupTicksPerSecond=2.000000
        RelativeWarmupTime=2.000000
        Name="SpriteEmitter37"
    End Object
    Emitters(0)=SpriteEmitter'SpriteEmitter37'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter38
        UseDirectionAs=PTDU_Right
        ColorScale(0)=(Color=(B=15,G=9,R=255))
        ColorScale(1)=(RelativeTime=1.000000,Color=(B=7,G=1,R=250))
        CoordinateSystem=PTCS_Relative
        MaxParticles=1
        StartLocationOffset=(X=50.000000)
        StartSizeRange=(X=(Min=-75.000000,Max=-75.000000),Y=(Min=25.000000,Max=25.000000))
        InitialParticlesPerSecond=500.000000
        AutomaticInitialSpawning=False
        Texture=Texture'AW-2004Particles.Weapons.PlasmaHeadRed'
        LifetimeRange=(Min=0.200000,Max=0.200000)
        StartVelocityRange=(X=(Max=10.000000))
        VelocityLossRange=(X=(Min=1.000000,Max=1.000000))
        WarmupTicksPerSecond=2.000000
        RelativeWarmupTime=2.000000
        Name="SpriteEmitter38"
    End Object
    Emitters(1)=SpriteEmitter'SpriteEmitter38'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter39
        UseColorScale=True
        ColorScale(1)=(RelativeTime=0.100000,Color=(R=255))
        ColorScale(2)=(RelativeTime=0.500000,Color=(R=251))
        ColorScale(3)=(RelativeTime=1.000000)
        CoordinateSystem=PTCS_Relative
        MaxParticles=20
        StartLocationOffset=(X=75.000000)
        SpinParticles=True
        SpinsPerSecondRange=(X=(Max=0.200000))
        StartSpinRange=(X=(Max=1.000000))
        UseSizeScale=True
        UseRegularSizeScale=False
        SizeScale(0)=(RelativeSize=1.000000)
        SizeScale(1)=(RelativeTime=1.000000)
        StartSizeRange=(X=(Min=12.500000,Max=17.500000))
        UniformSize=True
        DetailMode=DM_High
        Texture=Texture'AW-2004Particles.Weapons.SmokePanels1'
        TextureUSubdivisions=4
        TextureVSubdivisions=4
        BlendBetweenSubdivisions=True
        LifetimeRange=(Min=0.400000,Max=0.400000)
        StartVelocityRange=(X=(Min=-500.000000,Max=-500.000000))
        WarmupTicksPerSecond=1.000000
        RelativeWarmupTime=1.000000
        Name="SpriteEmitter39"
    End Object
    Emitters(2)=SpriteEmitter'SpriteEmitter39'
    bNoDelete=False
}
