//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSRVChainGunFireEffect extends ONSWeaponAmbientEmitter;

simulated function SetEmitterStatus(bool bEnabled)
{
	Emitters[0].UseCollision = ( !Level.bDropDetail && (Level.DetailMode != DM_Low) && (VSize(Level.GetLocalPlayerController().ViewTarget.Location - Location) < 1600));
	if(bEnabled)
	{
		Emitters[0].ParticlesPerSecond = 20.0;
		Emitters[0].InitialParticlesPerSecond = 20.0;
		Emitters[0].AllParticlesDead = false;

		Emitters[1].ParticlesPerSecond = 30.0;
		Emitters[1].InitialParticlesPerSecond = 30.0;
		Emitters[1].AllParticlesDead = false;
	}
	else
	{
		Emitters[0].ParticlesPerSecond = 0.0;
		Emitters[0].InitialParticlesPerSecond = 0.0;

		Emitters[1].ParticlesPerSecond = 0.0;
		Emitters[1].InitialParticlesPerSecond = 0.0;
	}
}

DefaultProperties
{
    Begin Object Class=MeshEmitter Name=MeshEmitter0
        StaticMesh=StaticMesh'VMmeshEmitted.EJECTA.EjectedBRASSsm'
        Acceleration=(Z=-500.000000)
        UseCollision=True
        DampingFactorRange=(X=(Min=0.500000,Max=0.500000),Y=(Min=0.500000,Max=0.500000),Z=(Min=0.500000,Max=0.500000))
        CoordinateSystem=PTCS_Independent
        MaxParticles=30
        RespawnDeadParticles=False
        StartLocationOffset=(X=-80.000000,Y=-6,Z=10)
        SpawnOnlyInDirectionOfNormal=True
        MeshNormal=(Z=0.000000)
        UseRotationFrom=PTRS_Actor
        SpinParticles=True
        SpinsPerSecondRange=(X=(Min=0.100000,Max=1.000000),Y=(Min=0.100000,Max=1.000000),Z=(Min=0.100000,Max=1.000000))
        StartSizeRange=(X=(Min=0.050000,Max=0.050000),Y=(Min=0.050000,Max=0.050000),Z=(Min=0.050000,Max=0.050000))
        ParticlesPerSecond=0.0
        InitialParticlesPerSecond=0.0
        AutomaticInitialSpawning=False
        LifetimeRange=(Min=1.500000,Max=1.500000)
        StartVelocityRange=(X=(Min=-150.000000,Max=150.000000),Y=(Min=-250.000000,Max=-250.000000),Z=(Min=50.000000,Max=150.000000))
        StartVelocityRadialRange=(Min=-250.000000,Max=250.000000)
        Name="MeshEmitter0"
    End Object
    Emitters(0)=MeshEmitter'MeshEmitter0'

    Begin Object Class=SpriteEmitter Name=SpriteEmitter0
        CoordinateSystem=PTCS_Relative
        MaxParticles=6
        RespawnDeadParticles=False
        StartLocationOffset=(Z=10.000000)
        StartLocationShape=PTLS_Sphere
        UseRotationFrom=PTRS_Normal
        SpinParticles=True
        SpinsPerSecondRange=(X=(Min=7.000000,Max=11.000000))
        UseSizeScale=True
        UseRegularSizeScale=False
        SizeScale(0)=(RelativeTime=0.250000,RelativeSize=0.750000)
        SizeScale(1)=(RelativeTime=0.500000,RelativeSize=0.100000)
        StartSizeRange=(X=(Min=20.000000,Max=20.000000))
        UniformSize=True
        ParticlesPerSecond=0.0
        InitialParticlesPerSecond=0.0
        AutomaticInitialSpawning=False
        Texture=Texture'VMparticleTextures.TankFiringP.CloudParticleOrangeBMPtex'
        TextureUSubdivisions=4
        TextureVSubdivisions=4
        BlendBetweenSubdivisions=True
        UseRandomSubdivision=True
        LifetimeRange=(Min=0.050000,Max=0.050000)
        StartVelocityRange=(X=(Min=0.000000,Max=0.000000))
        WarmupTicksPerSecond=50.000000
        RelativeWarmupTime=2.000000
        Name="SpriteEmitter0"
    End Object
    Emitters(1)=SpriteEmitter'SpriteEmitter0'

    DrawScale3D=(X=0.250000,Y=0.250000,Z=0.250000)
    bUnlit=False
    bNoDelete=False
    bHardAttach=True
	RemoteRole=ROLE_None
	Physics=PHYS_None
	bBlockActors=False
	CullDistance=4000.0
}
