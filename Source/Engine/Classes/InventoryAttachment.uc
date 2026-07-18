class InventoryAttachment extends Actor
	native
	nativereplication;

cpptext
{
	INT* GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

var bool bFastAttachmentReplication; // only replicates the subset of actor properties needed by basic attachments whose 
									 // common properties don't vary from their defaults
var int inventoryattachment_dummy; // packing for hammer. disregard.  --ryan.

function InitFor(Inventory I)
{
	Instigator = I.Instigator;
}
		
defaultproperties
{
	bOnlyDrawIfAttached=true
	NetUpdateFrequency=10
	DrawType=DT_Mesh
	RemoteRole=ROLE_SimulatedProxy
	bAcceptsProjectors=True
	bUseLightingFromBase=True
    bOnlyDirtyReplication=true
    bFastAttachmentReplication=true
    AttachmentBone=righthand
}