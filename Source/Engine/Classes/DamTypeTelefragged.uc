class DamTypeTelefragged extends DamageType
	abstract;

defaultproperties
{
	DeathString="%o was telefragged by %k"
	MaleSuicide="%o was telefragged by %k"
	FemaleSuicide="%o was telefragged by %k"

	bAlwaysSevers=true
    bAlwaysGibs=true
    GibPerterbation=1.0
    bLocationalHit=false
    bArmorStops=false
}