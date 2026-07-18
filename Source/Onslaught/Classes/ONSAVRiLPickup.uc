//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSAVRiLPickup extends UTWeaponPickup;

#exec OBJ LOAD FILE=VMWeaponsSM.usx

static function StaticPrecache(LevelInfo L)
{
	L.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.AVRiLtex');
	L.AddPrecacheMaterial(Texture'AW-2004Particles.Weapons.DustSmoke');
    L.AddPrecacheMaterial(Texture'ONSInterface-TX.avrilRETICLE');
    L.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.LGRreticleRed');
	L.AddPrecacheStaticMesh(StaticMesh'VMWeaponsSM.AVRiLGroup.AVRiLprojectileSM');
	L.AddPrecacheStaticMesh(StaticMesh'VMWeaponsSM.AVRiLsm');
}

simulated function UpdatePrecacheMaterials()
{
	Level.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.AVRiLtex');
	Level.AddPrecacheMaterial(Texture'VMParticleTextures.VehicleExplosions.VMExp2_framesANIM');
	Level.AddPrecacheMaterial(Texture'AW-2004Particles.Weapons.DustSmoke');
    Level.AddPrecacheMaterial(Texture'ONSInterface-TX.avrilRETICLE');
    Level.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.LGRreticleRed');
	super.UpdatePrecacheMaterials();
}

simulated function UpdatePrecacheStaticMeshes()
{
	super.UpdatePrecacheStaticMeshes();
	Level.AddPrecacheStaticMesh(StaticMesh'VMWeaponsSM.AVRiLGroup.AVRiLprojectileSM');
	Level.AddPrecacheStaticMesh(StaticMesh'VMWeaponsSM.AVRiLsm');
}

DefaultProperties
{
    InventoryType=class'ONSAVRiL'
    //pickup message displayed to the player
    PickupMessage="You got the AVRiL."
    PickupSound=Sound'PickupSounds.FlakCannonPickup'
    PickupForce="ONSAVRiLPickup"
    MaxDesireability=+0.7
    StaticMesh=StaticMesh'VMWeaponsSM.AVRiLsm'
    DrawType=DT_StaticMesh
    DrawScale=0.05
    Standup=(Y=0.25,Z=0.0)
}
