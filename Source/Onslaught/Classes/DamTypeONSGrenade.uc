//-----------------------------------------------------------
//
//-----------------------------------------------------------
class DamTypeONSGrenade extends WeaponDamageType;

DefaultProperties
{
    //death messages to the player
    DeathString="%o played with %k's happy fun ball..."
    MaleSuicide="Silly %o, grenades are for enemies..."
    FemaleSuicide="Silly %o, grenades are for enemies..."

    //what weapon class causes this message
    WeaponClass=class'ONSGrenadeLauncher'
    bDelayedDamage=true
}
