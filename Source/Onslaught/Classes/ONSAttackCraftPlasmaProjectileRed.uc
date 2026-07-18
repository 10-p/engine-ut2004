//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSAttackCraftPlasmaProjectileRed extends ONSPlasmaProjectile;

DefaultProperties
{
    MyDamageType=class'DamTypeAttackCraftPlasma'
    PlasmaEffectClass=class'Onslaught.ONSRedPlasmaFireEffect'
    HitEffectClass=class'Onslaught.ONSPlasmaHitRed'
    Damage=25
    DamageRadius=200.0
    Speed=1500
    MaxSpeed=12500
    AccelerationMagnitude=24000
}
