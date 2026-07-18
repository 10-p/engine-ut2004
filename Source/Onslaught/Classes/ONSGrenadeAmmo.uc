//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSGrenadeAmmo extends Ammunition;

DefaultProperties
{
    ItemName="Grenades" //our name
    IconMaterial=Material'HudContent.Generic.HUD'
    IconCoords=(X1=460,Y1=343,X2=488,Y2=392)

    PickupClass=class'ONSGrenadeAmmoPickup' // what our ammo pickup class is
    MaxAmmo=50
    InitialAmount=10
}
