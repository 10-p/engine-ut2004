//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSGreenPlasmaFireEffect extends Emitter;

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
    Begin Object Class=SpriteEmitter Name=SpriteEmitter8
        UseDirectionAs=PTDU_Scale
        UseColorScale=True
        ColorScale(0)=(Color=(G=192))
        ColorScale(1)=(RelativeTime=0.800000,Color=(G=192))
        ColorScale(2)=(RelativeTime=1.000000,Color=(G=192))
        CoordinateSystem=PTCS_Relative
        MaxParticles=2
        StartLocationOffset=(X=150.000000)
        SpinParticles=True
        StartSpinRange=(X=(Max=1.000000))
        StartSizeRange=(X=(Min=75.000000))
        UniformSize=True
        InitialParticlesPerSecond=8000.000000
        Texture=Texture'AW-2004Particles.Weapons.PlasmaStar'
        LifetimeRange=(Min=0.200000,Max=0.200000)
        Name="SpriteEmitter8"
    End Object
    Emitters(0)=SpriteEmitter'SpriteEmitter8'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter12
        UseDirectionAs=PTDU_Right
        CoordinateSystem=PTCS_Relative
        MaxParticles=1
        StartLocationOffset=(X=50.000000)
        StartSizeRange=(X=(Min=-150.000000,Max=-150.000000),Y=(Min=50.000000,Max=55.000000))
        InitialParticlesPerSecond=500.000000
        AutomaticInitialSpawning=False
        Texture=Texture'AW-2004Particles.Weapons.PlasmaHead'
        LifetimeRange=(Min=0.200000,Max=0.200000)
        StartVelocityRange=(X=(Max=10.000000))
        VelocityLossRange=(X=(Min=1.000000,Max=1.000000))
        Name="SpriteEmitter12"
    End Object
    Emitters(1)=SpriteEmitter'SpriteEmitter12'
    Begin Object Class=SpriteEmitter Name=SpriteEmitter13
        UseColorScale=True
        ColorScale(1)=(RelativeTime=0.100000,Color=(B=128,G=255,R=128))
        ColorScale(2)=(RelativeTime=0.500000,Color=(G=251))
        ColorScale(3)=(RelativeTime=1.000000)
        CoordinateSystem=PTCS_Relative
        MaxParticles=20
        DetailMode=DM_High
        StartLocationOffset=(X=150.000000)
        SpinParticles=True
        SpinsPerSecondRange=(X=(Max=0.200000))
        StartSpinRange=(X=(Max=1.000000))
        UseSizeScale=True
        UseRegularSizeScale=False
        SizeScale(0)=(RelativeSize=1.000000)
        SizeScale(1)=(RelativeTime=1.000000)
        StartSizeRange=(X=(Min=25.000000,Max=35.000000))
        UniformSize=True
        Texture=Texture'AW-2004Particles.Weapons.SmokePanels1'
        TextureUSubdivisions=4
        TextureVSubdivisions=4
        BlendBetweenSubdivisions=True
        LifetimeRange=(Min=0.500000,Max=0.500000)
        StartVelocityRange=(X=(Min=-1000.000000,Max=-1000.000000))
        WarmupTicksPerSecond=1.000000
        RelativeWarmupTime=2.000000
        Name="SpriteEmitter13"
    End Object
    Emitters(2)=SpriteEmitter'SpriteEmitter13'
    bNoDelete=False
}
