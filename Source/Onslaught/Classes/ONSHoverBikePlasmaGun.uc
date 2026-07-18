//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSHoverBikePlasmaGun extends ONSWeapon;

#exec OBJ LOAD FILE=..\Animations\ONSWeapons-A.ukx

static function StaticPrecache(LevelInfo L)
{
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHead');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.SmokePanels1');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar2');
    L.AddPrecacheMaterial(Material'EpicParticles.Flares.FlashFlare1');
    L.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHeadDesat');
}

simulated function UpdatePrecacheMaterials()
{
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHead');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.SmokePanels1');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaStar2');
    Level.AddPrecacheMaterial(Material'EpicParticles.Flares.FlashFlare1');
    Level.AddPrecacheMaterial(Material'AW-2004Particles.Weapons.PlasmaHeadDesat');

    Super.UpdatePrecacheMaterials();
}

state ProjectileFireMode
{
    function AltFire(Controller C)
    {
    }
}

DefaultProperties
{
    Mesh=Mesh'ONSWeapons-A.PlasmaGun'
    YawBone=PlasmaGunBarrel
    YawStartConstraint=57344
    YawEndConstraint=8192
    PitchBone=PlasmaGunBarrel
    PitchUpLimit=5000
    PitchDownLimit=60000
    FireSoundClass=sound'ONSVehicleSounds-S.HoverBike.HoverBikeFire01'
    FireForce="HoverBikeFire"
    ProjectileClass=class'Onslaught.ONSHoverBikePlasmaProjectile'
    FireInterval=0.2
    AltFireInterval=0.5
    WeaponFireAttachmentBone=PlasmaGunBarrel
    WeaponFireOffset=25.0
    bAimable=True
    RotationsPerSecond=0.8
    DualFireOffset=25
    AIInfo(0)=(bLeadTarget=true)
    AIInfo(1)=(bLeadTarget=true)
}
