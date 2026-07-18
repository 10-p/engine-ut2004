class DamTypeTankShell extends VehicleDamageType
	abstract;

static function GetHitEffects(out class<xEmitter> HitEffects[4], int VictimHealth )
{
    HitEffects[0] = class'HitSmoke';

    if( VictimHealth <= 0 )
        HitEffects[1] = class'HitFlameBig';
    else if ( FRand() < 0.8 )
        HitEffects[1] = class'HitFlame';
}

static function ScoreKill(Controller Killer, Controller Killed)
{
	if (Killed != None && Killer != Killed && Vehicle(Killed.Pawn) != None && Vehicle(Killed.Pawn).bCanFly)
	{
		//Maybe add to game stats?
		if (PlayerController(Killer) != None)
			PlayerController(Killer).ReceiveLocalizedMessage(class'ONSVehicleKillMessage', 5);
	}
}

defaultproperties
{
	DeathString="%k's tank shell blew %o into flaming bits."
	MaleSuicide="%o fired his rocket prematurely."
	FemaleSuicide="%o fired her rocket prematurely."

	bDetonatesGoop=true
	KDamageImpulse=20000
	VehicleMomentumScaling=1.3
	bThrowRagdoll=true
	GibPerterbation=0.15
	bFlaming=true
	bDelayedDamage=true
	VehicleClass=class'ONSHoverTank'
}
