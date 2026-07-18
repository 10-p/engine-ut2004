class DamTypeAttackCraftMissle extends VehicleDamageType
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
    DeathString="%o couldn't avoid %k's air-to-air missile."
    MaleSuicide="%o blasted himself out of the sky."
    FemaleSuicide="%o blasted herself out of the sky."

    VehicleDamageScaling=1.5
    VehicleMomentumScaling=0.75
    bDelayedDamage=true
    VehicleClass=class'ONSAttackCraft'
}
