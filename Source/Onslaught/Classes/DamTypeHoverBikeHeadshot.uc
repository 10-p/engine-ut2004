class DamTypeHoverBikeHeadshot extends DamTypeRoadKill
	abstract;

defaultproperties
{
	DeathString="%k clipped off %o's head"

	bLocationalHit=true
	bNeverSevers=false
	bAlwaysSevers=true
	bSpecial=true
	VehicleClass=class'ONSHoverBike'
}
