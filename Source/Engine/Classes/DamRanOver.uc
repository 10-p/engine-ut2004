class DamRanOver extends DamageType
	abstract;

defaultproperties
{
	DeathString="%k ran over %o"
	MaleSuicide="%o ran over himself"
	FemaleSuicide="%o ran over herself"

    GibPerterbation=0.5
    GibModifier=2.0
    bLocationalHit=false
    bNeverSevers=true
    bKUseTearOffMomentum=true
    bExtraMomentumZ=false
    bVehicleHit=true
}
