class DamTypeAttackCraftPlasma extends VehicleDamageType
	abstract;

static function ScoreKill(Controller Killer, Controller Killed)
{
	if (Killed != None && Killer != Killed && Vehicle(Killed.Pawn) != None && Vehicle(Killed.Pawn).bCanFly)
	{
		//Maybe add to game stats?
		if (PlayerController(Killer) != None)
			PlayerController(Killer).ReceiveLocalizedMessage(class'ONSVehicleKillMessage', 6);
	}
}

defaultproperties
{
	DeathString="%k's Raptor filled %o with plasma."
	MaleSuicide="%o fried himself with his own plasma blast."
	FemaleSuicide="%o fried herself with her own plasma blast."
	FlashFog=(X=700.00000,Y=0.000000,Z=0.00000)
	bDetonatesGoop=true
	bDelayedDamage=true
	VehicleClass=class'ONSAttackCraft'
}
