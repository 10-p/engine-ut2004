//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSMineLayerPickup extends UTWeaponPickup;

#exec OBJ LOAD FILE=VMWeaponsSM.usx

static function StaticPrecache(LevelInfo L)
{
	L.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.SpiderMineTEX');
	L.AddPrecacheMaterial(Shader'VMWeaponsTX.PlayerWeaponsGroup.ParasiteMineImplantTEXshad');
	L.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.SpiderMineBLUETEX');
	L.AddPrecacheMaterial(Texture'XGameShaders.WeaponShaders.bio_flash');
	L.AddPrecacheMaterial(Texture'AW-2004Particles.Weapons.BeamFragment');
	L.AddPrecacheStaticMesh(default.StaticMesh);
}

simulated function UpdatePrecacheMaterials()
{
	Level.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.SpiderMineTEX');
	Level.AddPrecacheMaterial(Shader'VMWeaponsTX.PlayerWeaponsGroup.ParasiteMineImplantTEXshad');
	Level.AddPrecacheMaterial(Texture'VMWeaponsTX.PlayerWeaponsGroup.SpiderMineBLUETEX');
	Level.AddPrecacheMaterial(Texture'XGameShaders.WeaponShaders.bio_flash');
	Level.AddPrecacheMaterial(Texture'AW-2004Particles.Weapons.BeamFragment');
	super.UpdatePrecacheMaterials();
}


DefaultProperties
{
    InventoryType=class'ONSMineLayer'
    //pickup message displayed to the player
    PickupMessage="You got the Mine Layer."
    PickupSound=Sound'PickupSounds.FlakCannonPickup'
    PickupForce="ONSMineLayerPickup"

	MaxDesireability=+0.7
    //mesh & draw type to use
    StaticMesh=StaticMesh'ONSWeapons-SM.MineLayerPickup'
    DrawType=DT_StaticMesh
    DrawScale=0.3
    LockerOffset=35.0
    Standup=(Y=0.25,Z=0.0)
}
