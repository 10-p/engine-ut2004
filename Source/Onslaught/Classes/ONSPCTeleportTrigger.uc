//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPCTeleportTrigger extends Triggers;

function UsedBy(Pawn user)
{
	if (Owner != None)
		Owner.UsedBy(user);
}

function Touch(Actor Other)
{
	local Pawn P;

	//hack so bots realize they're close enough for node teleporting
	P = Pawn(Other);
	if (P != None && AIController(P.Controller) != None && P.Controller.RouteGoal == Owner)
		P.Controller.MoveTimer = -1;
}

DefaultProperties
{
    CollisionRadius=230.0
    CollisionHeight=200.0
    bNoDelete=False
    bStatic=False
    bStasis=true
}
