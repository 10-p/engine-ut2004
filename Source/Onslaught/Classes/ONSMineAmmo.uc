//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSMineAmmo extends Ammunition;

DefaultProperties
{
    ItemName="Parasite Mines" //our name
    IconMaterial=Material'HudContent.Generic.HUD'
    IconCoords=(X1=451,Y1=393,X2=495,Y2=441)

    PickupClass=class'ONSMineAmmoPickup' // what our ammo pickup class is
    MaxAmmo=25
    InitialAmount=4
}
