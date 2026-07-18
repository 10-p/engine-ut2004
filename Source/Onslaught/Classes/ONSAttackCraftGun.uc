//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSAttackCraftGun extends ONSWeapon;

#exec OBJ LOAD FILE=..\Animations\ONSWeapons-A.ukx

var class<Projectile> TeamProjectileClasses[2];
var float MinAim;

static function StaticPrecache(LevelInfo L)
{
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStarRed');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHeadRed');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHeadBlue');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.SmokePanels1');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar2');
    L.AddPrecacheMaterial(Material'EpicParticles.Flares.FlashFlare1');
    L.AddPrecacheMaterial(Material'EmitterTextures.MultiFrame.rockchunks02');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaFlare');
}

simulated function UpdatePrecacheMaterials()
{
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStarRed');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHeadRed');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHeadBlue');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.SmokePanels1');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar2');
    Level.AddPrecacheMaterial(Material'EpicParticles.Flares.FlashFlare1');
    Level.AddPrecacheMaterial(Material'EmitterTextures.MultiFrame.rockchunks02');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaFlare');

    Super.UpdatePrecacheMaterials();
}

function byte BestMode()
{
	local bot B;

	B = Bot(Instigator.Controller);
	if ( B == None )
		return 0;

	if ( (Vehicle(B.Enemy) != None)
	     && (B.Enemy.bCanFly || B.Enemy.IsA('ONSHoverCraft')) && (FRand() < 0.3 + 0.1 * B.Skill) )
		return 1;
	else
		return 0;
}

state ProjectileFireMode
{
	function Fire(Controller C)
	{
		if (Vehicle(Owner) != None && Vehicle(Owner).Team < 2)
			ProjectileClass = TeamProjectileClasses[Vehicle(Owner).Team];
		else
			ProjectileClass = TeamProjectileClasses[0];

		Super.Fire(C);
	}

	function AltFire(Controller C)
	{
		local ONSAttackCraftMissle M;
		local Vehicle V, Best;
		local float CurAim, BestAim;

		M = ONSAttackCraftMissle(SpawnProjectile(AltFireProjectileClass, True));
		if (M != None)
		{
			if (AIController(Instigator.Controller) != None)
			{
				V = Vehicle(Instigator.Controller.Enemy);
				if (V != None && (V.bCanFly || V.IsA('ONSHoverCraft')) && Instigator.FastTrace(V.Location, Instigator.Location))
					M.SetHomingTarget(V);
			}
			else
			{
				BestAim = MinAim;
				for (V = Level.Game.VehicleList; V != None; V = V.NextVehicle)
					if ((V.bCanFly || V.IsA('ONSHoverCraft')) && V != Instigator && Instigator.GetTeamNum() != V.GetTeamNum())
					{
						CurAim = Normal(V.Location - WeaponFireLocation) dot vector(WeaponFireRotation);
						if (CurAim > BestAim && Instigator.FastTrace(V.Location, Instigator.Location))
						{
							Best = V;
							BestAim = CurAim;
						}
					}
				if (Best != None)
					M.SetHomingTarget(Best);
			}
		}
	}
}

DefaultProperties
{
    Mesh=Mesh'ONSWeapons-A.PlasmaGun'
    YawBone=PlasmaGunBarrel
    YawStartConstraint=0
    YawEndConstraint=65535
    PitchBone=PlasmaGunBarrel
    PitchUpLimit=18000
    PitchDownLimit=49153
    FireSoundClass=sound'ONSVehicleSounds-S.LaserSounds.Laser01'
    AltFireSoundClass=sound'ONSVehicleSounds-S.AVRiL.AVRiLFire01'
    FireForce="Laser01"
    AltFireForce="Laser01"
    ProjectileClass=class'ONSAttackCraftPlasmaProjectileRed'
    TeamProjectileClasses(0)=class'ONSAttackCraftPlasmaProjectileRed'
    TeamProjectileClasses(1)=class'ONSAttackCraftPlasmaProjectileBlue'
    FireInterval=0.2
    AltFireProjectileClass=class'ONSAttackCraftMissle'
    AltFireInterval=3.0
    WeaponFireAttachmentBone=PlasmaGunBarrel
    WeaponFireOffset=0.0
    bAimable=True
    RotationsPerSecond=1.2
    DualFireOffset=50
    AIInfo(0)=(bLeadTarget=true,RefireRate=0.95)
    AIInfo(1)=(bLeadTarget=true,AimError=400,RefireRate=0.50)
    MinAim=0.900
}
