//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSHoverBikePlasmaProjectile extends ONSPlasmaProjectile;

DefaultProperties
{
    PlasmaEffectClass=class'Onslaught.ONSPurplePlasmaSmallFireEffect'
    HitEffectClass=class'Onslaught.ONSPlasmaHitPurple'
    Speed=500
    MaxSpeed=7000
    AccelerationMagnitude=16000
    Damage=30
    DamageRadius=190.0
    MyDamageType=class'DamTypeHoverBikePlasma'
}
