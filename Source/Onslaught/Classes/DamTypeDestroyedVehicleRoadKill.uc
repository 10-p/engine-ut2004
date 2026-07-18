class DamTypeDestroyedVehicleRoadKill extends DamageType
	abstract;

defaultproperties
{
	DeathString="A vehicle %k destroyed crushed %o"
	MaleSuicide="%o couldn't avoid the vehicle he destroyed."
	FemaleSuicide="%o couldn't avoid the vehicle she destroyed."

	GibPerterbation=0.5
	GibModifier=2.0
	bLocationalHit=false
	bNeverSevers=true
	bKUseTearOffMomentum=true
	bExtraMomentumZ=false
}
