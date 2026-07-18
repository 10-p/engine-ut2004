//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSMineAmmoPickup extends UTAmmoPickup;

DefaultProperties
{
    InventoryType=class'ONSMineAmmo' //what item to create in inventory

    AmmoAmount=8

    PickupMessage="You picked up some parasite mines"
    PickupSound=Sound'PickupSounds.FlakAmmoPickup'
    PickupForce="FlakAmmoPickup"  // jdf

    StaticMesh=StaticMesh'ONSWeapons-SM.MineLayerAmmo' //mesh to use
    DrawType=DT_StaticMesh
    DrawScale=0.4
}
