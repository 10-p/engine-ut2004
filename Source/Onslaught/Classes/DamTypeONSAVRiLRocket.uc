//-----------------------------------------------------------
//
//-----------------------------------------------------------
class DamTypeONSAVRiLRocket extends WeaponDamageType;

DefaultProperties
{
    //death messages to the player
    DeathString="%k blew %o away with an AVRiL."
    MaleSuicide="%o pointed his gun the wrong way."
    FemaleSuicide="%o pointed her gun the wrong way."

    //what weapon class causes this message
    WeaponClass=class'ONSAVRiL'

    VehicleDamageScaling=1.6
    bDelayedDamage=true
}
