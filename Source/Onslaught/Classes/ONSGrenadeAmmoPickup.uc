//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSGrenadeAmmoPickup extends UTAmmoPickup;

DefaultProperties
{
    InventoryType=class'ONSGrenadeAmmo' //what item to create in inventory

    AmmoAmount=5

    PickupMessage="You picked up some grenades"
    PickupSound=Sound'PickupSounds.FlakAmmoPickup'
    PickupForce="FlakAmmoPickup"  // jdf

    StaticMesh=StaticMesh'ONSWeapons-SM.GrenadeLauncherAmmo' //mesh to use
    DrawType=DT_StaticMesh
    DrawScale=0.25
}
