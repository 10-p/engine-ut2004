class DamTypePRVCombo extends VehicleDamageType
	abstract;

defaultproperties
{
    DeathString="%o couldn't escape the awesome power of %k's skymine combo."
    MaleSuicide="%o was a little hasty detonating his skymines."
    FemaleSuicide="%o was a little hasty detonation her skymines."

    VehicleMomentumScaling=0.5
    VehicleDamageScaling=0.75
    bDelayedDamage=true
    VehicleClass=class'ONSPRVSideGunPawn'
}

