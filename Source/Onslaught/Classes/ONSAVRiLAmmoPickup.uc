//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSAVRiLAmmoPickup extends UTAmmoPickup;

DefaultProperties
{
    InventoryType=class'ONSAVRiLAmmo' //what item to create in inventory

    AmmoAmount=5

    PickupMessage="You picked up some anti-vehicle rockets"
    PickupSound=Sound'PickupSounds.FlakAmmoPickup'
    PickupForce="FlakAmmoPickup"  // jdf

    //TEMPORARY - FIXME
    StaticMesh=StaticMesh'ONSWeapons-SM.AVRiLAmmo' //mesh to use
    DrawType=DT_StaticMesh
    DrawScale=0.4
}
