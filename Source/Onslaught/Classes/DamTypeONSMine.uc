//-----------------------------------------------------------
//
//-----------------------------------------------------------
class DamTypeONSMine extends WeaponDamageType;

DefaultProperties
{
    //death messages to the player
    DeathString="%o trespassed on %k's property"
    MaleSuicide="%o tried out one of his own mines... it worked!"
    FemaleSuicide="%o tried out one of her own mines... it worked!"

    //what weapon class causes this message
    WeaponClass=class'ONSMineLayer'

    VehicleDamageScaling=1.5
    bDelayedDamage=true
}
