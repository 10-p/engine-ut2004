class DamTypeRoadkill extends VehicleDamageType
	abstract;

var int MessageSwitchBase, NumMessages;

static function string DeathMessage(PlayerReplicationInfo Killer, PlayerReplicationInfo Victim)
{
	if (default.VehicleClass != None)
		return Default.DeathString @ default.VehicleClass.default.VehiclePositionString $ ".";
	else
		return Default.DeathString $ ".";
}

static function ScoreKill(Controller Killer, Controller Killed)
{
	local TeamPlayerReplicationInfo TPRI;

	if (Killer == Killed)
		return;

	TPRI = TeamPlayerReplicationInfo(Killer.PlayerReplicationInfo);
	if (TPRI != None)
	{
		TPRI.ranovercount++;
		if (TPRI.ranovercount == 10 && UnrealPlayer(Killer) != None)
		{
			UnrealPlayer(Killer).ClientDelayedAnnouncementNamed('RoadRampage', 15);
			return;
		}
	}

	if (PlayerController(Killer) != None)
		PlayerController(Killer).ReceiveLocalizedMessage(Default.MessageClass, Rand(Default.NumMessages) + Default.MessageSwitchBase);
}

defaultproperties
{
	DeathString="%k ran over %o"
	MaleSuicide="%o ran over himself."
	FemaleSuicide="%o ran over herself."

	GibPerterbation=0.5
	GibModifier=2.0
	bLocationalHit=false
	bNeverSevers=true
	bKUseTearOffMomentum=true
	bExtraMomentumZ=false
	bVehicleHit=true
	
	MessageClass=class'ONSVehicleKillMessage'
	NumMessages=4
}
