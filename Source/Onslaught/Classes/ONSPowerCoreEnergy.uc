//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPowerCoreEnergy extends Actor
    abstract;

DefaultProperties
{
    DrawType=DT_StaticMesh
    bMovable=False
    bStasis=True
    RemoteRole=ROLE_None
    StaticMesh=StaticMesh'VMStructures.CoreGroup.CoreEnergy'

    bCollideActors=False
    bCollideWorld=False
    bBlockKarma=False
    bIgnoreEncroachers=True
    bBlockActors=False
    bProjTarget=False
    bBlockZeroExtentTraces=False
    bBlockNonZeroExtentTraces=False
}
