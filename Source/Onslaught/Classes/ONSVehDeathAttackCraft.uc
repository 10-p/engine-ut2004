class ONSVehDeathAttackCraft extends Emitter;

#exec OBJ LOAD FILE="..\Textures\ExplosionTex.utx"
#exec OBJ LOAD FILE="..\StaticMeshes\ONSDeadVehicles-SM.usx"

defaultproperties
{
	bNoDelete=False
	AutoDestroy=True

	Begin Object Class=SpriteEmitter Name=SpriteEmitter1
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		Acceleration=(Z=50.000000)
		ColorScale(0)=(Color=(A=255))
		ColorScale(1)=(RelativeTime=0.300000,Color=(A=255))
		ColorScale(2)=(RelativeTime=1.000000)
		Opacity=0.500000
		MaxParticles=3
		DetailMode=DM_SuperHigh
		StartLocationRange=(X=(Max=-80.000000))
		StartLocationShape=PTLS_All
		SphereRadiusRange=(Min=32.000000,Max=32.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.750000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=2.500000)
		InitialParticlesPerSecond=60000.000000
		DrawStyle=PTDS_AlphaBlend
		Texture=ExplosionTex.Framed.exp1_frames
		TextureUSubdivisions=2
		TextureVSubdivisions=4
		LifetimeRange=(Min=1.000000,Max=1.200000)
		Name="SpriteEmitter1"
	End Object
	Emitters(0)=SpriteEmitter'SpriteEmitter1'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter0
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		MaxParticles=6
		StartLocationRange=(X=(Min=-100.000000))
		StartLocationShape=PTLS_All
		SphereRadiusRange=(Min=32.000000,Max=32.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.500000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=2.000000)
		InitialParticlesPerSecond=4000.000000
		Texture=ExplosionTex.Framed.exp1_frames
		TextureUSubdivisions=2
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.400000,Max=0.600000)
		Name="SpriteEmitter0"
	End Object
	Emitters(1)=SpriteEmitter'SpriteEmitter0'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter16
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		MaxParticles=4
		StartLocationRange=(X=(Min=50.000000,Max=-200.000000),Y=(Min=-50.000000,Max=50.000000),Z=(Min=-50.000000,Max=20.000000))
		SphereRadiusRange=(Min=32.000000,Max=64.000000)
		UseRotationFrom=PTRS_Actor
		SpinsPerSecondRange=(X=(Max=0.100000))
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=1.000000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=2.000000)
		InitialParticlesPerSecond=5000.000000
		Texture=ExplosionTex.Framed.exp2_frames
		TextureUSubdivisions=4
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.600000,Max=0.800000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter16"
	End Object
	Emitters(2)=SpriteEmitter'SpriteEmitter16'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter3
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		UseRandomSubdivision=True
		ColorScale(0)=(Color=(A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(A=255))
		ColorScale(2)=(RelativeTime=1.000000)
		Opacity=0.500000
		MaxParticles=5
		DetailMode=DM_SuperHigh
		AddLocationFromOtherEmitter=2
		StartLocationShape=PTLS_Sphere
		SphereRadiusRange=(Max=16.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.800000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=1.000000)
		DrawStyle=PTDS_AlphaBlend
		Texture=AW-2004Particles.Weapons.SmokePanels2
		TextureUSubdivisions=4
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter3"
	End Object
	Emitters(3)=SpriteEmitter'SpriteEmitter3'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter2
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		ColorScale(0)=(Color=(B=192,G=192,R=192))
		ColorScale(1)=(RelativeTime=0.600000,Color=(B=128,G=128,R=128))
		ColorScale(2)=(RelativeTime=1.000000)
		MaxParticles=12
		DetailMode=DM_SuperHigh
		StartLocationRange=(X=(Min=-4.000000,Max=4.000000),Y=(Min=-4.000000,Max=4.000000),Z=(Min=-4.000000,Max=4.000000))
		AddLocationFromOtherEmitter=5
		StartLocationShape=PTLS_Sphere
		SphereRadiusRange=(Max=16.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(1)=(RelativeTime=0.200000,RelativeSize=0.800000)
		SizeScale(2)=(RelativeTime=1.000000,RelativeSize=1.000000)
		InitialParticlesPerSecond=14.000000
		Texture=ExplosionTex.Framed.exp1_frames
		TextureUSubdivisions=2
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter2"
	End Object
	Emitters(4)=SpriteEmitter'SpriteEmitter2'

	Begin Object Class=MeshEmitter Name=MeshEmitter0
		StaticMesh=ONSDeadVehicles-SM.RAPTORexploded.RaptorWing
		UseMeshBlendMode=False
		UseParticleColor=True
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		AutomaticInitialSpawning=False
		Acceleration=(Z=-800.000000)
		ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
		ColorScale(2)=(RelativeTime=1.000000,Color=(B=255,G=255,R=255))
		MaxParticles=1
		DetailMode=DM_High
		StartLocationOffset=(Y=60.000000,Z=38.000000)
		UseRotationFrom=PTRS_Actor
		SpinsPerSecondRange=(X=(Max=0.500000),Y=(Max=0.500000),Z=(Max=2.000000))
		InitialParticlesPerSecond=5000.000000
		DrawStyle=PTDS_AlphaBlend
		LifetimeRange=(Min=1.000000,Max=1.000000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		StartVelocityRadialRange=(Min=-300.000000,Max=-800.000000)
		GetVelocityDirectionFrom=PTVD_AddRadial
		Name="MeshEmitter0"
	End Object
	Emitters(5)=MeshEmitter'MeshEmitter0'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter5
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		ColorScale(0)=(Color=(B=192,G=192,R=192))
		ColorScale(1)=(RelativeTime=0.600000,Color=(B=128,G=128,R=128))
		MaxParticles=12
		DetailMode=DM_SuperHigh
		StartLocationRange=(X=(Min=-4.000000,Max=4.000000),Y=(Min=-4.000000,Max=4.000000),Z=(Min=-4.000000,Max=4.000000))
		AddLocationFromOtherEmitter=8
		StartLocationShape=PTLS_Sphere
		SphereRadiusRange=(Max=16.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.500000)
		SizeScale(1)=(RelativeTime=0.300000,RelativeSize=0.800000)
		SizeScale(2)=(RelativeTime=1.000000,RelativeSize=1.000000)
		InitialParticlesPerSecond=14.000000
		Texture=ExplosionTex.Framed.exp1_frames
		TextureUSubdivisions=2
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter5"
	End Object
	Emitters(6)=SpriteEmitter'SpriteEmitter5'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter4
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		UseRandomSubdivision=True
		ColorScale(0)=(Color=(A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(A=255))
		ColorScale(2)=(RelativeTime=1.000000)
		Opacity=0.500000
		MaxParticles=5
		DetailMode=DM_SuperHigh
		AddLocationFromOtherEmitter=8
		StartLocationShape=PTLS_Sphere
		SphereRadiusRange=(Max=16.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.800000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=1.000000)
		DrawStyle=PTDS_AlphaBlend
		Texture=AW-2004Particles.Weapons.SmokePanels2
		TextureUSubdivisions=4
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter4"
	End Object
	Emitters(7)=SpriteEmitter'SpriteEmitter4'

	Begin Object Class=MeshEmitter Name=MeshEmitter1
		StaticMesh=ONSDeadVehicles-SM.RAPTORexploded.RaptorWing
		UseMeshBlendMode=False
		UseParticleColor=True
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		AutomaticInitialSpawning=False
		Acceleration=(Z=-800.000000)
		ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
		ColorScale(2)=(RelativeTime=1.000000,Color=(B=255,G=255,R=255))
		MaxParticles=1
		DetailMode=DM_High
		StartLocationOffset=(Y=-60.000000,Z=38.000000)
		UseRotationFrom=PTRS_Actor
		SpinsPerSecondRange=(X=(Max=0.500000),Y=(Max=0.500000),Z=(Max=2.000000))
		StartSizeRange=(Y=(Min=-1.000000,Max=-1.000000))
		InitialParticlesPerSecond=5000.000000
		DrawStyle=PTDS_AlphaBlend
		LifetimeRange=(Min=1.000000,Max=1.000000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		StartVelocityRadialRange=(Min=-300.000000,Max=-800.000000)
		GetVelocityDirectionFrom=PTVD_AddRadial
		Name="MeshEmitter1"
	End Object
	Emitters(8)=MeshEmitter'MeshEmitter1'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter9
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		UseRandomSubdivision=True
		ColorScale(0)=(Color=(A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(A=255))
		ColorScale(2)=(RelativeTime=1.000000)
		Opacity=0.500000
		MaxParticles=5
		DetailMode=DM_SuperHigh
		AddLocationFromOtherEmitter=11
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.800000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=1.000000)
		DrawStyle=PTDS_AlphaBlend
		Texture=AW-2004Particles.Weapons.SmokePanels2
		TextureUSubdivisions=4
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter9"
	End Object
	Emitters(9)=SpriteEmitter'SpriteEmitter9'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter10
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		ColorScale(0)=(Color=(B=192,G=192,R=192))
		ColorScale(1)=(RelativeTime=0.600000,Color=(B=128,G=128,R=128))
		ColorScale(2)=(RelativeTime=1.000000)
		MaxParticles=12
		DetailMode=DM_SuperHigh
		StartLocationRange=(X=(Min=-4.000000,Max=4.000000),Y=(Min=-4.000000,Max=4.000000),Z=(Min=-4.000000,Max=4.000000))
		AddLocationFromOtherEmitter=11
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(1)=(RelativeTime=0.200000,RelativeSize=0.800000)
		SizeScale(2)=(RelativeTime=1.000000,RelativeSize=1.000000)
		InitialParticlesPerSecond=14.000000
		Texture=ExplosionTex.Framed.exp1_frames
		TextureUSubdivisions=2
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter10"
	End Object
	Emitters(10)=SpriteEmitter'SpriteEmitter10'

	Begin Object Class=MeshEmitter Name=MeshEmitter2
		StaticMesh=ONSDeadVehicles-SM.RAPTORexploded.RaptorTailWing
		UseMeshBlendMode=False
		UseParticleColor=True
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		AutomaticInitialSpawning=False
		Acceleration=(Z=-800.000000)
		ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
		ColorScale(2)=(RelativeTime=1.000000,Color=(B=128,G=128,R=128))
		MaxParticles=1
		DetailMode=DM_High
		StartLocationOffset=(X=-150.000000,Y=40.000000,Z=60.000000)
		UseRotationFrom=PTRS_Actor
		SpinsPerSecondRange=(X=(Max=0.500000),Y=(Max=0.500000),Z=(Max=2.000000))
		StartSizeRange=(Y=(Min=-1.000000,Max=-1.000000))
		InitialParticlesPerSecond=5000.000000
		DrawStyle=PTDS_AlphaBlend
		LifetimeRange=(Min=1.000000,Max=1.000000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		StartVelocityRange=(X=(Min=150.000000,Max=150.000000),Y=(Min=300.000000,Max=300.000000))
		StartVelocityRadialRange=(Min=-600.000000,Max=-800.000000)
		GetVelocityDirectionFrom=PTVD_AddRadial
		Name="MeshEmitter2"
	End Object
	Emitters(11)=MeshEmitter'MeshEmitter2'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter11
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		UseRandomSubdivision=True
		ColorScale(0)=(Color=(A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(A=255))
		ColorScale(2)=(RelativeTime=1.000000)
		Opacity=0.500000
		MaxParticles=5
		DetailMode=DM_SuperHigh
		AddLocationFromOtherEmitter=14
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.800000)
		SizeScale(1)=(RelativeTime=1.000000,RelativeSize=1.000000)
		StartSizeRange=(X=(Min=70.000000,Max=70.000000))
		DrawStyle=PTDS_AlphaBlend
		Texture=AW-2004Particles.Weapons.SmokePanels2
		TextureUSubdivisions=4
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter11"
	End Object
	Emitters(12)=SpriteEmitter'SpriteEmitter11'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter12
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		UseSizeScale=True
		UseRegularSizeScale=False
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		ColorScale(0)=(Color=(B=192,G=192,R=192))
		ColorScale(1)=(RelativeTime=0.600000,Color=(B=128,G=128,R=128))
		MaxParticles=12
		DetailMode=DM_SuperHigh
		StartLocationRange=(X=(Min=-4.000000,Max=4.000000),Y=(Min=-4.000000,Max=4.000000),Z=(Min=-4.000000,Max=4.000000))
		AddLocationFromOtherEmitter=14
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		SizeScale(0)=(RelativeSize=0.500000)
		SizeScale(1)=(RelativeTime=0.300000,RelativeSize=0.800000)
		SizeScale(2)=(RelativeTime=1.000000,RelativeSize=1.000000)
		StartSizeRange=(X=(Min=70.000000,Max=70.000000))
		InitialParticlesPerSecond=14.000000
		Texture=ExplosionTex.Framed.exp1_frames
		TextureUSubdivisions=2
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.500000,Max=0.600000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter12"
	End Object
	Emitters(13)=SpriteEmitter'SpriteEmitter12'

	Begin Object Class=MeshEmitter Name=MeshEmitter3
		StaticMesh=ONSDeadVehicles-SM.RAPTORexploded.RaptorTailWing
		UseMeshBlendMode=False
		UseParticleColor=True
		UseColorScale=True
		RespawnDeadParticles=False
		AlphaTest=False
		SpinParticles=True
		AutomaticInitialSpawning=False
		Acceleration=(Z=-800.000000)
		ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
		ColorScale(2)=(RelativeTime=1.000000,Color=(B=255,G=255,R=255))
		FadeOutStartTime=0.500000
		MaxParticles=1
		DetailMode=DM_High
		StartLocationOffset=(X=-150.000000,Y=-40.000000,Z=60.000000)
		UseRotationFrom=PTRS_Actor
		SpinsPerSecondRange=(X=(Max=0.500000),Y=(Max=0.500000),Z=(Max=2.000000))
		InitialParticlesPerSecond=5000.000000
		DrawStyle=PTDS_AlphaBlend
		LifetimeRange=(Min=1.000000,Max=1.000000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		StartVelocityRadialRange=(Min=-200.000000,Max=-500.000000)
		GetVelocityDirectionFrom=PTVD_AddRadial
		Name="MeshEmitter3"
	End Object
	Emitters(14)=MeshEmitter'MeshEmitter3'

	Begin Object Class=SpriteEmitter Name=SpriteEmitter8
		RespawnDeadParticles=False
		SpinParticles=True
		UniformSize=True
		AutomaticInitialSpawning=False
		BlendBetweenSubdivisions=True
		MaxParticles=28
		DetailMode=DM_SuperHigh
		AddLocationFromOtherEmitter=16
		StartLocationShape=PTLS_Sphere
		SphereRadiusRange=(Max=6.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(X=(Max=1.000000))
		StartSizeRange=(X=(Min=50.000000,Max=50.000000))
		InitialParticlesPerSecond=32.000000
		Texture=ExplosionTex.Framed.exp2_frames
		TextureUSubdivisions=4
		TextureVSubdivisions=4
		LifetimeRange=(Min=0.400000,Max=0.500000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		Name="SpriteEmitter8"
	End Object
	Emitters(15)=SpriteEmitter'SpriteEmitter8'

	Begin Object Class=MeshEmitter Name=MeshEmitter4
		StaticMesh=ONSDeadVehicles-SM.RAPTORexploded.RaptorGun
		UseMeshBlendMode=False
		UseParticleColor=True
		UseColorScale=True
		RespawnDeadParticles=False
		SpinParticles=True
		AutomaticInitialSpawning=False
		Acceleration=(Z=-500.000000)
		ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
		ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
		ColorScale(2)=(RelativeTime=1.000000,Color=(B=128,G=128,R=128))
		MaxParticles=1
		DetailMode=DM_High
		StartLocationOffset=(X=14.000000,Y=-50.000000,Z=-16.000000)
		UseRotationFrom=PTRS_Actor
		StartSpinRange=(Y=(Min=-0.050000,Max=-0.050000))
		InitialParticlesPerSecond=500.000000
		DrawStyle=PTDS_AlphaBlend
		LifetimeRange=(Min=1.000000,Max=1.000000)
		InitialDelayRange=(Min=0.200000,Max=0.200000)
		StartVelocityRange=(X=(Min=800.000000,Max=1000.000000),Y=(Min=-200.000000,Max=200.000000),Z=(Min=100.000000,Max=100.000000))
		Name="MeshEmitter4"
	End Object
        Emitters(16)=MeshEmitter'MeshEmitter5'

	Begin Object Class=MeshEmitter Name=MeshEmitter5
      		StaticMesh=AW-2004Particles.Debris.Veh_Debris2
     		UseParticleColor=True
        	UseColorScale=True
	        RespawnDeadParticles=False
	        SpinParticles=True
	        AutomaticInitialSpawning=False
	        Acceleration=(Z=-400.000000)
	        ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
        	ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
	        ColorScale(2)=(RelativeTime=1.000000,Color=(B=128,G=128,R=128))
	        MaxParticles=15
	        DetailMode=DM_SuperHigh
        	StartLocationShape=PTLS_Sphere
	        SphereRadiusRange=(Min=8.000000,Max=8.000000)
        	SpinsPerSecondRange=(X=(Max=0.200000),Y=(Max=0.200000),Z=(Max=0.200000))
	        StartSpinRange=(X=(Max=1.000000),Y=(Max=1.000000),Z=(Max=1.000000))
	        StartSizeRange=(X=(Min=0.500000),Y=(Min=0.500000),Z=(Min=0.500000))
	        InitialParticlesPerSecond=5000.000000
	        DrawStyle=PTDS_AlphaBlend
	        LifetimeRange=(Min=1.500000,Max=2.000000)
	        InitialDelayRange=(Min=0.150000,Max=0.150000)
        	StartVelocityRadialRange=(Min=300.000000,Max=800.000000)
	        VelocityLossRange=(X=(Min=0.500000,Max=1.000000),Y=(Min=0.500000,Max=1.000000),Z=(Min=0.500000,Max=1.000000))
	        GetVelocityDirectionFrom=PTVD_AddRadial
	        Name="MeshEmitter5"
	    End Object
	    Emitters(17)=MeshEmitter'MeshEmitter5'
	
	    Begin Object Class=MeshEmitter Name=MeshEmitter6
        	StaticMesh=AW-2004Particles.Debris.Veh_Debris1
	        RenderTwoSided=True
	        UseParticleColor=True
	        UseColorScale=True
        	RespawnDeadParticles=False
	        SpinParticles=True
	        AutomaticInitialSpawning=False
	        Acceleration=(Z=-800.000000)
	        ColorScale(0)=(Color=(B=192,G=192,R=192,A=255))
	        ColorScale(1)=(RelativeTime=0.850000,Color=(B=128,G=128,R=128,A=255))
    	    ColorScale(2)=(RelativeTime=1.000000,Color=(B=128,G=128,R=128))
	        MaxParticles=15
	        DetailMode=DM_SuperHigh
	        StartLocationShape=PTLS_Sphere
	        SphereRadiusRange=(Min=8.000000,Max=8.000000)
	        SpinsPerSecondRange=(X=(Max=0.200000),Y=(Max=0.200000),Z=(Max=0.200000))
	        StartSpinRange=(X=(Max=1.000000),Y=(Max=1.000000),Z=(Max=1.000000))
	        StartSizeRange=(X=(Min=0.500000),Y=(Min=0.500000),Z=(Min=0.500000))
	        InitialParticlesPerSecond=5000.000000
	        DrawStyle=PTDS_AlphaBlend
	        LifetimeRange=(Min=1.500000,Max=2.000000)
	        InitialDelayRange=(Min=0.150000,Max=0.150000)
	        StartVelocityRange=(Z=(Min=100.000000,Max=300.000000))
	        StartVelocityRadialRange=(Min=300.000000,Max=800.000000)
	        VelocityLossRange=(X=(Min=0.500000,Max=1.000000),Y=(Min=0.500000,Max=1.000000),Z=(Min=0.500000,Max=1.000000))
	        GetVelocityDirectionFrom=PTVD_AddRadial
	        Name="MeshEmitter6"
    End Object
    Emitters(18)=MeshEmitter'MeshEmitter6'
}